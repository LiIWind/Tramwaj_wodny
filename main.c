#include "common.h"
#include "logger.h"

int N = 10, M = 3, K = 3, T1 = 2, T2 = 3, R = 5;

static volatile sig_atomic_t cleanup_flag = 0;
static int g_semid = -1;

void cleanup_handler(int sig) {
    (void)sig;
    cleanup_flag = 1;
}

//zbiera procesy zombie
void sigchld_handler(int sig) {
    (void)sig;
    int saved_errno = errno;
    
    while (waitpid(-1, NULL, WNOHANG) > 0) {
        
    }
    errno = saved_errno;
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
        shmctl(shmid, IPC_RMID, NULL);
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
        shmctl(shmid, IPC_RMID, NULL);
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
        semctl(semid, 0, IPC_RMID);
        shmctl(shmid, IPC_RMID, NULL);
        logger_close();
        exit(1);
    }
    
    if (pid_kapitan == 0) {
        signal(SIGCHLD, SIG_DFL);
        execl("./kapitan", "kapitan", NULL);
        perror("Blad execl kapitana");
        exit(1);
    }
    
    logger_log(LOG_INFO, EVENT_KAPITAN_START, "Uruchomiono kapitana (PID: %d)", pid_kapitan);
    
    //uruchom dyspozytora
    pid_t pid_dyspozytor = fork();
    if (pid_dyspozytor == -1) {
        perror("Blad fork dyspozytora");
        logger_error_errno(EVENT_BLAD_SYSTEM, "Blad tworzenia procesu dyspozytora");
        kill(pid_kapitan, SIGTERM);
        semctl(semid, 0, IPC_RMID);
        shmctl(shmid, IPC_RMID, NULL);
        logger_close();
        exit(1);
    }
    
    if (pid_dyspozytor == 0) {
        signal(SIGCHLD, SIG_DFL);
        execl("./dyspozytor", "dyspozytor", NULL);
        perror("Blad execl dyspozytora");
        exit(1);
    }
    
    logger_log(LOG_INFO, EVENT_DYSPOZYTOR_START, "Uruchomiono dyspozytora (PID: %d)", pid_dyspozytor);
    
    //generowanie pasazerow
    printf("Generowanie pasazerow...\n\n");
    
    int liczba_pasazerow = 10000;
    int wygenerowano = 0;
    
    wspolne = (StanStatku*)shmat(shmid, NULL, 0);
    if (wspolne == (void*)-1) {
        perror("Blad shmat");
        kill(pid_kapitan, SIGTERM);
        kill(pid_dyspozytor, SIGTERM);
        semctl(semid, 0, IPC_RMID);
        shmctl(shmid, IPC_RMID, NULL);
        logger_close();
        exit(1);
    }
    
    for (int i = 0; i < liczba_pasazerow && !cleanup_flag; i++) {
        //sprawdz czy symulacja sie skonczyla
        sem_wait(semid, SEM_MUTEX);
        int koniec = wspolne->koniec_symulacji;
        sem_signal(semid, SEM_MUTEX);
        
        if (koniec) {
            printf("Symulacja zakonczona - przerywam generowanie pasazerow\n");
            break;
        }
        
        pid_t pid = fork();
        if (pid == -1) {
            perror("Blad fork pasazera");
            continue;
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

    sigset_t block_chld, old_mask;
    sigemptyset(&block_chld);
    sigaddset(&block_chld, SIGCHLD);
    
    sigprocmask(SIG_BLOCK, &block_chld, &old_mask);
     
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
        
        // Czekaj na kapitana i dyspozytora z krotkim timeoutem
        int wait_count = 0;
        while (wait_count < 20) {
            int ret1 = waitpid(pid_kapitan, NULL, WNOHANG);
            int ret2 = waitpid(pid_dyspozytor, NULL, WNOHANG);
            if ((ret1 != 0 || ret1 == -1) && (ret2 != 0 || ret2 == -1)) {
                break;
            }
            wait_count++;
        }
        
        // Jesli nadal dzialaja, wyslij SIGKILL
        kill(pid_kapitan, SIGKILL);
        kill(pid_dyspozytor, SIGKILL);
        waitpid(pid_kapitan, NULL, 0);
        waitpid(pid_dyspozytor, NULL, 0);
        
    } else {
        // Normalne zakonczenie - czekaj na kapitana i dyspozytora
        int status;
        waitpid(pid_kapitan, &status, 0);
        logger_log(LOG_INFO, EVENT_KAPITAN_STOP, "Kapitan zakonczyl prace");
        
        waitpid(pid_dyspozytor, &status, 0);
        logger_log(LOG_INFO, EVENT_DYSPOZYTOR_STOP, "Dyspozytor zakonczyl prace");
    }
    
    sigprocmask(SIG_SETMASK, &old_mask, NULL);
    
    // Wyslij SIGTERM do pozostalych pasazerow (jesli jeszcze nie wyslano)
    if (!cleanup_flag) {
        printf("Wysyłam SIGTERM do pasazerow...\n");
        signal(SIGTERM, SIG_IGN);
        kill(0, SIGTERM);
    }

    printf("Czekam na zakończenie pasazerow...\n");
    
    int zakonczone = 0;
    time_t start_wait = time(NULL);
    
    while (time(NULL) - start_wait < 5) {
        pid_t child;
        while ((child = waitpid(-1, NULL, WNOHANG)) > 0) {
            zakonczone++;
        }
        if (child == -1 && errno == ECHILD) {
            break;
        }
    }

    while (waitpid(-1, NULL, WNOHANG) > 0) {
    }
    
    wspolne = (StanStatku*)shmat(shmid, NULL, 0);
    if (wspolne != (void*)-1) {
        printf("RAPORT KONCOWY\n");
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