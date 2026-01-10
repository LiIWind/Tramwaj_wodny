#include "common.h"

int main(){
	pid_t pid = getpid();
	printf("Pasazer przybyl do portu pid: %d\n", pid);

	//podlaczenie do zasobow
	key_t key = ftok(PATH_NAME, PROJECT_ID);
	int shmid = shmget(key, sizeof(StanStatku), 0666);
	int semid = semget(key, LICZBA_SEM, 0666);

	if(shmid == -1 || semid == -1) return 1;

	StanStatku *wspolne = (StanStatku*) shmat(shmid, NULL, 0);

	if (wspolne == (void*)-1){
		//jesli main usunal zasoby pasazer konczy dzialanie
		return 1;
	}

	while (1){
		if(zajmij_zasob(semid, SEM_DOSTEP) == -1) return 0;
		int stan_statku = wspolne->czy_plynie;
		int liczba_ludzi = wspolne->pasazerowie_statek;
		zwolnij_zasob(semid, SEM_DOSTEP);

		if(stan_statku == 1 || liczba_ludzi >= N){
			usleep(100000);;
		}
		else{
			break;
		}
	}

	//proba wejscia na mostek, jesli SEM_MOSTEK == 0 to tu sie zatrzyma
	if(zajmij_zasob(semid, SEM_MOSTEK) == -1) return 0;

	//proba wejscia na statek
	if(zajmij_zasob(semid, SEM_STATEK) == -1) return 0;

	//zajecie pamieci zeby inny zasob nie pisal nic w tym momencie
	if(zajmij_zasob(semid, SEM_DOSTEP) == -1) return 0;

	wspolne->pasazerowie_mostek++; //wejscie na mostek
	printf("Pasazer (%d) wszedl na mostek (Mostek: %d/%d)\n", pid, wspolne->pasazerowie_mostek, K);

	wspolne->pasazerowie_statek++;
	wspolne->pasazerowie_mostek--;

	printf("Pasazer (%d) wszedl na statek (Statek: %d/%d)\n", pid, wspolne->pasazerowie_statek, N);

	zwolnij_zasob(semid, SEM_DOSTEP); //koniec edycji pamieci

	zwolnij_zasob(semid, SEM_MOSTEK); //zwolnienie miejsca na mostku

	sleep(T1 + T2 + 2);

	//zejscie ze statku
	if(zajmij_zasob(semid, SEM_DOSTEP) == -1) return 0;

	wspolne->pasazerowie_statek--;
	printf("Pasazer %d schodzi, na statku zostalo %d pasazerow\n", pid, wspolne->pasazerowie_statek);

	zwolnij_zasob(semid, SEM_DOSTEP);

	zwolnij_zasob(semid, SEM_STATEK);

	shmdt(wspolne);
	return 0;
}

