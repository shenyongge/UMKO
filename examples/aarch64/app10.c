#include "mylibc.h"


int add(int a, int b)
{
    int c = a + b;
    my_printf("a(%d) + b(%d) = c(%d)\n\n\n", a, b, c);
    return c;
}


int sub(int a, int b)
{
    int c = a - b;
    my_printf("a(%d) - b(%d) = c(%d)\n\n\n", a, b, c);
    return c;
}

void APP_Root()
{
    char buff[] = "Hello world! First reloc file Execute!!\n";
    my_write(FILENO_STDOUT, buff, sizeof(buff));
    int c = add(100, 200);
    int d = sub(9000, 1000);
    my_printf("c = %d, d = %d \n\n\n", c, d);
    my_exit(0);
}

void _start()
{
    APP_Root();
}