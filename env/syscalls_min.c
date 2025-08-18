#include <sys/stat.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>

int _close(int fd)              { (void)fd; errno = EBADF; return -1; }
int _fstat(int fd, struct stat *st){ (void)fd; st->st_mode = S_IFCHR; return 0; }
int _isatty(int fd)             { (void)fd; return 1; }
int _lseek(int fd, int ptr, int dir){ (void)fd;(void)ptr;(void)dir; return 0; }
int _read(int fd, void *buf, size_t cnt){ (void)fd;(void)buf;(void)cnt; errno = EINVAL; return -1; }
int _write(int fd, const void *buf, size_t cnt){ (void)fd;(void)buf; return cnt; }
void *_sbrk(ptrdiff_t incr)     { extern char _end; static char *brk=&_end; char *p=brk; brk+=incr; return p; }
void _exit(int status)          { (void)status; while(1){ __asm volatile("wfi"); } }
