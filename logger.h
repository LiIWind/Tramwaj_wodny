#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>

typedef enum {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR,
    LOG_CRITICAL
} LogLevel;

typedef enum {
    EVENT_SYSTEM_START,
    EVENT_SYSTEM_STOP,
    EVENT_KAPITAN_START,
    EVENT_KAPITAN_STOP,
    EVENT_DYSPOZYTOR_START,
    EVENT_DYSPOZYTOR_STOP,
    EVENT_PASAZER_PRZYBYCIE,
    EVENT_PASAZER_WEJSCIE_MOSTEK,
    EVENT_PASAZER_WEJSCIE_STATEK,
    EVENT_PASAZER_WYJSCIE_STATEK,
    EVENT_PASAZER_OPUSZCZENIE,
    EVENT_PASAZER_WYPCHNIETY,
    EVENT_REJS_START,
    EVENT_REJS_KONIEC,
    EVENT_ZALADUNEK_START,
    EVENT_ZALADUNEK_KONIEC,
    EVENT_SYGNAL_ODPLYNIECIE,
    EVENT_SYGNAL_KONIEC_PRACY,
    EVENT_BLAD_SYSTEM
} EventType;

int logger_init(const char *filename, int write_header);
void logger_set_semid(int semid);
void logger_close(void);

void logger_log(LogLevel level, EventType event, const char *format, ...);
void logger_error_errno(EventType event, const char *msg);
void logger_pasazer_event(EventType event, pid_t pid, int ma_rower, const char *info);
void logger_rejs_event(EventType event, int numer, int pasazerow, int rowerow, const char *przystanek);
void logger_sygnal(const char *sygnal, pid_t od, pid_t do_kogo);

const char* get_log_level_name(LogLevel level);
const char* get_event_type_name(EventType event);

#endif 
