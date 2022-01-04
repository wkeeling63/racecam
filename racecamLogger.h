#ifndef RACECAMLGGER_H_
#define RACECAMLOGGER_H_

void log_error(char* format, ...);
void log_warning(char* format, ...);
void log_status(char* format, ...);
void log_debug(char* format, ...);

/*
 * Default level is LOG_MAX_LEVEL_ERROR_WARNING_STATUS
 */ 

#define LOG_MAX_LEVEL_ERROR 0
#define LOG_MAX_LEVEL_ERROR_WARNING_STATUS 1
#define LOG_MAX_LEVEL_ERROR_WARNING_STATUS_DEBUG 2

void logger_set_log_level(const int level);

/*
 * Default dest is syslog
 */
void logger_reset_state(void);
int logger_set_log_file(const char* filename);
void logger_set_out_stdout();

#endif /* RACECAMLOGGER_H_ */
