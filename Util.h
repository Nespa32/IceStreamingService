
#include <sys/time.h>

#define nullptr NULL

#define LOG_ERROR(fmt, ...)                                 \
    do {                                                    \
        fprintf(stderr, "%s:%d:%s(): " fmt "\n",            \
            __FILE__, __LINE__, __func__, ##__VA_ARGS__);   \
    } while (0)

inline long getMSTime()
{
    timeval t;
    gettimeofday(&t, NULL);
    return t.tv_sec * 1e3 + t.tv_usec / 1e3;
}
