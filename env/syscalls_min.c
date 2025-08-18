#include <sys/stat.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/unistd.h>
#include <stddef.h>

int _close(int fd)              { (void)fd; errno = EBADF; return -1; }
int _fstat(int fd, struct stat *st){ (void)fd; st->st_mode = S_IFCHR; return 0; }
int _isatty(int fd)             { (void)fd; return 1; }
int _lseek(int fd, int ptr, int dir){ (void)fd;(void)ptr;(void)dir; return 0; }
int _read(int fd, void *buf, size_t cnt){ (void)fd;(void)buf;(void)cnt; errno = EINVAL; return -1; }


// ARM semihosting: SYS_WRITE (0x05)
// r0 = 0x05, r1 = &{fd, buf, len} -> returns #bytes NOT written
static inline int sh_write(int fd, const void *buf, size_t len) {
  struct { int fd; const void *buf; size_t len; } args = { fd, buf, len };
  register uint32_t r0 __asm__("r0") = 0x05;
  register void    *r1 __asm__("r1") = &args;
  __asm volatile ("bkpt 0xAB" : "+r"(r0) : "r"(r1) : "memory");
  return (int)r0;
}

static inline int dbg_attached(void) {
  volatile uint32_t *DHCSR = (uint32_t *)0xE000EDF0;
  return (*DHCSR & 1u) != 0; // C_DEBUGEN
}

int _write(int fd, const void *buf, size_t len) {
  //if ((fd == STDOUT_FILENO || fd == STDERR_FILENO) && dbg_attached()) {
    if (fd == STDOUT_FILENO || fd == STDERR_FILENO) {
// optional: translate '\n' -> "\r\n" for nicer consoles
    size_t off = 0;
    while (off < len) {
      char tmp[96];
      size_t n = len - off; if (n > sizeof(tmp)) n = sizeof(tmp);
      for (size_t i = 0; i < n; i++) {
        char c = ((const char*)buf)[off+i];
        if (c == '\n') { tmp[i++] = '\r'; if (i == n) { n = i; break; } tmp[i] = '\n'; }
        else tmp[i] = c;
      }
      int notw = sh_write(fd, tmp, n);
      int w = (int)n - notw;
      if (w <= 0) break;
      off += (size_t)w;
    }
    return (int)off;
  }
  return -1; // no debugger attached: drop
}
void *_sbrk(ptrdiff_t incr)     { extern char _end; static char *brk=&_end; char *p=brk; brk+=incr; return p; }
void _exit(int status)          { (void)status; while(1){ __asm volatile("wfi"); } }
