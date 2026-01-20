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


extern int N;  // Pojemnosc tramwaju (pasazerowie)
extern int M;  // Miejsca na rowery (M < N)
extern int K;  // Pojemnosc mostka (K < N)
extern int T1; // Czas zaladunku [s]
extern int T2; // Czas rejsu [s]
extern int R;  // Maksymalna liczba rejsow

#define PROJECT_ID 'W'
#define PATH_NAME "."

//Semafory
#define SEM_MUTEX           0   // Mutex do pamieci dzielonej 
#define SEM_MOSTEK          1   // Licznik miejsc na mostku (init: K) 
#define SEM_STATEK_LUDZIE   2   // Licznik miejsc na statku (init: N) 
#define SEM_STATEK_ROWERY   3   // Licznik miejsc na rowery (init: M) 
#define SEM_LOGGER          4   // Mutex do loggera 
#define SEM_ZALADUNEK_WAWEL 5   // Pasazerowie Wawel czekaja na otwarcie zaladunku 
#define SEM_ZALADUNEK_TYNIEC 6  // Pasazerowie Tyniec czekaja na otwarcie zaladunku 
#define SEM_WEJSCIE         7   // Kapitan wpuszcza pasazerow na statek 
#define SEM_ROZLADUNEK      8   // Pasazerowie czekaja na rozladunek 
#define SEM_ROZLADUNEK_KONIEC 9 // Ostatni pasazer sygnalizuje kapitanowi 
#define SEM_DYSPOZYTOR_READY 10 // Dyspozytor czeka na kapitana 
#define SEM_DYSPOZYTOR_EVENT 11 // Zdarzenia dla dyspozytora 
#define LICZBA_SEM          12

//Przystanki
#define PRZYSTANEK_WAWEL  0
#define PRZYSTANEK_TYNIEC 1

//Status kapitana
#define STATUS_ZALADUNEK   0
#define STATUS_REJS        1
#define STATUS_ROZLADUNEK  2
#define STATUS_STOP        3

#define MAX_PASAZEROW_MOSTEK 200

#define RED "\033[31m"
#define GREEN "\033[32m"
#define BLUE "\033[34m"
#define RESET "\033[0m"

//Struktura pasazera w kolejce
typedef struct {
    pid_t pid;
    int ma_rower;     // 1 jesli ma rower 
    int rozmiar;      // 1 lub 2 (z rowerem) 
} PasazerInfo;

//Stan wspoldzielony
typedef struct {
    //Stany statku
    int pasazerowie_na_statku;
    int rowery_na_statku;
    int aktualny_przystanek;
    int liczba_rejsow;
    int koniec_symulacji;
    
    //Stan kapitana
    pid_t pid_kapitan;
    int status_kapitana;
    int zaladunek_otwarty;
    int rejs_id;
    
    //Kolejka na mostku
    PasazerInfo kolejka_mostek[MAX_PASAZEROW_MOSTEK];
    int liczba_na_mostku;
    int wypychanie_aktywne;

    int miejsca_zajete_mostek;
    
    //Lista wypchnietych w biezacym cyklu
    pid_t wypchnieci[MAX_PASAZEROW_MOSTEK];
    int liczba_wypchnietych;
    
    //Pasazerowie czekajacy na wejscie na statek
    int pasazerow_czekajacych_na_wejscie;
    
    //Pasazerowie na statku do rozladunku
    int pasazerow_do_rozladunku;
    
    //Statystyki
    int total_pasazerow_wawel;
    int total_pasazerow_tyniec;
    int pasazerow_odrzuconych;
} StanStatku;

//Operacje na semaforach
static inline int sem_op(int semid, int sem_num, int op, int flags) {
    struct sembuf operacja = {sem_num, op, flags};
    while (semop(semid, &operacja, 1) == -1) {
        if (errno == EINTR) continue;
        if (errno == EIDRM || errno == EINVAL) return -1;
        return -1;
    }
    return 0;
}

//Zajmij zasób
static inline int sem_wait(int semid, int sem_num) {
    return sem_op(semid, sem_num, -1, 0);
}

//Zwolnij zasób
static inline int sem_signal(int semid, int sem_num) {
    return sem_op(semid, sem_num, 1, 0);
}

//Zeruj semafror
static inline int sem_reset(int semid, int sem_num, int value) {
    if (semctl(semid, sem_num, SETVAL, value) == -1) {
        if (errno != EIDRM && errno != EINVAL) {
            perror("sem_reset");
        }
        return -1;
    }
    return 0;
}

//Zajmij wiele jednostek
static inline int sem_wait_n(int semid, int sem_num, int n) {
    struct sembuf operacja = {sem_num, -n, 0};
    while (semop(semid, &operacja, 1) == -1) {
        if (errno == EINTR) continue;
        if (errno == EIDRM || errno == EINVAL) return -1;
        return -1;
    }
    return 0;
}

//Zwolnij wiele jednostek
static inline int sem_signal_n(int semid, int sem_num, int n) {
    struct sembuf operacja = {sem_num, n, 0};
    if (semop(semid, &operacja, 1) == -1) {
        if (errno != EIDRM && errno != EINVAL) {
            perror("sem_signal_n");
        }
        return -1;
    }
    return 0;
}

//Odczytaj wartość semafora
static inline int sem_getval(int semid, int sem_num) {
    int val = semctl(semid, sem_num, GETVAL);
    if (val == -1 && errno != EIDRM && errno != EINVAL) {
        perror("sem_getval");
    }
    return val;
}

//Ustaw wartość semafora
static inline int sem_setval(int semid, int sem_num, int val) {
    if (semctl(semid, sem_num, SETVAL, val) == -1) {
        perror("sem_setval");
        return -1;
    }
    return 0;
}

static inline const char* get_przystanek_nazwa(int przystanek) {
    return (przystanek == PRZYSTANEK_WAWEL) ? "Krakow Wawel" : "Tyniec";
}

static inline int waliduj_parametry(void) {
    if (K >= N) {
        fprintf(stderr, "Blad: pojemnosc mostka K (%d) musi byc mniejsza niz pojemnosc statku N (%d)\n", K, N);
        return -1;
    }
    if (M >= N) {
        fprintf(stderr, "Blad: liczba miejsc na rowery M (%d) musi byc mniejsza niz pojemnosc statku N (%d)\n", M, N);
        return -1;
    }
    if (N <= 0 || M <= 0 || K <= 0 || T1 <= 0 || T2 <= 0 || R <= 0) {
        fprintf(stderr, "Blad: wszystkie parametry musza byc wieksze od 0\n");
        return -1;
    }
    return 0;
}

static inline void wyswietl_konfiguracje(void) {
    printf("=== System Tramwaju Wodnego ===\n");
    printf("Trasa: Krakow Wawel - Tyniec\n");
    printf("Pojemnosc tramwaju (N): %d\n", N);
    printf("Miejsca na rowery (M): %d\n", M);
    printf("Pojemnosc mostka (K): %d\n", K);
    printf("Czas zaladunku (T1): %d s\n", T1);
    printf("Czas rejsu (T2): %d s\n", T2);
    printf("Maksymalna liczba rejsow (R): %d\n", R);
    printf("===============================\n\n");
}

static inline int wczytaj_parametry(void) {
    printf("Podaj parametry systemu:\n");
    
    do {
        printf("Pojemnosc tramwaju N: ");
        if (scanf("%d", &N) != 1) {
            fprintf(stderr, "Blad: podaj liczbe calkowita\n");
            while (getchar() != '\n');
            continue;
        }
        if (N < 1) fprintf(stderr, "Blad: wartosc musi byc wieksza od 0\n");
    } while (N < 1);

    do {
        printf("Miejsca na rowery M (< %d): ", N);
        if (scanf("%d", &M) != 1) {
            while (getchar() != '\n');
            continue;
        }
        if (M < 1 || M >= N) fprintf(stderr, "Blad: wartosc musi byc w zakresie 1-%d\n", N - 1);
    } while (M < 1 || M >= N);

    do {
        printf("Pojemnosc mostka K (< %d): ", N);
        if (scanf("%d", &K) != 1) {
            while (getchar() != '\n');
            continue;
        }
        if (K < 1 || K >= N) fprintf(stderr, "Blad: wartosc musi byc w zakresie 1-%d\n", N - 1);
    } while (K < 1 || K >= N);

    do {
        printf("Czas zaladunku T1 [s]: ");
        if (scanf("%d", &T1) != 1) {
            while (getchar() != '\n');
            continue;
        }
        if (T1 < 1) fprintf(stderr, "Blad: wartosc musi byc wieksza od 0\n");
    } while (T1 < 1);

    do {
        printf("Czas rejsu T2 [s]: ");
        if (scanf("%d", &T2) != 1) {
            while (getchar() != '\n');
            continue;
        }
        if (T2 < 1) fprintf(stderr, "Blad: wartosc musi byc wieksza od 0\n");
    } while (T2 < 1);

    do {
        printf("Maksymalna liczba rejsow R: ");
        if (scanf("%d", &R) != 1) {
            while (getchar() != '\n');
            continue;
        }
        if (R < 1) fprintf(stderr, "Blad: wartosc musi byc wieksza od 0\n");
    } while (R < 1);

    printf("\n");
    return 0;
}

static inline int zapisz_parametry_do_pliku(void) {
    FILE *f = fopen("/tmp/tramwaj_config.txt", "w");
    if (f == NULL) {
        perror("Blad zapisu konfiguracji");
        return -1;
    }
    fprintf(f, "%d\n%d\n%d\n%d\n%d\n%d\n", N, M, K, T1, T2, R);
    fclose(f);
    return 0;
}

static inline int wczytaj_parametry_z_pliku(void) {
    FILE *f = fopen("/tmp/tramwaj_config.txt", "r");
    if (f == NULL) {
        //Domyslne wartosci
        N = 10; M = 3; K = 3; T1 = 2; T2 = 3; R = 5;
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
