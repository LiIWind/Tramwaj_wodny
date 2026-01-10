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

#define SEM_MOSTEK 0
#define SEM_STATEK 1
#define SEM_DOSTEP 2
#define LICZBA_SEM 3


typedef struct {
	int pasazerowie_statek; //aktualnie pasazerow na statku
	int rowery_statek; //aktualnie rowerow na statku
	int pasazerowie_mostek; //aktualnie pasazerow na mostku
	int czy_plynie; // 0 - w porcie, 1 - plynie
	int liczba_rejsow; //licznik rejsow
	pid_t pid_kapitan;
	int status_kapitana;
} StanStatku;


//funkcje pomocnicze
static inline int zajmij_zasob(int semid, int sem_num){
	struct sembuf operacja = {sem_num, -1, 0}; //zmniejszenie licznika np. wchodzac na mostek zmniejsza sie miejsce na mostku, jak jest 0 to czeka
	while (semop(semid, &operacja, 1) == -1){
		if(errno != EINTR) continue; //jesli przerwano sygnalem 

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

#endif
