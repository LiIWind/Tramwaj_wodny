#include "common.h"
#include "logger.h"

int N = 10, M = 3, K = 3, T1 = 2, T2 = 3, R = 5;

static volatile sig_atomic_t koniec_pracy = 0;

void sigterm_handler(int sig) {
    (void)sig;
    koniec_pracy = 1;
}

int main() {
    if (wczytaj_parametry_z_pliku() == -1) {
        fprintf(stderr, "Dyspozytor - blad wczytywania parametrow\n");
        exit(1);
    }
    
    if (logger_init("tramwaj_wodny.log", 0) == -1) {
        fprintf(stderr, "Dyspozytor - blad inicjalizacji loggera\n");
        exit(1);
    }
    
    key_t key = ftok(PATH_NAME, PROJECT_ID);
    if (key == -1) {
        perror("Dyspozytor - blad ftok");
        logger_close();
        exit(1);
    }
    
    int shmid = shmget(key, sizeof(StanStatku), 0);
    if (shmid == -1) {
        perror("Dyspozytor - blad shmget");
        logger_close();
        exit(1);
    }
    
    StanStatku *wspolne = (StanStatku*)shmat(shmid, NULL, 0);
    if (wspolne == (void*)-1) {
        perror("Dyspozytor - blad shmat");
        logger_close();
        exit(1);
    }
    
    int semid = semget(key, LICZBA_SEM, 0);
    if (semid == -1) {
        perror("Dyspozytor - blad semget");
        shmdt(wspolne);
        logger_close();
        exit(1);
    }
    
    logger_set_semid(semid);
    
    struct sigaction sa_term;
    sa_term.sa_handler = sigterm_handler;
    sigemptyset(&sa_term.sa_mask);
    sa_term.sa_flags = 0;
    sigaction(SIGTERM, &sa_term, NULL);
    sigaction(SIGINT, &sa_term, NULL);
    
    logger_log(LOG_INFO, EVENT_DYSPOZYTOR_START, "Dyspozytor rozpoczyna prace");
    printf(GREEN "[DYSPOZYTOR]" RESET "Rozpoczynam prace (PID: %d)\n", getpid());
    fflush(stdout);
    
    if (sem_wait(semid, SEM_DYSPOZYTOR_READY) == -1) {
        logger_error_errno(EVENT_BLAD_SYSTEM, "Blad oczekiwania na kapitana");
        shmdt(wspolne);
        logger_close();
        exit(1);
    }
    
    //Pobierz PID kapitana
    sem_wait(semid, SEM_MUTEX);
    pid_t pid_kapitan = wspolne->pid_kapitan;
    sem_signal(semid, SEM_MUTEX);
    
    logger_log(LOG_INFO, EVENT_DYSPOZYTOR_START, 
               "Zidentyfikowano kapitana (PID: %d)", pid_kapitan);
    printf(GREEN "[DYSPOZYTOR]" RESET "Kapitan gotowy (PID: %d)\n", pid_kapitan);
    fflush(stdout);
    
    int wyslalem_sigusr1 = 0;
    int wyslalem_sigusr2 = 0;
    
    // Glowna petla - monitorowanie rejsow
    while (!koniec_pracy) {
        // Czekaj na zdarzenie od kapitana
        if (sem_wait(semid, SEM_DYSPOZYTOR_EVENT) == -1) {
            if (errno == EINTR) {
                if (koniec_pracy) break;
                continue;
            }
            if (errno == EIDRM || errno == EINVAL) {
                break;
            }
            continue;
        }
        
        // Sprawdz aktualny stan
        sem_wait(semid, SEM_MUTEX);
        
        int rejsow = wspolne->liczba_rejsow;
        int koniec = wspolne->koniec_symulacji;
        int status = wspolne->status_kapitana;
        
        sem_signal(semid, SEM_MUTEX);
        
        if (koniec || rejsow >= R) {
            logger_log(LOG_INFO, EVENT_DYSPOZYTOR_STOP,
                      "Koncze prace (rejsow: %d/%d)", rejsow, R);
            printf(GREEN "[DYSPOZYTOR]" RESET "Koncze prace (rejsow: %d/%d)\n", rejsow, R);
            fflush(stdout);
            break;
        }
        
        
        //SIGUSR1 - wczesniejszy odjazd po 2 rejsach, podczas zaladunku
        if (rejsow >= 2 && !wyslalem_sigusr1 && status == STATUS_ZALADUNEK) {
            if (kill(pid_kapitan, SIGUSR1) == 0) {
                logger_sygnal("SIGUSR1 (wczesniejszy odjazd)", getpid(), pid_kapitan);
                logger_log(LOG_INFO, EVENT_SYGNAL_ODPLYNIECIE,
                          "Wyslano SIGUSR1 do kapitana");
                printf(GREEN "\n[DYSPOZYTOR]" RESET "- SIGUSR1 - Wczesniejszy odjazd (po %d rejsach)\n\n",
                       rejsow);
                fflush(stdout);
                wyslalem_sigusr1 = 1;
            } else if (errno != ESRCH) {
                perror("Dyspozytor - blad SIGUSR1");
            }
        }
        
        //SIGUSR2 - koniec pracy po 75% rejsow
        if (rejsow >= (R * 3 / 4) && !wyslalem_sigusr2) {
            if (kill(pid_kapitan, SIGUSR2) == 0) {
                logger_sygnal("SIGUSR2 (koniec pracy)", getpid(), pid_kapitan);
                logger_log(LOG_INFO, EVENT_SYGNAL_KONIEC_PRACY,
                          "Wyslano SIGUSR2 do kapitana");
                printf(GREEN "\n[DYSPOZYTOR]" RESET "- SIGUSR2 - Koniec pracy (po %d rejsach)\n\n",
                       rejsow);
                fflush(stdout);
                wyslalem_sigusr2 = 1;
                break;
            } else if (errno != ESRCH) {
                perror("Dyspozytor - blad SIGUSR2");
            }
        }
    }
    
    logger_log(LOG_INFO, EVENT_DYSPOZYTOR_STOP, "Dyspozytor konczy prace");
    printf(GREEN "[DYSPOZYTOR]" RESET "Koniec pracy\n");
    fflush(stdout);
    
    shmdt(wspolne);
    logger_close();
    return 0;
}
