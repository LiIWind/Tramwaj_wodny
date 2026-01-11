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


// Parametry do zadania
#define N 10 //pojemnosc statku
#define M 3 //miejsca na rowery
#define K 3 //pojemnosc mostka
#define T1 2 //czas oczekiwania w sekundach
#define T2 3 //czas rejsu w sekundach
#define R 5  //limit rejsow

#define PROJECT_ID 'D'
#define PATH_NAME "."

#define SEM_MOSTEK 0 //limity mostka
#define SEM_STATEK_LUDZIE 1 //limit ludzi na statku
#define SEM_STATEK_ROWERY 2 //limit rowerow na statku
#define SEM_DOSTEP 3 //mutex do pamieci dzielonej
#define SEM_KIERUNEK 4 //kontrola kierunku ruchu na mostku
#define LICZBA_SEM 5

#define KIERUNEK_BRAK 0
#define KIERUNEK_NA_STATEK 1
#define KIERUNEK_ZE_STATKU 2

typedef struct {
	int pasazerowie_statek; //aktualnie pasazerow na statku
	int rowery_statek; //aktualnie rowerow na statku
	int pasazerowie_mostek; //aktualnie pasazerow na mostku
	int czy_plynie; // 0 - w porcie, 1 - plynie
	int liczba_rejsow; //licznik rejsow
	int kierunek_mostka; //kierunek ruchu na mostku
	pid_t pid_kapitan;
	int status_kapitana; //0 - rejs, 1- port
	int koniec_symulacji;
} StanStatku;


//funkcje pomocnicze
static inline int zajmij_zasob(int semid, int sem_num){
	struct sembuf operacja = {sem_num, -1, 0}; //zmniejszenie licznika np. wchodzac na mostek zmniejsza sie miejsce na mostku, jak jest 0 to czeka
	while (semop(semid, &operacja, 1) == -1){
		if(errno == EINTR) continue; //jesli przerwano sygnalem 

		if(errno == EIDRM || errno == EINVAL) return -1; //jesli usunieto semafor

		perror("Blad zajmowania semafora");
		return -1;
	}
	return 0;
}

static inline int zwolnij_zasob(int semid, int sem_num){
        struct sembuf operacja = {sem_num, 1, 0}; //zwiekszenie licznika
        if (semop(semid, &operacja, 1) == -1){
                perror("Blad zwalniania semafora");
		return -1;
        }
	return 0;
}

//proba podjecia zasobu bez blokowania
static inline int sprobuj_zajac_zasob(int semid, int sem_num) {
	struct sembuf operacja = {sem_num, -1, IPC_NOWAIT};
	if (semop(semid, &operacja, 1) == -1){
		if(errno == EAGAIN) return 0; //zasob zajety
		if(errno == EIDRM || errno == EINVAL) return -1; //semafor usuniety
		return -1;
	}
	return 1; //sukces
}

//bezpieczny odczyt z pamieci dzielonej
static inline int bezpieczny_odczyt_int(int semid, int *zmienna){
	if (zajmij_zasob(semid, SEM_DOSTEP) == -1) return -1;
	int wartosc = *zmienna;
	zwolnij_zasob(semid, SEM_DOSTEP);
	return wartosc;
}
#endif
