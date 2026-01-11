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

	//walidacja parametrow
	if (K >= N){
		printf("Blad, pojemnosc mostka K musi byc mniejsza niz pojemnosc statku N\n");
		exit(1);
	}

	if (M >= N){
                printf("Blad, liczba miejsc na rowery M musi byc mniejsza nic pojemnosc statku\n");
                exit(1);
        }

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
	wspolne->kierunek_mostka = KIERUNEK_BRAK;
	wspolne->status_kapitana = 1; //w porcie
	wspolne->koniec_symulacji = 0;
	wspolne->pid_kapitan = 0;

	printf("Pamiec gotowa (ID: %d), stan: 0 pasazerow.\n",shmid);
	shmdt(wspolne); //odlaczenie pamieci maina

	//tworzy zestaw 3 semaforow dla mostka statku i dostepu
	int semid = semget(key, LICZBA_SEM, 0666 | IPC_CREAT);
	if (semid == -1){
		perror("Blad semget");
		shmctl(shmid, IPC_RMID, NULL);
		exit(1);
	}

	//ustawienie limitow
	ustaw_semafor(semid, SEM_MOSTEK, K); //max K osob na mostku
	ustaw_semafor(semid, SEM_STATEK_LUDZIE, N); //max N osob na statku
	ustaw_semafor(semid, SEM_STATEK_ROWERY, M);
	ustaw_semafor(semid, SEM_DOSTEP, 1); //tylko 1 proces edytuje pamiec
	ustaw_semafor(semid, SEM_KIERUNEK, 1);

	printf("Semafory gotowe ID: %d, mostek: %d, statek ludzie: %d miejsc, statek rowery: %d miejsc\n",
		semid, K, N, M);

	//uruchomienie procesow
	pid_t pid_kapitan = fork();
	if(pid_kapitan == -1){
		perror("Blad fork kapitana");
		semctl(semid, 0, IPC_RMID);
		shmctl(semid, IPC_RMID, NULL);
		exit(1);
	}

	if (pid_kapitan == 0) {
		//proces potomny
		execl("./kapitan", "kapitan", NULL);
		perror("Blad execl kapitana");
		exit(1);
	}
	sleep(1);

	pid_t pid_dyspozytor = fork();
	if(pid_dyspozytor == -1){
                perror("Blad fork dyspozytora");
		kill(pid_kapitan, SIGTERM);
                semctl(semid, 0, IPC_RMID);
                shmctl(shmid, IPC_RMID, NULL);
                exit(1);
        }

        if (pid_dyspozytor == 0) {
                //proces potomny
                execl("./dyspozytor", "dyspozytor", NULL);
                perror("Blad execl dyspozytora");
                exit(1);
        }

	//generowanie pasazerow
	printf("Generowanie pasazerow\n");
	int liczba_pasazerow = 20;
	for (int i=0; i<liczba_pasazerow; i++){
		usleep(800000);//0.8 sekundy

		pid_t pid = fork();
		if (pid == -1){
			perror("Blad ftok pasazera");
			continue;
		}

		if(pid == 0){
			//proces potomny zamienia sie w pasazera
			execl("./pasazer", "pasazer", NULL);
			perror("Blad execl pasazera");
			exit(1);
		}
	}

	waitpid(pid_kapitan, NULL, 0);
	waitpid(pid_dyspozytor, NULL, 0);

	sleep(2);
	kill(0, SIGTERM);

	sleep(1);

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
