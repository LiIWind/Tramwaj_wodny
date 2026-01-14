#include "common.h"
#include "logger.h"

int N, M, K, T1, T2, R;

volatile sig_atomic_t koniec_pracy = 0;
volatile sig_atomic_t wymuszone_wyplyniecie = 0;

void obsluga_sygnalow(int sig){
	if (sig == SIGUSR1){
		wymuszone_wyplyniecie = 1;
		printf("Kapitan otrzymal rozkaz wyplyniecia\n");
		logger_log(LOG_INFO, EVENT_SYGNAL_ODPLYNIECIE, "Kapitan otrzymal rozkaz wyplyniecia");
	}
	else if (sig == SIGUSR2){
		koniec_pracy = 1;
		printf("Kapitan otrzymal rozkaz do zakonczenia pracy\n");
		logger_log(LOG_INFO, EVENT_SYGNAL_KONIEC_PRACY, "Kapitan otrzymal rozkaz zakonczenia pracy");
	}
	else if (sig == SIGTERM){
		koniec_pracy =1;
	}
}

int main(){
	if (wczytaj_parametry_z_pliku() == -1){
		fprintf(stderr, "Kapitan: blad wczytywania parametrow\n");
		exit(1);
	}

	pid_t moj_pid = getpid();
	printf("Kapitan gotowy do pracy. PID: %d\n", moj_pid);
	logger_log(LOG_INFO, EVENT_KAPITAN_START, "Kapitan rozpoczyna prace (pid: %d)", moj_pid);

	struct sigaction sa;
	sa.sa_handler = obsluga_sygnalow;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        	perror("Blad sigaction SIGUSR1");
		logger_error_errno(EVENT_BLAD_SYSTEM, "Blad sigaction SIGUSR1");
        	exit(1);
    	}
    	if (sigaction(SIGUSR2, &sa, NULL) == -1) {
        	perror("Blad sigaction SIGUSR2");
		logger_error_errno(EVENT_BLAD_SYSTEM, "Blad sigaction SIGUSR2");
        	exit(1);
    	}
    	if (sigaction(SIGTERM, &sa, NULL) == -1) {
        	perror("Blad sigaction SIGTERM");
		logger_error_errno(EVENT_BLAD_SYSTEM, "Blad sigaction SIGTERM");
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
		logger_error_errno(EVENT_BLAD_SYSTEM, "Kapitan - blad ftok");
		exit(1);
	}

	//pobranie ID pamieci
	int shmid = shmget(key, sizeof(StanStatku), 0600);
	//pobranie ID semaforow
	int semid = semget(key, LICZBA_SEM, 0600);

	if (shmid == -1 || semid == -1){
		perror("Kapitan - brak zasobow");
		logger_error_errno(EVENT_BLAD_SYSTEM, "Kapitan - brak zasobow");
		exit(1);
	}

	logger_init("tramwaj_wodny.log", 0);
	logger_set_semid(semid);

	//Podlaczenie kapitana
	StanStatku *statek = (StanStatku*) shmat(shmid, NULL, 0);
	if(statek == (void*)-1){
		perror("Kapitan - blad shmat");
		logger_error_errno(EVENT_BLAD_SYSTEM, "Kapitan - blad shmat");
		exit(1);
	}

	if(zajmij_zasob(semid, SEM_DOSTEP) == -1){
		shmdt(statek);
		exit(1);
	}

	statek->pid_kapitan = moj_pid;
	statek->status_kapitana = 1; //w porcie
	statek->aktualny_przystanek = PRZYSTANEK_WAWEL;
	zwolnij_zasob(semid, SEM_DOSTEP);

	logger_log(LOG_INFO, EVENT_KAPITAN_START, "Kapitan zarejestrowany w systemie, przystanek startowy: %s",
		get_przystanek_nazwa(PRZYSTANEK_WAWEL));
	printf("Kapitan zarejestrowany, przystanek startowy : %s, maksymalnie rejsow: %d\n", 
		get_przystanek_nazwa(PRZYSTANEK_WAWEL), R);

	while(1){
		if(zajmij_zasob(semid, SEM_DOSTEP) == -1) break;
		int rejsy = statek->liczba_rejsow;
		int przystanek = statek->aktualny_przystanek;
		zwolnij_zasob(semid, SEM_DOSTEP);

		if(rejsy>=R || koniec_pracy){
			break;
		}

		const char *od_przystanek = get_przystanek_nazwa(przystanek);
		const char *do_przystanek = get_przystanek_nazwa(1 - przystanek);

		printf("REJS %d/%d: %s - %s\n", rejsy + 1, R, od_przystanek, do_przystanek);

		logger_log(LOG_INFO, EVENT_ZALADUN_START, "Rozpoczecie zaladunku do rejsu %d/%d: %s - %s",
			rejsy + 1, R, od_przystanek, do_przystanek);

		printf("Kapitan zaczyna zaladunek, czas oczekiwania: %d sekund\n", T1);

		if(zajmij_zasob(semid, SEM_DOSTEP) == -1) break;
		statek->czy_plynie = 0;
		statek->status_kapitana = 1; //w porcie
		statek->kierunek_mostka = KIERUNEK_NA_STATEK;
		zwolnij_zasob(semid, SEM_DOSTEP);

		wymuszone_wyplyniecie = 0;

		//Odblokowanie SIGUSR1 podczas zaladunku
		sigprocmask(SIG_UNBLOCK, &maska, &stara_maska);

		if (!wymuszone_wyplyniecie){
			sleep(T1);//oczekiwanie na pasazerow
		} else {
			printf("Wymuszone wyplyniecie\n");
			logger_log(LOG_WARNING, EVENT_SYGNAL_ODPLYNIECIE, "Wymuszono wyplyniecie");
			sleep(1);
		}

		//blokowania SIGUSR1 podczas rejsu
		sigprocmask(SIG_SETMASK, &stara_maska, NULL);

		if (koniec_pracy){
			printf("Kapitan odwoluje rejs i wysadza pasazerow\n");
			logger_log(LOG_WARNING, EVENT_KAPITAN_STOP, "Kapitan odwoluje rejs i konczy prace");

			if(zajmij_zasob(semid, SEM_DOSTEP) == -1) break;
			statek->czy_plynie = 1;
			int pasazerow = statek->pasazerowie_statek;
			zwolnij_zasob(semid, SEM_DOSTEP);

			if(pasazerow > 0){
				printf("Kapitan wysadza %d pasazerow i konczy prace\n", pasazerow);
				logger_log(LOG_INFO, EVENT_ZALADUN_KONIEC, "Kapitan wysadza %d pasazerow", pasazerow);
				goto ROZLADUNEK;
			}
			break;
		}

		//Sprawdzenie mostka
		if(zajmij_zasob(semid, SEM_DOSTEP) == -1) break;
		if(statek->pasazerowie_mostek > 0){
			printf("Ktos jest na mostku, oczekiwanie na zejscie\n");
			logger_log(LOG_WARNING, EVENT_REJS_START, "Ktos jest na mostku, oczekiwanie na zejscie");
		}

		statek->czy_plynie = 1;
		statek->status_kapitana = 0;
		statek->kierunek_mostka = KIERUNEK_BRAK;
		int pasazerow = statek->pasazerowie_statek;
		int rowerow = statek->rowery_statek;
		int skad = przystanek;

		zwolnij_zasob(semid, SEM_DOSTEP);

		printf("Kapitan odplywa z przystanku: %s, liczba pasazerow: %d, liczba rowerow: %d, czas rejsu %ds\n",
			od_przystanek, pasazerow, rowerow, T2);
		logger_rejs_event(EVENT_REJS_START, rejsy + 1, pasazerow, rowerow, od_przystanek);

		sleep(T2);

		int dokad = 1 - skad;

		printf("Kapitan doplynal do przystanku %s, pasazerowie opusczaja statek\n", get_przystanek_nazwa(dokad));
		logger_rejs_event(EVENT_REJS_KONIEC, rejsy + 1, pasazerow, rowerow, get_przystanek_nazwa(dokad));

ROZLADUNEK:
		if(zajmij_zasob(semid, SEM_DOSTEP) == -1) break;
		statek->kierunek_mostka = KIERUNEK_ZE_STATKU;
		statek->aktualny_przystanek = dokad;
		zwolnij_zasob(semid, SEM_DOSTEP);

		int proba = 0;
		int max_prob = 300;
		while(proba < max_prob){
			if(zajmij_zasob(semid, SEM_DOSTEP) == -1) break;
			int pozostalo_statek = statek->pasazerowie_statek;
			int pozostalo_mostek = statek->pasazerowie_mostek;
			zwolnij_zasob(semid, SEM_DOSTEP);

			if(pozostalo_statek == 0){
				if(pozostalo_mostek > 0) {
					if(proba % 10 == 0){
						printf("Czekanie na pusty mostek\n");
					}
					usleep(100000);
					proba++;
					continue;
				}
				break;
			}
			if (proba % 10 == 0 && proba > 0) {
				printf("Wysiadanie - pozostalo %d na statku i %d na mostku\n",
					pozostalo_statek, pozostalo_mostek);
			}
			usleep(100000);
			proba++;
		}

		printf("Statek pusty\n");
		logger_log(LOG_INFO, EVENT_ZALADUN_KONIEC, "Rejs %d zakonczony, statek pusty", rejsy+1);


		if(zajmij_zasob(semid, SEM_DOSTEP) == -1) break;
		statek->liczba_rejsow++;
		statek->kierunek_mostka = KIERUNEK_BRAK;
		statek->status_kapitana = 1;
		int nowe_rejsy = statek->liczba_rejsow;
		zwolnij_zasob(semid, SEM_DOSTEP);

		printf("Rejs %d/%d zakonczony pomyslnie\n", nowe_rejsy, R);

		if (koniec_pracy){
			printf("Kapitan konczy prace po %d rejsach\n", nowe_rejsy);
			logger_log(LOG_INFO, EVENT_KAPITAN_STOP, "Kapitan konczy prace po %d rejsach\n", nowe_rejsy);
			break;
		}

		if(nowe_rejsy >= R){
			printf("Kapitan wykonal maksymalna liczbe rejsow: %d\n", R);
			logger_log(LOG_INFO, EVENT_KAPITAN_STOP, "Kapitan wykonal maksymalna liczbe rejsow: %d", R);
			break;
		}

		printf("Zaladunek do kolejnego rejsu w %s\n", get_przystanek_nazwa(dokad));
		logger_log(LOG_INFO, EVENT_ZALADUN_START,
			"Zaladunek do kolejnego rejsu w %s", get_przystanek_nazwa(dokad));

		sleep(2);
	}


	if(zajmij_zasob(semid, SEM_DOSTEP) != -1){
		statek->koniec_symulacji = 1;
		int wykonano_rejsow = statek->liczba_rejsow;
		zwolnij_zasob(semid, SEM_DOSTEP);
		printf("Kapitan konczy zmiane. \n");
		printf("Wykonanych rejsow: %d/%d\n", wykonano_rejsow, R);
		logger_log(LOG_INFO, EVENT_KAPITAN_STOP, "Kapitan konczy zmiane, wykonanych rejsow: %d/%d\n", wykonano_rejsow, R);
	}

	shmdt(statek);
	return 0;
}
