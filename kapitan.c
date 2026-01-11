#include "common.h"

volatile sig_atomic_t koniec_pracy = 0;
volatile sig_atomic_t wymuszone_wyplyniecie = 0;

void obsluga_sygnalow(int sig){
	if (sig == SIGUSR1){
		printf("Kapitan otrzymal rozkaz wyplyniecia\n");
		wymuszone_wyplyniecie = 1;
	}
	else if (sig == SIGUSR2){
		printf("Kapitan otrzymal rozkaz do zakonczenia pracy\n");
		koniec_pracy = 1;
	}
	else if (sig == SIGTERM){
		koniec_pracy =1;
	}
}

int main(){
	printf("Kapitan gotowy do pracy. PID: %d\n", getpid());

	struct sigaction sa;
	sa.sa_handler = obsluga_sygnalow;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        	perror("Blad sigaction SIGUSR1");
        	exit(1);
    	}
    	if (sigaction(SIGUSR2, &sa, NULL) == -1) {
        	perror("Blad sigaction SIGUSR2");
        	exit(1);
    	}
    	if (sigaction(SIGTERM, &sa, NULL) == -1) {
        	perror("Blad sigaction SIGTERM");
        	exit(1);
    	}

	//maska blokuje SIGUSR1(wyplyn)
	sigset_t maska, stara_maska;
	sigemptyset(&maska);
	sigaddset(&maska, SIGUSR1);

	//pobranie tego samego klucza co main
	key_t key = ftok(PATH_NAME, PROJECT_ID);
	if(key == -1){
		perror("Kapitan - blad ftok");
		exit(1);
	}

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
	if(statek == (void*)-1){
		perror("Kapitan - blad shmat");
		exit(1);
	}

	if(zajmij_zasob(semid, SEM_DOSTEP) == -1){
		shmdt(statek);
		exit(1);
	}

	statek->pid_kapitan = getpid();
	statek->status_kapitana = 1; //w porcie
	zwolnij_zasob(semid, SEM_DOSTEP);

	//Test - odczytanie wartosci ustawionych przez maina
	printf("Kapitan - Stan pasazerow: %d, Stan rejsow: %d, maksymalnie rejsow: %d\n", statek->pasazerowie_statek, statek->liczba_rejsow, R);

	while(1){
		if(zajmij_zasob(semid, SEM_DOSTEP) == -1) break;
		int rejsy = statek->liczba_rejsow;
		zwolnij_zasob(semid, SEM_DOSTEP);

		if(rejsy>=R || koniec_pracy){
			break;
		}

		printf("Kapitan zaczyna zaladunek do rejsu nr %d/%d\n", statek->liczba_rejsow + 1, R);

		if(zajmij_zasob(semid, SEM_DOSTEP) == -1) break;
		statek->czy_plynie = 0;
		statek->status_kapitana = 1;
		statek->kierunek_mostka = KIERUNEK_NA_STATEK;
		zwolnij_zasob(semid, SEM_DOSTEP);

		wymuszone_wyplyniecie = 0;

		//Odblokowanie SIGUSR1 podczas zaladunku
		sigprocmask(SIG_UNBLOCK, &maska, &stara_maska);

		if (!wymuszone_wyplyniecie){
			sleep(T1);//oczekiwanie na pasazerow
		} else {
			printf("Wymuszone wyplyniecie\n");
			sleep(1);
		}

		//blokowania SIGUSR1 podczas rejsu
		sigprocmask(SIG_SETMASK, &stara_maska, NULL);

		if (koniec_pracy){
			printf("Kapitan odwoluje rejs i wysadza pasazerow\n");
			if(zajmij_zasob(semid, SEM_DOSTEP) == -1) break;
			statek->czy_plynie = 1;
			int pasazerow = statek->pasazerowie_statek;
			zwolnij_zasob(semid, SEM_DOSTEP);

			if(pasazerow > 0){
				printf("Kapitan wysadza %d pasazerow i konczy prace\n", pasazerow);
				goto ROZLADUNEK;
			}
			break;
		}

		//Sprawdzenie mostka
		if(zajmij_zasob(semid, SEM_DOSTEP) == -1) break;
		if(statek->pasazerowie_mostek > 0){
			printf("Ktos jest na mostku\n");
		}

		statek->czy_plynie = 1;
		statek->status_kapitana = 0;
		statek->kierunek_mostka = KIERUNEK_BRAK;
		int pasazerow = statek->pasazerowie_statek;
		int rowerow = statek->rowery_statek;
		zwolnij_zasob(semid, SEM_DOSTEP);

		printf("Kapitan odplywa, liczba pasazerow: %d, liczba rowerow: %d, czas rejsu %ds\n", pasazerow, rowerow, T2);

		sleep(T2);

		printf("Kapitan doplynal do celu, pasazerowie opusczaja statek\n");

ROZLADUNEK:
		if(zajmij_zasob(semid, SEM_DOSTEP) == -1) break;
		statek->kierunek_mostka = KIERUNEK_ZE_STATKU;
		zwolnij_zasob(semid, SEM_DOSTEP);

		while(1){
			if(zajmij_zasob(semid, SEM_DOSTEP) == -1) break;
			int pozostalo = statek->pasazerowie_statek;
			zwolnij_zasob(semid, SEM_DOSTEP);

			if(pozostalo == 0) break;

			usleep(100000);
		}
		printf("Statek pusty\n");

		if(zajmij_zasob(semid, SEM_DOSTEP) == -1) break;
		statek->liczba_rejsow++;
		statek->kierunek_mostka = KIERUNEK_BRAK;
		zwolnij_zasob(semid, SEM_DOSTEP);

		if (koniec_pracy){
			printf("Kapitan konczy prace po %d rejsach\n", statek->liczba_rejsow);
			break;
		}

		if(statek->liczba_rejsow >= R){
			printf("Kapitan wykonal maksymalna liczbe rejsow\n");
			break;
		}
		printf("Kapitan - Rejs %d zakonczony, kolejni pasazerowie moga wchodzic\n", statek->liczba_rejsow);
		sleep(2);
	}


	if(zajmij_zasob(semid, SEM_DOSTEP) != -1){
		statek->koniec_symulacji = 1;
		zwolnij_zasob(semid, SEM_DOSTEP);
	}
	shmdt(statek);
	printf("Kapitan konczy zmiane. \n");
	return 0;
}
