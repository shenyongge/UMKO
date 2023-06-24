
#ifndef __MYLIBC_H__
#define __MYLIBC_H__

#include <stdint.h>
#include <unistd.h>

#define FILENO_STDIN  0
#define FILENO_STDOUT 1
#define FILENO_STDERR 2

#ifdef __cplusplus
extern "C" {
#endif 

intptr_t my_mmap(
	void *addr, size_t length, int prot, int flags, int fd, off_t offset);

int my_munmap(void *addr, size_t length);

intptr_t my_mremap(
	void *addr, size_t old_length, size_t new_length, int flags, void *new_addr);

int64_t my_write(int fileno, const void *ptr, size_t len);

void my_exit(int code);

int my_printf(const char *fmt, ...);

#ifdef __cplusplus
}
#endif 

#endif
