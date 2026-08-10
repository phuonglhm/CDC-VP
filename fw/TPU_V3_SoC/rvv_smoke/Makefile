# SPDX-License-Identifier: Apache-2.0
#
# Freestanding RV32GCV smoke image (decision record D4).
#
# No libc, no libm, no newlib startup. The xPack toolchain ships no vector
# multilib, so linking any of them into an RVV build would pull scalar objects
# compiled for a different -march; -nostdlib -nostartfiles removes the question
# instead of answering it badly.

TOOLDIR ?= /opt/toolchains/xpack-riscv-none-elf-gcc-15.2.0-1/bin
CROSS   ?= $(TOOLDIR)/riscv-none-elf-

CC      := $(CROSS)gcc
OBJCOPY := $(CROSS)objcopy
OBJDUMP := $(CROSS)objdump
READELF := $(CROSS)readelf

ARCH := rv32gcv_zvl512b
ABI  := ilp32d

# -O1, not -O2: the checks in main.c read back memory the inline assembly
# wrote, and the point is to exercise the vector load/store path rather than to
# let the optimiser prove the answer. -O0 would be worse — it buries the vector
# instructions under stack traffic that makes a TLM trace unreadable.
CFLAGS := -march=$(ARCH) -mabi=$(ABI) \
          -O1 -g -std=c11 \
          -ffreestanding -nostdlib -nostartfiles \
          -fno-builtin -fno-common \
          -Wall -Wextra -Werror \
          -I.


# --no-warn-rwx-segments: this is a bare-metal image on a flat probe memory with
# no MMU and no loader that could enforce W^X, so one RWX LOAD segment is the
# honest description of what it is. Splitting it would add PHDRS complexity that
# protects nothing here.
LDFLAGS := -march=$(ARCH) -mabi=$(ABI) \
           -nostdlib -nostartfiles -static \
           -T link.ld -Wl,--build-id=none -Wl,--gc-sections \
           -Wl,--no-warn-rwx-segments

ELF := rvv_smoke.elf
# Second image, same startup and linker script, different payload: the D10
# vector-trap tests. Kept beside the smoke image rather than in its own tree so
# there is one crt0 and one exit protocol.
TRAP_ELF := rvv_trap.elf

all: $(ELF) $(ELF).dis attributes.txt $(TRAP_ELF) $(TRAP_ELF).dis

$(ELF): crt0.S main.c link.ld sim_exit.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ crt0.S main.c
	@echo "built $@"

# The gate inspects both: the disassembly must actually contain the required
# vector instructions, and the ISA attributes must say rv32gcv with zvl512b.
# A build that silently fell back to a scalar -march would otherwise look fine.
$(TRAP_ELF): crt0.S trap_main.c link.ld sim_exit.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ crt0.S trap_main.c
	@echo "built $@"

%.elf.dis: %.elf
	$(OBJDUMP) -d -M no-aliases $< > $@

attributes.txt: $(ELF)
	$(READELF) -A $< > $@

# Fails loudly rather than reporting a pass on an image that never emitted a
# vector instruction.
.PHONY: verify
verify: $(ELF).dis attributes.txt $(TRAP_ELF).dis
	@grep -q 'rv32i.*_v' attributes.txt \
	    || { echo "FAIL: ELF attributes do not report the V extension"; exit 1; }
	@grep -q 'zvl512b' attributes.txt \
	    || { echo "FAIL: ELF attributes do not report zvl512b"; exit 1; }
	@for insn in vsetvli vle32.v vse32.v vadd.vv vmul.vv vwmulu.vv vmseq.vv \
	             vcpop.m vredsum.vs vslideup.vi vfadd.vv; do \
	    grep -q "\b$$insn\b" $(ELF).dis \
	        || { echo "FAIL: $$insn is missing from the disassembly"; exit 1; }; \
	done
	@grep -q '\bvluxei64.v\b' $(TRAP_ELF).dis \
	    || { echo "FAIL: the trap image lacks the EEW=64 indexed access"; exit 1; }
	@echo "rvv_smoke verify PASS"

.PHONY: clean
clean:
	rm -f $(ELF) $(ELF).dis $(TRAP_ELF) $(TRAP_ELF).dis attributes.txt
