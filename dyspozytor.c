#include "common.h"
#include "logger.h"

int N, M, K, T1, T2, R;

int main(){
	if (wczytaj_parametry_z_pliku() == -1) {
		fprintf(stderr, "Dyzpozytor: blad wczytywania parametrow\n");
		exit(1);
	}

        printf("Dyzpozytor obserwuje. PID: %d\n", getpid());
	logger_log(LOG_INFO, EVENT_DYSPOZYTOR_START, "Dyzpozytor obserwuje. PID: %d\n", getpid());

	key_t key = ftok(PATH_NAME, PROJECT_ID);
	if(key == -1){
		perror("Dyspozytor - blad ftok");
		logger_error_errno(EVENT_BLAD_SYSTEM, "Dyspozytor - blad ftok");
		return 1;
	}

	int shmid = shmget(key, sizeof(StanStatku), 0600);
	int semid = semget(key, LICZBA_SEM, 0600);
	if (shmid == -1 || semid == -1){
		perror("dyspozytor - brak zasobow");
		logger_error_errno(EVENT_BLAD_SYSTEM, "dyspozytor - brak zasobow");
		return 1;
	}

	logger_init("tramwaj_wodny.log", 0);
	logger_set_semid(semid);

	StanStatku *wspolne = (StanStatku*) shmat(shmid, NULL, 0);
	if(wspolne == (void*)-1){
		perror("Dyspozytor - blad shmat");
		logger_error_errno(EVENT_BLAD_SYSTEM, "Dyspozytor - blad shmat");
		return 1;
	}

	if(zajmij_zasob(semid, SEM_DOSTEP) == -1) {
		shmdt(wspolne);
		return 1;
	}

	pid_t pid_kapitana = wspolne->pid_kapitan;

	zwolnij_zasob(semid, SEM_DOSTEP);

	if(pid_kapitana == 0){
		printf("BLAD, dyspozytor nie moze znalezc pid kapitana\n");
		logger_log(LOG_ERROR, EVENT_BLAD_SYSTEM, "BLAD, dyspozytor nie moze znalezc pid kapitana");
		shmdt(wspolne);
		return 1;
	}

	printf("Dyspozytor ma pid kapitana (PID: %d)\n", pid_kapitana);
	logger_log(LOG_INFO, EVENT_DYSPOZYTOR_START, "Dyspozytor ma pid kapitana (PID: %d)", pid_kapitana);

	//wymuszenie wyplyniecia po czasie T1 + 2
	int czas_do_sygnalu1 = T1 - 5;
	printf("Dyspozytor chce wyslac sygnal do wyplyniecia za %d sekund\n", czas_do_sygnalu1);
	logger_log(LOG_INFO, EVENT_DYSPOZYTOR_START, "Dyspozytor chce wyslac sygnal do wyplyniecia za %d sekund", czas_do_sygnalu1);
	sleep(czas_do_sygnalu1);

	printf("Dyspozytor proba wyslania sygnalu SIGUSR1\n");

	int proba = 0;
	int wyslano_sygnal1=0;

	printf("Sprawdzanie statusu kapitana\n");

	while(proba < 50) {
		if(zajmij_zasob(semid, SEM_DOSTEP) == -1){
			shmdt(wspolne);
			return 1;
		}
		int status = wspolne->status_kapitana;
		int koniec = wspolne->koniec_symulacji;
		int przystanek = wspolne->aktualny_przystanek;

		zwolnij_zasob(semid, SEM_DOSTEP);

		if(koniec){
			printf("Kapitan skonczyl zmiane, dyspozytor konczy obserwowac");
			logger_log(LOG_INFO, EVENT_DYSPOZYTOR_STOP, "Kapitan skonczyl zmiane, dyspozytor konczy obserwowac");
			shmdt(wspolne);
			return 0;
		}

		if(status == 1){ //kapitan w porcie
			printf("Kapitan w porcie %s, dyspozytor wysyla sygnal do odplyniecia\n", get_przystanek_nazwa(przystanek));
			logger_sygnal("SIGUSR1", getpid(), pid_kapitana);

			if(kill(pid_kapitana, SIGUSR1) == -1){
				perror("Dyspozytor - blad wysylania SIGUSR1");
				logger_error_errno(EVENT_BLAD_SYSTEM, "Blad wysylania SIGUSR1");
			} else {
				printf("Sygnal SIGUSR1 wyslany pomyslnie\n");
				logger_log(LOG_INFO, EVENT_SYGNAL_ODPLYNIECIE, "Sygnal SIGUSR1 wyslany pomyslnie");
				wyslano_sygnal1 = 1;
				break;
			}
		}
		usleep(100000);
		proba++;
	}
	if(proba >=50 && !wyslano_sygnal1){
		printf("Kapitan nie byl w porcie, dyspozytor nie wysyla sygnalu do wczesniejszego odplyniecia\n");
		logger_log(LOG_WARNING, EVENT_DYSPOZYTOR_START, 
			"Kapitan nie byl w porcie, dyspozytor nie wysyla sygnalu do wczesniejszego odplyniecia");
	}


	int czas_do_sygnalu2 = (T1 + T2 + 3) * 2 + T1; //czeka az zrobi 2 rejsy i bedzie w trakcie zaladunku 3
	printf("Dyspozytor chce wyslac sygnal do konca pracy za %d sekund\n", czas_do_sygnalu2);
	logger_log(LOG_INFO, EVENT_DYSPOZYTOR_START, "Dyspozytor chce wyslac sygnal do konca pracy za %d sekund", czas_do_sygnalu2);

	for(int i = 0; i < czas_do_sygnalu2; i++) {
        	sleep(1);

		if(zajmij_zasob(semid, SEM_DOSTEP) == -1){
			shmdt(wspolne);
			return 1;
		}
		int czy_koniec = wspolne->koniec_symulacji;
		zwolnij_zasob(semid, SEM_DOSTEP);

		if(czy_koniec) {
			printf("Dyspozytor: Wykryto wczesniejszy koniec symulacji\n");
			logger_log(LOG_INFO, EVENT_DYSPOZYTOR_STOP, "Dyspozytor konczy wczesniej - limit rejsow");
			shmdt(wspolne);
			return 0;
        }
    }

	if(zajmij_zasob(semid, SEM_DOSTEP) == -1){
		shmdt(wspolne);
		return 1;
	}

	int rejsy = wspolne->liczba_rejsow;
	int koniec = wspolne->koniec_symulacji;

	zwolnij_zasob(semid, SEM_DOSTEP);

	if(koniec){
		printf("Kapitan zakonczyl prace, %d wykonanych rejsow\n", rejsy);
		logger_log(LOG_INFO, EVENT_DYSPOZYTOR_STOP, "Kapitan zakonczyl prace, %d wykonanych rejsow", rejsy);
	} else {
		printf("Dyspozytor wysyla sygnal do zakonczenia pracy\n");
		logger_sygnal("SIGUSR2", getpid(), pid_kapitana);

		if(kill(pid_kapitana, SIGUSR2) == -1){
			perror("Dyspozytor blad wysylania SIGUSR2");
			logger_error_errno(EVENT_BLAD_SYSTEM, "Blad wysylania SIGUSR2");
		} else {
			printf("Sygnal SIGUSR2 wyslany pomyslnie\n");
			logger_log(LOG_INFO, EVENT_SYGNAL_KONIEC_PRACY,"Sygnal SIGUSR2 wyslany pomyslnie");

		}
	}

	printf("Dyspozytor czeka na zakonczenie pracy kapitana\n");
	logger_log(LOG_INFO, EVENT_DYSPOZYTOR_STOP, "Dyspozytor czeka na zakonczenie pracy kapitana");

	int oczekiwanie = 0;
	while (oczekiwanie < 100) {
		if (zajmij_zasob(semid, SEM_DOSTEP) == -1) {
			break;
		}

		int koniec_teraz = wspolne->koniec_symulacji;
		int rejsy_teraz = wspolne->liczba_rejsow;

		zwolnij_zasob(semid, SEM_DOSTEP);

		if (koniec_teraz) {
			printf("Kapitan zakonczyl prace\n");
			printf("Wykonanych rejsow: %d/%d\n", rejsy_teraz, R);

			logger_log(LOG_INFO, EVENT_DYSPOZYTOR_STOP,
				"Kapitan zakonczyl prace Wykonanych rejsow: %d", rejsy_teraz);
			break;
		}
		if (oczekiwanie % 20 == 0 && oczekiwanie > 0) {
			printf("Oczekiwanie (rejsy: %d/%d)\n", rejsy_teraz, R);
		}

		sleep(1);
		oczekiwanie++;
	}

	shmdt(wspolne);
	printf("Dyspozytor konczy zmiane. \n");
	logger_log(LOG_INFO, EVENT_DYSPOZYTOR_STOP, "Dyspozytor konczy zmiane.");
	return 0;
}

