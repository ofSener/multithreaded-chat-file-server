#include "../common.h"
#include <stdarg.h>

// Global log file mutex
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

char* get_timestamp(void) {
    static char timestamp[64];
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    return timestamp;
}

void log_message(const char* format, ...) {
    pthread_mutex_lock(&log_mutex);
    
    FILE* log_file = fopen("server_log.txt", "a");
    if (log_file) {
        fprintf(log_file, "%s - ", get_timestamp());
        
        va_list args;
        va_start(args, format);
        vfprintf(log_file, format, args);
        va_end(args);
        
        fprintf(log_file, "\n");
        fclose(log_file);
    }
    
    // Also print to console
    printf("%s - ", get_timestamp());
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
    
    pthread_mutex_unlock(&log_mutex);
} 