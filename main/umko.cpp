
#include <stddef.h>
#include <stdint.h>
#include "module.h"
#include "logger.h"

using namespace ELFIO;

struct args {
    bool flag_quiet;
    bool flag_break;
    std::vector<char *> args;
    std::vector<char *> rel_objs;
};

struct Env {
    SysEnv sys_env;
    std::vector<Module> mods;
    void (*root)(void);
};

Env &GetEnv()
{
    static Env env;
    return env;
}

void RegisterFunc(const char *funcname, void *func)
{
    GetEnv().sys_env.add_symbol(funcname, (uint64_t)func);
}


static void print_usage(char **argv)
{
	printf("usage: %s <elf_rel_file> [<elf_rel_file>] ..\n"
	       "\t--args <string>   : args for rel file(to be done)\n"
	       "\t--help            : this message\n", argv[0]);
}


uint32_t parser_args(int argc, char **argv, args &arg)
{
	if (argc == 1) {
		print_usage(argv);
		return -1;
	}

	for (int i = 1; i < argc; ++i) {
		if (!strcmp(argv[i], "--help")) {
			print_usage(argv);
			return -1;
		}

		if (!strcmp(argv[i], "--args")) {
			if (++i == argc) {
				print_usage(argv);
				return -1;
			}
            arg.args.push_back(argv[i]);
			continue;
		}
		arg.rel_objs.push_back(argv[i]);
	}
    return 0;
}


typedef void (*Entry)(void);
int main(int argc, char **argv)
{
    args arg;
    uint32_t ret = parser_args(argc, argv, arg);
    if (ret != 0) {
        return -1;
    } 
    uint32_t obj_num = arg.rel_objs.size();
    if (obj_num == 0) {
        return -1;
    }
    auto &env = GetEnv();
    env.mods.resize(obj_num);
    
    for (int i = 0; i < obj_num; i++) {
        auto &mod = env.mods[i];
        mod.set_obj_path(arg.rel_objs[i]);
        load_module(mod, env.sys_env);
    }
    uint64_t entry_addr = env.sys_env.get_entry();
    if (entry_addr) {
        log_info("execute entry_func at 0x%lx\n", entry_addr);
        Entry entry_func = (Entry)entry_addr;
        entry_func();
    }

    return 0;
}