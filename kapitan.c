#include "common.h"
#include "logger.h"

int N = 10, M = 3, K = 3, T1 = 2, T2 = 3, R = 5;

static volatile sig_atomic_t wczesniejszy_odjazd = 0;
static volatile sig_atomic_t koniec_pracy = 0;

void sigusr1_handler(int sig) {
    (void)sig;
    wczesniejszy_odjazd = 1;
}

void sigusr2_handler(int sig) {
    (void)sig;
    koniec_pracy = 1;
}

void sigterm_handler(int sig) {
    (void)sig;
    koniec_pracy = 1;
}

//Wypycha pasazerow z mostka od ostatniego w kolejce
void wypchnij_z_mostka(int semid, StanStatku *wspolne) {
    int liczba = wspolne->liczba_na_mostku;
    
    if (liczba == 0) {
        return;
    }
    
    printf(BLUE "[KAPITAN]" RESET "Wypycham %d pasazerow z mostka (od ostatniego)\n", liczba);
    fflush(stdout);
    
    logger_log(LOG_INFO, EVENT_ZALADUNEK_KONIEC, 
               "Wypychanie %d pasazerow z mostka", liczba);
    
    wspolne->wypychanie_aktywne = 1;
    wspolne->liczba_wypchnietych = 0;
    
    //Wypychanie od ostatniego
    for (int i = liczba - 1; i >= 0; i--) {
        PasazerInfo *p = &wspolne->kolejka_mostek[i];
        
        //Dodaj do listy wypchnietych
        wspolne->wypchnieci[wspolne->liczba_wypchnietych++] = p->pid;
        
        printf(BLUE "[KAPITAN]" RESET "Wypychasz pasazera PID:%d%s\n", 
               p->pid, p->ma_rower ? " [ROWER]" : "");
        fflush(stdout);
        
        logger_pasazer_event(EVENT_PASAZER_WYPCHNIETY, p->pid, p->ma_rower,
                            "wypchniety z mostka");
        
        //Wyslij sygnal do pasazera
        kill(p->pid, SIGUSR1);
        
        //Zwolnij miejsce na mostku
        sem_signal_n(semid, SEM_MOSTEK, p->rozmiar);
        wspolne->miejsca_zajete_mostek -= p->rozmiar;
        
        //Zwolnij zarezerwowane miejsce na statku
        sem_signal(semid, SEM_STATEK_LUDZIE);
        if (p->ma_rower) {
            sem_signal(semid, SEM_STATEK_ROWERY);
        }
        
        //Obudz pasazera zeby sprawdzil ze jest wypchniety
        sem_signal(semid, SEM_WEJSCIE);
    }
    
    wspolne->liczba_na_mostku = 0;
    wspolne->miejsca_zajete_mostek = 0;
    wspolne->wypychanie_aktywne = 0;
}

int main() {
    if (wczytaj_parametry_z_pliku() == -1) {
        fprintf(stderr, "Kapitan - blad wczytywania parametrow\n");
        exit(1);
    }

    if (logger_init("tramwaj_wodny.log", 0) == -1) {
        fprintf(stderr, "Kapitan - blad inicjalizacji loggera\n");
        exit(1);
    }

    key_t key = ftok(PATH_NAME, PROJECT_ID);
    if (key == -1) {
        perror("Kapitan - blad ftok");
        logger_close();
        exit(1);
    }

    int shmid = shmget(key, sizeof(StanStatku), 0);
    if (shmid == -1) {
        perror("Kapitan - blad shmget");
        logger_close();
        exit(1);
    }

    StanStatku *wspolne = (StanStatku*)shmat(shmid, NULL, 0);
    if (wspolne == (void*)-1) {
        perror("Kapitan - blad shmat");
        logger_close();
        exit(1);
    }

    int semid = semget(key, LICZBA_SEM, 0);
    if (semid == -1) {
        perror("Kapitan - blad semget");
        shmdt(wspolne);
        logger_close();
        exit(1);
    }

    logger_set_semid(semid);

    //Rejestracja kapitana
    sem_wait(semid, SEM_MUTEX);
    wspolne->pid_kapitan = getpid();
    sem_signal(semid, SEM_MUTEX);

    //informacja dla dyspozytora, ze kapitan jest gotowy
    sem_signal(semid, SEM_DYSPOZYTOR_READY);

    struct sigaction sa_usr1, sa_usr2, sa_term;

    sa_usr1.sa_handler = sigusr1_handler;
    sigemptyset(&sa_usr1.sa_mask);
    sa_usr1.sa_flags = 0;
    sigaction(SIGUSR1, &sa_usr1, NULL);

    sa_usr2.sa_handler = sigusr2_handler;
    sigemptyset(&sa_usr2.sa_mask);
    sa_usr2.sa_flags = 0;
    sigaction(SIGUSR2, &sa_usr2, NULL);

    sa_term.sa_handler = sigterm_handler;
    sigemptyset(&sa_term.sa_mask);
    sa_term.sa_flags = 0;
    sigaction(SIGTERM, &sa_term, NULL);
    sigaction(SIGINT, &sa_term, NULL);

    logger_log(LOG_INFO, EVENT_KAPITAN_START, "Kapitan rozpoczyna prace (PID: %d)", getpid());
    printf(BLUE "[KAPITAN]" RESET" Rozpoczynam prace (PID: %d)\n", getpid());
    fflush(stdout);

    //Glowna petla rejsow
    while (1) {
        sem_wait(semid, SEM_MUTEX);
        
        int rejsow = wspolne->liczba_rejsow;
        int przystanek = wspolne->aktualny_przystanek;
        
        if (koniec_pracy && wspolne->status_kapitana == STATUS_ZALADUNEK) {
            wspolne->koniec_symulacji = 1;
            wspolne->status_kapitana = STATUS_STOP;
            wypchnij_z_mostka(semid, wspolne);
            sem_signal(semid, SEM_MUTEX);
            
            logger_log(LOG_INFO, EVENT_SYGNAL_KONIEC_PRACY,
                      "SIGUSR2 podczas zaladunku - koncze bez rejsu");
            printf(BLUE "[KAPITAN]" RESET "SIGUSR2 - koncze prace bez rejsu\n");
            fflush(stdout);
            break;
        }
        
        if (wspolne->koniec_symulacji || rejsow >= R) {
            wspolne->koniec_symulacji = 1;
            wspolne->status_kapitana = STATUS_STOP;
            sem_signal(semid, SEM_MUTEX);
            
            logger_log(LOG_INFO, EVENT_KAPITAN_STOP, 
                      "Zakonczono prace (rejsow: %d/%d)", rejsow, R);
            printf(BLUE "[KAPITAN]" RESET "Zakonczono - wykonano %d/%d rejsow\n", rejsow, R);
            fflush(stdout);
            break;
        }
        
        //Rozpocznij zaladunek
        wspolne->status_kapitana = STATUS_ZALADUNEK;
        wspolne->zaladunek_otwarty = 1;
        wspolne->rejs_id++;
        wspolne->liczba_wypchnietych = 0;
        
        int rejs_id = wspolne->rejs_id;
        
        sem_signal(semid, SEM_MUTEX);
        
        logger_log(LOG_INFO, EVENT_ZALADUNEK_START,
                  "Zaladunek #%d na przystanku %s", rejs_id, get_przystanek_nazwa(przystanek));
        printf(BLUE"\n[KAPITAN]"RESET "=== REJS #%d - ZALADUNEK na %s ===\n",
               rejs_id, get_przystanek_nazwa(przystanek));
        fflush(stdout);
        
        wczesniejszy_odjazd = 0;
        
        int sem_zaladunek = (przystanek == PRZYSTANEK_WAWEL) 
                          ? SEM_ZALADUNEK_WAWEL 
                          : SEM_ZALADUNEK_TYNIEC;
        //Otworz zaladunek
        for (int i = 0; i < N; i++) {
            sem_signal(semid, sem_zaladunek);
        }
        
        //Czas zaladunku
        for (int sekunda = 0; sekunda < T1; sekunda++) {
            if (wczesniejszy_odjazd) {
                sem_wait(semid, SEM_MUTEX);
                wspolne->zaladunek_otwarty = 0;
                sem_signal(semid, SEM_MUTEX);
                
                for (int i = 0; i < 200; i++) {
                    sem_trywait(semid, sem_zaladunek);
                }
                for (int i = 0; i < 200; i++) {
                    sem_trywait(semid, SEM_WEJSCIE);
                }
                
                logger_log(LOG_INFO, EVENT_SYGNAL_ODPLYNIECIE,
                          "Wczesniejszy odjazd po %d/%d s", sekunda, T1);
                printf(BLUE "[KAPITAN]" RESET "SIGUSR1 - wczesniejszy odjazd po %d/%d s\n", sekunda, T1);
                fflush(stdout);
                break;
            }
            
            if (koniec_pracy) {
                sem_wait(semid, SEM_MUTEX);
                wspolne->zaladunek_otwarty = 0;
                sem_signal(semid, SEM_MUTEX);
                break;
            }
            
            //Wpuszczaj pasazerow z mostka
            sem_wait(semid, SEM_MUTEX);
            int na_mostku = wspolne->liczba_na_mostku;
            int czekajacych = wspolne->pasazerow_czekajacych_na_wejscie;
            sem_signal(semid, SEM_MUTEX);
            
            int do_wyslania = (na_mostku > czekajacych) ? na_mostku : czekajacych;
            for (int j = 0; j < do_wyslania; j++) {
                sem_signal(semid, SEM_WEJSCIE);
            }
            
            sem_signal(semid, SEM_DYSPOZYTOR_EVENT);
            
            //Symulacja uplywu czasu
            sleep(1);
        }
        
        //Zamknij zaladunek
        sem_wait(semid, SEM_MUTEX);
        wspolne->zaladunek_otwarty = 0;
        sem_signal(semid, SEM_MUTEX);
        
        for (int i = 0; i < 200; i++) {
            sem_trywait(semid, sem_zaladunek);
        }
        
        for (int i = 0; i < 200; i++) {
            sem_trywait(semid, SEM_WEJSCIE);
        }
        
        sem_wait(semid, SEM_MUTEX);
        
        wypchnij_z_mostka(semid, wspolne);

        int pasazerow = wspolne->pasazerowie_na_statku;
        int rowerow = wspolne->rowery_na_statku;
        int na_mostku = wspolne->liczba_na_mostku;
        
        printf(BLUE "[KAPITAN]" RESET "Zaladunek zakonczony: %d pasazerow, %d rowerow\n", 
               pasazerow, rowerow);
        fflush(stdout);
        logger_log(LOG_INFO, EVENT_ZALADUNEK_KONIEC,
                  "Zaladunek zakonczony: %d pasazerow, %d rowerow", pasazerow, rowerow);

        if (na_mostku > 0) {
            wypchnij_z_mostka(semid, wspolne);
        }
        
        //Sprawdz SIGUSR2 po zamknieciu zaladunku
        if (koniec_pracy) {
            wspolne->koniec_symulacji = 1;
            wspolne->status_kapitana = STATUS_STOP;
            
            //Jesli sa pasazerowie na statku to musza wysiasc
            if (pasazerow > 0) {
                wspolne->pasazerow_do_rozladunku = pasazerow;
                sem_signal(semid, SEM_MUTEX);
                
                for (int i = 0; i < pasazerow + 10; i++) {
                    sem_signal(semid, SEM_ROZLADUNEK);
                }
                //Czekaj na zejscie wszystkich
                sem_wait(semid, SEM_ROZLADUNEK_KONIEC);
            } else {
                sem_signal(semid, SEM_MUTEX);
            }
            
            logger_log(LOG_INFO, EVENT_SYGNAL_KONIEC_PRACY,
                      "SIGUSR2 - koncze po zaladunku");
            printf(BLUE "[KAPITAN]" RESET "SIGUSR2 - koncze prace\n");
            fflush(stdout);
            break;
        }
        
        sem_signal(semid, SEM_MUTEX);
        
        //Rozpocznij rejs
        sem_wait(semid, SEM_MUTEX);
        
        wspolne->status_kapitana = STATUS_REJS;
        wspolne->liczba_rejsow++;
        rejsow = wspolne->liczba_rejsow;
        
        if (przystanek == PRZYSTANEK_WAWEL) {
            wspolne->total_pasazerow_wawel += pasazerow;
        } else {
            wspolne->total_pasazerow_tyniec += pasazerow;
        }
        
        wspolne->pasazerow_do_rozladunku = pasazerow;
        
        sem_signal(semid, SEM_MUTEX);
        
        const char* cel = get_przystanek_nazwa(
            przystanek == PRZYSTANEK_WAWEL ? PRZYSTANEK_TYNIEC : PRZYSTANEK_WAWEL);
        
        logger_rejs_event(EVENT_REJS_START, rejsow, pasazerow, rowerow,
                         get_przystanek_nazwa(przystanek));
        printf(BLUE "[KAPITAN]" RESET "REJS #%d START: %s -> %s (%d pasazerow, %d rowerow)\n",
               rejsow, get_przystanek_nazwa(przystanek), cel, pasazerow, rowerow);
        fflush(stdout);
        
        //Czas rejsu T2 - symulacja czasu
        for (int sekunda = 0; sekunda < T2; sekunda++) {
            if (koniec_pracy) {
                //SIGUSR2 w trakcie rejsu - kontynuujemy normalnie
                logger_log(LOG_INFO, EVENT_SYGNAL_KONIEC_PRACY,
                          "SIGUSR2 w trakcie rejsu - koncze rejs normalnie");
                printf(BLUE "[KAPITAN]" RESET "SIGUSR2 w trakcie rejsu - dokoncze rejs\n");
                fflush(stdout);
            }
            //Symulacja upływu czasu
            sleep(1);
        }
        
        //Dotarcie do celu
        sem_wait(semid, SEM_MUTEX);
        
        int nowy_przystanek = (przystanek == PRZYSTANEK_WAWEL) 
                            ? PRZYSTANEK_TYNIEC 
                            : PRZYSTANEK_WAWEL;
        wspolne->aktualny_przystanek = nowy_przystanek;
        wspolne->status_kapitana = STATUS_ROZLADUNEK;
        
        sem_signal(semid, SEM_MUTEX);
        
        logger_rejs_event(EVENT_REJS_KONIEC, rejsow, pasazerow, rowerow,
                         get_przystanek_nazwa(nowy_przystanek));
        printf(BLUE "[KAPITAN]" RESET "REJS #%d KONIEC - dotarlismy do %s\n",
               rejsow, get_przystanek_nazwa(nowy_przystanek));
        printf(BLUE  "[KAPITAN]" RESET "Rozpoczynam rozladunek\n");
        fflush(stdout);
        
        //Otworz rozladunek
        if (pasazerow > 0) {
            for (int i = 0; i < pasazerow + 10; i++) {
                sem_signal(semid, SEM_ROZLADUNEK);
            }
            
            //Czekaj az ostatni pasazer zejdzie
            sem_wait(semid, SEM_ROZLADUNEK_KONIEC);
            
            for (int i = 0; i < pasazerow + 10; i++) {
                sem_trywait(semid, SEM_ROZLADUNEK);
            }
        }
        
        logger_log(LOG_INFO, EVENT_REJS_KONIEC, "Rozladunek zakonczony");
        printf(BLUE "[KAPITAN]" RESET "Rozladunek zakonczony\n");
        fflush(stdout);
        
        //Sprawdz czy konczymy po tym rejsie
        if (koniec_pracy) {
            sem_wait(semid, SEM_MUTEX);
            wspolne->koniec_symulacji = 1;
            wspolne->status_kapitana = STATUS_STOP;
            sem_signal(semid, SEM_MUTEX);
            
            printf(BLUE "[KAPITAN]" RESET "Koncze prace po rejsie #%d\n", rejsow);
            fflush(stdout);
            break;
        }
    }
    
    //Obudz dyspozytora zeby mogl zakonczyc
    sem_signal(semid, SEM_DYSPOZYTOR_EVENT);
    
    //Obudz wszystkich czekajacych pasazerow zeby mogli zakonczyc
    printf(BLUE "[KAPITAN]" RESET "Budze oczekujacych pasazerow...\n");
    fflush(stdout);
    
    for (int i = 0; i < 200; i++) {
        sem_signal(semid, SEM_ZALADUNEK_WAWEL);
        sem_signal(semid, SEM_ZALADUNEK_TYNIEC);
        sem_signal(semid, SEM_WEJSCIE);
        sem_signal(semid, SEM_ROZLADUNEK);
    }
    
    logger_log(LOG_INFO, EVENT_KAPITAN_STOP, "Kapitan konczy prace");
    printf(BLUE "[KAPITAN]" RESET "Koniec pracy\n");
    fflush(stdout);
    
    shmdt(wspolne);
    logger_close();
    return 0;
}