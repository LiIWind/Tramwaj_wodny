#include "common.h"

int main(){
	printf("Start systemu tramwaju wodnego oraz tworzenie zasobow\n");

	//unikalny klucz
	key_t key = ftok(PATH_NAME, PROJECT_ID);
	if (key == -1){
		perror("Blad ftok");
		exit(1);
	}

	//Tworzenie pamieci dzielonej
	int shmid = shmget(key, sizeof(StanStatku), 0666 | IPC_CREAT);
	if (shmid == -1) {
		perror("Blad shmget"); exit(1);
	}

	//Przylaczenie pamieci i inicjalizacja zerami
	StanStatku *wspolne = (StanStatku*) shmat(shmid, NULL, 0);
	if (wspolne == (void*)-1){
		perror("Blad shmat");
		exit(1);
	}

	//Wartosci poczatkowe
	wspolne->pasazerowie_statek = 0;
    	wspolne->rowery_statek = 0;
    	wspolne->pasazerowie_mostek = 0;
	wspolne->czy_plynie = 0;
	wspolne->liczba_rejsow = 0;

	printf("Pamiec gotowa (ID: %d), stan: 0 pasazerow.\n",shmid);
	shmdt(wspolne); //odlaczenie pamieci maina

	//uruchomienie procesow
	pid_t pid_kapitan = fork();
	if (pid_kapitan == 0) {
		//proces potomny
		execl("./kapitan", "kapitan", NULL);
		perror("Blad uruchomienia kapitana");
		exit(1);
	}

	pid_t pid_dyspozytor = fork();
        if (pid_dyspozytor == 0) {
                //proces potomny
                execl("./dyspozytor", "dyspozytor", NULL);
                perror("Blad uruchomienia dyspozytora");
                exit(1);
        }

	wait(NULL);
	wait(NULL);

	//Sprzatanie
	printf("Czyszczenie zasobow\n");
	if (shmctl(shmid, IPC_RMID, NULL) == -1){
		perror("Blad usuwania pamieci");
	}

	printf("Koniec symulacji\n");
	return 0;
}
