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
# F5 concurrency probe: two harts, different rounding modes, interleaved.
FP_ELF := rvv_fp.elf

# Spike differential corpus. The only image with its own startup and linker
# script, because it is the only one that has to run on a second simulator:
# it exits through HTIF instead of the memory-mapped exit register, and it is
# linked at 0x8000_0000 because Spike's boot ROM occupies 0x1000 and would
# shadow a flat image based at 0. See link_dram.ld.
SIG_ELF := rvv_sig.elf
# D12/D13 conformance gate: the properties the differential corpus cannot show,
# because they are about what the model does on the way to the result.
CONF_ELF := rvv_conf.elf
# Interrupt gate: set_irq() delivery, cause, level-triggered clear and masking.
IRQ_ELF := rvv_irq.elf

SIG_LDFLAGS := -march=$(ARCH) -mabi=$(ABI) \
               -nostdlib -nostartfiles -static \
               -T link_dram.ld -Wl,--build-id=none -Wl,--gc-sections \
               -Wl,--no-warn-rwx-segments

all: $(ELF) $(ELF).dis attributes.txt $(TRAP_ELF) $(TRAP_ELF).dis \
     $(FP_ELF) $(FP_ELF).dis $(SIG_ELF) $(SIG_ELF).dis \
     $(CONF_ELF) $(CONF_ELF).dis $(IRQ_ELF) $(IRQ_ELF).dis

$(ELF): crt0.S main.c link.ld sim_exit.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ crt0.S main.c
	@echo "built $@"

# The gate inspects both: the disassembly must actually contain the required
# vector instructions, and the ISA attributes must say rv32gcv with zvl512b.
# A build that silently fell back to a scalar -march would otherwise look fine.
$(TRAP_ELF): crt0.S trap_main.c link.ld sim_exit.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ crt0.S trap_main.c
	@echo "built $@"

$(FP_ELF): crt0.S fp_main.c link.ld sim_exit.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ crt0.S fp_main.c
	@echo "built $@"

$(CONF_ELF): crt0.S conf_main.c link.ld sim_exit.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ crt0.S conf_main.c
	@echo "built $@"

$(IRQ_ELF): crt0.S irq_main.c link.ld sim_exit.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ crt0.S irq_main.c
	@echo "built $@"

$(SIG_ELF): crt0_sig.S sig_main.c link_dram.ld sig_layout.h
	$(CC) $(CFLAGS) $(SIG_LDFLAGS) -o $@ crt0_sig.S sig_main.c
	@echo "built $@"

%.elf.dis: %.elf
	$(OBJDUMP) -d -M no-aliases $< > $@

attributes.txt: $(ELF)
	$(READELF) -A $< > $@

# Fails loudly rather than reporting a pass on an image that never emitted a
# vector instruction.
.PHONY: verify
verify: $(ELF).dis attributes.txt $(TRAP_ELF).dis $(FP_ELF).dis $(CONF_ELF).dis
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
	@grep -q '\bvfadd.vv\b' $(FP_ELF).dis \
	    || { echo "FAIL: the FP probe lacks vfadd.vv"; exit 1; }
	@# The differential corpus is worthless if a symbol the two harnesses look
	@# up is missing: fesvr would silently dump nothing and the comparison
	@# would pass on two empty files.
	@for sym in begin_signature end_signature tohost fromhost __ram_end; do \
	    $(READELF) -sW $(SIG_ELF) | grep -q " $$sym$$" \
	        || { echo "FAIL: $$sym is missing from $(SIG_ELF)"; exit 1; }; \
	done
	@# Everything the image links must sit below the top of RAM, because the
	@# mid-vector fault probe deliberately addresses the last words of memory.
	@$(READELF) -lW $(SIG_ELF) | awk '/LOAD/ { \
	    end = strtonum($$3) + strtonum($$6); \
	    if (end > 0x80100000) { print "FAIL: segment ends past the top of RAM"; exit 1 } }'
	@# The 32 D12 encodings are hand-written `.word`s, because the point of
	@# the gate is that the *model* refuses them; the assembler is happy to emit
	@# them and objdump renders them back as `.word` (a `.word` in `.text` gets a
	@# `$$d` mapping symbol, so the disassembler treats it as data). Grepping for
	@# the mnemonic therefore proves nothing.
	@#
	@# Instead the encodings are re-derived: assemble all 32 mnemonics here and
	@# compare against the constants the firmware uses. A typo would otherwise
	@# turn the gate into repeated probes of nonsense words, which would
	@# still "raise an illegal instruction" and still pass.
	@printf '\t.text\n' > .d12_enc.S
	@for m in vluxei64 vloxei64 vsuxei64 vsoxei64; do \
	    printf '\t%s.v v8, (a0), v16\n' "$$m" >> .d12_enc.S; done
	@for n in 2 3 4 5 6 7 8; do for m in vluxseg vloxseg vsuxseg vsoxseg; do \
	    printf '\t%s%sei64.v v8, (a0), v16\n' "$$m" "$$n" >> .d12_enc.S; done; done
	@$(CC) -march=$(ARCH) -mabi=$(ABI) -c -o .d12_enc.o .d12_enc.S
	@$(OBJDUMP) -d .d12_enc.o | awk '/\tv[sl][ou]x/ { printf "0x%s\n", $$2 }' | sort > .d12_enc.want
	@test "$$(wc -l < .d12_enc.want)" = "32" \
	    || { echo "FAIL: expected 32 index-EEW=64 encodings, got $$(wc -l < .d12_enc.want)"; rm -f .d12_enc.*; exit 1; }
	@grep -oE '\.word 0x[0-9a-f]{8}' conf_main.c | awk '{print $$2}' | sort -u > .d12_enc.got
	@if ! comm -23 .d12_enc.want .d12_enc.got | { ! grep -q .; }; then \
	    echo "FAIL: conf_main.c is missing index-EEW=64 encodings the assembler produces:"; \
	    comm -23 .d12_enc.want .d12_enc.got; rm -f .d12_enc.*; exit 1; fi
	@# ...and every one of them must survive into the image.
	@for w in $$(cat .d12_enc.want); do \
	    grep -q "$${w#0x}" $(CONF_ELF).dis \
	        || { echo "FAIL: encoding $$w is missing from $(CONF_ELF)"; rm -f .d12_enc.*; exit 1; }; \
	done
	@rm -f .d12_enc.*
	@echo "rvv_smoke verify PASS"

.PHONY: clean
clean:
	rm -f .d12_enc.* $(ELF) $(ELF).dis $(TRAP_ELF) $(TRAP_ELF).dis $(FP_ELF) $(FP_ELF).dis \
	      $(SIG_ELF) $(SIG_ELF).dis $(CONF_ELF) $(CONF_ELF).dis \
	      $(IRQ_ELF) $(IRQ_ELF).dis attributes.txt
