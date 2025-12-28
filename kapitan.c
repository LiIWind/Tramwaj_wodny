#include "common.h"

int main(){
	printf("Kapitan gotowy do pracy. PID: %d\n", getpid());

	//pobranie tego samego klucza co main
	key_t key = ftok(PATH_NAME, PROJECT_ID);

	//pobranie ID pamieci
	int shmid = shmget(key, sizeof(StanStatku), 0666);
	if (shmid == -1){
		perror("Kapitan - brak pamieci");
		exit(1);
	}

	//Podlaczenie kapitana
	StanStatku *statek = (StanStatku*) shmat(shmid, NULL, 0);

	//Test - odczytanie wartosci ustawionych przez maina
	printf("Kapitan - Stan pasazerow: %d, Stan rejsow, %d\n", statek->pasazerowie_statek, statek->liczba_rejsow);

	sleep(2);

	//odlaczenie przed koncem
	shmdt(statek);
	printf("Kapitan konczy zmiane. \n");
	return 0;
}
