#include "mylibc.h"

#define xxstr(s) #s
#define xstr(s) xxstr(s)


#define SYS_write     0x40
#define SYS_exit      0x5d
#define SYS_munmap    0xd7
#define SYS_mremap    0xd8
#define SYS_mmap      0xde

intptr_t my_mmap(
	void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
	register uint64_t x0 asm ("x0") = (uintptr_t)addr;
	register uint64_t x1 asm ("x1") = length;
	register uint64_t x2 asm ("x2") = prot;
	register uint64_t x3 asm ("x3") = flags;
	register uint64_t x4 asm ("x4") = fd;
	register uint64_t x5 asm ("x5") = offset;

	asm volatile (
		"mov	x8, " xstr(SYS_mmap) "\n\t"
		"svc	0"
		: "+r" (x0)
		: "r" (x1), "r" (x2), "r" (x3), "r" (x4), "r" (x5)
		: "x8", "memory");

	return x0;
}

int my_munmap(void *addr, size_t length)
{
	register uint64_t x0 asm ("x0") = (uintptr_t)addr;
	register uint64_t x1 asm ("x1") = length;

	asm volatile (
		"mov	x8, " xstr(SYS_munmap) "\n\t"
		"svc	0"
		: "+r" (x0)
		: "r" (x1)
		: "x2", "x3", "x4", "x5", "x8", "memory");

	return x0;
}

intptr_t my_mremap(
	void *addr, size_t old_length, size_t new_length, int flags, void *new_addr)
{
	register uint64_t x0 asm ("x0") = (uintptr_t)addr;
	register uint64_t x1 asm ("x1") = old_length;
	register uint64_t x2 asm ("x2") = new_length;
	register uint64_t x3 asm ("x3") = flags;
	register uint64_t x4 asm ("x4") = (uintptr_t)new_addr;

	asm volatile (
		"mov	x8, " xstr(SYS_mremap) "\n\t"
		"svc	0"
		: "+r" (x0)
		: "r" (x1), "r" (x2), "r" (x3), "r" (x4)
		: "x8", "memory");

	return x0;
}

int64_t my_write(int fileno, const void *ptr, size_t len)
{
	register uint64_t x0 asm ("x0") = fileno;
	register uint64_t x1 asm ("x1") = (uintptr_t) ptr;
	register uint64_t x2 asm ("x2") = len;

	asm volatile (
		"mov	x8, " xstr(SYS_write) "\n\t"
		"svc	0"
		: "+r" (x0)
		: "r" (x1), "r" (x2)
		: "x3", "x4", "x5", "x8", "memory");

	return x0;
}

void my_exit(int code)
{
	register uint64_t x0 asm ("x0") = code;

	asm volatile (
		"mov	x8, " xstr(SYS_exit) "\n\t"
		"svc	0"
		: : "r" (x0));
}



#define PRINT_BUF_LEN 64

typedef __builtin_va_list va_list;
#define va_start(v, l) __builtin_va_start(v, l)
#define va_end(v)      __builtin_va_end(v)
#define va_arg(v, l)   __builtin_va_arg(v, l)
#define va_copy(d, s)  __builtin_va_copy(d, s)

static void simple_outputchar(char **str, char c)
{
        if (str) {
                **str = c;
                ++(*str);
        } else {
            ;
        }
}

enum flags { PAD_ZERO = 1, PAD_RIGHT = 2 };

static int prints(char **out, const char *string, int width, int flags)
{
        int pc = 0, padchar = ' ';

        if (width > 0) {
                int len = 0;
                const char *ptr;
                for (ptr = string; *ptr; ++ptr)
                        ++len;
                if (len >= width)
                        width = 0;
                else
                        width -= len;
                if (flags & PAD_ZERO)
                        padchar = '0';
        }
        if (!(flags & PAD_RIGHT)) {
                for (; width > 0; --width) {
                        simple_outputchar(out, padchar);
                        ++pc;
                }
        }
        for (; *string; ++string) {
                simple_outputchar(out, *string);
                ++pc;
        }
        for (; width > 0; --width) {
                simple_outputchar(out, padchar);
                ++pc;
        }

        return pc;
}

static int simple_outputi(char **out, long long i, int base, int sign,
                          int width, int flags, int letbase)
{
        char print_buf[PRINT_BUF_LEN];
        char *s;
        int t, neg = 0, pc = 0;
        unsigned long long u = i;

        if (i == 0) {
                print_buf[0] = '0';
                print_buf[1] = '\0';
                return prints(out, print_buf, width, flags);
        }

        if (sign && base == 10 && i < 0) {
                neg = 1;
                u = -i;
        }

        s = print_buf + PRINT_BUF_LEN - 1;
        *s = '\0';

        while (u) {
                t = u % base;
                if (t >= 10)
                        t += letbase - '0' - 10;
                *--s = t + '0';
                u /= base;
        }

        if (neg) {
                if (width && (flags & PAD_ZERO)) {
                        simple_outputchar(out, '-');
                        ++pc;
                        --width;
                } else {
                        *--s = '-';
                }
        }

        return pc + prints(out, s, width, flags);
}

static int simple_vsprintf(char **out, const char *format, va_list ap)
{
        int width, flags;
        int pc = 0;
        char scr[2];
        union {
                char c;
                char *s;
                int i;
                unsigned int u;
                long li;
                unsigned long lu;
                long long lli;
                unsigned long long llu;
                short hi;
                unsigned short hu;
                signed char hhi;
                unsigned char hhu;
                void *p;
        } u;

        for (; *format != 0; ++format) {
                if (*format == '%') {
                        ++format;
                        width = flags = 0;
                        if (*format == '\0')
                                break;
                        if (*format == '%')
                                goto out;
                        if (*format == '-') {
                                ++format;
                                flags = PAD_RIGHT;
                        }
                        while (*format == '0') {
                                ++format;
                                flags |= PAD_ZERO;
                        }
                        if (*format == '*') {
                                width = va_arg(ap, int);
                                format++;
                        } else {
                                for (; *format >= '0' && *format <= '9';
                                     ++format) {
                                        width *= 10;
                                        width += *format - '0';
                                }
                        }
                        switch (*format) {
                        case ('d'):
                                u.i = va_arg(ap, int);
                                pc += simple_outputi(
                                        out, u.i, 10, 1, width, flags, 'a');
                                break;

                        case ('b'):
                                u.i = va_arg(ap, int);
                                pc += simple_outputi(
                                        out, u.i, 2, 1, width, flags, 'a');
                                break;

                        case ('u'):
                                u.u = va_arg(ap, unsigned int);
                                pc += simple_outputi(
                                        out, u.u, 10, 0, width, flags, 'a');
                                break;

                        case ('p'):
                                u.llu = va_arg(ap, unsigned long);
                                pc += simple_outputi(
                                        out, u.llu, 16, 0, width, flags, 'a');
                                break;

                        case ('x'):
                                u.u = va_arg(ap, unsigned int);
                                pc += simple_outputi(
                                        out, u.u, 16, 0, width, flags, 'a');
                                break;

                        case ('X'):
                                u.u = va_arg(ap, unsigned int);
                                pc += simple_outputi(
                                        out, u.u, 16, 0, width, flags, 'A');
                                break;

                        case ('c'):
                                u.c = va_arg(ap, int);
                                scr[0] = u.c;
                                scr[1] = '\0';
                                pc += prints(out, scr, width, flags);
                                break;

                        case ('s'):
                                u.s = va_arg(ap, char *);
                                pc += prints(out,
                                             u.s ? u.s : "(null)",
                                             width,
                                             flags);
                                break;
                        case ('l'):
                                ++format;
                                switch (*format) {
                                case ('d'):
                                        u.li = va_arg(ap, long);
                                        pc += simple_outputi(out,
                                                             u.li,
                                                             10,
                                                             1,
                                                             width,
                                                             flags,
                                                             'a');
                                        break;

                                case ('u'):
                                        u.lu = va_arg(ap, unsigned long);
                                        pc += simple_outputi(out,
                                                             u.lu,
                                                             10,
                                                             0,
                                                             width,
                                                             flags,
                                                             'a');
                                        break;

                                case ('x'):
                                        u.lu = va_arg(ap, unsigned long);
                                        pc += simple_outputi(out,
                                                             u.lu,
                                                             16,
                                                             0,
                                                             width,
                                                             flags,
                                                             'a');
                                        break;

                                case ('X'):
                                        u.lu = va_arg(ap, unsigned long);
                                        pc += simple_outputi(out,
                                                             u.lu,
                                                             16,
                                                             0,
                                                             width,
                                                             flags,
                                                             'A');
                                        break;

                                case ('l'):
                                        ++format;
                                        switch (*format) {
                                        case ('d'):
                                                u.lli = va_arg(ap, long long);
                                                pc += simple_outputi(out,
                                                                     u.lli,
                                                                     10,
                                                                     1,
                                                                     width,
                                                                     flags,
                                                                     'a');
                                                break;

                                        case ('u'):
                                                u.llu = va_arg(
                                                        ap, unsigned long long);
                                                pc += simple_outputi(out,
                                                                     u.llu,
                                                                     10,
                                                                     0,
                                                                     width,
                                                                     flags,
                                                                     'a');
                                                break;

                                        case ('x'):
                                                u.llu = va_arg(
                                                        ap, unsigned long long);
                                                pc += simple_outputi(out,
                                                                     u.llu,
                                                                     16,
                                                                     0,
                                                                     width,
                                                                     flags,
                                                                     'a');
                                                break;

                                        case ('X'):
                                                u.llu = va_arg(
                                                        ap, unsigned long long);
                                                pc += simple_outputi(out,
                                                                     u.llu,
                                                                     16,
                                                                     0,
                                                                     width,
                                                                     flags,
                                                                     'A');
                                                break;

                                        default:
                                                break;
                                        }
                                        break;
                                default:
                                        break;
                                }
                                break;
                        case ('h'):
                                ++format;
                                switch (*format) {
                                case ('d'):
                                        u.hi = va_arg(ap, int);
                                        pc += simple_outputi(out,
                                                             u.hi,
                                                             10,
                                                             1,
                                                             width,
                                                             flags,
                                                             'a');
                                        break;

                                case ('u'):
                                        u.hu = va_arg(ap, unsigned int);
                                        pc += simple_outputi(out,
                                                             u.lli,
                                                             10,
                                                             0,
                                                             width,
                                                             flags,
                                                             'a');
                                        break;

                                case ('x'):
                                        u.hu = va_arg(ap, unsigned int);
                                        pc += simple_outputi(out,
                                                             u.lli,
                                                             16,
                                                             0,
                                                             width,
                                                             flags,
                                                             'a');
                                        break;

                                case ('X'):
                                        u.hu = va_arg(ap, unsigned int);
                                        pc += simple_outputi(out,
                                                             u.lli,
                                                             16,
                                                             0,
                                                             width,
                                                             flags,
                                                             'A');
                                        break;

                                case ('h'):
                                        ++format;
                                        switch (*format) {
                                        case ('d'):
                                                u.hhi = va_arg(ap, int);
                                                pc += simple_outputi(out,
                                                                     u.hhi,
                                                                     10,
                                                                     1,
                                                                     width,
                                                                     flags,
                                                                     'a');
                                                break;

                                        case ('u'):
                                                u.hhu = va_arg(ap,
                                                               unsigned int);
                                                pc += simple_outputi(out,
                                                                     u.lli,
                                                                     10,
                                                                     0,
                                                                     width,
                                                                     flags,
                                                                     'a');
                                                break;

                                        case ('x'):
                                                u.hhu = va_arg(ap,
                                                               unsigned int);
                                                pc += simple_outputi(out,
                                                                     u.lli,
                                                                     16,
                                                                     0,
                                                                     width,
                                                                     flags,
                                                                     'a');
                                                break;

                                        case ('X'):
                                                u.hhu = va_arg(ap,
                                                               unsigned int);
                                                pc += simple_outputi(out,
                                                                     u.lli,
                                                                     16,
                                                                     0,
                                                                     width,
                                                                     flags,
                                                                     'A');
                                                break;

                                        default:
                                                break;
                                        }
                                        break;
                                default:
                                        break;
                                }
                                break;
                        default:
                                break;
                        }
                } else {
                out:
                        if (*format == '\n')
                                simple_outputchar(out, '\r');
                        simple_outputchar(out, *format);
                        ++pc;
                }
        }
        if (out)
                **out = '\0';
        return pc;
}

int my_printf(const char *fmt, ...)
{
	char g_buff[1024];
	va_list va;
	va_start(va, fmt);
	char *out = g_buff;
	int res = simple_vsprintf(&out, fmt, va);
	va_end(va);

	my_write(FILENO_STDOUT, g_buff, res);
	return res;
}
