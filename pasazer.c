#include "common.h"
#include "logger.h"

int N = 10, M = 3, K = 3, T1 = 2, T2 = 3, R = 5;

static volatile sig_atomic_t wypchniety = 0;
static volatile sig_atomic_t koniec = 0;

void sigusr1_handler(int sig) {
    (void)sig;
    wypchniety = 1;
}

void sigterm_handler(int sig) {
    (void)sig;
    koniec = 1;
}


//Sprawdzenie czy pasazer nie zostal wypechniety
static int czy_wypchniety(StanStatku *wspolne, pid_t pid) {
    for (int i = 0; i < wspolne->liczba_wypchnietych; i++) {
        if (wspolne->wypchnieci[i] == pid) {
            return 1;
        }
    }
    return 0;
}

//Dodanie pasazera do kolejki na mostku
static int dodaj_do_kolejki(StanStatku *wspolne, pid_t pid, int ma_rower) {
    if (wspolne->liczba_na_mostku >= MAX_PASAZEROW_MOSTEK) {
        return -1;
    }
    
    int idx = wspolne->liczba_na_mostku;
    wspolne->kolejka_mostek[idx].pid = pid;
    wspolne->kolejka_mostek[idx].ma_rower = ma_rower;
    wspolne->kolejka_mostek[idx].rozmiar = ma_rower ? 2 : 1;
    wspolne->liczba_na_mostku++;
    
    return idx;
}

//Usuwanie pasazera z kolejki na mostku
static void usun_z_kolejki(StanStatku *wspolne, pid_t pid) {
    for (int i = 0; i < wspolne->liczba_na_mostku; i++) {
        if (wspolne->kolejka_mostek[i].pid == pid) {
            for (int j = i; j < wspolne->liczba_na_mostku - 1; j++) {
                wspolne->kolejka_mostek[j] = wspolne->kolejka_mostek[j + 1];
            }
            wspolne->liczba_na_mostku--;
            return;
        }
    }
}

int main() {
    pid_t moj_pid = getpid();
    
    srand(time(NULL) ^ moj_pid);
    int ma_rower = (rand() % 100) < 30 ? 1 : 0;  //30% szans na rower
    int rozmiar = ma_rower ? 2 : 1;
    int moj_przystanek = (rand() % 2) ? PRZYSTANEK_WAWEL : PRZYSTANEK_TYNIEC; //50% szans na przystanek
    
    if (wczytaj_parametry_z_pliku() == -1) {
        fprintf(stderr, "Pasazer - blad wczytywania parametrow\n");
        exit(1);
    }
    
    if (logger_init("tramwaj_wodny.log", 0) == -1) {
        exit(1);
    }
    
    struct sigaction sa_usr1, sa_term;
    
    sa_usr1.sa_handler = sigusr1_handler;
    sigemptyset(&sa_usr1.sa_mask);
    sa_usr1.sa_flags = 0;
    sigaction(SIGUSR1, &sa_usr1, NULL);
    
    sa_term.sa_handler = sigterm_handler;
    sigemptyset(&sa_term.sa_mask);
    sa_term.sa_flags = 0;
    sigaction(SIGTERM, &sa_term, NULL);
    
    key_t key = ftok(PATH_NAME, PROJECT_ID);
    if (key == -1) {
        perror("Pasazer - blad ftok");
        logger_close();
        exit(1);
    }
    
    int shmid = shmget(key, sizeof(StanStatku), 0);
    if (shmid == -1) {
        perror("Pasazer - blad shmget");
        logger_close();
        exit(1);
    }
    
    StanStatku *wspolne = (StanStatku*)shmat(shmid, NULL, 0);
    if (wspolne == (void*)-1) {
        perror("Pasazer - blad shmat");
        logger_close();
        exit(1);
    }
    
    int semid = semget(key, LICZBA_SEM, 0);
    if (semid == -1) {
        perror("Pasazer - blad semget");
        shmdt(wspolne);
        logger_close();
        exit(1);
    }
    
    logger_set_semid(semid);
    
    logger_pasazer_event(EVENT_PASAZER_PRZYBYCIE, moj_pid, ma_rower,
                        moj_przystanek == PRZYSTANEK_WAWEL ? "Wawel" : "Tyniec");
    printf("[PASAZER %d] Przybylem na przystanek %s %s\n", moj_pid,
           get_przystanek_nazwa(moj_przystanek),
           ma_rower ? "[ROWER]" : "");
    fflush(stdout);
    
    int ostatni_rejs_proba = -1;  //ID rejsu w ktorym ostatnio probowal wejsc
    
    //Glowna petla proba wejscia na statek
    while (!koniec) {
        //Sprawdz czy symulacja sie nie skonczyla
        sem_wait(semid, SEM_MUTEX);
        if (wspolne->koniec_symulacji) {
            sem_signal(semid, SEM_MUTEX);
            logger_pasazer_event(EVENT_PASAZER_OPUSZCZENIE, moj_pid, ma_rower,
                                "symulacja zakończona");
            break;
        }
        int aktualny_rejs = wspolne->rejs_id;
        sem_signal(semid, SEM_MUTEX);
        
        //Czekanie na otwarcie zaladunku na przystanku pasazera
        int sem_zaladunek = (moj_przystanek == PRZYSTANEK_WAWEL)
                          ? SEM_ZALADUNEK_WAWEL
                          : SEM_ZALADUNEK_TYNIEC;
        
        if (sem_wait(semid, sem_zaladunek) == -1) {
            if (errno == EIDRM || errno == EINVAL) {
                break;
            }
            continue;
        }
        
        //Sprawdz stan czy zaladunek nadal otwarty i czy to przystanek pasazera
        sem_wait(semid, SEM_MUTEX);
        
        if (wspolne->koniec_symulacji) {
            sem_signal(semid, SEM_MUTEX);
            break;
        }
        
        aktualny_rejs = wspolne->rejs_id;
        
        //Jesli probowal juz w tym rejsie i nie udalo sie czekaj na nastepny
        if (aktualny_rejs == ostatni_rejs_proba) {
            sem_signal(semid, SEM_MUTEX);
            continue;
        }
        
        if (!wspolne->zaladunek_otwarty || 
            wspolne->aktualny_przystanek != moj_przystanek) {
            sem_signal(semid, SEM_MUTEX);
            continue;
        }
        
        sem_signal(semid, SEM_MUTEX);
        
        //Rezerwuj miejsce na statku
        if (sem_wait(semid, SEM_STATEK_LUDZIE) == -1) {
            if (errno == EIDRM || errno == EINVAL) break;
            continue;
        }
        
        //Sprawdz czy zaladunek nadal otwarty po uzyskaniu miejsca
        sem_wait(semid, SEM_MUTEX);
        if (!wspolne->zaladunek_otwarty || wspolne->koniec_symulacji ||
            wspolne->aktualny_przystanek != moj_przystanek) {
            sem_signal(semid, SEM_MUTEX);
            sem_signal(semid, SEM_STATEK_LUDZIE);
            //Zaladunek sie skonczyl czekaj na nastepny
            continue;
        }
        sem_signal(semid, SEM_MUTEX);
        
        //Jesli ma rower, rezerwuj miejsce na rower
        if (ma_rower) {
            if (sem_trywait(semid, SEM_STATEK_ROWERY) == -1) {
                //Brak miejsca na rower zwolnij miejsce na ludzi i czekaj
                sem_signal(semid, SEM_STATEK_LUDZIE);
                
                sem_wait(semid, SEM_MUTEX);
                wspolne->pasazerow_odrzuconych++;
                ostatni_rejs_proba = wspolne->rejs_id;
                sem_signal(semid, SEM_MUTEX);
                
                printf("[PASAZER %d] Brak miejsca na rower - czekam na nastepny rejs\n", moj_pid);
                fflush(stdout);
                //Musze poczekac na nastepny cykl zaladunku
                continue;
            }
        }
        
        //Rezerwuj miejsce na mostku
        if (sem_wait_n(semid, SEM_MOSTEK, rozmiar) == -1) {
            sem_signal(semid, SEM_STATEK_LUDZIE);
            if (ma_rower) sem_signal(semid, SEM_STATEK_ROWERY);
            continue;
        }
        
        //Sprawdz czy zaladunek nadal otwarty
        sem_wait(semid, SEM_MUTEX);
        
        if (!wspolne->zaladunek_otwarty || wspolne->koniec_symulacji ||
            wspolne->wypychanie_aktywne) {
            sem_signal(semid, SEM_MUTEX);
            
            sem_signal_n(semid, SEM_MOSTEK, rozmiar);
            sem_signal(semid, SEM_STATEK_LUDZIE);
            if (ma_rower) sem_signal(semid, SEM_STATEK_ROWERY);
            
            printf("[PASAZER %d] Za pozno - zaladunek zamkniety\n", moj_pid);
            fflush(stdout);
            continue;
        }
        
        //Dodanie do kolejki na mostku
        int pozycja = dodaj_do_kolejki(wspolne, moj_pid, ma_rower);
        if (pozycja == -1) {
            sem_signal(semid, SEM_MUTEX);
            sem_signal_n(semid, SEM_MOSTEK, rozmiar);
            sem_signal(semid, SEM_STATEK_LUDZIE);
            if (ma_rower) sem_signal(semid, SEM_STATEK_ROWERY);
            continue;
        }
        
        wspolne->pasazerow_czekajacych_na_wejscie++;
        
        sem_signal(semid, SEM_MUTEX);
        
        logger_pasazer_event(EVENT_PASAZER_WEJSCIE_MOSTEK, moj_pid, ma_rower,
                            "na mostku");
        printf("[PASAZER %d] Wszedlem na mostek (pozycja: %d)%s\n",
               moj_pid, pozycja + 1, ma_rower ? " [ROWER - 2 miejsca]" : "");
        fflush(stdout);
        
        wypchniety = 0;
        
        //Czekanie na pozwolenie wejscia na statek
        if (sem_wait(semid, SEM_WEJSCIE) == -1) {
            sem_wait(semid, SEM_MUTEX);
            usun_z_kolejki(wspolne, moj_pid);
            wspolne->pasazerow_czekajacych_na_wejscie--;
            sem_signal(semid, SEM_MUTEX);
            
            sem_signal_n(semid, SEM_MOSTEK, rozmiar);
            sem_signal(semid, SEM_STATEK_LUDZIE);
            if (ma_rower) sem_signal(semid, SEM_STATEK_ROWERY);
            continue;
        }
        
        //Sprawdz czy nie zostal wypchnienty
        sem_wait(semid, SEM_MUTEX);
        
        int zostal_wypchniety = wypchniety || czy_wypchniety(wspolne, moj_pid);
        
        if (zostal_wypchniety) {
            //Wypchnienty kapitan juz zwolnil zasoby
            wspolne->pasazerow_czekajacych_na_wejscie--;
            sem_signal(semid, SEM_MUTEX);
            
            logger_pasazer_event(EVENT_PASAZER_WYPCHNIETY, moj_pid, ma_rower,
                                "wypchnienty - czekam na nastepny rejs");
            printf("[PASAZER %d] Zostalem wypchnienty - czekam na nastepny rejs\n", moj_pid);
            fflush(stdout);
            
            wypchniety = 0;
            continue;
        }
        
        //Wejscie na statek
        usun_z_kolejki(wspolne, moj_pid);
        wspolne->pasazerow_czekajacych_na_wejscie--;
        wspolne->pasazerowie_na_statku++;
        if (ma_rower) wspolne->rowery_na_statku++;
        
        int na_statku = wspolne->pasazerowie_na_statku;
        int rowerow = wspolne->rowery_na_statku;
        
        sem_signal(semid, SEM_MUTEX);
        
        //Zwolnij miejsce na mostku
        sem_signal_n(semid, SEM_MOSTEK, rozmiar);
        
        logger_pasazer_event(EVENT_PASAZER_WEJSCIE_STATEK, moj_pid, ma_rower,
                            "na statku");
        printf("[PASAZER %d] Wszedlem na STATEK (pasazerow: %d, rowerow: %d)\n",
               moj_pid, na_statku, rowerow);
        fflush(stdout);
        
        //Czekanie na rozladunek
        if (sem_wait(semid, SEM_ROZLADUNEK) == -1) {
            sem_wait(semid, SEM_MUTEX);
            wspolne->pasazerowie_na_statku--;
            if (ma_rower) wspolne->rowery_na_statku--;
            sem_signal(semid, SEM_MUTEX);
            
            sem_signal(semid, SEM_STATEK_LUDZIE);
            if (ma_rower) sem_signal(semid, SEM_STATEK_ROWERY);
            break;
        }
        
        //Schodzenie ze statku
        logger_pasazer_event(EVENT_PASAZER_WYJSCIE_STATEK, moj_pid, ma_rower,
                            "schodzi ze statku");
        printf("[PASAZER %d] Schodze ze STATKU\n", moj_pid);
        fflush(stdout);
        
        sem_wait_n(semid, SEM_MOSTEK, rozmiar);
        
        sem_wait(semid, SEM_MUTEX);
        
        wspolne->pasazerowie_na_statku--;
        if (ma_rower) wspolne->rowery_na_statku--;
        wspolne->pasazerow_do_rozladunku--;
        
        int ostatni = (wspolne->pasazerow_do_rozladunku == 0);
        int nowy_przystanek = wspolne->aktualny_przystanek;
        
        sem_signal(semid, SEM_MUTEX);
        
        sem_signal_n(semid, SEM_MOSTEK, rozmiar);
        sem_signal(semid, SEM_STATEK_LUDZIE);
        if (ma_rower) sem_signal(semid, SEM_STATEK_ROWERY);
        
        //Jesli ostatni powiadom kapitana
        if (ostatni) {
            sem_signal(semid, SEM_ROZLADUNEK_KONIEC);
        }
        
        logger_pasazer_event(EVENT_PASAZER_OPUSZCZENIE, moj_pid, ma_rower,
                            get_przystanek_nazwa(nowy_przystanek));
        printf("[PASAZER %d] Opuscilem statek na przystanku %s - KONIEC\n",
               moj_pid, get_przystanek_nazwa(nowy_przystanek));
        fflush(stdout);
        
        //Pasazer konczy po jednym rejsie
        break;
    }
    
    shmdt(wspolne);
    logger_close();
    return 0;
}
