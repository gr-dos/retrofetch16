# Build retrofch.exe for 16-bit MS-DOS with Open Watcom and libc16.
#   make                         fetch the pinned libc16 release into
#                                build/libc16, then build retrofch.exe
#   make LIBC16_ROOT=../libc16   use a libc16 checkout built with its make
#   make test                    host unit tests, then DOS tests under
#                                DOSBox-X
#
# retrofch.exe is compiled and linked against libc16 only (its ABI is in
# libc16's docs/ABI.md), with the Open Watcom snapshot libc16 was built
# with: the libc16 tree's tools/fetch-watcom fetches it into build/tools.
# src/cpu.asm is assembled with that snapshot's wasm.

LIBC16_ROOT ?= build/libc16
# A release tree, or a checkout, whose make stages the same layout in
# build/stage.
LIBC16_DIR  := $(if $(wildcard $(LIBC16_ROOT)/build/stage/VERSION),$(LIBC16_ROOT)/build/stage,$(LIBC16_ROOT))
LIBC16      := $(LIBC16_DIR)/lib/LIBC16.LIB
TOOLS_ENV   := build/tools.env
WATCOM_ENV   = . ./$(TOOLS_ENV) && PATH=$$WATCOM/binl64:$$PATH

VERSION := $(shell cat VERSION)

CC     = wcc
CFLAGS = -q -zq -zl -zld -x -ms -0 -os -s -bt=dos -wx -we \
         -I$(LIBC16_DIR)/include '-DVERSION="$(VERSION)"'
# 8086 by default; the file enables later instructions where it may.
# -fpi87: inline FPU instructions, not calls to an emulator.
AS      = wasm
ASFLAGS = -q -zq -0 -fpi87 -fp0 -ms -bt=dos

# C99 for unsigned long long, which the 64-bit capacity arithmetic needs
HOSTCC     ?= cc
HOSTCFLAGS ?= -std=c99 -pedantic -Wall -Wextra -Werror -O2

# Portable sources, also compiled on the host for the unit tests
PORTABLE = src/describe.c src/format.c src/layout.c src/logo.c \
           src/options.c src/out.c
SRCS = src/main.c src/probe.c src/video.c $(PORTABLE)
HDRS = $(wildcard src/*.h)

OBJS = $(patsubst src/%.c,build/dos/%.obj,$(SRCS)) build/dos/cpu.obj

UNIT = build/test_format build/test_describe build/test_layout \
       build/test_options

retrofch.exe: $(OBJS) $(LIBC16) $(TOOLS_ENV)
	$(WATCOM_ENV) && wlink option quiet, nodefaultlibs, map=build/retrofch.map \
	  format dos name $@ $(foreach o,$(OBJS),file $(o)) library $(LIBC16)

build/dos/%.obj: src/%.c $(HDRS) VERSION $(LIBC16) $(TOOLS_ENV)
	@mkdir -p build/dos
	$(WATCOM_ENV) && $(CC) $(CFLAGS) -fo=$@ $<

build/dos/cpu.obj: src/cpu.asm $(TOOLS_ENV)
	@mkdir -p build/dos
	$(WATCOM_ENV) && $(AS) $(ASFLAGS) -fo=$@ src/cpu.asm

# The pinned libc16 release (scripts/fetch-libc16). A LIBC16_ROOT given
# on the command line must already be built.
ifeq ($(LIBC16_ROOT),build/libc16)
$(LIBC16): scripts/fetch-libc16
	scripts/fetch-libc16 $(LIBC16_ROOT)
endif

# The Open Watcom snapshot libc16 names, as shell assignments.
$(TOOLS_ENV): $(LIBC16)
	mkdir -p build
	watcom=$$(sh $(LIBC16_DIR)/tools/fetch-watcom $(CURDIR)/build/tools) \
	  && printf 'WATCOM=%s\nexport WATCOM\n' "$$watcom" > $@

build/test_%: tests/test_%.c tests/unit.h $(PORTABLE) $(HDRS)
	@mkdir -p build
	$(HOSTCC) $(HOSTCFLAGS) -Isrc -o $@ $< $(PORTABLE)

unit: $(UNIT)
	@for t in $(UNIT); do echo "$$t"; ./$$t || exit 1; done

# Test fixtures for tests/run.sh: grfake.com imitates the GR-DOS
# identification call, scrdump.com saves the text screen.
FIXTURES = build/grfake.com build/scrdump.com

build/%.com: tests/%.asm $(TOOLS_ENV)
	@mkdir -p build
	$(WATCOM_ENV) && $(AS) -q -zq -0 -bt=dos -fo=build/$*.obj $< \
	  && wlink option quiet, nodefaultlibs format dos com name $@ \
	     file build/$*.obj

dostest: retrofch.exe $(FIXTURES)
	sh tests/run.sh $(CURDIR)/retrofch.exe $(CURDIR)/build

test: unit dostest

# Keeps the fetched libc16 and Open Watcom (build/libc16, build/tools).
clean:
	rm -rf *.o *.obj *.err *.map retrofch.exe \
	  $(filter-out build/libc16 build/libc16.zip build/tools,$(wildcard build/*))

.PHONY: unit dostest test clean
