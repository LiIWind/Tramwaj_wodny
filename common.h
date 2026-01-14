#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <string.h>


// Parametry do zadania
extern int N; //pojemnosc statku
extern int M; //miejsca na rowery
extern int K; //pojemnosc mostka
extern int T1; //czas oczekiwania w sekundach
extern int T2; //czas rejsu w sekundach
extern int R; //limit rejsow

#define PROJECT_ID 'D'
#define PATH_NAME "."

#define SEM_MOSTEK 0 //limity mostka
#define SEM_STATEK_LUDZIE 1 //limit ludzi na statku
#define SEM_STATEK_ROWERY 2 //limit rowerow na statku
#define SEM_DOSTEP 3 //mutex do pamieci dzielonej
#define SEM_LOGGER 4
#define LICZBA_SEM 5

//kierunek mostka
#define KIERUNEK_BRAK 0
#define KIERUNEK_NA_STATEK 1
#define KIERUNEK_ZE_STATKU 2

#define PRZYSTANEK_WAWEL 0
#define PRZYSTANEK_TYNIEC 1

typedef struct {
	int pasazerowie_statek; //aktualnie pasazerow na statku
	int rowery_statek; //aktualnie rowerow na statku
	int pasazerowie_mostek; //aktualnie pasazerow na mostku
	int czy_plynie; // 0 - w porcie, 1 - plynie
	int liczba_rejsow; //licznik rejsow
	int kierunek_mostka; //kierunek ruchu na mostku
	int aktualny_przystanek;
	pid_t pid_kapitan;
	int status_kapitana; //0 - rejs, 1- port
	int koniec_symulacji;
	int total_pasazerow_wawel;
	int total_pasazerow_tyniec;
	int pasazerow_odrzuconych;
} StanStatku;


//funkcje pomocnicze
static inline int zajmij_zasob(int semid, int sem_num){
	struct sembuf operacja = {sem_num, -1, 0}; //zmniejszenie licznika np. wchodzac na mostek zmniejsza sie miejsce na mostku, jak jest 0 to czeka
	while (semop(semid, &operacja, 1) == -1){
		if(errno == EINTR) continue; //jesli przerwano sygnalem 

		if(errno == EIDRM || errno == EINVAL) return -1; //jesli usunieto semafor

		if(errno != EIDRM && errno != EINVAL) {
			perror("Blad zajmowania semafora");
		}
		return -1;
	}
	return 0;
}

static inline int zwolnij_zasob(int semid, int sem_num){
        struct sembuf operacja = {sem_num, 1, 0}; //zwiekszenie licznika
        if (semop(semid, &operacja, 1) == -1){
		if(errno != EIDRM && errno != EINVAL) {
                	perror("Blad zwalniania semafora");
		}
		return -1;
        }
	return 0;
}

static inline int zajmij_zasob_ilosc(int semid, int sem_num, int ilosc){
	struct sembuf operacja = {sem_num, -ilosc, 0};
	while (semop(semid, &operacja, 1) == -1){
		if(errno == EINTR) continue; 
		if(errno == EIDRM || errno == EINVAL) return -1;
		if(errno != EIDRM && errno != EINVAL) {
			perror("Blad zajmowania semafora (ilosc)");
		}
		return -1;
	}
	return 0;
}

static inline int zwolnij_zasob_ilosc(int semid, int sem_num, int ilosc){
    struct sembuf operacja = {sem_num, ilosc, 0};
    if (semop(semid, &operacja, 1) == -1){
	if(errno != EIDRM && errno != EINVAL) {
		perror("Blad zwalniania semafora (ilosc)");
        }
	return -1;
    }
    return 0;
}

static inline const char* get_przystanek_nazwa(int przystanek) {
	return (przystanek == PRZYSTANEK_WAWEL) ? "Krakow Wawel" : "Tyniec";
}

static inline int waliduj_parametry(void) {
	if (K >= N){
		fprintf(stderr, "Blad, pojemnosc mostka K (%d) musi byc mniejsza niz pojemnosc statku N (%d)\n", K, N);
		return -1;
	}

	if (M >= N){
                fprintf(stderr, "Blad, Liczba miejsc (%d) musi byc mniejsza niz pojemnosc statku N (%d)\n", M, N);
                return -1;
        }

	if (N <= 0 || M <=0 || K <= 0){
                fprintf(stderr, "Blad, Parametry N, M i K musza byc wieksze od 0\n");
                return -1;
        }

	if (T1 <=0 || T2 <= 0){
                fprintf(stderr, "Czasy T1 i T2 musza byc wieksze od 0\n");
                return -1;
        }

	if (R <= 0){
                fprintf(stderr, "Blad, Liczba rejsow R musi byc wieksza od 0\n");
                return -1;
        }

	return 0;
}

static inline void wyswietl_konfiguracje(void) {
	printf("System Tramwaju Wodnego\n");
	printf("Trasa: Krakow Wawel - Tyniec\n");
	printf("Pojemnosc tramwaju: %d\n", N);
	printf("Miejsca na rowery: %d\n", M);
	printf("Pojemnosc mostka: %d\n", K);
	printf("Czas zaladunku T1: %d\n", T1);
	printf("Czas rejsu T2: %d\n", T2);
	printf("Maksymalna liczba rejsow: %d\n", R);
}

static inline int wczytaj_parametry(void) {
	printf("Podaj parametry systemu:\n");

	do {
		printf("Pojemnosc tramwaju: ");
		if(scanf("%d", &N) != 1){
			fprintf(stderr, "Blad: podaj liczbe calkowita\n");
			while(getchar() != '\n');
			continue;
		}
		if (N < 1){
			fprintf(stderr, "Blad wartosc musi byc wieksza od 0\n");
			N = 0;
		}
	} while (N < 1);

	do {
                printf("Miejsca na rowery: ");
                if(scanf("%d", &M) != 1){
                        fprintf(stderr, "Blad: podaj liczbe calkowita\n");
                        while(getchar() != '\n');
                        continue;
                }
                if (M < 0 || M >= N){
                        fprintf(stderr, "Blad wartosc musi byc w zakresie od 0 - %d\n", N-1);
                        M=-1;
                }
        } while (M < 0);

	do {
                printf("Pojemnosc mostka: ");
                if(scanf("%d", &K) != 1){
                        fprintf(stderr, "Blad: podaj liczbe calkowita\n");
                        while(getchar() != '\n');
                        continue;
                }
                if (K < 1 || K >= N){
                        fprintf(stderr, "Blad wartosc musi byc w zakresie od 1 - %d\n", N-1);
                        K=0;
                }
        } while (K < 1);

	do {
                printf("Czas zaladunku T1 w sekundach: ");
                if(scanf("%d", &T1) != 1){
                        fprintf(stderr, "Blad: podaj liczbe calkowita\n");
                        while(getchar() != '\n');
                        continue;
                }
                if (T1 < 1){
                        fprintf(stderr, "Blad wartosc musi byc wieksza od 1\n");
                        T1=0;
                }
        } while (T1 < 1);

	do {
                printf("Czas rejsu T2 w sekundach: ");
                if(scanf("%d", &T2) != 1){
                        fprintf(stderr, "Blad: podaj liczbe calkowita\n");
                        while(getchar() != '\n');
                        continue;
                }
                if (T2 < 1){
                        fprintf(stderr, "Blad wartosc musi byc wieksza od 0\n");
                        T2 = 0;
                }
        } while (T2 < 1);

	do {
                printf("Maksymalna liczba rejsow R: ");
                if(scanf("%d", &R) != 1){
                        fprintf(stderr, "Blad: podaj liczbe calkowita\n");
                        while(getchar() != '\n');
                        continue;
                }
                if (R < 1){
                        fprintf(stderr, "Blad wartosc musi byc wieksza od 0\n");
                        R=0;
                }
        } while (R < 1);

	printf("\n");
	return 0;
}

//dla procesow potomnych
static inline int zapisz_parametry_do_pliku(void) {
	FILE *f = fopen("/tmp/tramwaj_wodny_config.txt", "w");
	if (f == NULL) {
		perror("Blad zapisu konfiguracji");
		return -1;
	}
	fprintf(f, "%d\n%d\n%d\n%d\n%d\n%d\n", N, M, K, T1, T2, R);
	fclose(f);
	return 0;
}

static inline int wczytaj_parametry_z_pliku(void) {
	FILE *f = fopen("/tmp/tramwaj_wodny_config.txt", "r");
	if (f == NULL) {
		//wartosci domyslne
		N = 10;
		M = 3;
		K = 3;
		T1 = 2;
		T2 = 3;
		R = 5;
		return 0;
	}
	if (fscanf(f, "%d\n%d\n%d\n%d\n%d\n%d\n", &N, &M, &K, &T1, &T2, &R) != 6) {
		fclose(f);
		return -1;
	}
	fclose(f);
	return 0;
}

#endif

