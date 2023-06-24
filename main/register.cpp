#include "register.h"
#include "stdio.h"


RegFunc(vprintf);
#if 0
extern "C" {
void __stack_chk_fail(void);
void __stack_chk_guard(void);
}
RegFunc(__stack_chk_guard);
RegFunc(__stack_chk_fail);
#endif

//RegFunc(__stack_chk_fail);