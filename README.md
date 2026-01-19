# Projekt Systemy Operacyjne - Tramwaj Wodny

**Autor** Paweł Drabik  
**Numer albumu** 155171  

---

## 1. Opis projektu

W sezonie letnim po Wiśle na trasie Kraków Wawel – Tyniec kursuje tramwaj wodny o pojemności N
pasażerów. Dodatkowo tramwajem można przewieźć M rowerów (M<N). Statek z lądem jest
połączony mostkiem o pojemności K (K<N). Na statek próbują dostać się pasażerowie, z tym, że na
statek nie może ich wejść więcej niż N, a wchodząc na statek na mostku nie może być ich
równocześnie więcej niż K. Jeżeli na statek wchodzi osoba z rowerem na mostku zajmuje 2 miejsca.
Statek co określoną ilość czasu T1 (np.: jedną godzinę) wypływa w rejs. W momencie odpływania
kapitan statku musi dopilnować, aby na mostku nie było żadnego wchodzącego pasażera
(pasażerowie, którzy nie weszli na statek muszą zejść z mostka począwszy od ostatniego w kolejce).
Jednocześnie musi dopilnować by liczba pasażerów na statku nie przekroczyła N, a liczba rowerów
M. Dodatkowo statek może odpłynąć przed czasem T1 w momencie otrzymania polecenia (sygnał1)
od dyspozytora. Rejs trwa określoną ilość czasu równą T2. Po dotarciu do Tyńca pasażerowie
opuszczają statek. Po opuszczeniu statku przez ostatniego pasażera, kolejni (inni) pasażerowie
próbują dostać się na pokład w rejs powrotny (mostek jest na tyle wąski, że w danym momencie ruch
może odbywać się tylko w jedną stronę). Statek może wykonać maksymalnie R rejsów w danym dniu
lub przerwać ich wykonywanie po otrzymaniu polecenia (sygnał2) od dyspozytora (jeżeli to polecenie
nastąpi podczas załadunku, statek nie wypływa w rejs, a pasażerowie opuszczają statek. Jeżeli
polecenie dotrze do kapitana w trakcie rejsu statek kończy bieżący rejs normalnie – dopływa do
przystanku w Tyńcu lub w Krakowie.

## 2 Założenia projektowe

### 2.1 Założenia zadania
- Pasażerowie przybywają na przystanki Wawel lub Tyniec losowo (50% szans)
- Pasażer może mieć rower (30& szans)
- Na tramwaj może wejść maksymalnie N pasażerów
- Na mostku może znajnować się jednocześnie M pasazerow
- Pasażer z rowerem zajmuje 2 miejsca na mostku
- Kapitan musi dopilnować, aby na mostku nie było żadnego pasażera w momencie odpływania
- Pasażerowie obecni na mostku podczas odpływania muszą go opuścić zaczynając od ostatniego w kolejce
- Tramwaj odpływa co T1 czasu, lub po otrzymaniu sygnału od dyspozytora
- Rejs trwa T2 czasu
- Tramwaj może wykonać maksymalnie R rejsów w ciągu dnia lub przerwać ich wykonywanie po otrzymaniu sygnału od dyspozytora
- Tramwaj kursuje na trasie Kraków Wawel - Tyniec
- Mostek jest jednokierunkowy

### 2.2 Architektura systemu
System oparty jest na architekturze wieloprocesowej z następującymi procesami:
-Main - Proces główny - tworzy zasoby IPC oraz uruchamia pozostałe procesy
-Kapitan - Zarządza statkiem, załadunkiem oraz rozładunkiem pasażerów
-Dyspozytor - Monitoruje symulację oraz wysyła sygnały do kapitana
-Pasażer - Próbuje wejść na statek

### 2.3 Parametry systemu
Następujące parametry są wprowadzane przez użytkownika:
- **N** - Pojemność tramwaju (maksymalna liczba pasażerów)
- **M** - Liczba miejsc na rowery (M < N)
- **K** - Pojemność mostka (K < N)
- **T1** - Czas załadunku w sekundach
- **T2** - Czas rejsu w sekundach
- **R** - Maksymalna liczba rejsów

### 2.4 Walidacja danych
Wszystkie wprowadzane parametry są walidowane:
- Wartości muszą być większe od 0
- M musi być mniejsze od N
- K musi być mniejsze od N
- Nieprawidłowe dane powodują wyświetlenie komunikatu błędu i ponowne zapytanie

### 2.5 Pliki projektu

| Plik | Opis |
|------|------|
| common.h | Definicje struktur, stałych, funkcji pomocniczych |
| logger.h | Nagłówek modułu logowania |
| logger.c | Implementacja systemu logowania |
| main.c | Proces główny - inicjalizacja i zarządzanie |
| kapitan.c | Logika kapitana statku |
| dyspozytor.c | Logika dyspozytora (wysyłanie sygnałów) |
| pasazer.c | Logika pojedynczego pasażera |

---

## 3. Opis implementacji

### 3.1 Mechanizmy IPC

#### 3.1.1 Pamięć dzielona

Pamięć dzielona służy do przechowywania wspólnych stanów systemu

```c
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
```

#### 3.1.2 Semafory

System wykorzystuje 12 semaforów do synchronizacji:

| Semafor | Init | Funkcja |
|---------|------|---------|
| SEM_MUTEX | 1 | Wzajemne wykluczanie przy dostępie do pamięci dzielonej |
| SEM_MOSTEK | K | Licznik miejsc na mostku |
| SEM_STATEK_LUDZIE | N | Licznik miejsc na statku |
| SEM_STATEK_ROWERY | M | Licznik miejsc na rowery |
| SEM_LOGGER | 1 | Synchronizacja dostępu do pliku log |
| SEM_ZALADUNEK_WAWEL | 0 | Sygnalizacja otwarcia załadunku na Wawelu |
| SEM_ZALADUNEK_TYNIEC | 0 | Sygnalizacja otwarcia załadunku w Tyńcu |
| SEM_WEJSCIE | 0 | Kapitan wpuszcza pasażerów na statek |
| SEM_ROZLADUNEK | 0 | Sygnalizacja rozpoczęcia rozładunku |
| SEM_ROZLADUNEK_KONIEC | 0 | Ostatni pasażer sygnalizuje zejście |
| SEM_DYSPOZYTOR_READY | 0 | Dyspozytor czeka na gotowość kapitana |
| SEM_DYSPOZYTOR_EVENT | 0 | Zdarzenia dla dyspozytora |

#### 3.1.3 Sygnały

System obsługuje sygnały:

| Sygnał | Nadawca | Odbiorca | Działanie |
|--------|---------|----------|-----------|
| SIGUSR1 | Dyspozytor | Kapitan | Wcześniejszy odjazd przed czasem T1 |
| SIGUSR2 | Dyspozytor | Kapitan | Zakończenie pracy przed wykonaniem R rejsów |
| SIGUSR1 | Kapitan | Pasażer | Wypychanie pasażera z mostka |
| SIGTERM | Main | Wszystkie | Zakończenie symulacji |
| SIGINT | System | Main | Przerwanie przez użytkownika |

### 3.2 Algorytmy

#### 3.2.1 Algorytm załadunku pasażerów

```
1. Kapitan otwiera załadunek (ustawia flage)
2. Sygnalizuje otwarcie załadunku semaforem N razy
3. Przez T1 czasu:
  a. Sprawdza czy otrzymał sygnał do wcześniejszego odjazdu SIGUSR1
  b. Wpuszcza pasażerów z mostka na statek
  c. Sygnalizuje dyspozytorowi zdarzenie
4. Zamyka załadunek (czyści semafory)
5. Wypycha pasażerów pozostałych na mostku od ostatniego
6. Rozpoczyna rejs
```

#### 3.2.2 Algorytm wchodzenia pasażera na statek

```
1. Pasażer czeka na semafor załadunku odpowiedni dla jego przystanku
2. Rezerwuje miejsce na statku
3. Jeżeli ma rower to rezerwuje miejsce na rower
4. Rezerwuje miejsce na mostku (1 lub 2 dla rowerzysty)
5. Dodaje się do kolejki na mostku
6. Czeka na semafor wejścia
7. Sprawdza czy nie został wypchnięty
8. Wchodzi na statek i zwalnia miejsce na mostku
9. Czeka na rozładunek
10. Schodzi ze statku
```

#### 3.2.3 Algorytm wypychania pasażerów z mostka

```
1. Kapitan pobiera liczbę pasażerów na mostku
2. Od ostatniego:
  a. Dodaje PID do listy wypchniętych
  b. Wysyła SIGUSR1 do pasażera
  c. Zwalnia miejsce na mostku
  d. Zwalnia miejsce zarezerwowane na statku
  e. Budzi pasażera
3. Zeruje kolejke na mostku
```

---

## 4. Opis implementacji

### 4.1 Proces główny main.c

1. Wczytuje parametry od użytkownika z walidacją
2. Tworzy pamięć dzieloną i semafory
3. Uruchamia proces kapitana (fork() i exec())
4. Uruchamia proces dyspozytora (fork() i exec())
5. Generuje procesy pasażerów (10000 procesów)
6. Czeka na zakończenie kapitana i dyspozytora
7. Generuje raport końcowy
8. Czyści zasoby

### 4.2 Proces kapitana

Główna pętla kapitana:

1. Otwórz załadunek
2. Przez T1 czasu wpuszczaj pasażerów
3. Zamknij załadunek
4. Wypchnij pasażerów z mostka
5. Rozpocznij rejs i płyń przez T2 czasu
6. Dotrzyj do celu
7. Rozładuj pasażerów
8. Powtórz

### 4.3 Proces dyspozytora

1. Monitoruj przebieg symulacji
2. Wyślij sygnał do wcześniejszego odjazdu po 2 rejsach
3. Wyślij sygnal do końca pracy po 75% rejsów

### 4.4 Proces pasażera

1. Przybądz na losowy przystanek z rowerem lub bez
2. Czekaj na otwarcie załadunku
3. Rezerwuj miejsce na statku
4. Rezerwuj miejsce na mostku
5. Wejdź na mostek
6. Czekaj na pozwolenie wejścia na statek
7. Wejdź na statek
8. Zwolnij miejsce na mostku
9. Czekaj na rozładunek
10. Zejdź ze statku
11. Zwolnij miejsce na statku
12. Koniec

---

## 5. Testy

### 5.1 **Test nr. 1 ***
