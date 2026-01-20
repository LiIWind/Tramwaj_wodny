#include "common.h"
#include "logger.h"

int N = 10, M = 3, K = 3, T1 = 2, T2 = 3, R = 5;

static volatile sig_atomic_t cleanup_flag = 0;
static int g_semid = -1;
static int g_shmid = -1;
static pid_t g_pid_kapitan = -1;
static pid_t g_pid_dyspozytor = -1;

void cleanup_handler(int sig) {
    (void)sig;
    cleanup_flag = 1;
}

void cleanup_all_resources(void) {
    printf("\n[MAIN] Czyszczenie zasobow...\n");
    
    // Ustaw flage konca w pamieci dzielonej
    if (g_shmid != -1) {
        StanStatku *ws = (StanStatku*)shmat(g_shmid, NULL, 0);
        if (ws != (void*)-1) {
            ws->koniec_symulacji = 1;
            shmdt(ws);
        }
    }
    
    // Wyslij SIGTERM do kapitana i dyspozytora
    if (g_pid_kapitan > 0) {
        kill(g_pid_kapitan, SIGTERM);
    }
    if (g_pid_dyspozytor > 0) {
        kill(g_pid_dyspozytor, SIGTERM);
    }
    
    // Wyslij SIGTERM do wszystkich procesow w grupie (pasazerow)
    signal(SIGTERM, SIG_IGN);
    kill(0, SIGTERM);
    
    // Usun semafory - to odblokuje wszystkich czekajacych
    if (g_semid != -1) {
        printf("[MAIN] Usuwam semafory (semid=%d)...\n", g_semid);
        if (semctl(g_semid, 0, IPC_RMID) == -1) {
            if (errno != EIDRM && errno != EINVAL) {
                perror("Blad usuwania semaforow");
            }
        }
        g_semid = -1;
    }
    
    int wait_count = 0;
    while (wait_count < 20) {
        int ret1 = (g_pid_kapitan > 0) ? waitpid(g_pid_kapitan, NULL, WNOHANG) : -1;
        int ret2 = (g_pid_dyspozytor > 0) ? waitpid(g_pid_dyspozytor, NULL, WNOHANG) : -1;
        if ((ret1 != 0 || ret1 == -1) && (ret2 != 0 || ret2 == -1)) {
            break;
        }
        wait_count++;
    }
    
    if (g_pid_kapitan > 0) {
        kill(g_pid_kapitan, SIGKILL);
        waitpid(g_pid_kapitan, NULL, 0);
        g_pid_kapitan = -1;
    }
    if (g_pid_dyspozytor > 0) {
        kill(g_pid_dyspozytor, SIGKILL);
        waitpid(g_pid_dyspozytor, NULL, 0);
        g_pid_dyspozytor = -1;
    }
    
    // Zbierz pozostale procesy zombie - agresywnie
    int bez_zmian = 0;
    while (bez_zmian < 1000) {
        pid_t child = waitpid(-1, NULL, WNOHANG);
        if (child > 0) {
            bez_zmian = 0;
        } else if (child == -1 && errno == ECHILD) {
            break;
        } else {
            bez_zmian++;
        }
    }
    //Ostateczne zbieranie
    while (waitpid(-1, NULL, WNOHANG) > 0) {}
    
    // Usun pamiec dzielona
    if (g_shmid != -1) {
        printf("[MAIN] Usuwam pamiec dzielona (shmid=%d)...\n", g_shmid);
        if (shmctl(g_shmid, IPC_RMID, NULL) == -1) {
            if (errno != EIDRM && errno != EINVAL) {
                perror("Blad usuwania pamieci dzielonej");
            }
        }
        g_shmid = -1;
    }
    
    // Usun plik konfiguracyjny
    unlink("/tmp/tramwaj_config.txt");
    
    printf("[MAIN] Zasoby wyczyszczone.\n");
}

//zbiera procesy zombie
void sigchld_handler(int sig) {
    (void)sig;
    int saved_errno = errno;
    
    while (waitpid(-1, NULL, WNOHANG) > 0) {
        //zbieramy wszystkie zakończone procesy
    }
    errno = saved_errno;
}

//Aktywne zbieranie zombie
static void zbierz_zombie(void) {
    while (waitpid(-1, NULL, WNOHANG) > 0) {
        //zbieramy wszystkie dostępne zombie
    }
}

int main() {
    printf("TRAMWAJ WODNY Krakow Wawel - Tyniec\n\n");
    
    //usun stary log
    unlink("tramwaj_wodny.log");
    
    if (wczytaj_parametry() == -1) {
        fprintf(stderr, "Blad wczytywania parametrow\n");
        exit(1);
    }
    
    if (waliduj_parametry() == -1) {
        fprintf(stderr, "Blad walidacji parametrow\n");
        exit(1);
    }
    
    //zapisz parametry do pliku dla procesów potomnych
    if (zapisz_parametry_do_pliku() == -1) {
        exit(1);
    }
    
    if (logger_init("tramwaj_wodny.log", 1) == -1) {
        fprintf(stderr, "Blad inicjalizacji loggera\n");
        exit(1);
    }
    
    wyswietl_konfiguracje();
    
    logger_log(LOG_INFO, EVENT_SYSTEM_START, "Start systemu tramwaju wodnego");
    logger_log(LOG_INFO, EVENT_SYSTEM_START, 
               "Parametry: N=%d, M=%d, K=%d, T1=%d, T2=%d, R=%d",
               N, M, K, T1, T2, R);
    
    struct sigaction sa;
    sa.sa_handler = cleanup_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    struct sigaction sa_chld;
    sa_chld.sa_handler = sigchld_handler;
    sigemptyset(&sa_chld.sa_mask);
    sa_chld.sa_flags = SA_RESTART | SA_NOCLDSTOP; // SA_RESTART - nie przerywaj funkcji systemowych, SA_NOCLDSTOP - nie sygnalizuj zatrzymania (tylko zakończenie)
    
    sigaction(SIGCHLD, &sa_chld, NULL);
    
    key_t key = ftok(PATH_NAME, PROJECT_ID);
    if (key == -1) {
        perror("Blad ftok");
        logger_error_errno(EVENT_BLAD_SYSTEM, "Blad generowania klucza IPC");
        logger_close();
        exit(1);
    }
    
    int shmid = shmget(key, sizeof(StanStatku), 0600 | IPC_CREAT);
    if (shmid == -1) {
        perror("Blad shmget");
        logger_error_errno(EVENT_BLAD_SYSTEM, "Blad tworzenia pamieci dzielonej");
        logger_close();
        exit(1);
    }
    
    StanStatku *wspolne = (StanStatku*)shmat(shmid, NULL, 0);
    if (wspolne == (void*)-1) {
        perror("Blad shmat");
        logger_error_errno(EVENT_BLAD_SYSTEM, "Blad przyłączania pamieci dzielonej");
        cleanup_all_resources();
        logger_close();
        exit(1);
    }
    
    memset(wspolne, 0, sizeof(StanStatku));
    wspolne->aktualny_przystanek = PRZYSTANEK_WAWEL;
    wspolne->status_kapitana = STATUS_ZALADUNEK;
    
    shmdt(wspolne);
    
    int semid = semget(key, LICZBA_SEM, 0600 | IPC_CREAT);
    if (semid == -1) {
        perror("Blad semget");
        logger_error_errno(EVENT_BLAD_SYSTEM, "Blad tworzenia semaforow");
        cleanup_all_resources();
        logger_close();
        exit(1);
    }
    
    g_semid = semid;

    //inicjalizacja semaforów
    sem_setval(semid, SEM_MUTEX, 1);
    sem_setval(semid, SEM_MOSTEK, K);
    sem_setval(semid, SEM_STATEK_LUDZIE, N);
    sem_setval(semid, SEM_STATEK_ROWERY, M);
    sem_setval(semid, SEM_LOGGER, 1);
    sem_setval(semid, SEM_ZALADUNEK_WAWEL, 0);
    sem_setval(semid, SEM_ZALADUNEK_TYNIEC, 0);
    sem_setval(semid, SEM_WEJSCIE, 0);
    sem_setval(semid, SEM_ROZLADUNEK, 0);
    sem_setval(semid, SEM_ROZLADUNEK_KONIEC, 0);
    sem_setval(semid, SEM_DYSPOZYTOR_READY, 0);
    sem_setval(semid, SEM_DYSPOZYTOR_EVENT, 0);
    
    logger_set_semid(semid);
    
    printf("Zasoby IPC utworzone (shmid=%d, semid=%d)\n\n", shmid, semid);
    logger_log(LOG_INFO, EVENT_SYSTEM_START, "Zasoby IPC gotowe");
    
    //uruchom kapitana
    pid_t pid_kapitan = fork();
    if (pid_kapitan == -1) {
        perror("Blad fork kapitana");
        logger_error_errno(EVENT_BLAD_SYSTEM, "Blad tworzenia procesu kapitana");
        cleanup_all_resources();
        logger_close();
        exit(1);
    }
    
    if (pid_kapitan == 0) {
        signal(SIGCHLD, SIG_DFL);
        execl("./kapitan", "kapitan", NULL);
        perror("Blad execl kapitana");
        exit(1);
    }

    g_pid_kapitan = pid_kapitan;
    
    logger_log(LOG_INFO, EVENT_KAPITAN_START, "Uruchomiono kapitana (PID: %d)", pid_kapitan);
    
    //uruchom dyspozytora
    pid_t pid_dyspozytor = fork();
    if (pid_dyspozytor == -1) {
        perror("Blad fork dyspozytora");
        logger_error_errno(EVENT_BLAD_SYSTEM, "Blad tworzenia procesu dyspozytora");
        kill(pid_kapitan, SIGTERM);
        cleanup_all_resources();
        logger_close();
        exit(1);
    }
    
    if (pid_dyspozytor == 0) {
        signal(SIGCHLD, SIG_DFL);
        execl("./dyspozytor", "dyspozytor", NULL);
        perror("Blad execl dyspozytora");
        exit(1);
    }

    g_pid_dyspozytor = pid_dyspozytor;
    
    logger_log(LOG_INFO, EVENT_DYSPOZYTOR_START, "Uruchomiono dyspozytora (PID: %d)", pid_dyspozytor);
    
    //generowanie pasazerow
    printf("Generowanie pasazerow...\n\n");
    
    int liczba_pasazerow = 10000;
    int wygenerowano = 0;
    
    wspolne = (StanStatku*)shmat(shmid, NULL, 0);
    if (wspolne == (void*)-1) {
        perror("Blad shmat");
        logger_error_errno(EVENT_BLAD_SYSTEM, "Blad przylaczania pamieci dzielonej przed generowaniem pasazerow");
        cleanup_all_resources();
        logger_close();
        exit(1);
    }
    
    for (int i = 0; i < liczba_pasazerow && !cleanup_flag; i++) {
        //Aktywne zbieranie zombie co 100 pasażerów
        if (i % 100 == 0) {
            zbierz_zombie();
        }
        
        //sprawdz czy symulacja sie skonczyla
        sem_wait(semid, SEM_MUTEX);
        int koniec = wspolne->koniec_symulacji;
        sem_signal(semid, SEM_MUTEX);
        
        if (koniec) {
            printf("Symulacja zakonczona - przerywam generowanie pasazerow\n");
            break;
        }
        
        logger_close();
        pid_t pid = fork();
        if (pid == -1) {
            perror("Blad fork pasazera");
            logger_error_errno(EVENT_BLAD_SYSTEM, "Blad tworzenia procesu pasazera");
            printf("[MAIN] Blad fork pasazera - czyszcze zasoby i koncze...\n");
            shmdt(wspolne);
            cleanup_all_resources();
            logger_log(LOG_ERROR, EVENT_BLAD_SYSTEM, "Zakonczenie z powodu bledu fork");
            logger_close();
            exit(1);
        }
        
        if (pid == 0) {
            signal(SIGCHLD, SIG_DFL);
            execl("./pasazer", "pasazer", NULL);
            perror("Blad execl pasazera");
            exit(1);
        }
        
        wygenerowano++;
        //usleep(50000);
    }
    
    shmdt(wspolne);
    
    printf("Wygenerowano %d pasazerow\n\n", wygenerowano);
    logger_log(LOG_INFO, EVENT_SYSTEM_START, "Wygenerowano %d pasazerow", wygenerowano);
    
    printf("Czekam na zakonczenie kapitana i dyspozytora...\n");
     
    if (cleanup_flag) {
        // Ustaw flage konca w pamieci dzielonej
        StanStatku *ws = (StanStatku*)shmat(shmid, NULL, 0);
        if (ws != (void*)-1) {
            ws->koniec_symulacji = 1;
            shmdt(ws);
        }
        
        // Wyslij SIGTERM do kapitana i dyspozytora
        kill(pid_kapitan, SIGTERM);
        kill(pid_dyspozytor, SIGTERM);
        
        // Wyslij SIGTERM do wszystkich pasazerow
        signal(SIGTERM, SIG_IGN);
        kill(0, SIGTERM);
        
        // Usun semafory - to odblokuje wszystkich czekajacych
        printf("Usuwam semafory aby odblokowac procesy...\n");
        if (semctl(semid, 0, IPC_RMID) == -1) {
            if (errno != EIDRM && errno != EINVAL) {
                perror("Blad usuwania semaforow");
            }
        }
        g_semid = -1;
        
        // Aktywnie zbiera zombie podczas oczekiwania
        int wait_count = 0;
        int kapitan_zakonczony = 0;
        int dyspozytor_zakonczony = 0;
        
        while (wait_count < 100 && (!kapitan_zakonczony || !dyspozytor_zakonczony)) {
            zbierz_zombie();
            
            if (!kapitan_zakonczony) {
                int ret = waitpid(pid_kapitan, NULL, WNOHANG);
                if (ret == pid_kapitan || ret == -1) {
                    kapitan_zakonczony = 1;
                }
            }
            if (!dyspozytor_zakonczony) {
                int ret = waitpid(pid_dyspozytor, NULL, WNOHANG);
                if (ret == pid_dyspozytor || ret == -1) {
                    dyspozytor_zakonczony = 1;
                }
            }
            wait_count++;
        }
        
        // Jesli nadal dzialaja, wyslij SIGKILL
        if (!kapitan_zakonczony) {
            kill(pid_kapitan, SIGKILL);
            waitpid(pid_kapitan, NULL, 0);
        }
        if (!dyspozytor_zakonczony) {
            kill(pid_dyspozytor, SIGKILL);
            waitpid(pid_dyspozytor, NULL, 0);
        }
        
    } else {
        // Normalne zakonczenie - czekaj na kapitana i dyspozytora ze zbieraniem zombie
        int kapitan_zakonczony = 0;
        int dyspozytor_zakonczony = 0;
        
        while (!kapitan_zakonczony || !dyspozytor_zakonczony) {
            zbierz_zombie();
            
            if (!kapitan_zakonczony) {
                int ret = waitpid(pid_kapitan, NULL, WNOHANG);
                if (ret == pid_kapitan) {
                    kapitan_zakonczony = 1;
                    logger_log(LOG_INFO, EVENT_KAPITAN_STOP, "Kapitan zakonczyl prace");
                } else if (ret == -1 && errno != EINTR) {
                    kapitan_zakonczony = 1;
                }
            }
            if (!dyspozytor_zakonczony) {
                int ret = waitpid(pid_dyspozytor, NULL, WNOHANG);
                if (ret == pid_dyspozytor) {
                    dyspozytor_zakonczony = 1;
                    logger_log(LOG_INFO, EVENT_DYSPOZYTOR_STOP, "Dyspozytor zakonczyl prace");
                } else if (ret == -1 && errno != EINTR) {
                    dyspozytor_zakonczony = 1;
                }
            }
        }
    }

    wspolne = (StanStatku*)shmat(shmid, NULL, 0);
    if (wspolne != (void*)-1) {
        sem_wait(semid, SEM_MUTEX);
        
        int przewiezieni = wspolne->total_pasazerow_wawel + wspolne->total_pasazerow_tyniec;
        int juz_odrzuceni = wspolne->pasazerow_odrzuconych;
        
        // Pasażerowie którzy nie zdążyli się zliczyć = wygenerowano - przewiezieni - już_odrzuceni
        // To są ci którzy czekają na semaforach i zostaną zabici przez SIGTERM
        int niezliczeni = wygenerowano - przewiezieni - juz_odrzuceni;
        if (niezliczeni > 0) {
            wspolne->pasazerow_odrzuconych += niezliczeni;
        }
        
        sem_signal(semid, SEM_MUTEX);
        shmdt(wspolne);
    }
    
    // Wyslij SIGTERM do pozostalych pasazerow (jesli jeszcze nie wyslano)
    if (!cleanup_flag) {
        printf("Wysyłam SIGTERM do pasazerow...\n");
        signal(SIGTERM, SIG_IGN);
        kill(0, SIGTERM);
    }

    printf("Czekam na zakończenie pasazerow...\n");
    
    //Agresywne zbieranie wszystkich zombie
    int zakonczone = 0;
    int poprzednie_zakonczone = -1;
    int bez_zmian = 0;
    
    //Zbieraj dopóki są zombie do zebrania
    while (bez_zmian < 1000) {
        poprzednie_zakonczone = zakonczone;
        
        pid_t child;
        while ((child = waitpid(-1, NULL, WNOHANG)) > 0) {
            zakonczone++;
        }
        
        if (child == -1 && errno == ECHILD) {
            //Nie ma więcej procesów potomnych
            break;
        }
        
        if (zakonczone == poprzednie_zakonczone) {
            bez_zmian++;
        } else {
            bez_zmian = 0;
        }
    }
    
    //Ostateczne zbieranie
    while (waitpid(-1, NULL, WNOHANG) > 0) {
        zakonczone++;
    }
    
    wspolne = (StanStatku*)shmat(shmid, NULL, 0);
    if (wspolne != (void*)-1) {
        printf("RAPORT KONCOWY\n");
        printf("Wygenerowano %d pasazerow\n", wygenerowano);
        printf("Liczba rejsów: %d/%d\n", wspolne->liczba_rejsow, R);
        printf("Pasazerowie przewiezieni:\n");
        printf("  - z Wawelu: %d\n", wspolne->total_pasazerow_wawel);
        printf("  - z Tynca: %d\n", wspolne->total_pasazerow_tyniec);
        printf("  - lacznie: %d\n", 
               wspolne->total_pasazerow_wawel + wspolne->total_pasazerow_tyniec);
        printf("Pasazerowie odrzuceni: %d\n", wspolne->pasazerow_odrzuconych);
        printf("\n\n");
        
        logger_log(LOG_INFO, EVENT_SYSTEM_STOP,
                  "Raport: rejsy=%d, pasazerowie=%d, odrzuceni=%d",
                  wspolne->liczba_rejsow,
                  wspolne->total_pasazerow_wawel + wspolne->total_pasazerow_tyniec,
                  wspolne->pasazerow_odrzuconych);
        
        shmdt(wspolne);
    }
    
    printf("Usuwanie zasobow IPC...\n");
    
    if (semctl(semid, 0, IPC_RMID) == -1) {
        if (errno != EIDRM && errno != EINVAL) {
            perror("Blad usuwania semaforow");
        }
    }
    
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        if (errno != EIDRM && errno != EINVAL) {
            perror("Blad usuwania pamieci dzielonej");
        }
    }
    
    unlink("/tmp/tramwaj_config.txt");
    
    logger_log(LOG_INFO, EVENT_SYSTEM_STOP, "Zakonczenie systemu tramwaju wodnego");
    
    printf("Symulacja zakonczona. Log zapisano w tramwaj_wodny.log\n");
    
    logger_close_final();
    return 0;
}