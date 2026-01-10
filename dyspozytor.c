#include "common.h"

int main(){
        printf("Dyzpozytor obserwuje. PID: %d\n", getpid());

	key_t key = ftok(PATH_NAME, PROJECT_ID);
	int shmid = shmget(key, sizeof(StanStatku), 0666);
	int semid = semget(key, LICZBA_SEM, 0666);
	if (shmid == -1) return 1;
	StanStatku *wspolne = (StanStatku*) shmat(shmid, NULL, 0);

	sleep(1);
	pid_t pid_kapitana = wspolne->pid_kapitan;
	printf("Dyspozytor ma pid kapitana (PID: %d)\n", pid_kapitana);

	sleep(T1 + 1);
	printf("Dyspozytor chce wyslac sygnal do wyplyniecia\n");

	while(1){
		zajmij_zasob(semid, SEM_DOSTEP);
		int status = wspolne->status_kapitana;
		zwolnij_zasob(semid, SEM_DOSTEP);

		if (status == 1){
			printf("Kapitan jest w porcie, dyspozytor wysyla sygnal\n");
			kill(pid_kapitana, SIGUSR1);
			break;
		} else{
			//printf("Kapitana nie ma w porcie, dyspozytor czeka z sygnalem\n");
			usleep(30000);
		}
	}

	sleep(T2 + 2 + T1 + T2 + 2); //czeka az zrobi 2 rejsy i bedzie w trakcie 3
	printf("Dyspozytor wysyla sygnal do konca pracy\n");
	kill(pid_kapitana, SIGUSR2);

	shmdt(wspolne);

        printf("Dyspozytor konczy zmiane. \n");
        return 0;
}

