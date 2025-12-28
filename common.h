#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>



// Parametry do zadania
#define N 10 //pojemnosc statku
#define M 3 //miejsca na rowery
#define K 5 //pojemnosc mostka
#define T1 2 //czas oczekiwania w sekundach
#define T2 3 //czas rejsu w sekundach
#define R 5  //limit rejsow

#define PROJECT_ID 'T'
#define PATH_NAME "."

typedef struct {
	int pasazerowie_statek; //aktualnie pasazerow na statku
	int rowery_statek; //aktualnie rowerow na statku
	int pasazerowie_mostek; //aktualnie pasazerow na mostku
	int czy_plynie; // 0 - w porcie, 1 - plynie
	int liczba_rejsow; //licznik rejsow
} StanStatku;

#endif
