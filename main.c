#include "common.h"

//funkcja pomocnicza
void ustaw_semafor(int semid, int sem_num, int val) {
    if (semctl(semid, sem_num, SETVAL, val) == -1) {
        perror("Blad ustawiania wartosci semafora");
        exit(1);
    }
}

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

	//tworzy zestaw 3 semaforow dla mostka statku i dostepu
	int semid = semget(key, LICZBA_SEM, 0666 | IPC_CREAT);
	if (semid == -1){
		perror("Blad semget");
		exit(1);
	}

	//ustawienie limitow
	ustaw_semafor(semid, SEM_MOSTEK, K); //max K osob na mostku
	ustaw_semafor(semid, SEM_STATEK, N); //max N osob na statku
	ustaw_semafor(semid, SEM_DOSTEP, 1); //tylko 1 proces edytuje pamiec

	printf("Semafory gotowe ID: %d, mostek: %d, statek: %d\n", semid, K, N);

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

	//generowanie pasazerow
	int liczba_pasazerow = 15;
	for (int i=0; i<liczba_pasazerow; i++){
		usleep(500000);//0.5 sekundy
		if(fork()==0){
			//proces potomny zamienia sie w pasazera
			execl("./pasazer", "pasazer", NULL);
			perror("Blad uruchomienia pasazera");
			exit(1);
		}
	}

	waitpid(pid_kapitan, NULL, 0);
	waitpid(pid_dyspozytor, NULL, 0);

	sleep(2);
	kill(0, SIGTERM);

	//Sprzatanie
	printf("Czyszczenie zasobow\n");
	if (shmctl(shmid, IPC_RMID, NULL) == -1){
		perror("Blad usuwania pamieci");
	}

	if (semctl(semid, 0, IPC_RMID) == -1){
		perror("Blad usuwania semaforow");
	}

	printf("Koniec symulacji\n");
	return 0;
}
