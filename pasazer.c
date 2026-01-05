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

	//Oczekiwanie na koniec rejsu
	sleep(3);

	//Zejscie ze statku

	shmdt(wspolne);
	return 0;
}

