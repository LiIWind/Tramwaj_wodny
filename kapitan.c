#include "common.h"

volatile int koniec_pracy = 0;
volatile int wymuszone_wyplyniecie = 0;

void obsluga_sygnalow(int sig){
	if (sig == SIGUSR1){
		printf("Kapitan otrzymal rozkaz wyplyniecia\n");
		wymuszone_wyplyniecie = 1;
	}
	else if (sig == SIGUSR2){
		printf("Kapitan otrzymal rozkaz do zakonczenia pracy\n");
		koniec_pracy = 1;
	}
}

int main(){
	printf("Kapitan gotowy do pracy. PID: %d\n", getpid());

	signal(SIGUSR1, obsluga_sygnalow);
	signal(SIGUSR2, obsluga_sygnalow);

	//maska blokuje SIGUSR1(wyplyn) ale puszcz SIGUSR2 (koniec)
	sigset_t maska;
	sigemptyset(&maska);
	sigaddset(&maska, SIGUSR1);
	//blokuje na starcie, odblokuje w fazie zaladunku
	sigprocmask(SIG_BLOCK, &maska, NULL);

	//pobranie tego samego klucza co main
	key_t key = ftok(PATH_NAME, PROJECT_ID);

	//pobranie ID pamieci
	int shmid = shmget(key, sizeof(StanStatku), 0666);
	//pobranie ID semaforow
	int semid = semget(key, LICZBA_SEM, 0666);

	if (shmid == -1 || semid == -1){
		perror("Kapitan - brak zasobow");
		exit(1);
	}

	//Podlaczenie kapitana
	StanStatku *statek = (StanStatku*) shmat(shmid, NULL, 0);

	statek->pid_kapitan = getpid();
	statek->status_kapitana = 0;

	//Test - odczytanie wartosci ustawionych przez maina
	printf("Kapitan - Stan pasazerow: %d, Stan rejsow, %d\n", statek->pasazerowie_statek, statek->liczba_rejsow);

	while(statek->liczba_rejsow < R && koniec_pracy == 0){
		printf("Kapitan zaczyna zaladunek do rejsu nr %d\n", statek->liczba_rejsow + 1);

		if(zajmij_zasob(semid, SEM_DOSTEP) == -1) break;
		statek->czy_plynie = 0;
		statek->status_kapitana = 1;

		zwolnij_zasob(semid, SEM_DOSTEP);

		wymuszone_wyplyniecie = 0;

		sigprocmask(SIG_UNBLOCK, &maska, NULL);

		if (wymuszone_wyplyniecie == 0){
			sleep(T1);//oczekiwanie na pasazerow
		} else {
			printf("Wymuszone wyplyniecie pasazerowie maja 1 sekunde na wejscie\n");
			sleep(1);
		}

		sigprocmask(SIG_BLOCK, &maska, NULL);

		if(zajmij_zasob(semid, SEM_DOSTEP) != -1){
			statek->status_kapitana = 0;
			zwolnij_zasob(semid, SEM_DOSTEP);
		}

		if (koniec_pracy == 1){
			printf("Kapitan odwoluje rejs i wysadza pasazerow\n");
			statek->czy_plynie = 1;
			goto ROZLADUNEK;
		}

		//Sprawdzenie mostka
		if(zajmij_zasob(semid, SEM_DOSTEP) == -1) break;
		if(statek->pasazerowie_mostek > 0){
			printf("Ktos jest na mostku\n");
		}

		statek->czy_plynie = 1;
		printf("Kapitan odplywa, liczba pasazerow: %d, czas rejsu %ds\n", statek->pasazerowie_statek, T2);

		zwolnij_zasob(semid, SEM_DOSTEP);

		sleep(T2);

		printf("Kapitan doplynal do celu, pasazerowie opusczaja statek\n");

		ROZLADUNEK:
		while(1){
			if(zajmij_zasob(semid, SEM_DOSTEP) == -1) break;

			int ilosc_ludzi = statek->pasazerowie_statek;

			zwolnij_zasob(semid, SEM_DOSTEP);

			if(ilosc_ludzi > 0){
				usleep(500000); //0.5s
			}else{
				break;
			}
		}

		if(zajmij_zasob(semid, SEM_DOSTEP) == -1) break;

		statek->liczba_rejsow++;

		if (koniec_pracy == 1){
			printf("Statek pusty, koniec pracy\n");
			zwolnij_zasob(semid, SEM_DOSTEP);
			break;
		}

		if(statek->liczba_rejsow < R){
			printf("Statek pusty, nowi pasazerowie moga wchodzic\n");
		}else{
			printf("Statek pusty, to byl ostatni rejs\n");
		}

		zwolnij_zasob(semid, SEM_DOSTEP);

		sleep(2);
	}


	//odlaczenie przed koncem
	shmdt(statek);
	printf("Kapitan konczy zmiane. \n");
	return 0;
}
