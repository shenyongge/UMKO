
#ifndef __RELOC_H__
#define __RELOC_H__

#include <stdint.h>

int do_relocate_add(uint32_t reloc_type, void *loc, uint64_t val);

#endif