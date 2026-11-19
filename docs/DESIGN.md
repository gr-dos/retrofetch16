# retrofetch16 — Design

Tracking issue: [gr-dos/retrofetch16#1](https://github.com/gr-dos/retrofetch16/issues/1).

## 1. Purpose

A colorful, compact, Fastfetch-style system information program for DOS
that runs on a plain 8086/8088 and safely identifies newer CPUs, math
coprocessors and other hardware: an OS logo on the left, a report on the
right, and disk statistics.

## 2. Naming

| | |
|---|---|
| Repository | `gr-dos/retrofetch16` |
| Program | `RETROFCH.EXE`, command `retrofch` |
| `--version` | `retrofch (retrofetch16) VERSION` |

The issue proposed `RETROFTCH.COM`. `RETROFTCH` is nine characters, not
an 8.3 name: DOS would store it as `RETROFTC`. `RETROFCH` keeps "retro"
and "fetch" readable in eight. It is an `.EXE` because libc16 has no tiny
model and does not support `.COM` programs (its `DESIGN.md`, non-goals);
the issue allows an `.EXE` when the implementation needs one. A `.COM`
would mean an assembly reimplementation (hello16's layout 3), which the
size, about 18 KB, does not call for.

## 3. Fork or new: the evaluation

The issue asked whether to adapt or fork
[leahneukirchen/dosfetch](https://github.com/leahneukirchen/dosfetch) or
to write a new program with Open Watcom and libc16, with dosfetch as the
leading candidate.

### 3.1 What dosfetch is

Revision `93ce618` (2023-08-03): `dosfetch.pas` (176 lines of Turbo
Pascal 6/7 with inline `asm`), a prebuilt `dosfetch.exe` (8,896 bytes),
`screenshot.png` and `README.md`. It shows a three-color `DOS` banner and
seven lines: OS, shell, floppy drives, the current drive's disk usage,
base memory, extended memory, and whether the BIOS reports an FPU.

### 3.2 Provenance review

- **The waiver.** The README and the source's header say: "Written by
  Leah Neukirchen … To the extent possible under law, the creator of
  this work has waived all copyright and related or neighboring rights
  to this work." This is a public-domain-style dedication by the
  creator, not a CC0 license text.
- **Other contributors.** Of the twelve commits, nine are Leah
  Neukirchen's. Three are not: Nagy Tibor's "Use CRT windowing
  procedure" (the `window` calls), Peter Hofmann's "disksize(): Treat
  registers as unsigned integers", and Doeke Zanstra's README edit. The
  waiver speaks for "the creator"; the contributors made no statement
  of their own. Their changes are small, but a fork would carry them
  with no explicit grant.
- **The banner.** The `DOS` banner appears to be set in a FIGlet font
  (its letter shapes match FIGlet's "univers"), whose own provenance and
  license the waiver cannot cover.
- **The binary.** `dosfetch.exe` is built with Turbo Pascal and so
  contains Borland's runtime library, which the waiver cannot cover
  either. It cannot be redistributed on the strength of the waiver.
- **The screenshot** shows the program's output only.

No copied routines or third-party data were found in the Pascal source
apart from the banner; the BIOS and DOS calls it makes are documented
interfaces.

### 3.3 Technical review

From the source, and a build with Free Pascal (the prebuilt binary was
not run for this review):

- **Toolchain.** The README asks for Turbo Pascal 6 or later, which is
  proprietary and not available to CI. Free Pascal 3.2.2's i8086 target
  does build the source unchanged
  (`ppcross8086 -WmSmall -Mtp dosfetch.pas`: 23,006 bytes, small model),
  so a fork could be built reproducibly, though with a second toolchain
  and runtime beside the Open Watcom and libc16 of every other gr-dos
  `*16` program.
- **Output.** It uses the CRT unit: `clrscr` clears the screen and
  `window` positions the text, and CRT writes to the screen directly,
  so the report cannot be redirected to a file. Turbo Pascal 7's CRT
  unit, which the README names, is also known for "Runtime error 200"
  during its delay calibration on fast CPUs.
- **8086 safety and accuracy.**
  - It calls INT 15h AH=88h on every machine; its own comment notes
    that this "could be wrong on XT". On error it falls back to INT 15h
    E801h, then reads the AT CMOS (ports 70h/71h) without checking for
    an AT.
  - It calls AX=3306h (DOS 5+) on every DOS. On DOS 3 and 4 the call is
    an invalid subfunction and BL and BH are left as they were, so the
    version shown comes from the previous call's registers.
  - It names the DOS from the OEM byte alone (IBM, FreeDOS, MS-DOS).
  - It reports an FPU from the BIOS equipment word only, and detects no
    CPU at all.
  - It reports only the current drive, divides by its total without a
    check, and uses floating point for the percentage.

### 3.4 Decision

**A new implementation** in Open Watcom C and libc16, with no code,
artwork or data taken from dosfetch:

- every gr-dos `*16` program is built the same way (gr-dos/hello16's
  conventions: Open Watcom, libc16, DOSBox-X tests, the release
  workflow); a fork would bring a second toolchain and runtime, and an
  executable a quarter larger than this whole program for seven lines;
- the parts that would carry over (seven BIOS and DOS calls and a
  banner) are the smallest part of the work; CPU and FPU detection,
  color output without CRT or ANSI.SYS, disk statistics, GR-DOS
  identification and tests are all new;
- the contributors' changes, the banner's font and the binary's
  runtime are left out without having to resolve them.

dosfetch is credited in the README as the inspiration and the source of
the idea and the first set of fields.

## 4. Architecture

Probing is kept apart from naming and layout, so that everything except
the probes is portable C, compiled and tested on the host:

```
probe.c, cpu.asm  ->  struct sysinfo  ->  describe.c  ->  struct report
  (DOS only)          (raw values)        (portable)       (markup lines)
                                                                |
                      logo.c  ->  layout.c  ->  out.c (plain, ANSI)
                                                video.c (BIOS, DOS only)
```

- `sysinfo.h` holds raw values (class numbers, flags, counts), never
  text, so the unit tests can describe any machine.
- Lines are written in a small markup: the byte 01h and a hex digit
  set the color. `out.c` drops the codes (plain), turns them into SGR
  escapes (`--ansi`), or hands them to `video.c` as attributes, mapped
  to normal and bright on monochrome screens. Code page 437 graphics
  map to ASCII with `--ascii`.
- `layout.c` zips the logo's lines with the report's, cutting every
  line to the screen width less one column.

## 5. Safety

The program runs on an 8088 with 64 KB and no FPU, and must never run an
instruction or call that is not there.

- **CPU instructions.** `src/cpu.asm` is assembled for the 8086
  (`.8086`). `.286p`, `.386` and `.586` are turned on only around code
  that runs after `cpu_probe` has proved them: SMSW only after the FLAGS
  test has shown a 286 or later; 32-bit EFLAGS only on a 386 or later;
  CPUID only when EFLAGS.ID toggles; RDTSC only when CPUID reports a
  time-stamp counter; XMS function 88h's 32-bit registers only on a 386
  with XMS 3.0. The C code is compiled with `-0` (8086 only) and uses no
  floating point.
- **Alignment checks.** Toggling EFLAGS.AC in virtual 8086 mode (CPL 3)
  could raise an alignment fault if the memory manager sets CR0.AM, so
  the toggle runs with interrupts off and the stack dword-aligned.
- **FPU.** FNINIT, FNSTSW and FNSTCW have no WAIT and are harmless
  without an FPU; a delay loop after each lets an 8087 finish. The
  infinity comparison, which waits, runs only once an FPU has answered.
  `wasm -fpi87` puts the FPU instructions inline instead of calling an
  emulator.
- **BIOS and DOS calls.** INT 15h memory calls only on a 286 or later;
  INT 2Fh (XMS, MSCDEX, Windows) only on DOS 3 or later with a nonzero
  vector; INT 67h only when the EMS driver's `EMMXXXX0` name is there;
  AX=3306h only on DOS 5 or later; AX=7303h only on DOS 7 or later;
  IOCTL 4408h and 4409h only on DOS 3.1 or later; AX=33FFh only on
  FreeDOS (OEM FDh). The GR-DOS call (AX=3347h) is safe on any DOS by
  its design (an invalid subfunction of function 33h elsewhere).
- **Drives.** A: and B: are never queried unless asked for, and
  removable and CD-ROM drives are listed without being queried. A
  critical-error handler answers Fail (DOS 3.1 and later, where Fail
  exists), so a failed query never prompts.
- **Memory.** About 30 KB in all, including a 2 KB stack; no heap.

## 6. Identification

### 6.1 CPU

| Step | Test | Result |
|---|---|---|
| 1 | Clear FLAGS bits 12–15; read back | all 1: 8086-class or 80186-class |
| 2 | `SHL AX, 33` | 0: 8086-class (count not masked); else 80186-class |
| 3 | `AAD 16` on AX=0102h | 0Ch: NEC V-series (AAD always base 10); 12h: Intel |
| 4 | Self-modifying code, two distances | prefetch queue 4 (8088, V20, 80188) or 6 (8086, V30, 80186), or none seen |
| 5 | SMSW: PE set | virtual 8086 mode: a 386 or later |
| 6 | Set FLAGS bits 12–14 in real mode | stuck at 0: 80286 |
| 7 | Toggle EFLAGS.AC | fixed: 80386 |
| 8 | Toggle EFLAGS.ID | fixed: 486-class without CPUID; else CPUID |
| 9 | CPUID 0, 1, 80000002h–80000004h | vendor, family, model, brand string |
| 10 | RDTSC over three BIOS ticks | MHz estimate |

Step 4 first changes the instruction right after a `REP STOSB`. A real
8086-family CPU has always fetched it, so if the change takes effect
there is no prefetch queue: an emulator, and the name stays
`8088/8086`. Otherwise it writes backwards over the fifth byte after
the `REP STOSB`, which a 6-byte queue has already fetched and a 4-byte
queue has not.

Not told apart: the 386SX and 386DX (no safe real-mode test), the
80286 makers, and 486-class CPUs without CPUID (Intel, AMD, Cyrix,
and the 486SX with a 487). The names say so.

### 6.2 DOS

| Kernel | Test |
|---|---|
| GR-DOS | AX=3347h, BX=4752h, two nonces in DX: AX=4752h, DX the nonce complemented, CL ≥ 1, BL ≤ 99, both answers equal (gr-dos `docs/grver.md`, "Recognizing GR-DOS") |
| DR DOS, Novell DOS | AX=4452h answers with carry clear and a version code 10xxh |
| FreeDOS | OEM FDh; AX=33FFh gives the kernel string |
| any other | `DOS n.nn (OEM: name)`: what AH=30h says, attributed to the OEM byte rather than asserted |

The version alone never names a kernel, since compatible kernels report
the same versions. Only GR-DOS has its own logo; other kernels get the
generic one.

### 6.3 Disks

See the README's "Disks". The 64-bit capacity arithmetic is df16's,
generalized from AH=36h's 16-bit counts to AX=7303h's 32-bit ones.

## 7. Reuse by GR-DOS's installer

GR-DOS's native installer
([gr-dos/gr-dos#28](https://github.com/gr-dos/gr-dos/issues/28)) may
need the CPU and FPU class without the display program. The interface,
small-model `__cdecl`, needing nothing from libc16:

```c
unsigned cpu_probe(void);   /* low byte: CPU_8086 .. CPU_CPUID (sysinfo.h);
                               100h: NEC V-series; 200h: V86 mode */
unsigned cpu_queue(void);   /* classes 0 and 1 only: 4, 6, or 0 */
unsigned fpu_probe(void);   /* FPU_NONE, FPU_PROJ, FPU_AFFINE */
void cpu_cpuid(unsigned long leaf, unsigned long *regs);  /* class 5 only */
void cpu_rdtsc(unsigned long *tsc);        /* only with CPUID's TSC flag */
```

`cpu_name` and `fpu_name` in `describe.c` turn a `struct cpuinfo` into
text and depend only on `<string.h>` and `format.c`. Taking the two
files is enough; if a second consumer appears, they can move to a small
library of their own.

## 8. Testing

- **Host unit tests** (`make unit`): size formatting and capacity
  arithmetic at the 16-, 32- and 64-bit edges; CPU, FPU, DOS, machine,
  video and drive naming for every class; whole reports for an 8088 XT,
  a 286 without XMS, and a 486 under EMM386 and Windows; logo widths,
  layout, cutting, ANSI codes, ASCII mapping and monochrome attributes;
  options.
- **DOSBox-X** (`make dostest`): every option and error; plain,
  ASCII, ANSI and logo-less output checked line by line against
  patterns; the CPU matrix of README "Building"; GR-DOS detection with a
  TSR imitating the call, and rejection of one that ignores the nonce;
  the screen in color and monochrome, characters and attributes.
- **By hand, not automated:** DOS 2.11, 3.30 and 7.10 (DOSBox-X
  `dos ver=`): IOCTL gating, no INT 2Fh on DOS 2, and AX=7303h on a
  45.7 GB volume.

### 8.1 Real hardware

Not yet done. The checklist:

- an 8088 or V20 XT (for example the NuXT): the CPU name and queue
  length, FPU with and without an 8087, MDA/Hercules/CGA, no floppy
  access in a plain `retrofch`, and the time it takes;
- a 286 and a 386: extended memory with and without HIMEM.SYS;
- a 486 or Pentium under EMM386: `(V86 mode)`, EMS, and the MHz
  estimate against the known clock;
- GR-DOS itself: the GR-DOS logo and `OS: GR-DOS n.nn`.

## 9. Future work

- Logos for other positively identified kernels (FreeDOS, DR DOS), as
  original artwork.
- A CPU clock estimate for CPUs without a time-stamp counter, from a
  timed loop of known cycle counts (with wide error bars).
- Mouse, sound card and network adapter lines.
- Large volumes on DOS before 7 (FAT16 beyond 2 GB is not possible
  there; other filesystems through redirectors may report more).
