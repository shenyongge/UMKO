#include <cstdint>
#include <elfio/elfio.hpp>
#include "logger.h"
#include "reloc.h"

using namespace ELFIO;
int do_relocate_add(uint32_t reloc_type, void *loc, uint64_t val)
{
    switch (reloc_type) {
		case R_X86_64_NONE:
			break;
		case R_X86_64_64:
			if (*(uint64_t *)loc != 0)
				goto invalid_relocation;
			*(uint64_t *)loc = val;
			break;
		case R_X86_64_32:
			if (*(uint32_t *)loc != 0)
				goto invalid_relocation;
			*(uint32_t *)loc = val;
			if (val != *(uint32_t *)loc)
				goto overflow;
			break;
		case R_X86_64_32S:
			if (*(int32_t *)loc != 0)
				goto invalid_relocation;
			*(int32_t *)loc = val;
			if ((int64_t)val != *(int32_t *)loc)
				goto overflow;
			break;
		case R_X86_64_PC32:
		case R_X86_64_PLT32:
			if (*(uint32_t *)loc != 0)
				goto invalid_relocation;
			val -= (uint64_t)loc;
			*(uint32_t *)loc = val;
			break;
		case R_X86_64_PC64:
			if (*(uint64_t *)loc != 0)
				goto invalid_relocation;
			val -= (uint64_t)loc;
			*(uint64_t *)loc = val;
			break;
		default:
			log_error("Unknown rela relocation: type %d loc %p, val %lx\n",
			       reloc_type, loc, val);
			return -ENOEXEC;
		}
		return 0;
invalid_relocation:
	log_error("x86/modules: Skipping invalid relocation target, existing value is nonzero for type %d, loc %p, val %lx\n",
	       reloc_type, loc, val);
	return -ENOEXEC;

overflow:
	log_error("overflow in relocation type %d loc %p, val %lx\n",
	       reloc_type, loc, val);
	return -ENOEXEC;
}
