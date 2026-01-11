#include "common.h"

int main(){
        printf("Dyzpozytor obserwuje. PID: %d\n", getpid());

	key_t key = ftok(PATH_NAME, PROJECT_ID);
	if(key == -1){
		perror("Dyspozytor - blad ftok");
		return 1;
	}

	int shmid = shmget(key, sizeof(StanStatku), 0666);
	int semid = semget(key, LICZBA_SEM, 0666);
	if (shmid == -1 || semid == -1){
		perror("dyspozytor - brak zasobow");
		return 1;
	}

	StanStatku *wspolne = (StanStatku*) shmat(shmid, NULL, 0);
	if(wspolne == (void*)-1){
		perror("Dyspozytor - blad shmat");
		return 1;
	}

	sleep(1);

	if(zajmij_zasob(semid, SEM_DOSTEP) == -1) {
		shmdt(wspolne);
		return 1;
	}

	pid_t pid_kapitana = wspolne->pid_kapitan;
	zwolnij_zasob(semid, SEM_DOSTEP);

	if(pid_kapitana == 0){
		printf("BLAD, dyspozytor nie moze znalezc pid kapitana\n");
		shmdt(wspolne);
		return 1;
	}

	printf("Dyspozytor ma pid kapitana (PID: %d)\n", pid_kapitana);

	//wymuszenie wyplyniecia po czasie T1 + 2
	int czas_do_sygnalu1 = T1 + 2;
	printf("Dyspozytor chce wyslac sygnal do wyplyniecia za %d sekund\n", czas_do_sygnalu1);
	sleep(czas_do_sygnalu1);

	printf("Dyspozytor czeka az kapitan bedzie w porcie\n");
	int proba = 0;
	while(proba < 50) {
		if(zajmij_zasob(semid, SEM_DOSTEP) == -1){
			shmdt(wspolne);
			return 1;
		}
		int status = wspolne->status_kapitana;
		int koniec = wspolne->koniec_symulacji;
		zwolnij_zasob(semid, SEM_DOSTEP);

		if(koniec){
			printf("Kapitan skonczyl zmiane, dyspozytor konczy obserwowac");
			shmdt(wspolne);
			return 0;
		}

		if(status == 1){ //kapitan w porcie
			printf("Kapitan w porcie, dyspozytor wysyla sygnal do odplyniecia\n");
			if(kill(pid_kapitana, SIGUSR1) == -1){
				perror("Dyspozytor - blad wysylania SIGUSR1");
			}
			break;
		}
		usleep(100000);
		proba++;
	}

	if(proba >=50){
		printf("Kapitan nie byl w porcie, dyspozytor nie wysyla sygnalu do wczesniejszego odplyniecia\n");
	}

	int czas_do_sygnalu2 = (T1 + T2 + 3) * 2 + T1; //czeka az zrobi 2 rejsy i bedzie w trakcie zaladunku 3
	printf("Dyspozytor chce wyslac sygnal do konca pracy za %d sekund\n", czas_do_sygnalu2);
	sleep(czas_do_sygnalu2);

	if(zajmij_zasob(semid, SEM_DOSTEP) == -1){
		shmdt(wspolne);
		return 1;
	}

	int koniec = wspolne->koniec_symulacji;
	int rejsy = wspolne->liczba_rejsow;
	zwolnij_zasob(semid, SEM_DOSTEP);

	if(koniec){
		printf("Kapitan zakonczyl prace, %d wykonanych rejsow\n", rejsy);
	} else {
		printf("Dyspozytor wysyla sygnal do zakonczenia pracy\n");
		if(kill(pid_kapitana, SIGUSR2) == -1){
			perror("Dyspozytor blad wysylania SIGUSR2");
		}
	}

	printf("Dyspozytor czeka na zakonczenie pracy kapitana\n");
	shmdt(wspolne);
        printf("Dyspozytor konczy zmiane. \n");
        return 0;
}

