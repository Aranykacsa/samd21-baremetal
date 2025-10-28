// env/syscalls_min.c
#include <sys/stat.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/unistd.h>
#include <stddef.h>
#include "uart.h"

int _getpid(void) {return 1;}
int _kill(int pid, int sig) {(void)pid;(void)sig;errno = EINVAL;return -1;}
int _close(int fd)              { (void)fd; errno = EBADF; return -1; }
int _fstat(int fd, struct stat *st){ (void)fd; st->st_mode = S_IFCHR; return 0; }
int _isatty(int fd)             { (void)fd; return 1; }
int _lseek(int fd, int ptr, int dir){ (void)fd;(void)ptr;(void)dir; return 0; }
int _read(int fd, void *buf, size_t cnt){ (void)fd;(void)buf;(void)cnt; errno = EINVAL; return -1; }

#define DEBUG 1
#if DEBUG == 1
static inline int sh_write(const char *buf, size_t len) {
    struct {
        int fd;
        const char *buf;
        size_t len;
    } args = {1, buf, len}; // fd=1 → stdout

    register uint32_t r0 __asm__("r0") = 0x05; // SYS_WRITE
    register void    *r1 __asm__("r1") = &args;

    __asm volatile ("bkpt 0xAB" : "+r"(r0) : "r"(r1) : "memory");
    return (int)r0;
}

int _write(int fd, const void *buf, size_t len) {
    if (fd == 1 || fd == 2) { // stdout/stderr
        return len - sh_write(buf, len);
    }
    return -1;
}

#else
int _write(int fd, const void *buf, size_t len) {
    (void)fd;
    (void)buf;
    (void)len;
    return (int)len;
}
#endif

void *_sbrk(ptrdiff_t incr)     { extern char _end; static char *brk=&_end; char *p=brk; brk+=incr; return p; }
void _exit(int status)          { (void)status; while(1){ __asm volatile("wfi"); } }
