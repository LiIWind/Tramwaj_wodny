#include "common.h"
#include "logger.h"

int N= 10;
int M = 3;
int K = 3;
int T1 = 2;
int T2 = 3;
int R = 5;

//funkcja pomocnicza
void ustaw_semafor(int semid, int sem_num, int val) {
    if (semctl(semid, sem_num, SETVAL, val) == -1) {
        perror("Blad ustawiania wartosci semafora");
        logger_error_errno(EVENT_BLAD_SYSTEM, "Blad ustawiania wartosci semafora");
	exit(1);
    }
}

volatile sig_atomic_t cleanup_flag = 0;

void cleanup_handler(int sig) {
    cleanup_flag = 1;
}

int main(){
	printf("Start systemu tramwaju wodnego Wawel - Tyniec\n");
	unlink("tramwaj_wodny.log");

	if (wczytaj_parametry() == -1) {
		fprintf(stderr, "Blad wczytywania parametrow\n");
		exit(1);
	}

	if (logger_init("tramwaj_wodny.log", 1) == -1) {
		fprintf(stderr, "Blad inicjalizacji systemu logowania\n");
		exit(1);
	}

	logger_log(LOG_INFO, EVENT_SYSTEM_START, "Start systemu tramwaju wodnego");

	if (waliduj_parametry() == -1) {
		logger_log(LOG_CRITICAL, EVENT_BLAD_SYSTEM, "Nieprawidlowe parametry konfiguracji");
		logger_close();
		exit(1);
	}

	logger_log(LOG_INFO, EVENT_SYSTEM_START, "Parametry dodane poprawnie");

	if (zapisz_parametry_do_pliku() == -1) {
		logger_log(LOG_CRITICAL, EVENT_BLAD_SYSTEM, "Blad zapisu parametrow do pliku");
		logger_close();
		exit(1);
	}

	wyswietl_konfiguracje();

	struct sigaction sa;
	sa.sa_handler = cleanup_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	//unikalny klucz
	key_t key = ftok(PATH_NAME, PROJECT_ID);
	if (key == -1){
		perror("Blad ftok");
		logger_error_errno(EVENT_BLAD_SYSTEM, "Blad generowanie klucza IPC");
		logger_close();
		exit(1);
	}

	logger_log(LOG_INFO, EVENT_SYSTEM_START, "Klucz IPC wygenerowany: %d", key);

	//Tworzenie pamieci dzielonej
	int shmid = shmget(key, sizeof(StanStatku), 0600 | IPC_CREAT);
	if (shmid == -1) {
		perror("Blad shmget");
		logger_error_errno(EVENT_BLAD_SYSTEM, "Blad tworzenia pamieci dzielonej");
		logger_close();
		exit(1);
	}
	logger_log(LOG_INFO, EVENT_SYSTEM_START, "Pamiec dzielona utworzona: %d", shmid);

	//Przylaczenie pamieci i inicjalizacja zerami
	StanStatku *wspolne = (StanStatku*) shmat(shmid, NULL, 0);
	if (wspolne == (void*)-1){
		perror("Blad shmat");
		logger_error_errno(EVENT_BLAD_SYSTEM, "Blad przylaczania pamieci dzielonej");
		shmctl(shmid, IPC_RMID, NULL);
		logger_close();
		exit(1);
	}

	//Wartosci poczatkowe
	memset(wspolne, 0, sizeof(StanStatku));
	wspolne->aktualny_przystanek = PRZYSTANEK_WAWEL;
	wspolne->pasazerowie_statek = 0;
    	wspolne->rowery_statek = 0;
    	wspolne->pasazerowie_mostek = 0;
	wspolne->czy_plynie = 0;
	wspolne->liczba_rejsow = 0;
	wspolne->kierunek_mostka = KIERUNEK_BRAK;
	wspolne->status_kapitana = 1; //w porcie
	wspolne->koniec_symulacji = 0;
	wspolne->pid_kapitan = 0;

	logger_log(LOG_INFO, EVENT_SYSTEM_START, "Pamiec gotowa, stan: 0 pasazerow, przystanek: Wawel");
	printf("Pamiec gotowa (ID: %d), stan: 0 pasazerow, przystanek: Wawel\n",shmid);

	shmdt(wspolne); //odlaczenie pamieci maina

	//Tworzenie zestawu semaforow
	int semid = semget(key, LICZBA_SEM, 0600 | IPC_CREAT);
	if (semid == -1){
		perror("Blad semget");
		logger_error_errno(EVENT_BLAD_SYSTEM, "Blad tworzenia semaforow");
		shmctl(shmid, IPC_RMID, NULL);
		logger_close();
		exit(1);
	}

	//ustawienie limitow
	ustaw_semafor(semid, SEM_MOSTEK, K); //max K osob na mostku
	ustaw_semafor(semid, SEM_STATEK_LUDZIE, N); //max N osob na statku
	ustaw_semafor(semid, SEM_STATEK_ROWERY, M);
	ustaw_semafor(semid, SEM_DOSTEP, 1); //tylko 1 proces edytuje pamiec
	ustaw_semafor(semid, SEM_LOGGER, 1);

	logger_set_semid(semid);

	printf("Semafory gotowe ID: %d, mostek: %d, statek ludzie: %d miejsc, statek rowery: %d miejsc\n",
		semid, K, N, M);
	logger_log(LOG_INFO, EVENT_SYSTEM_START,
		"Semafory gotowe ID: %d, mostek: %d, statek ludzie: %d miejsc, statek rowery: %d miejsc\n", semid, K, N, M);

	//uruchomienie procesow
	pid_t pid_kapitan = fork();
	if(pid_kapitan == -1){
		perror("Blad fork kapitana");
		logger_error_errno(EVENT_BLAD_SYSTEM, "Blad tworzenia procesu kapitana");
		semctl(semid, 0, IPC_RMID);
		shmctl(shmid, IPC_RMID, NULL);
		logger_close();
		exit(1);
	}

	if (pid_kapitan == 0) {
		//proces potomny
		execl("./kapitan", "kapitan", NULL);
		perror("Blad execl kapitana");
		logger_error_errno(EVENT_BLAD_SYSTEM, "Blad execl kapitana");
		exit(1);
	}
	logger_log(LOG_INFO, EVENT_KAPITAN_START, "Proces kapitana uruchomiony (PID: %d)", pid_kapitan);

	sleep(1);

	pid_t pid_dyspozytor = fork();
	if(pid_dyspozytor == -1){
                perror("Blad fork dyspozytora");
		logger_error_errno(EVENT_BLAD_SYSTEM, "Blad tworzenia procesu dyspozytora");
		kill(pid_kapitan, SIGTERM);
                semctl(semid, 0, IPC_RMID);
                shmctl(shmid, IPC_RMID, NULL);
		logger_close();
                exit(1);
        }

        if (pid_dyspozytor == 0) {
                //proces potomny
                execl("./dyspozytor", "dyspozytor", NULL);
                perror("Blad execl dyspozytora");
		logger_error_errno(EVENT_BLAD_SYSTEM, "Blad execl dyspozytora");
                exit(1);
        }

	logger_log(LOG_INFO, EVENT_DYSPOZYTOR_START, "Proces dyspozytora uruchomiony (PID: %d)", pid_dyspozytor);

	//generowanie pasazerow
	printf("Generowanie pasazerow\n");

	int liczba_pasazerow = 100;
	logger_log(LOG_INFO, EVENT_SYSTEM_START, "Generowanie %d pasazerow", liczba_pasazerow);

	wspolne = (StanStatku*) shmat(shmid, NULL, 0);
	if (wspolne == (void*)-1){
		perror("Main - blad shmat");
		logger_error_errno(EVENT_BLAD_SYSTEM, "Main - blad shmat");
		kill(pid_kapitan, SIGTERM);
		kill(pid_dyspozytor, SIGTERM);
		semctl(semid, 0, IPC_RMID);
		shmctl(shmid, IPC_RMID, NULL);
		logger_close();
		exit(1);
	}

	for (int i=0; i<liczba_pasazerow; i++){
		if (cleanup_flag) break;

		if(zajmij_zasob(semid, SEM_DOSTEP) != -1) {
			int koniec = wspolne->koniec_symulacji;
			zwolnij_zasob(semid, SEM_DOSTEP);

		if(koniec) {
			printf("Symulacja zakonczona, przerywam generowanie pasazerow (wygenerowano %d/%d)\n", 
				i, liczba_pasazerow);
			logger_log(LOG_INFO, EVENT_SYSTEM_START, 
					"Symulacja zakonczona, przerwano generowanie pasazerow (%d/%d)", 
					i, liczba_pasazerow);
			break;
			}
		}

		usleep(800000);//0.8 sekundy

		pid_t pid = fork();
		if (pid == -1){
			perror("Blad ftok pasazera");
			logger_error_errno(EVENT_BLAD_SYSTEM, "Blad tworzenia procesu pasazera");
			continue;
		}

		if(pid == 0){
			//proces potomny zamienia sie w pasazera
			execl("./pasazer", "pasazer", NULL);
			perror("Blad execl pasazera");
			logger_error_errno(EVENT_BLAD_SYSTEM, "Blad execl pasazera");
			exit(1);
		}

		logger_log(LOG_DEBUG, EVENT_PASAZER_PRZYBYCIE, "Wygenerowano pasazera nr %d (Pid: %d)", i + 1, pid);

	}

	printf("Zakonczono generowanie pasazerow\n");
	logger_log(LOG_INFO, EVENT_SYSTEM_START,"Zakonczono generowanie pasazerow");

	shmdt(wspolne);

	waitpid(pid_kapitan, NULL, 0);
	logger_log(LOG_INFO, EVENT_KAPITAN_STOP, "Proces kapitana zakonczony");
	waitpid(pid_dyspozytor, NULL, 0);
	logger_log(LOG_INFO, EVENT_DYSPOZYTOR_STOP, "Proces dyspozytora zakonczony");

	//Sprzatanie
	printf("Czyszczenie zasobow\n");
	logger_log(LOG_INFO, EVENT_SYSTEM_STOP, "Czyszczenie zasobow");

	sleep(2);

	StanStatku *stats_final = (StanStatku*) shmat(shmid, NULL, 0);
    	if(stats_final != (void*)-1) {
        	printf("\nRAPORT KONCOWY (PAMIEC DZIELONA)\n");
        	printf("Liczba rejsow: %d\n", stats_final->liczba_rejsow);
        	printf("Pasazerowie lacznie (Wawel + Tyniec): %d\n",
			stats_final->total_pasazerow_wawel + stats_final->total_pasazerow_tyniec);
        	printf(" - z Wawelu: %d\n", stats_final->total_pasazerow_wawel);
        	printf(" - z Tynca: %d\n", stats_final->total_pasazerow_tyniec);
        	printf("Pasazerowie odrzuceni: %d\n", stats_final->pasazerow_odrzuconych);

		logger_log(LOG_INFO, EVENT_SYSTEM_STOP,
			"Raport koncowy - Rejsy: %d, Pasazerowie: %d, Odrzuceni: %d",
			stats_final->liczba_rejsow,
			stats_final->total_pasazerow_wawel + stats_final->total_pasazerow_tyniec,
			stats_final->pasazerow_odrzuconych);

		shmdt(stats_final);
	}

	signal(SIGTERM, SIG_IGN);

	kill(0, SIGTERM);

	while(wait(NULL) > 0 || errno == EINTR);

	logger_set_semid(-1);

	if (semctl(semid, 0, IPC_RMID) == -1){
		if (errno != EIDRM && errno != EINVAL) {
			perror("Blad usuwania semaforow");
			logger_error_errno(EVENT_BLAD_SYSTEM, "Blad usuwania semaforow");
		}
	} else {
		logger_log(LOG_INFO, EVENT_SYSTEM_STOP, "Semafory usuniete");
	}

	if (shmctl(shmid, IPC_RMID, NULL) == -1){
		if (errno != EIDRM && errno != EINVAL) {
                	perror("Blad usuwania pamieci");
                	logger_error_errno(EVENT_BLAD_SYSTEM, "Blad usuwania pamieci");
		}
        } else {
                logger_log(LOG_INFO, EVENT_SYSTEM_STOP, "Pamiec usunieta");
        }

	logger_log(LOG_INFO, EVENT_SYSTEM_STOP, "Zakonczenie systemu tramwaju wodnego");

	unlink("/tmp/tramwaj_wodny_config.txt");

	printf("Koniec symulacji, raport zapisano w pliku tramwaj_wodny.log\n");

	logger_close();
	return 0;
}
