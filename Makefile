SHELL := /bin/bash
XDIR:=/u/cs452/public/xdev
ARCH=cortex-a72
TRIPLE=aarch64-none-elf
XBINDIR:=$(XDIR)/bin
CC:=$(XBINDIR)/$(TRIPLE)-gcc
OBJCOPY:=$(XBINDIR)/$(TRIPLE)-objcopy
OBJDUMP:=$(XBINDIR)/$(TRIPLE)-objdump

BENCHMARK_SIZE ?= 4
BENCHMARK ?= 0
BENCHMARK_TYPE ?= 0

# COMPILE OPTIONS
# -ffunction-sections causes each function to be in a separate section (linker script relies on this)
WARNINGS=-Wall -Wextra -Wpedantic -Wno-unused-const-variable
PREPROC_VARS=-DBENCHMARK=$(BENCHMARK) -DBENCHMARK_MSG_SIZE=$(BENCHMARK_SIZE) -DBENCHMARK_TYPE=${BENCHMARK_TYPE}
CFLAGS:=-g -pipe -static $(WARNINGS) $(PREPROC_VARS) -ffreestanding -nostartfiles\
	-mcpu=$(ARCH) -static-pie -mstrict-align -fno-builtin -mgeneral-regs-only -O3

# -Wl,option tells g++ to pass 'option' to the linker with commas replaced by spaces
# doing this rather than calling the linker ourselves simplifies the compilation procedure
LDFLAGS:=-Wl,-nmagic -Wl,-Tlinker.ld

# Source files and include dirs
SOURCES := $(wildcard *.c) $(wildcard *.S) $(wildcard user/*.c) $(wildcard train/*.c) $(wildcard traindata/*.c)
# Create .o and .d files for every .cc and .S (hand-written assembly) file
OBJECTS := $(patsubst %.c, %.o, $(patsubst %.S, %.o, $(SOURCES)))
DEPENDS := $(patsubst %.c, %.d, $(patsubst %.S, %.d, $(SOURCES)))

define DEPENDABLE_VAR

.PHONY: phony
$1: phony
	@if [[ `cat $1 2>&1` != '$($1)' ]]; then \
		echo -n $($1) > $1 ; \
	fi

endef

# The first rule is the default, ie. "make", "make all" and "make kernel.img" mean the same
all: kernel.img

clean:
	rm -f $(OBJECTS) $(DEPENDS) kernel.elf kernel.img

kernel.img: kernel.elf
	$(OBJCOPY) $< -O binary $@

kernel.elf: $(OBJECTS) linker.ld
	$(CC) $(CFLAGS) $(filter-out %.ld, $^) -o $@ $(LDFLAGS)
	@$(OBJDUMP) -d kernel.elf | fgrep -q q0 && printf "\n***** WARNING: SIMD INSTRUCTIONS DETECTED! *****\n\n" || true

%.o: %.c Makefile BENCHMARK BENCHMARK_SIZE BENCHMARK_TYPE
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

%.o: %.S Makefile
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

# declare dependable vars
$(eval $(call DEPENDABLE_VAR,BENCHMARK))
$(eval $(call DEPENDABLE_VAR,BENCHMARK_TYPE))
$(eval $(call DEPENDABLE_VAR,BENCHMARK_SIZE))

-include $(DEPENDS)
