#include "common.h"
#include "logger.h"

int N, M, K, T1, T2, R;

int main(){
	if(wczytaj_parametry_z_pliku() == -1) {
		fprintf(stderr, "Pasazer: blad wczytania parametrow\n");
		exit(1);
	}

	pid_t pid = getpid();

	//30% szans ze pasazer ma rower
	srand(time(NULL) ^ pid);
	int ma_rower = (rand() % 100) < 30;

	//50% szans na przystanek wawel/tyniec
	int moj_przystanek = (rand() % 2);

	printf("Pasazer przybyl do portu w %s (pid: %d)%s",
		get_przystanek_nazwa(moj_przystanek), pid, ma_rower ? " z rowerem\n" : "\n");
	logger_pasazer_event(EVENT_PASAZER_PRZYBYCIE, pid, ma_rower, "przybyl do portu");

	//podlaczenie do zasobow
	key_t key = ftok(PATH_NAME, PROJECT_ID);
	if (key == -1){
		perror("Pasazer - blad ftok");
		logger_error_errno(EVENT_BLAD_SYSTEM, "Pasazer - blad ftok");
		return 1;
	}

	int shmid = shmget(key, sizeof(StanStatku), 0600);
	int semid = semget(key, LICZBA_SEM, 0600);

	if(shmid == -1 || semid == -1) {
		return 1; //zasoby usuniete - koniec
	}

	logger_init("tramwaj_wodny.log", 0);
	logger_set_semid(semid);

	StanStatku *wspolne = (StanStatku*) shmat(shmid, NULL, 0);

	if (wspolne == (void*)-1){
		//jesli main usunal zasoby pasazer konczy dzialanie
		return 1;
	}

	//czekanie na gotowosc statku
	int czekanie = 0;
	int wyswietlono_info = 0;
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
		int przystanek_statku = wspolne->aktualny_przystanek;

		zwolnij_zasob(semid, SEM_DOSTEP);

		if (koniec) {
			if(!wyswietlono_info){
            			printf("Symulacja zakonczona pasazer(pid: %d) wraca do domu\n", pid);
            			logger_pasazer_event(EVENT_PASAZER_OPUSZCZENIE, pid, ma_rower, "symulacja zakonczona");
				if (zajmij_zasob(semid, SEM_DOSTEP) != -1) {
					wspolne->pasazerow_odrzuconych++;
					zwolnij_zasob(semid, SEM_DOSTEP);
				}
				wyswietlono_info = 1;
			}
			shmdt(wspolne);
            		return 0;
        	}

		int moze_wejsc = 0;
		if(!plynie && kierunek == KIERUNEK_NA_STATEK && przystanek_statku == moj_przystanek) {
			if(ludzi < N) {
				if(!ma_rower || rowerow < M) {
					moze_wejsc = 1;
				}
			}
		}

		if (moze_wejsc) {
			break;
		}

		if (czekanie == 0){
			printf("Pasazer (pid: %d) czeka na statek w %s\n",
				pid, get_przystanek_nazwa(moj_przystanek));
		}

		if (czekanie % 20 == 0 && czekanie > 0) {
			printf("Pasazer (pid: %d) nadal czeka (plynie: %s, ludzi: %d/%d)%s\n",
				pid, plynie ? "TAK" : "NIE", ludzi, N, ma_rower ? " z rowerem" : "");
		}

		czekanie++;
		usleep(500000);

		if (czekanie > 100) {
                        printf("Pasazer (pid: %d) rezygnuje - za dlugie oczekiwanie\n", pid);
			logger_pasazer_event(EVENT_PASAZER_OPUSZCZENIE, pid, ma_rower, "za dlugie oczekiwanie");
			if (zajmij_zasob(semid, SEM_DOSTEP) != -1) {
				wspolne->pasazerow_odrzuconych++;
				zwolnij_zasob(semid, SEM_DOSTEP);
			}
			shmdt(wspolne);
			return 0;
		}
	}

	printf("Pasazer (pid: %d) probuje wejsc na mostek%s\n", pid, ma_rower ? " z rowerem" : "");

	int miejsca_mostek = ma_rower ? 2 : 1;
	int boarded = 0;

	while(!boarded){
		//proba wejscia na mostek, jesli SEM_MOSTEK == 0 to tu sie zatrzyma
		if(zajmij_zasob_ilosc(semid, SEM_MOSTEK, miejsca_mostek) == -1){
			logger_error_errno(EVENT_BLAD_SYSTEM, "Pasazer - blad zajecia mostka");
			shmdt(wspolne);
			return 0;
		}
		//zajecie miejsca na mostku
		if (zajmij_zasob(semid, SEM_DOSTEP) == -1) {
        		zwolnij_zasob_ilosc(semid, SEM_MOSTEK, miejsca_mostek);
        		shmdt(wspolne);
        		return 0;
    		}

                int plynie = wspolne->czy_plynie;
                int kierunek = wspolne->kierunek_mostka;
                int przystanek_statku = wspolne->aktualny_przystanek;
                int ludzie = wspolne->pasazerowie_statek;
                int rowerow = wspolne->rowery_statek;
                int pas_mostek = wspolne->pasazerowie_mostek;

		if (plynie || kierunek != KIERUNEK_NA_STATEK || przystanek_statku != moj_przystanek
                        || pas_mostek != 0
                        || ludzie >= N || (ma_rower && rowerow >= M)) {
                        zwolnij_zasob(semid, SEM_DOSTEP);
                        zwolnij_zasob_ilosc(semid, SEM_MOSTEK, miejsca_mostek);
			usleep(200000);
			continue;
                }

		wspolne->pasazerowie_mostek += miejsca_mostek;
		int przystanek = wspolne->aktualny_przystanek;

		printf("Pasazer (%d) wszedl na mostek w %s (zajmuje %d miejsc, mostek: %d/%d)\n",
			pid,get_przystanek_nazwa(przystanek), miejsca_mostek, wspolne->pasazerowie_mostek, K);
		logger_pasazer_event(EVENT_PASAZER_WEJSCIE_MOSTEK, pid, ma_rower, "wszedl na mostek");

		zwolnij_zasob(semid, SEM_DOSTEP);


		if (zajmij_zasob(semid, SEM_STATEK_LUDZIE) == -1) {
                        // nie udalo sie zarezerwowac, cofnij zmiany na mostku
                        if (zajmij_zasob(semid, SEM_DOSTEP) != -1) {
                                wspolne->pasazerowie_mostek -= miejsca_mostek;
                                zwolnij_zasob(semid, SEM_DOSTEP);
                        }
                        zwolnij_zasob_ilosc(semid, SEM_MOSTEK, miejsca_mostek);
                        shmdt(wspolne);
                        return 0;
                }

                if(ma_rower){
                        if(zajmij_zasob(semid, SEM_STATEK_ROWERY) == -1){
                                zwolnij_zasob(semid, SEM_STATEK_LUDZIE);
                                if(zajmij_zasob(semid, SEM_DOSTEP) != -1) {
                                        wspolne->pasazerowie_mostek -= miejsca_mostek;
                                        zwolnij_zasob(semid, SEM_DOSTEP);
                                }
                                zwolnij_zasob_ilosc(semid, SEM_MOSTEK, miejsca_mostek);
                                shmdt(wspolne);
                                return 0;
                        }
                }

                if (zajmij_zasob(semid, SEM_DOSTEP) == -1) {
                        zwolnij_zasob(semid, SEM_STATEK_LUDZIE);
                        if (ma_rower) zwolnij_zasob(semid, SEM_STATEK_ROWERY);
                        if (zajmij_zasob(semid, SEM_DOSTEP) != -1) {
                                wspolne->pasazerowie_mostek -= miejsca_mostek;
                                zwolnij_zasob(semid, SEM_DOSTEP);
                        }
                        zwolnij_zasob_ilosc(semid, SEM_MOSTEK, miejsca_mostek);
                        shmdt(wspolne);
                        return 0;
                }

                wspolne->pasazerowie_statek++;
                if(ma_rower) wspolne ->rowery_statek++;
                wspolne->pasazerowie_mostek -= miejsca_mostek;

                if (przystanek == PRZYSTANEK_WAWEL) {
                        wspolne->total_pasazerow_wawel++;
                } else {
                        wspolne->total_pasazerow_tyniec++;
                }

                printf("Pasazer (%d) wszedl na statek (Pasazerowie: %d/%d, rowery: %d/%d)\n",
                        pid, wspolne->pasazerowie_statek, N, wspolne->rowery_statek, M);

                logger_pasazer_event(EVENT_PASAZER_WEJSCIE_STATEK, pid, ma_rower, "wszedl na statek");

                zwolnij_zasob(semid, SEM_DOSTEP); //koniec edycji pamieci

                zwolnij_zasob_ilosc(semid, SEM_MOSTEK, miejsca_mostek);

                boarded = 1;
        }

	sleep(T1 + T2);

	//czekanie na kierunek mostka
	int oczekiwanie_wyjscie = 0;
	while(1){
		if(zajmij_zasob(semid, SEM_DOSTEP) == -1) {
			shmdt(wspolne);
			return 0;
		}
		int kierunek = wspolne->kierunek_mostka;
		int nowy_przystanek = wspolne->aktualny_przystanek;
		int koniec = wspolne->koniec_symulacji;

		zwolnij_zasob(semid, SEM_DOSTEP);

		if (koniec) {
			shmdt(wspolne);
			return 0;
		}

		if (kierunek == KIERUNEK_ZE_STATKU){
			moj_przystanek = nowy_przystanek;
			break;
		}

		if (oczekiwanie_wyjscie % 20 == 0 && oczekiwanie_wyjscie > 0) {
			printf("Pasazer (pid: %d) czeka na sygnal do zejscia\n", pid);
		}

		oczekiwanie_wyjscie++;
		usleep(100000);
	}

	if(zajmij_zasob(semid, SEM_DOSTEP) == -1){
		shmdt(wspolne);
		return 0;
	}

	wspolne->pasazerowie_statek--;
	if(ma_rower) wspolne->rowery_statek--;

	printf("Pasazer %d schodzi, na statku zostalo %d pasazerow, %d rowerow\n",
		pid, wspolne->pasazerowie_statek, wspolne->rowery_statek);

	logger_pasazer_event(EVENT_PASAZER_WYJSCIE_STATEK, pid, ma_rower, "schodzi ze statku");

	zwolnij_zasob(semid, SEM_DOSTEP);

	zwolnij_zasob(semid, SEM_STATEK_LUDZIE);
	if (ma_rower) zwolnij_zasob(semid, SEM_STATEK_ROWERY);

	if(zajmij_zasob_ilosc(semid, SEM_MOSTEK, miejsca_mostek) == -1){
		shmdt(wspolne);
		return 0;
	}

	if(zajmij_zasob(semid, SEM_DOSTEP) == -1) {
		zwolnij_zasob_ilosc(semid, SEM_MOSTEK, miejsca_mostek);
		shmdt(wspolne);
		return 0;
	}

	wspolne->pasazerowie_mostek += miejsca_mostek;
	zwolnij_zasob(semid, SEM_DOSTEP);

	usleep(100000);

	if(zajmij_zasob(semid, SEM_DOSTEP) == -1) {
		zwolnij_zasob_ilosc(semid, SEM_MOSTEK, miejsca_mostek);
		shmdt(wspolne);
		return 0;
	}

	wspolne->pasazerowie_mostek -= miejsca_mostek;
	zwolnij_zasob(semid, SEM_DOSTEP);

	zwolnij_zasob_ilosc(semid, SEM_MOSTEK, miejsca_mostek);

	shmdt(wspolne);
	printf("Pasazer (%d) opuscil statek\n", pid);
	logger_pasazer_event(EVENT_PASAZER_OPUSZCZENIE, pid, ma_rower, "pomyslnie ukonczyl podroz");
	return 0;
}

