#include "common.h"

int main(){
	pid_t pid = getpid();

	//30% szans ze pasazer ma rower
	srand(time(NULL) ^ pid);
	int ma_rower = (rand() % 100) < 30;

	printf("Pasazer przybyl do portu pid: %d%s", pid, ma_rower ? " z rowerem\n" : "\n");

	//podlaczenie do zasobow
	key_t key = ftok(PATH_NAME, PROJECT_ID);
	if (key == -1){
		perror("Pasazer - blad ftok");
		return 1;
	}

	int shmid = shmget(key, sizeof(StanStatku), 0666);
	int semid = semget(key, LICZBA_SEM, 0666);

	if(shmid == -1 || semid == -1) {
		return 1; //zasoby usuniete - koniec
	}

	StanStatku *wspolne = (StanStatku*) shmat(shmid, NULL, 0);

	if (wspolne == (void*)-1){
		//jesli main usunal zasoby pasazer konczy dzialanie
		return 1;
	}


	//czekanie na gotowosc statku
	int czekanie = 0;
	while (1) {
		if (zajmij_zasob(semid, SEM_DOSTEP) == -1) {
			shmdt(wspolne);
			return 0;
		}

		int plynie = wspolne->czy_plynie;
        	int ludzi = wspolne->pasazerowie_statek;
        	int rowerow = wspolne->rowery_statek;
        	int koniec = wspolne->koniec_symulacji;
        	int kierunek = wspolne->kierunek_mostka;

		zwolnij_zasob(semid, SEM_DOSTEP);

		if (koniec) {
            		printf("Symulacja zakonczona pasazer(pid: %d) wraca do domu\n", pid);
            		shmdt(wspolne);
            		return 0;
        	}

		//sprawdza czy moze wejsc
		int moze_wejsc = 0;
		if(!plynie && kierunek == KIERUNEK_NA_STATEK) {
			if(ludzi < N) {
				if(!ma_rower || rowerow < M) {
					moze_wejsc = 1;
				}
			}
		}

		if (moze_wejsc) {
			break;
		}

		if (czekanie % 10 == 0){
			printf("Pasazer (pid: %d) czeka na statek (plynie: %s, ludzi: %d/%d)\n",
				pid, plynie ? "TAK" : "NIE", ludzi, N);
		}
		czekanie++;
		usleep(500000);
	}

	printf("Pasazer (pid: %d) probuje wejsc na mostek\n", pid);

	//proba wejscia na mostek, jesli SEM_MOSTEK == 0 to tu sie zatrzyma
	if(zajmij_zasob(semid, SEM_MOSTEK) == -1){
		shmdt(wspolne);
		return 0;
	}

	if(ma_rower){
		if(zajmij_zasob(semid, SEM_MOSTEK) == -1){
			zwolnij_zasob(semid, SEM_MOSTEK);
			shmdt(wspolne);
			return 0;
		}
	}

	//zajecie miejsca na mostku
	if (zajmij_zasob(semid, SEM_DOSTEP) == -1) {
        	zwolnij_zasob(semid, SEM_MOSTEK);
		if(ma_rower) zwolnij_zasob(semid, SEM_MOSTEK);
        	shmdt(wspolne);
        	return 0;
    	}

	int miejsca_mostek = ma_rower ? 2 : 1; //rower zajmuje 2 miejsca
	wspolne->pasazerowie_mostek += miejsca_mostek;
	printf("Pasazer (%d) wszedl na mostek (zajmuje %d miejsc, mostek: %d/%d)\n",
		pid, miejsca_mostek, wspolne->pasazerowie_mostek, K);

	zwolnij_zasob(semid, SEM_DOSTEP);

	//sprawdzenie czy statek nie odplynal
	if (zajmij_zasob(semid, SEM_DOSTEP) == -1) {
        	zwolnij_zasob(semid, SEM_MOSTEK);
        	if (ma_rower) zwolnij_zasob(semid, SEM_MOSTEK);
        	shmdt(wspolne);
        	return 0;
    	}

	int czy_plynie_teraz = wspolne->czy_plynie;
	zwolnij_zasob(semid, SEM_DOSTEP);

	if(czy_plynie_teraz) {
		printf("Pasazer (%d) - statek odplynal schodzi z mostka\n", pid);

		if (zajmij_zasob(semid, SEM_DOSTEP) != -1) {
            		wspolne->pasazerowie_mostek -= miejsca_mostek;
			zwolnij_zasob(semid, SEM_DOSTEP);
        	}

        	zwolnij_zasob(semid, SEM_MOSTEK);
        	if (ma_rower) zwolnij_zasob(semid, SEM_MOSTEK);
        	shmdt(wspolne);
        	return 0;
    	}

	//rezerwacja miejsca
	if (zajmij_zasob(semid, SEM_STATEK_LUDZIE) == -1) {
		if (zajmij_zasob(semid, SEM_DOSTEP) != -1) {
			wspolne->pasazerowie_mostek -= miejsca_mostek;
			zwolnij_zasob(semid, SEM_DOSTEP);
		}
		zwolnij_zasob(semid, SEM_MOSTEK);
		if(ma_rower) zwolnij_zasob(semid, SEM_MOSTEK);
		shmdt(wspolne);
		return 0;
	}

	//rezerwajca miejsca z rowerem
	if(ma_rower){
		if(zajmij_zasob(semid, SEM_STATEK_ROWERY) == -1){
			zwolnij_zasob(semid, SEM_STATEK_LUDZIE);
			if(zajmij_zasob(semid, SEM_DOSTEP) != -1) {
				wspolne->pasazerowie_mostek -= miejsca_mostek;
				zwolnij_zasob(semid, SEM_DOSTEP);
			}
			zwolnij_zasob(semid, SEM_MOSTEK);
			zwolnij_zasob(semid, SEM_MOSTEK);
			shmdt(wspolne);
			return 0;
		}
	}

	//wejscie na statek
	if (zajmij_zasob(semid, SEM_DOSTEP) == -1) {
		zwolnij_zasob(semid, SEM_STATEK_LUDZIE);
        	if (ma_rower) zwolnij_zasob(semid, SEM_STATEK_ROWERY);
        	if (zajmij_zasob(semid, SEM_DOSTEP) != -1) {
            		wspolne->pasazerowie_mostek -= miejsca_mostek;
            		zwolnij_zasob(semid, SEM_DOSTEP);
        	}
        	zwolnij_zasob(semid, SEM_MOSTEK);
        	if (ma_rower) zwolnij_zasob(semid, SEM_MOSTEK);
        	shmdt(wspolne);
        	return 0;
    	}

	wspolne->pasazerowie_statek++;
	if(ma_rower) wspolne ->rowery_statek++;
	wspolne->pasazerowie_mostek -= miejsca_mostek;

	printf("Pasazer (%d) wszedl na statek (Pasazerowie: %d/%d, rowery: %d/%d)\n",
		pid, wspolne->pasazerowie_statek, N, wspolne->rowery_statek, M);

	zwolnij_zasob(semid, SEM_DOSTEP); //koniec edycji pamieci

	zwolnij_zasob(semid, SEM_MOSTEK); //zwolnienie miejsca na mostku
	if(ma_rower) zwolnij_zasob(semid, SEM_MOSTEK);

	sleep(T1 + T2);

	//czekanie na kierunek mostka
	while(1){
		if(zajmij_zasob(semid, SEM_DOSTEP) == -1) {
			shmdt(wspolne);
			return 0;
		}
		int kierunek = wspolne->kierunek_mostka;
		zwolnij_zasob(semid, SEM_DOSTEP);

		if (kierunek == KIERUNEK_ZE_STATKU) break;

		usleep(100000);
	}

	//zejscie ze statku
	if(zajmij_zasob(semid, SEM_MOSTEK) == -1){
		shmdt(wspolne);
		return 0;
	}

	if(ma_rower){
		if (zajmij_zasob(semid, SEM_MOSTEK) == -1){
			zwolnij_zasob(semid, SEM_MOSTEK);
			shmdt(wspolne);
			return 0;
		}
	}

	if(zajmij_zasob(semid, SEM_DOSTEP) == -1) {
		zwolnij_zasob(semid, SEM_MOSTEK);
		if (ma_rower) zwolnij_zasob(semid, SEM_MOSTEK);
		shmdt(wspolne);
		return 0;
	}

	wspolne->pasazerowie_mostek += miejsca_mostek;

	wspolne->pasazerowie_statek--;
	if(ma_rower) wspolne->rowery_statek--;
	printf("Pasazer %d schodzi, na statku zostalo %d pasazerow, %d rowerow\n", pid, wspolne->pasazerowie_statek, wspolne->rowery_statek);

	zwolnij_zasob(semid, SEM_DOSTEP);

	usleep(100000);

	if(zajmij_zasob(semid, SEM_DOSTEP) == -1) {
		zwolnij_zasob(semid, SEM_STATEK_LUDZIE);
		if (ma_rower) zwolnij_zasob(semid, SEM_STATEK_ROWERY);
		zwolnij_zasob(semid, SEM_MOSTEK);
		if (ma_rower) zwolnij_zasob(semid, SEM_MOSTEK);
    		shmdt(wspolne);
    		return 0;
	}

	wspolne->pasazerowie_mostek -= miejsca_mostek;
	zwolnij_zasob(semid, SEM_DOSTEP);

	zwolnij_zasob(semid, SEM_STATEK_LUDZIE);
	if (ma_rower) zwolnij_zasob(semid, SEM_STATEK_ROWERY);

	zwolnij_zasob(semid, SEM_MOSTEK);
	if (ma_rower) zwolnij_zasob(semid, SEM_MOSTEK);

	shmdt(wspolne);
	printf("Pasazer (%d) opuscil statek\n", pid);
	return 0;
}

