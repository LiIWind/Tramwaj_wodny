#include "logger.h"
#include "common.h"

static FILE *log_file = NULL;
static int logger_semid = -1;

static void lock_log(void) {
    if (logger_semid != -1) {
        sem_wait(logger_semid, SEM_LOGGER);
    }
}

static void unlock_log(void) {
    if (logger_semid != -1) {
        sem_signal(logger_semid, SEM_LOGGER);
    }
}

void logger_set_semid(int semid) {
    logger_semid = semid;
}

const char* get_log_level_name(LogLevel level) {
    switch(level) {
        case LOG_DEBUG: return "DEBUG";
        case LOG_INFO: return "INFO";
        case LOG_WARNING: return "WARNING";
        case LOG_ERROR: return "ERROR";
        case LOG_CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

const char* get_event_type_name(EventType event) {
    switch(event) {
        case EVENT_SYSTEM_START: return "SYSTEM_START";
        case EVENT_SYSTEM_STOP: return "SYSTEM_STOP";
        case EVENT_KAPITAN_START: return "KAPITAN_START";
        case EVENT_KAPITAN_STOP: return "KAPITAN_STOP";
        case EVENT_DYSPOZYTOR_START: return "DYSPOZYTOR_START";
        case EVENT_DYSPOZYTOR_STOP: return "DYSPOZYTOR_STOP";
        case EVENT_PASAZER_PRZYBYCIE: return "PASAZER_PRZYBYCIE";
        case EVENT_PASAZER_WEJSCIE_MOSTEK: return "PASAZER_WEJSCIE_MOSTEK";
        case EVENT_PASAZER_WEJSCIE_STATEK: return "PASAZER_WEJSCIE_STATEK";
        case EVENT_PASAZER_WYJSCIE_STATEK: return "PASAZER_WYJSCIE_STATEK";
        case EVENT_PASAZER_OPUSZCZENIE: return "PASAZER_OPUSZCZENIE";
        case EVENT_PASAZER_WYPCHNIETY: return "PASAZER_WYPCHNIETY";
        case EVENT_REJS_START: return "REJS_START";
        case EVENT_REJS_KONIEC: return "REJS_KONIEC";
        case EVENT_ZALADUNEK_START: return "ZALADUNEK_START";
        case EVENT_ZALADUNEK_KONIEC: return "ZALADUNEK_KONIEC";
        case EVENT_SYGNAL_ODPLYNIECIE: return "SYGNAL_ODPLYNIECIE";
        case EVENT_SYGNAL_KONIEC_PRACY: return "SYGNAL_KONIEC_PRACY";
        case EVENT_BLAD_SYSTEM: return "BLAD_SYSTEM";
        default: return "UNKNOWN_EVENT";
    }
}

int logger_init(const char *filename, int write_header) {
    log_file = fopen(filename, "a");
    if (log_file == NULL) {
        perror("Blad otwarcia pliku log");
        return -1;
    }
    setvbuf(log_file, NULL, _IONBF, 0);

    if (write_header) {
        time_t now = time(NULL);
        fprintf(log_file, "\nSystem Tramwaj Wodny - Log symulacji\n");
        fprintf(log_file, "Data rozpoczęcia: %s", ctime(&now));
        fprintf(log_file, "\n\n");
    }
    return 0;
}

void logger_close(void) {
    if (log_file != NULL) {
        time_t now = time(NULL);
        lock_log();
        fprintf(log_file, "\nKoniec logowania: %s\n", ctime(&now));
        fclose(log_file);
        log_file = NULL;
        unlock_log();
    }
}

void logger_log(LogLevel level, EventType event, const char *format, ...) {
    if (log_file == NULL) return;

    lock_log();

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_buffer[64];
    strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", tm_info);

    fprintf(log_file, "[%s] [%s] [%s] [PID:%d] ",
            time_buffer, get_log_level_name(level),
            get_event_type_name(event), getpid());

    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);

    fprintf(log_file, "\n");

    unlock_log();
}

void logger_error_errno(EventType event, const char *msg) {
    if (log_file == NULL) return;
    int saved_errno = errno;
    logger_log(LOG_ERROR, event, "%s: %s (errno=%d)", msg, strerror(saved_errno), saved_errno);
}

void logger_pasazer_event(EventType event, pid_t pid, int ma_rower, const char *info) {
    if (log_file == NULL) return;

    lock_log();

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_buffer[64];
    strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", tm_info);

    fprintf(log_file, "[%s] [INFO] [%s] [PID:%d] Pasazer %s%s%s\n",
            time_buffer,
            get_event_type_name(event),
            pid,
            ma_rower ? "[ROWER] " : "",
            info ? "- " : "",
            info ? info : "");

    unlock_log();
}

void logger_rejs_event(EventType event, int numer, int pasazerow, int rowerow, const char *przystanek) {
    if (log_file == NULL) return;

    lock_log();

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_buffer[64];
    strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", tm_info);

    fprintf(log_file, "[%s] [INFO] [%s] [PID:%d] Rejs #%d: %d pasazerow, %d rowerow - %s\n",
            time_buffer,
            get_event_type_name(event),
            getpid(),
            numer,
            pasazerow,
            rowerow,
            przystanek ? przystanek : "");

    unlock_log();
}

void logger_sygnal(const char *sygnal, pid_t od, pid_t do_kogo) {
    if (log_file == NULL) return;

    lock_log();

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_buffer[64];
    strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", tm_info);

    fprintf(log_file, "[%s] [INFO] [SYGNAL] Wyslano %s od PID:%d do PID:%d\n",
            time_buffer, sygnal, od, do_kogo);

    unlock_log();
}
