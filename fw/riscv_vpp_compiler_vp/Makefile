# SPDX-License-Identifier: Apache-2.0
#
# Firmware for the `riscv_vpp_compiler_vp` handoff package.
#
# These are also the commands the package documents as "the external
# cross-toolchain commands": the packaging gate rebuilds an example with them
# from outside the source tree and substitutes the result for the bundled one,
# so anything this Makefile does that a compiler team could not reproduce would
# fail there rather than being discovered by them.
#
# No libc, no libm, no newlib startup. The xPack toolchain ships no vector
# multilib, so linking any of them into an RVV build would pull scalar objects
# compiled for a different -march. `-ffreestanding -nostdlib -nostartfiles` is
# the documented profile for the first handoff (plan §16, Phase 4.5 non-goals).

TOOLDIR ?= /opt/toolchains/xpack-riscv-none-elf-gcc-15.2.0-1/bin
CROSS   ?= $(TOOLDIR)/riscv-none-elf-

CC      := $(CROSS)gcc
OBJDUMP := $(CROSS)objdump
READELF := $(CROSS)readelf

# The frozen compiler contract (plan §16, Phase 4.5). Every target compile and
# link uses exactly this pair; nothing here selects a different -march for a
# "scalar" example, because the ISA the package is handed off for is one ISA.
ARCH := rv32gcv_zvl512b
ABI  := ilp32d

# Where the shared map lives. The platform compiles against the same directory,
# so `-I` here is what makes "one common header" true rather than aspirational.
#
# Two locations, one rule. In the repository the header is in the platform's
# source tree; in the shipped SDK it sits beside this Makefile, because a
# package cannot reference a path outside itself. Probing for the file rather
# than rewriting this line at packaging time keeps the Makefile a compiler team
# receives byte-identical to the one that is tested here — a shipped build
# script that differs from the tested one is exactly the thing that fails on
# someone else's machine.
MAP_INCLUDE ?= $(if $(wildcard include/compiler_vp/host_io_map.h),include,../../platforms/riscv_vpp_compiler_vp/include)

COMMON := common
GEN    := $(COMMON)/link.ld

# -ffile-prefix-map: `-g` otherwise records the absolute directory `make` ran
# in, so the shipped example ELFs carry a path from whichever machine built
# them, and a Release and a Debug build of the platform produce byte-different
# images from identical sources. Mapping the build directory to a fixed name
# makes the images reproducible and keeps a build-machine path out of a package
# whose whole premise is that it points at nothing outside itself.
CFLAGS := -march=$(ARCH) -mabi=$(ABI) \
          -O2 -g -ffile-prefix-map=$(CURDIR)=riscv_vpp_compiler_vp -std=c11 \
          -ffreestanding -nostdlib -nostartfiles \
          -fno-builtin -fno-common \
          -Wall -Wextra -Werror \
          -I$(COMMON) -I$(MAP_INCLUDE)

# --no-warn-rwx-segments: a bare-metal image on flat RAM with no MMU and no
# loader that could enforce W^X, so one RWX LOAD segment is the honest
# description. Splitting it would add PHDRS complexity that protects nothing.
LDFLAGS := -march=$(ARCH) -mabi=$(ABI) \
           -nostdlib -nostartfiles -static \
           -T $(GEN) -Wl,--build-id=none -Wl,--gc-sections \
           -Wl,--no-warn-rwx-segments

# Auto-vectorization off for the scalar example. Phase 4.5 requires it to be
# real scalar-path evidence, and at -O2 GCC will happily vectorize the
# workload loop for an -march that has V.
SCALAR_CFLAGS := $(CFLAGS) -fno-tree-vectorize -fno-tree-slp-vectorize

SCALAR_DIR := examples/scalar_hello
VECTOR_DIR := examples/rvv_vector_add

SCALAR_ELF := $(SCALAR_DIR)/scalar_hello.elf
VECTOR_ELF := $(VECTOR_DIR)/rvv_vector_add.elf

ELVES := $(SCALAR_ELF) $(VECTOR_ELF)
DISASM := $(ELVES:.elf=.elf.dis)
ATTRS := $(ELVES:.elf=.attributes.txt)

.PHONY: all
all: $(ELVES) $(DISASM) $(ATTRS)

# The linker script is generated from the shared header, so the image's RAM
# origin cannot drift from the address the platform maps. `-P` suppresses line
# markers, which the linker would reject.
$(GEN): $(COMMON)/link.ld.in $(MAP_INCLUDE)/compiler_vp/host_io_map.h
	$(CC) -E -P -x c -I$(MAP_INCLUDE) $< -o $@

# Compile and link as separate steps, with named object files.
#
# The one-step `gcc -o image.elf a.S b.c c.c` spelling is shorter and was what
# this used to do. It has two drawbacks that matter for a shipped SDK. The
# driver names its intermediate objects randomly and records those names in the
# image's string table, so two builds of identical sources differ -- which makes
# "is this the image I tested?" unanswerable by comparison. And it hides the
# compile and link commands a compiler team is most likely to want to copy.
#
# Objects live beside their example rather than in a shared directory: the two
# are built with different flags (the scalar one disables auto-vectorization),
# so one `crt0.o` cannot serve both, and a shared name would silently give the
# second image the first one's objects.

SCALAR_OBJS := $(SCALAR_DIR)/crt0.o $(SCALAR_DIR)/host_io.o $(SCALAR_DIR)/main.o
VECTOR_OBJS := $(VECTOR_DIR)/crt0.o $(VECTOR_DIR)/host_io.o $(VECTOR_DIR)/main.o

$(SCALAR_DIR)/crt0.o: $(COMMON)/crt0.S $(COMMON)/host_io.h
	$(CC) $(SCALAR_CFLAGS) -c -o $@ $<

$(SCALAR_DIR)/host_io.o: $(COMMON)/host_io.c $(COMMON)/host_io.h
	$(CC) $(SCALAR_CFLAGS) -c -o $@ $<

$(SCALAR_DIR)/main.o: $(SCALAR_DIR)/main.c $(COMMON)/host_io.h
	$(CC) $(SCALAR_CFLAGS) -c -o $@ $<

$(SCALAR_ELF): $(SCALAR_OBJS) $(GEN)
	$(CC) $(LDFLAGS) -o $@ $(SCALAR_OBJS)
	@echo "built $@"

$(VECTOR_DIR)/crt0.o: $(COMMON)/crt0.S $(COMMON)/host_io.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(VECTOR_DIR)/host_io.o: $(COMMON)/host_io.c $(COMMON)/host_io.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(VECTOR_DIR)/main.o: $(VECTOR_DIR)/main.c $(COMMON)/host_io.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(VECTOR_ELF): $(VECTOR_OBJS) $(GEN)
	$(CC) $(LDFLAGS) -o $@ $(VECTOR_OBJS)
	@echo "built $@"

%.elf.dis: %.elf
	$(OBJDUMP) -d -M no-aliases $< > $@

%.attributes.txt: %.elf
	$(READELF) -A $< > $@

# ── the checks that make the two examples evidence ──────────────────────────
#
# Every one of these exists because the corresponding failure produces a
# *passing* run otherwise: an image that prints the right four lines while
# containing no vector instruction, or one built for a scalar -march that never
# noticed.
.PHONY: verify
verify: all
	@set -e; \
	for elf in $(ELVES); do \
	    grep -q 'rv32i.*_v' $${elf%.elf}.attributes.txt \
	        || { echo "FAIL: $$elf attributes do not report the V extension"; exit 1; }; \
	    grep -q 'zvl512b' $${elf%.elf}.attributes.txt \
	        || { echo "FAIL: $$elf attributes do not report zvl512b"; exit 1; }; \
	    $(READELF) -h $$elf | grep -q 'ELF32' \
	        || { echo "FAIL: $$elf is not ELF32"; exit 1; }; \
	done
	@# The scalar example must contain no vector instruction at all. No RISC-V
	@# scalar mnemonic begins with `v`, so the mnemonic column is a sound test;
	@# `-M no-aliases` keeps pseudo-instructions from hiding one.
	@if awk -F'\t' 'NF >= 3 && $$3 ~ /^v[a-z0-9]/ { print; found = 1 } \
	                END { exit !found }' $(SCALAR_DIR)/scalar_hello.elf.dis; then \
	    echo "FAIL: the scalar example contains vector instructions (see above)"; \
	    exit 1; \
	fi
	@# ...and the vector example must contain the ones the plan names.
	@for insn in vsetvli vsetvl vle32.v vadd.vv vse32.v; do \
	    grep -q "\b$$insn\b" $(VECTOR_DIR)/rvv_vector_add.elf.dis \
	        || { echo "FAIL: $$insn is missing from the vector example"; exit 1; }; \
	done
	@# Both paths must survive into the image as separate functions. The
	@# inline-asm reference exists so a codegen failure is distinguishable from
	@# a model failure, and it can only do that if the two are still apart.
	@#
	@# The trailing `(\.suffix)?` is not laxity: at -O2 GCC clones both loops on
	@# the constant element count and emits them as `vector_add_asm.constprop.0`
	@# and friends. That is the compiler doing its job, and an exact-match check
	@# would fail on a perfectly good image — the failure this guards against is
	@# a function being *inlined away*, not renamed.
	@for sym in vector_add_intrinsic vector_add_asm; do \
	    $(READELF) -sW $(VECTOR_ELF) | grep -qE " $$sym(\.[A-Za-z0-9_.]+)?$$" \
	        || { echo "FAIL: $$sym was dropped from the vector example"; exit 1; }; \
	done
	@# Every loadable segment must sit inside the RAM window the platform maps.
	@# The platform refuses an out-of-map image at load time; catching it here
	@# names the linker script instead of the runtime.
	@$(READELF) -lW $(SCALAR_ELF) $(VECTOR_ELF) | awk ' \
	    /LOAD/ { \
	        base = strtonum($$3); end = base + strtonum($$6); \
	        if (base < 0x80000000 || end > 0x84000000) { \
	            print "FAIL: segment [" $$3 ", +" $$6 "] is outside the RAM window"; \
	            exit 1 } }'
	@echo "riscv_vpp_compiler_vp firmware verify PASS"

.PHONY: clean
clean:
	rm -f $(ELVES) $(DISASM) $(ATTRS) $(GEN) $(SCALAR_OBJS) $(VECTOR_OBJS)
