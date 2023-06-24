#include <stdio.h>
#include <stdarg.h>

typedef int (*printf_ptr) (const char *fmt, ...);
typedef int (*vprintf_ptr) ( const char * format, va_list arg);
vprintf_ptr myprint = vprintf;

void my_printf(const char *fmt, ...) 
{
    va_list args;
    va_start(args, fmt);
    myprint(fmt, args);
    va_end(args);
}

const unsigned long __stack_chk_guard = 0x000a0dff;
extern "C"
void __stack_chk_fail(void)
{
	my_printf("stack-protector: stack is corrupted\n");
}


int add(int a, int b)
{
    int c = a + b;
    my_printf("a(%d) + b(%d) = c(%d)\n", a, b, c);
    return c;
}


int sub(int a, int b)
{
    int c = a - b;
    my_printf("a(%d) - b(%d) = c(%d)\n", a, b, c);
    return c;
}

class Complex {
public:
    Complex(int i, int j):x(i), y(j) {
        my_printf("Complex(%d, %d) is called!\n", i, j);
    }
    int x = 0;
    int y = 0;
};

Complex com1(33,55);

int x = 0, y = 1, z = 2;
extern "C"
void Construct()
{
    x = 77;
    y = 88;
    z = 99;
    com1 = Complex(66, 77);
}
extern "C"
void APP_Root()
{
    char buff[] = "Hello world! First reloc file Execute!!\n";
    my_printf(buff);
    int c = add(100, 200);
    int d = sub(9000, 1000);
    my_printf("c = %d, d = %d \n", c, d);
    int e = add(x, y);
    int f = sub(z, y);
    my_printf("e = %d, f = %d \n", e, f);

    my_printf("com1 = (%d, %d)!\n", com1.x, com1.y);
}
