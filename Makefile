BUILD_AARCH64_DIR := build/aarch64
BUILD_X86_64_DIR := build/x86_64
QEMU_USER := /home/os/qemu-7.2.0/build/qemu-aarch64

aarch64:
	mkdir -p $(BUILD_AARCH64_DIR)
	cd $(BUILD_AARCH64_DIR) && cmake ../.. -DCMAKE_TOOLCHAIN_FILE=../../toolchain_aarch64.cmake && make VERBOSE=1
	

x86_64:
	mkdir -p $(BUILD_X86_64_DIR)
	cd $(BUILD_X86_64_DIR) && cmake ../.. -DCMAKE_TOOLCHAIN_FILE=../../toolchain_x86.cmake && make VERBOSE=1

.PHONY: clean
clean:
	rm -rf build


AARCH64_ELF := ./$(BUILD_AARCH64_DIR)/main/umko
MYLIBC := ./$(BUILD_AARCH64_DIR)/examples/aarch64/CMakeFiles/app10.dir/mylibc.c.o
APP10 := ./$(BUILD_AARCH64_DIR)/examples/aarch64/CMakeFiles/app10.dir/app10.c.o


run_app10_aarch64:
	${QEMU_USER} ${AARCH64_ELF} ${MYLIBC} ${APP10}

APP00_OBJ_AARCH64 := ./$(BUILD_AARCH64_DIR)/examples/comm/CMakeFiles/app00.dir/app00.cpp.o

run_app00_aarch64:
	${QEMU_USER} ${AARCH64_ELF} ${APP00_OBJ_AARCH64}

X86_64_ELF := ./${BUILD_X86_64_DIR}/main/umko
APP00_OBJ_X86 := ./${BUILD_X86_64_DIR}/examples/comm/CMakeFiles/app00.dir/app00.cpp.o

run_app00_x86_64:
	${X86_64_ELF} ${APP00_OBJ_X86}

gdb_app00_x86_64:
	gdb -args ${X86_64_ELF} ${COMM1_OBJ}


all:aarch64 x86_64 run_app10_aarch64 run_app00_aarch64 run_app00_x86_64
	$(MAKE) -C . aarch64
	$(MAKE) -C . x86_64
	$(MAKE) -C . run_app10_aarch64
	$(MAKE) -C . run_app00_aarch64
	$(MAKE) -C . run_app00_x86_64

gdb_app10_aarch64:
	${QEMU_USER} -g 1234 ${AARCH64_ELF} ${APP00_OBJ_AARCH64}

gdb_app10_local:
	gdb-multiarch -x ./gdb.cmd
