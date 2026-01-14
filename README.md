# Projekt Tramwaj Wodny - Temat 11

**Autor:** Paweł Drabik   
**Numer albumu:** 155171  
**Grupa:** 1  
---
## 1. Opis projektu
### 1.1 Treść zadania
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
### 1.2 Cel projektu
Symulacja systemu tramwaju wodnego kursującego na trasie Wawel - Tyniec. Program składa się z trzech głównych procesów:
- Kapitan - zarządza rejsami oraz kontroluje rozładunek i załadunek pasażerów
- Dyspozytor - wysyła sygnały do wypłynięcia przed czasem oraz do zakończenia pracy
- Pasażer - próbuje wejść na statek
### 1.3 Parametry
- N - pojemność tramwaju (liczba pasażerów)
- M - liczba miejsc na rowery (M < N)
- K - pojemność mostka (K < N)
- T1 - czas załadunku w sekundach
- T2 - czas rejsu w sekundach
- R - maksymalna liczba rejsów w ciągu dnia
### 1.4 Założenia projektu
- Tramwaj ma pojemność N
- Tramwaj może przewieźć M rowerów
- Na statek można wejść poprzez mostek o pojemności K (K<N)
- Osoba z rowerem zajmuje na mostku 2 miejsca
- Statek odpływa co T1 czasu lub po otrzymaniu sygnału od dyspozytora
- Podczas odpływania na mostku nie może być żadnego pasażera
- Kapitan musi dopilnować, aby liczba pasażerów nie przekroczyła N, a liczba rowerów M
- Rejs trwa T2 czasu
- Ruch na mostku odbywa sie w jedną strone
- Trawmaj może wykonać maksymalnie R rejsów lub przerwać ich wykonywanie w momencie otrzymania sygnału od dyspozytora
- Po dotarciu do celu wszycy pasażerowie muszą opuścić tramwaj
## Architektura systemu
### 2.1 Struktura procesów
System wykorzystuje jeden proces główny main oraz trzy procesy potomne kapitan dyspozytor oraz pasazer
### 2.2 Mechanizmy IPC

System bazuje na komunikacji międzyprocesowej:

#### Pamięć dzielona:
- Przechowuje stan statku
- Zawiera liczniki pasazerów, rowerów oraz rejsów
- Jest chroniona semaforem 'SEM_DOSTEP'

#### Semafory
- `SEM_MOSTEK` - limit mostka
- `SEM_STATEK_LUDZIE` - limit pasażerów
- `SEM_STATEK_ROWERY` - limit rowerów
- `SEM_DOSTEP` - mutex do pamięci dzielonej
- `SEM_LOGGER` - synchronizacja zapisu do pliku log

#### Sygnały
- `SIGUSR1` - dyspozytor → kapitan (wymuszony odpływ)
- `SIGUSR2` - dyspozytor → kapitan (zakończenie pracy)
- `SIGTERM` - main → wszystkie procesy (awaryjne zakończenie)
- `SIGINT` - użytkownik → main (Ctrl+C)

### 2.3 Struktura danych w pamięci dzielonej
```c
typedef struct {
    int pasazerowie_statek;      // Aktualna liczba pasażerów na statku
    int rowery_statek;            // Aktualna liczba rowerów na statku
    int pasazerowie_mostek;       // Aktualna liczba osób na mostku
    int czy_plynie;               // 0 - w porcie, 1 - w rejsie
    int liczba_rejsow;            // Licznik wykonanych rejsów
    int kierunek_mostka;          // Kierunek ruchu na mostku
    int aktualny_przystanek;      // PRZYSTANEK_WAWEL lub PRZYSTANEK_TYNIEC
    pid_t pid_kapitan;            // PID procesu kapitana (dla dyspozytora)
    int status_kapitana;          // 0 - rejs, 1 - w porcie
    int koniec_symulacji;         // Flaga zakończenia
    int total_pasazerow_wawel;    // Statystyki: pasażerowie z Wawelu
    int total_pasazerow_tyniec;   // Statystyki: pasażerowie z Tyńca
    int pasazerow_odrzuconych;    // Statystyki: odrzuceni pasażerowie
} StanStatku;
```
