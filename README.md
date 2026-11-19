# retrofetch16

A colorful, compact, [Fastfetch](https://github.com/fastfetch-cli/fastfetch)-style
system information report for DOS, built with Open Watcom and
[libc16](https://github.com/gr-dos/libc16). It shows an OS logo on the
left and the CPU, FPU, DOS, memory, video and disks on the right.

    ┌──────────────────┐   PC
    │ C:\>ver          │   --
    │ DOS              │   OS: DOS 6.22 (OEM: Microsoft)
    │ C:\>_            │   Kernel: DOS 6.22
    └──────────────────┘   Machine: PC/XT-class, BIOS 01/10/86
          ▄▄▄▄▄▄▄▄         CPU: NEC V20
       ▀▀▀▀▀▀▀▀▀▀▀▀▀▀      FPU: 8087
                           Memory: 584K free / 640K
                           Video: CGA, 80x25 color
                           Shell: C:\COMMAND.COM
                           Floppy: 2 drives
                           Disk (C:): 15.7M / 19.6M (80%) ████████░░

(Illustrative; on screen the labels, logo and usage bars are in color,
followed by the 16-color palette.)

The program is `RETROFCH.EXE` (command `retrofch`), a small-model,
real-mode 8086 executable of about 18 KB that needs about 30 KB of
memory. It runs on any DOS from 2.0 on an original PC up, and identifies
newer CPUs without ever running an instruction the CPU has not been
shown to have.

## Usage

    retrofch [OPTION]...

| Option | Meaning |
| --- | --- |
| `-a`, `--all` | also query A:, B:, removable and CD-ROM drives |
| `--plain` | no colors |
| `--ansi` | colors as ANSI escapes, for ANSI.SYS or a file viewed in a terminal |
| `--mono` | monochrome attributes (normal and bright) on the screen |
| `--ascii` | 7-bit ASCII instead of code page 437 graphics |
| `--logo=L` | the logo: `grdos`, `dos` or `none` (default: chosen by the kernel) |
| `-h`, `--help`, `-?`, `/?` | show help |
| `--version` | show version |

Options are case-insensitive. Of `--plain`, `--ansi` and `--mono`, the
last one given wins.

### Output

- **On the screen**, colors go through the BIOS (INT 10h), so they need
  no ANSI.SYS and work on MDA, Hercules, CGA, EGA and VGA. On a
  monochrome display or in a black-and-white mode (BIOS modes 0, 2 and
  7) only normal and bright attributes are used, never underline, and
  the palette is left out.
- **Redirected** (`retrofch >info.txt`), or piped, the report is plain
  text with CR LF line endings and no escape codes. The art is code page
  437; add `--ascii` for 7-bit text.
- Lines are cut to fit the screen width less one column (79 on an
  80-column screen), so nothing wraps.

### The logo

The GR-DOS logo is shown only when the kernel answers the GR-DOS
identification call (INT 21h AX=3347h; gr-dos `docs/grver.md`) correctly
for two different nonces. Every other kernel gets the generic DOS logo,
whatever version it reports, since compatible kernels report the same
versions. `--logo=` overrides the choice; it changes only the art, never
the OS line.

### What is reported

| Line | Source |
| --- | --- |
| title | the network machine name (INT 21h AX=5E00h), or `PC` |
| OS | GR-DOS by its identification call; DR DOS family by AX=4452h; FreeDOS by OEM FDh; otherwise `DOS n.nn (OEM: name)` from AH=30h |
| Kernel | the reported version; the true version (AX=3306h, DOS 5+) when different; FreeDOS's kernel string (AX=33FFh) |
| Windows | Windows in enhanced mode (INT 2Fh AX=1600h), if running |
| Machine | the BIOS model byte (F000:FFFE) and date (F000:FFF5) |
| CPU | `cpu_probe` in `src/cpu.asm` ([CPU and FPU](#cpu-and-fpu)) |
| FPU | `fpu_probe` in `src/cpu.asm` |
| Memory | free: the block DOS gave the program (what any program could get); total: INT 12h |
| XMS / Extended | an XMS driver's free and total memory (INT 2Fh AX=4300h), else BIOS extended memory (INT 15h E801h or 88h), only on a 286 or later |
| EMS | the EMS driver's free and total pages (INT 67h), if `EMMXXXX0` is installed |
| Video, GPU | the adapter (INT 10h 1A00h, 12h, Hercules status port), mode, VESA BIOS version, memory and OEM string |
| Shell | `COMSPEC` |
| Floppy | the BIOS equipment word |
| Disk (X:) | used / total (percent used) and a usage bar ([Disks](#disks)) |

### CPU and FPU

| Shown | How |
| --- | --- |
| `Intel 8088`, `Intel 8086`, `8088/8086` | FLAGS bits 12–15 stuck at 1, shift counts not masked; prefetch queue 4 or 6 bytes, or not seen |
| `NEC V20`, `NEC V30`, `NEC V20/V30` | as above, and AAD ignores its operand |
| `80188`, `80186`, `80186/80188` | FLAGS bits 12–15 stuck at 1, shift counts masked; queue as above |
| `80286` | FLAGS bits 12–14 stuck at 0 in real mode |
| `80386 (SX/DX not told apart)` | bits 12–14 settable, EFLAGS.AC not |
| `486-class (no CPUID)` | AC settable, EFLAGS.ID not |
| the brand string, or vendor and family | CPUID |
| `@ ~N MHz` | only with CPUID and a time-stamp counter: counted over three BIOS timer ticks |
| `(V86 mode)` | MSW.PE set: running under EMM386, Windows or similar |

The prefetch queue length is measured by self-modifying code. Emulators
that do not model the queue show `8088/8086` (or `80186/80188`,
`NEC V20/V30`): the name says what is certain. The 386SX and 386DX
cannot be told apart from real mode safely, so they are not. The MHz
figure is an estimate: power management, turbo switches and emulators
change it, and it is never shown for CPUs without a time-stamp counter.

The FPU is found with FNINIT and FNSTSW/FNSTCW, which are harmless
without one, and told apart by how it compares infinities: projective
(`8087`, `80287`) or affine only (387-based: `80187` on a 186,
`80287XL-class` on a 286, `80387`, and `x87, integrated` when CPUID
says so). When the BIOS says a coprocessor is installed but
none answers, the line says so.

These routines are in `src/cpu.asm` with a C interface (`cpu_probe`,
`cpu_queue`, `fpu_probe`, `cpu_cpuid`, `cpu_rdtsc`), and are named by
`cpu_name` and `fpu_name` in `src/describe.c`. They have no other
dependencies, so GR-DOS's installer
([gr-dos/gr-dos#28](https://github.com/gr-dos/gr-dos/issues/28)) can use
them without the display program; see `docs/DESIGN.md`.

### Disks

By default every drive from C: to DOS's last drive is listed, and **A:
and B: are never touched**. Before a drive is queried:

- a letter not in use (IOCTL 4409h fails with 0Fh) is skipped;
- a SUBST drive is skipped (it reports another drive's space);
- a CD-ROM (MSCDEX) or removable drive (IOCTL 4408h) is listed as
  `not queried`, without touching the medium, unless `-a` is given.

Sizes come from AX=7303h on DOS 7 and later (FAT32, beyond 2 GB), and
otherwise from the classic AH=36h, whose 16-bit cluster counts cannot
describe volumes much beyond 2 GB. A query that fails is answered with
Fail at DOS's critical-error prompt (DOS 3.1 and later), so an empty
drive shows `not ready` instead of `Abort, Retry, Fail?`. The capacity
arithmetic and its 64-bit rounding come from
[gr-dos/df16](https://github.com/gr-dos/df16). Sizes are binary (1K =
1024 bytes), used and total rounded up, and the percentage rounded up;
the bar is green below 70%, yellow below 90% and red from 90%.

DOS before 3.1 has no IOCTL to tell an unused letter from a drive that
failed, so such a drive is skipped silently there. Network drives are
queried, and marked `net`.

### ERRORLEVEL

| ERRORLEVEL | Meaning |
| --- | --- |
| 0 | success |
| 1 | usage error |
| 2 | write error on standard output |

## Building

`retrofch.exe` links [libc16](https://github.com/gr-dos/libc16), a C
runtime for small-model DOS programs, instead of Open Watcom's own, and
is built with the Open Watcom toolchain that libc16 pins, including its
`wasm` assembler for `src/cpu.asm`. By default `make` runs
`scripts/fetch-libc16`, which downloads the libc16 release pinned in
that script (`libc16-VERSION.zip`), checks it against the SHA-256
written there before unpacking it, and unpacks it into `build/libc16`.
The release's `tools/fetch-watcom` then fetches the Open Watcom snapshot
libc16 was built with into `build/tools`, also checked by SHA-256. This
needs `curl`, `unzip`, `sha256sum` and `xz`. To move to a newer libc16,
set `version` and `sha256` in `scripts/fetch-libc16` to the new
release's (its `.sha256` asset), both in the same change (libc16's
`docs/CONSUMERS.md`, "Using libc16").

    make                         # fetches libc16 if needed, builds retrofch.exe
    make LIBC16_ROOT=../libc16   # or use a built local libc16 checkout
    make test                    # host unit tests, then DOS tests
    make unit                    # host unit tests only
    make dostest                 # tests/run.sh under DOSBox-X only
    make clean

The build is reproducible from the repository alone: libc16 and Open
Watcom are pinned by version and SHA-256, and nothing else is
downloaded.

The unit tests need a C99 host compiler (`HOSTCC`, default `cc`); they
compile the portable sources (everything but `main.c`, `probe.c`,
`video.c` and `cpu.asm`) and describe made-up machines, from an 8088 XT
to a Pentium under EMM386 and Windows. The DOS tests need `dosbox-x` on
the host (override with `DOSBOX=`; set `KEEP=1` to keep the work
directory). They run every option, then run `retrofch` on eleven
emulated CPUs (8086 with and without a prefetch queue and FPU, 80186,
80188, 286 with and without an FPU, 386, 486 without and with CPUID,
Pentium, Pentium MMX), with a TSR that imitates GR-DOS's identification
call (and one that answers it wrongly), and on the color and monochrome
screen, whose characters and attributes `tests/scrdump.asm` saves for
checking. The two test TSRs are built with the same Open Watcom
snapshot.

The version is the one line in `VERSION`, DOS style (0.01, 0.02, ...
1.00), not semantic versioning. When a PR that changes it is merged to
`main`, the release workflow tags the merge commit `v<VERSION>` and
publishes a GitHub release with `retrofch.exe` attached.

## GR-DOS

`retrofch.exe` is meant for GR-DOS's full (CF and hard disk)
distribution, through its `dist16` mechanism, which ships the `.exe` of
each `gr-dos/*16` repository's release. Its release contract expects an
asset named after the repository (`retrofetch.exe` or
`retrofetch16.exe`), and neither is a valid 8.3 name, so GR-DOS's
`dist16/config` needs

    include gr-dos/retrofetch16 asset=retrofch.exe

`RETROFCH` is not one of GR-DOS's own file names. The program is not
meant for the 360 KB boot floppy. GR-DOS's HELP lists `*16` programs in
its `DIST16` topic and sends readers to their `--help`.

## Provenance

retrofetch16 is a new implementation, inspired by Leah Neukirchen's
[dosfetch](https://github.com/leahneukirchen/dosfetch), a Turbo Pascal
neofetch clone for DOS whose author waived all copyright and related
rights to it. **No code, artwork or data from dosfetch is used.** The
idea of a neofetch-style DOS report and its first set of fields (OS,
shell, floppy drives, disk, base and extended memory, FPU) came from it.
`docs/DESIGN.md` records the evaluation of forking it against writing
anew, and the provenance review.

- `src/format.c` adapts the capacity arithmetic and size formatting of
  [gr-dos/df16](https://github.com/gr-dos/df16) (MIT, same author).
- The logos in `src/logo.c` are original, drawn for this project; no
  third-party artwork or fonts.
- The hardware and DOS calls are documented interfaces (Ralf Brown's
  Interrupt List, Intel's and the BIOS vendors' documentation, gr-dos
  `docs/grver.md`); the CPU and FPU tests are well-known techniques,
  written from their descriptions.
- `tests/unit.h` is from gr-dos/df16; the build, test and release
  layout follow [gr-dos/hello16](https://github.com/gr-dos/hello16).

## Files

- `src/main.c`: the command-line program
- `src/options.c`, `src/options.h`: option parsing
- `src/probe.c`, `src/probe.h`: the BIOS, DOS and hardware calls (DOS only)
- `src/cpu.asm`: the CPU, FPU and XMS probes (wasm)
- `src/sysinfo.h`: what the probes found, as raw values
- `src/describe.c`, `src/describe.h`: naming it, and the report's lines
- `src/format.c`, `src/format.h`: capacity arithmetic and sizes
- `src/logo.c`, `src/logo.h`: the artwork
- `src/layout.c`, `src/layout.h`: the logo beside the report
- `src/out.c`, `src/out.h`: colored output: plain and ANSI
- `src/video.c`, `src/video.h`: colored output through the BIOS (DOS only)
- `tests/test_*.c`: host unit tests
- `tests/run.sh`: DOS tests, run under DOSBox-X
- `tests/grfake.asm`, `tests/scrdump.asm`: test-only DOS fixtures
- `docs/DESIGN.md`: the design, the dosfetch evaluation and provenance
- `Makefile`: Open Watcom and libc16 build
- `scripts/fetch-libc16`: fetches the pinned libc16
- `VERSION`: the version, compiled in and printed by `--version`

## License

MIT. See `LICENSE`.
