
#ifndef __REGISTER_H__
#define __REGISTER_H__

void RegisterFunc(const char *funcname, void *func);

class AutoRegFunc {
public:
    AutoRegFunc(const char *funcname, void *func)
    {
        RegisterFunc(funcname, func);
    }
};

#define RegFuncName(name, func) AutoRegFunc g_register_##func(name, (void*)func)
#define RegFunc(name) RegFuncName(#name, name)



#endif