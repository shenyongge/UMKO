#ifndef __module_h__
#define __module_h__

#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
#include <elfio/elfio.hpp>

struct Layout {
    uint64_t total_size;
    uint64_t text_size;
    uint64_t ro_size;
    uint64_t ro_after_init_size;
    void *base;
};
struct SectionLayout {
    uint64_t offset;
};

struct FuncAddr {
    uint64_t consruct_func;
    uint64_t app_root_func;
};
using SymMap = std::unordered_map<std::string, uint64_t>;

class SysEnv {
public:
    uint32_t add_symbol(const char *name, uint64_t addr) 
    {
        map.emplace(name, addr);
        return 0;
    }
    uint32_t add_symbol(const std::string &name, uint64_t addr) 
    {
        map.emplace(name, addr);
        return 0;
    }
    uint32_t get_symbol(const std::string &name, uint64_t &addr) const
    {
        auto iter = map.find(name);
        if (iter != map.end()) {
            addr = iter->second;
            return 0;
        }
        return -1;
    }
    uint64_t get_entry() const
    {
        auto iter = map.find("_start");
        if (iter != map.end()) {
            return iter->second;
        }
        iter = map.find("APP_Root");
        if (iter != map.end()) {
            return iter->second;
        }
        return 0;
    }
private:
    SymMap map;
};

class Module {
public:
    ELFIO::elfio &get_elf()
    {
        return elf;
    }
    Layout &get_layout()
    {
        return layout;
    }
    std::vector<SectionLayout> &get_sec()
    {
        return sec_layout;
    }
    const char *get_obj_path()
    {
        return path;
    }
    void set_obj_path(const char * path_str)
    {
        path = path_str;
    }
    FuncAddr &get_func_addr()
    {
        return func;
    }
    uint32_t sym_sec_index = 0;

    void set_elf_addr(void *base_addr)
    {
        elf_addr = base_addr;
    }
    void *get_elf_addr()
    {
        return elf_addr;
    }
private:
    Layout layout = {};
    ELFIO::elfio elf;
    std::vector<SectionLayout> sec_layout;
    FuncAddr func = {0};
    const char *path = nullptr;
    void *elf_addr = nullptr;
};

uint32_t load_module(Module &mod, SysEnv &env);

#endif