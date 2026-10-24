#ifndef __RK_WIFI_SLOG__
#define __RK_WIFI_SLOG__

#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif


static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

static void thread_safe_print(const char *file, const char *func, int line, bool is_err, const char *fmt, ...)
{
    va_list args;
    va_list args_copy;
    char *buffer = NULL;
    int needed = 0;

    // Calculate the needed buffer size
    va_start(args, fmt);
    va_copy(args_copy, args);
    needed = vsnprintf(NULL, 0, fmt, args) + 200;  // +200 for prefix
    va_end(args);

    // Allocate the buffer
    buffer = malloc(needed + 1);    // +1 for null terminator
    if (!buffer) {
        pthread_mutex_lock(&log_mutex);
        fputs("ERROR: Log buffer allocation failed\n", stderr);
        pthread_mutex_unlock(&log_mutex);
        return;
    }

    // Format the log message
    int prefix_len = snprintf(buffer, needed, "[%s:%d] %s(): %s",
                              file, line, func, is_err ? "ERROR - " : "");
    
    // Print the log message
    vsnprintf(buffer + prefix_len, needed - prefix_len, fmt, args_copy);
    va_end(args_copy);

    // Ensure the log ends with a newline
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len-1] != '\n') {
        if (len < (size_t)needed - 1) {
            buffer[len] = '\n';
            buffer[len+1] = '\0';
        } else {
            buffer[needed-1] = '\n';
            buffer[needed] = '\0';
        }
    }

    pthread_mutex_lock(&log_mutex);
    fputs(buffer, stdout);
    pthread_mutex_unlock(&log_mutex);
    
    free(buffer);
}

#define pr_info(fmt, args...) \
    do { \
        thread_safe_print(__FILE__, __FUNCTION__, __LINE__, false, fmt, ##args); \
    } while (0)

#define pr_err(fmt, args...) \
    do { \
        thread_safe_print(__FILE__, __FUNCTION__, __LINE__, true, fmt, ##args); \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif  // __RK_WIFI_SLOG__
