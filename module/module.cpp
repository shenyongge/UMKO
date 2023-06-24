#include "module.h"
#include <cstdint>
#include <memory>
#include <sstream>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "logger.h"
#include "reloc.h"


using namespace ELFIO;

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
constexpr uint32_t ARCH_SHF_SMALL = 0;

uint32_t align_as(uint32_t input, uint32_t align)
{
    uint32_t ret = input % align;
    if (ret == 0) {
        return input;
    } else {
        return (input / align + 1) * align;
    }
}

#if 0
# define debug_align(X) align_as(X, 4096)
#else
# define debug_align(X) (X)
#endif

static long get_offset(uint64_t sec_align, uint64_t sec_size, 
            uint64_t &size)
{
	long ret = align_as(size, sec_align ? sec_align: 1);
	size = ret + sec_size;
	return ret;
}



uint32_t load_reloc_elf(Module &mod)
{
    const char *path = mod.get_obj_path();
    const int fd = open(path, O_RDONLY);

    if (fd < 0) {
        log_fatal("cannot open file %s\n", path);
        return -1;
    }

    struct stat sb;
    if (fstat(fd, &sb) < 0) {
        close(fd);
        log_fatal("load_reloc_elf:cannot stat file %s\n", path);
        return -1;
    }

    /* Get two distinct mappings to the same file -- first to be used for
        writable sections, second -- for the read-only/exec sections; use
        the first mapping for libelf purposes */
    void *p = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    close(fd);

    if(p == MAP_FAILED) {
        log_fatal("load_reloc_elf:cannot mmap file for %s\n", path);
        return -1;
    }

    std::string elfbuf((char *)p, sb.st_size);
    std::stringstream stream(elfbuf);
    elfio &elf = mod.get_elf();

    if (!elf.load(stream)) {
        log_fatal("load_reloc_elf:cannot load elf %s\n", path);
        return -1;
    }
    mod.set_elf_addr(p);
    log_info("loading file at addr [%p], length [%ld], path [%s]\n", p, sb.st_size, path);
    return 0;
}


void init_section_addr(Module &mod)
{
    elfio &elf = mod.get_elf();
    uint64_t base_addr = (uint64_t)mod.get_elf_addr();
    for (auto& sec : elf.sections ) { 
        auto offset = sec->get_offset();
        sec->set_address((Elf64_Addr)(base_addr + offset));
    }
}


/* Lay out the SHF_ALLOC sections in a way not dissimilar to how ld
   might -- code, read-only data, read-write data, small data.  Tally
   sizes, and place the offsets into sh_entsize fields: high bit means it
   belongs in init. */
static void layout_sections(Module &mod)
{
	static uint32_t const masks[][2] = {
		/* NOTE: all executable code must be the first section
		 * in this array; otherwise modify the text_size
		 * finder in the two loops below */
		{ SHF_EXECINSTR | SHF_ALLOC, ARCH_SHF_SMALL },
		{ SHF_ALLOC, SHF_WRITE | ARCH_SHF_SMALL },
		{ SHF_RO_AFTER_INIT | SHF_ALLOC, ARCH_SHF_SMALL },
		{ SHF_WRITE | SHF_ALLOC, ARCH_SHF_SMALL },
		{ ARCH_SHF_SMALL | SHF_ALLOC, 0 }
	};
    elfio &elf = mod.get_elf();
    uint32_t sec_num = elf.sections.size();
    auto &vsec = mod.get_sec();
    vsec.resize(sec_num);

	for (uint32_t i = 0; i < sec_num; i++){
        vsec[i].offset = ~0UL;
    }

    auto &layout = mod.get_layout();
	for (uint32_t m = 0; m < ARRAY_SIZE(masks); ++m) {
		for (uint32_t i = 0; i < sec_num; ++i) {
			auto sec = elf.sections[i];
            uint32_t sh_flag = sec->get_flags();
            uint32_t sh_align = sec->get_addr_align();
            uint32_t sh_size = sec->get_size();
			uint64_t offset = vsec[i].offset;

			if ((sh_flag & masks[m][0]) != masks[m][0]
			    || (sh_flag & masks[m][1])
			    || offset != ~0UL
			    ){
                    continue;
                }
				
			offset = get_offset(sh_align, sh_size, layout.total_size);
            log_debug("section [%2d] layout offset is 0x[%6lx] name [%s]\n", i, offset ,sec->get_name().c_str());
            vsec[i].offset = offset;
		}
		switch (m) {
		case 0: /* executable */
			layout.total_size = debug_align(layout.total_size);
			layout.text_size = layout.total_size;
			break;
		case 1: /* RO: text and ro-data */
			layout.total_size = debug_align(layout.total_size);
			layout.ro_size = layout.total_size;
			break;
		case 2: /* RO after init */ /* RO and RW split */
			//layout.total_size = debug_align(layout.total_size);
            layout.total_size = align_as(layout.total_size, 4096);
			layout.ro_after_init_size = layout.total_size;
			break;
		case 4: /* whole core */
			layout.total_size = debug_align(layout.total_size);
			break;
		}
	}
}


static int move_module(Module &mod)
{
    auto &layout = mod.get_layout();

	/* Do the allocs. */
    void *ptr = mmap(NULL, layout.total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) {
        log_fatal("move_module:mmap fail %ld\n",layout.total_size);
        return -1;
    }
	memset(ptr, 0, layout.total_size);
	layout.base = ptr;
    log_info("module mmap addr [%p], size [0x%lx]\n", ptr, layout.total_size);

    elfio &elf = mod.get_elf();
    auto &vsec = mod.get_sec();
    uint32_t sec_num = elf.sections.size();
	/* Transfer each section which specifies SHF_ALLOC */
	for (uint32_t i = 0; i < sec_num; i++) {
		auto sec = elf.sections[i];
        uint32_t sh_flag = sec->get_flags();
        uint32_t sh_type = sec->get_type();
        uint64_t addr = sec->get_address();
        uint64_t size = sec->get_size();

		if (!(sh_flag & SHF_ALLOC))
        {
			continue;
        }
        void *dest = (void *)((char *)layout.base + vsec[i].offset);

		if (sh_type != SHT_NOBITS) {
			memcpy(dest, (void *)addr, size);
        }
        log_debug("section layout:[%2d] copy from [0x%lx] to [%p], size 0x[%4lx], name [%s]\n", 
            i, addr, dest, size, sec->get_name().c_str());
		/* Update sh_addr to point to copy in image. */
		sec->set_address((unsigned long)dest);
		//debug("\t0x%lx %s\n",(long)shdr->sh_addr, info->secstrings + shdr->sh_name);
	}

	return 0;
}

uint32_t update_symbol_addr(Module &mod, SysEnv &env, symbol_section_accessor &symbols)
{
    std::string   name;
    Elf64_Addr    value;
    Elf_Xword     size;
    unsigned char bind;
    unsigned char type;
    Elf_Half      section_index;
    unsigned char other;
    Elf64_Addr    newValue;

    log_debug("symbol fixed:oigin addr,      new address, size, bind, type, sch_id, name\n");

    FuncAddr &func = mod.get_func_addr();
    elfio& elf = mod.get_elf();
    uint32_t sym_num =  symbols.get_symbols_num();
    uint32_t section_num = elf.sections.size();
    for (uint32_t i = 0; i < sym_num; i++) {
        bool b = symbols.get_symbol(i, name, value, size, bind, type, section_index,other);
        if (!b) {
            continue;
        }
		if (type == STT_SECTION) {
			/* Section symbols cannot index anything else but their respective sections */
			if (section_index == SHN_UNDEF || section_index >= SHN_LORESERVE || section_index >= section_num) {
				continue;
			}
            name = elf.sections[section_index]->get_name();
			newValue = elf.sections[section_index]->get_address();
		} else if (section_index != SHN_UNDEF && section_index < SHN_LORESERVE) {
			/* Non-section symbols without special indices must index valid sections */
			if (section_index >= section_num) {
				continue;
			}
			newValue = value + elf.sections[section_index]->get_address();
            if (bind == STB_GLOBAL) {
                env.add_symbol(name, newValue);   
                if (name == "Construct") {
                    func.consruct_func = newValue;
                }
            }
 
		} else if (section_index == SHN_UNDEF && bind == STB_GLOBAL) {
			/* Seek undefined symbols from this REL in previous RELs */
            auto ret = env.get_symbol(name, newValue);
			if (ret != 0) {
				log_fatal("undefined symbol '%s'\n", name.c_str());
                continue;
			}
		}  
        
        b = symbols.update_symbol(i, newValue, size, bind, type, section_index, other); 
        if (!b) {
            continue;
        } 
        log_debug("symbol fixed:%10lx, %16lx, %4lx, %4d, %4d, %6d, %s\n",
            value, newValue, size, bind, type, section_index, name.c_str());
    }
    return 0;
}

uint32_t layout_symbol_addr(Module &mod, SysEnv &env)
{
    elfio& elf = mod.get_elf();
    uint32_t sec_num = elf.sections.size();

	for (uint32_t i = 0; i < sec_num; i++) {
        auto sec = elf.sections[i];
        if (SHT_SYMTAB == sec->get_type()) {
            symbol_section_accessor symbols(elf, sec);   
            update_symbol_addr(mod, env, symbols); 
            mod.sym_sec_index = i;   
        }
    }   
    return 0;
}

int apply_relocate_add(const_relocation_section_accessor &relsec, 
	section &sec_to_fixed,
	symbol_section_accessor &symbols,
    uint32_t rela_sec_idx,
    uint32_t fixed_sec_idx)
{
	Elf64_Addr offset;
	Elf_Word   symbol_index;
	unsigned   rel_type;
	Elf_Sxword addend;

    std::string   name;
    Elf64_Addr    value;
    Elf_Xword     size;
    unsigned char bind;
    unsigned char sym_type;
    Elf_Half      section_index;
    unsigned char other;

	auto reloc_num = relsec.get_entries_num();

	auto sec_base_addr = sec_to_fixed.get_address();
	for (uint32_t i = 0; i < reloc_num; i++) {
		bool b = relsec.get_entry(i, offset, symbol_index, rel_type, addend);
		if (!b) {
			continue;
		}
		b = symbols.get_symbol(symbol_index, name, value, size, bind, sym_type, section_index, other);
		if (!b) {
			continue;
		}
		void *loc = (void *)(sec_base_addr + offset);
		Elf64_Addr val = value + addend;
		int ret = do_relocate_add(rel_type, loc, val);
        if (ret != 0) {
            log_warn("Reloc Fail:rela  %u sec %u index %03d offset %08lx Type %03x symIdx %03d symName %s Add %ld\n",
	        rela_sec_idx, fixed_sec_idx, i, offset, rel_type, symbol_index, name.c_str(), addend);
        }
	}
    return 0;
}

void relocate_symbol(Module &mod)
{
    elfio &elf = mod.get_elf();
    uint32_t sec_num = elf.sections.size();
    auto sym_sec = elf.sections[mod.sym_sec_index];

    symbol_section_accessor symbols(elf, sym_sec);

	for (uint32_t i = 0; i < sec_num; i++) {
		auto sec = elf.sections[i];
        uint32_t type = sec->get_type();
        if (type != SHT_RELA) {
            continue;
        }
        uint32_t info = sec->get_info();
        if (info >= sec_num) {
            log_error("rela section (%d,%s) link section(%d) is oversize(%d)!\n",
                i, sec->get_name().c_str(), info, sec_num);
            continue;
        }
        auto sec_to_fixed = elf.sections[info];
        auto sh_flag = sec_to_fixed->get_flags();
		if (!(sh_flag & SHF_ALLOC))
        {
            log_debug("rela section[%2d] for section [%2d] with flag [%2x] is skip! [%s]->[%s]\n",
                i, info, sh_flag, sec->get_name().c_str(), sec_to_fixed->get_name().c_str());
			continue;
        }
        const_relocation_section_accessor rel_sec(elf, sec);
        apply_relocate_add(rel_sec, *sec_to_fixed, symbols, i, info);

    }
}

uint32_t mod_protect(Module &mod)
{
    auto &layout = mod.get_layout();
    if(mprotect(layout.base, layout.ro_after_init_size, PROT_READ | PROT_EXEC)) {
        log_fatal("cannot mprotect memory %p size %d\n", layout.base, layout.ro_after_init_size);
		return -1;
	}
    log_debug("mprotect success!start [%p], end [%p], length [%ld]\n", layout.base, 
        (char *)layout.base + layout.ro_after_init_size, layout.ro_after_init_size);
    return 0;
}

typedef void (*InitFunc)(void);
uint32_t mod_init_and_construct(Module &mod)
{
    elfio& elf = mod.get_elf();
    uint32_t sec_num = elf.sections.size();

	for (uint32_t i = 0; i < sec_num; i++) {
        auto sec = elf.sections[i];
        if (SHT_INIT_ARRAY == sec->get_type()) {
            uint64_t addr = sec->get_address();
            uint64_t num = sec->get_size() / sec->get_entry_size(); 
            log_info("call init_array(0x%lx) count(%ld) for %s\n", addr, num, mod.get_obj_path());
            InitFunc *fn = (InitFunc *)addr;
            for (uint64_t j = 0; j < num; j++) {
                fn[j]();
            }
        }
    }  
    auto func = mod.get_func_addr();
    if (func.consruct_func) {
        log_info("call Construct(0x%lx) for %s\n", func.consruct_func, mod.get_obj_path());

        InitFunc fn = (InitFunc)func.consruct_func;
        fn();
    }
    return 0;
}

uint32_t load_module(Module &mod, SysEnv &env)
{
    load_reloc_elf(mod);

    init_section_addr(mod);

    layout_sections(mod);

    move_module(mod);

    layout_symbol_addr(mod, env);

    relocate_symbol(mod);

    mod_protect(mod);

    mod_init_and_construct(mod);
    return 0;
}
