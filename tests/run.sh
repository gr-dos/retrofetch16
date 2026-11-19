#!/bin/sh
#
# Test retrofch.exe under DOSBox-X.
#
# One batch file runs every DOS-side step, and the results are checked
# on the host afterwards.  Each step N records retrofch's exit status in
# RN.TXT and its standard output in ON.TXT.  DOS cannot redirect
# standard error, so diagnostics are not checked.  rc.bat uses goto
# rather than "if errorlevel N echo ... >file" because the DOSBox-X
# shell opens the redirection even when the test is false.
#
# The machine-dependent lines (sizes, host name) are checked against
# patterns.  Then retrofch runs once per emulated CPU type (the CPU
# matrix below) to check CPU and FPU detection, and twice with
# grfake.com, a TSR that answers the GR-DOS identification call, to
# check GR-DOS detection and its logo.  Last, it writes to the screen in
# color and in --mono, and scrdump.com saves the screen's characters and
# attributes for checking.
#
# To add a test: add a step to steps.bat (command, then "call rc N")
# and the matching rc/expect/check lines below.
#
# Usage: tests/run.sh [path/to/retrofch.exe [FIXTURE-DIR]]
#        FIXTURE-DIR holds grfake.com and scrdump.com (default: build)
# Environment: DOSBOX (default dosbox-x), KEEP=1 to keep the work
#              directory.

set -u

top=$(cd "$(dirname "$0")/.." && pwd)
exe=${1:-$top/retrofch.exe}
fixtures=${2:-$top/build}
grfake=$fixtures/grfake.com
scrdump=$fixtures/scrdump.com
DOSBOX=${DOSBOX:-dosbox-x}

command -v "$DOSBOX" >/dev/null || { echo "$DOSBOX not found" >&2; exit 2; }
[ -f "$exe" ] || { echo "$exe not found" >&2; exit 2; }
for f in "$grfake" "$scrdump"; do
    [ -f "$f" ] || { echo "$f not found" >&2; exit 2; }
done

work=$(mktemp -d "${TMPDIR:-/tmp}/retrofch.XXXXXX")
pass=0
fail=0
LC_ALL=C
export LC_ALL

# dosbox DIR BATCH [SETTING...]: runs DIR/BATCH with DIR as C:
dosbox() {
    dir=$1
    bat=$2
    shift 2
    set -- -set "cpu cycles=max" "$@"
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy timeout 300 \
        "$DOSBOX" -defaultconf -silent -nogui -nomenu "$@" \
        -c "mount c \"$dir\"" -c "c:" \
        -c "$bat" -c "exit" >"$dir/dosbox.log" 2>&1
}

# ------------------------------------------------------------------
# fixtures

cd "$work" || exit 2
mkdir main
cp "$exe" main/retrofch.exe

cat > main/rc.bat <<'EOS'
@echo off
if errorlevel 2 goto rc2
if errorlevel 1 goto rc1
echo rc0 >c:\r%1.txt
goto end
:rc1
echo rc1 >c:\r%1.txt
goto end
:rc2
echo rc2 >c:\r%1.txt
:end
EOS

cat > main/steps.bat <<'EOS'
@echo off
rem --- the report
retrofch >o1.txt
call rc 1
retrofch --ascii >o2.txt
call rc 2
retrofch --ansi >o3.txt
call rc 3
retrofch --plain >o4.txt
call rc 4
retrofch --logo=none --ascii >o5.txt
call rc 5
retrofch --logo=grdos --ascii >o6.txt
call rc 6
retrofch -a --logo=none >o7.txt
call rc 7
retrofch --mono --LOGO=DOS /? >o8.txt
call rc 8
rem --- help and version
retrofch --help >o9.txt
call rc 9
retrofch -h >o10.txt
call rc 10
retrofch /? >o11.txt
call rc 11
retrofch --version >o12.txt
call rc 12
rem --- errors
retrofch --bogus >o13.txt
call rc 13
retrofch extra >o14.txt
call rc 14
retrofch --logo=bogus >o15.txt
call rc 15
EOS

dosbox "$work/main" steps.bat

# CPU matrix: NAME CPUTYPE FPU (true/false)
matrix='
c8086 8086 false
c8088 8086_prefetch true
c186 80186 true
c188 80186_prefetch true
c286 286 true
c286nofpu 286 false
c386 386 true
c486old 486old true
c486 486 true
c586 pentium true
c586mmx pentium_mmx true
'
echo "$matrix" | while read -r name type fpu; do
    [ -n "$name" ] || continue
    mkdir "$name"
    cp "$exe" "$name/retrofch.exe"
    printf '@echo off\r\nretrofch --plain --logo=none >o.txt\r\n' \
        > "$name/s.bat"
    dosbox "$work/$name" s.bat -set "cpu cputype=$type" -set "cpu fpu=$fpu"
done

# GR-DOS: a kernel that answers the call, and one whose answer ignores
# the nonce (rejected: generic DOS)
for name in grgood grbad; do
    mkdir "$name"
    cp "$exe" "$name/retrofch.exe"
    cp "$grfake" "$name/grfake.com"
    arg=
    [ "$name" = grbad ] && arg=' b'
    printf '@echo off\r\ngrfake%s\r\nretrofch --ascii >o.txt\r\n' "$arg" \
        > "$name/s.bat"
    dosbox "$work/$name" s.bat
done

# The screen: retrofch's BIOS output in color and in --mono, saved by
# scrdump.com.  "cls" first, so that nothing scrolls off.
for name in scrcolor scrmono; do
    mkdir "$name"
    cp "$exe" "$name/retrofch.exe"
    cp "$scrdump" "$name/scrdump.com"
    arg=
    [ "$name" = scrmono ] && arg=' --mono'
    printf '@echo off\r\ncls\r\nretrofch%s\r\nscrdump\r\n' "$arg" \
        > "$name/s.bat"
    dosbox "$work/$name" s.bat
done

# ------------------------------------------------------------------
# checks

ok()     { pass=$((pass + 1)); }
not_ok() { echo "FAIL: $*"; fail=$((fail + 1)); }

# check NAME COMMAND...
check() {
    name=$1
    shift
    if "$@"; then ok; else not_ok "$name"; fi
}

cd "$work/main" || exit 2

# rc STEP EXPECTED-STATUS
rc() {
    got=$(tr -d ' \r' < "R$1.TXT" 2>/dev/null | sed 's/^rc//')
    if [ "$got" = "$2" ]; then
        ok
    else
        not_ok "step $1: exit status ${got:-missing}, expected $2"
    fi
}

# expect STEP < expected-lines; compares ON.TXT with CRs removed
expect() {
    tr -d '\r' < "O$1.TXT" 2>/dev/null > got.txt
    cat > want.txt
    if cmp -s got.txt want.txt; then
        ok
    else
        not_ok "step $1: output differs"
        diff want.txt got.txt | sed 's/^/    /'
    fi
}

# has FILE ERE...: each pattern matches some line of FILE (CRs removed)
has() {
    f=$1
    shift
    for re in "$@"; do
        if tr -d '\r' < "$f" 2>/dev/null | grep -Eq -- "$re"; then
            ok
        else
            not_ok "$f: no line matches '$re'"
        fi
    done
}

# lacks FILE ERE...: no line of FILE matches
lacks() {
    f=$1
    shift
    for re in "$@"; do
        if tr -d '\r' < "$f" 2>/dev/null | grep -Eq -- "$re"; then
            not_ok "$f: a line matches '$re'"
            tr -d '\r' < "$f" | grep -E -- "$re" | sed 's/^/    /'
        else
            ok
        fi
    done
}

# crlf FILE: every line ends in CR LF
crlf() {
    check "$1: lines end in CR LF" \
        sh -c "[ \$(grep -c \"\$(printf '\r')\$\" \"$1\") -eq \$(wc -l < \"$1\") ]"
}

# narrow FILE: no line is wider than 79 columns (escapes not counted)
narrow() {
    w=$(tr -d '\r' < "$1" | sed 's/\x1b\[[0-9;]*m//g' |
        awk '{ if (length($0) > w) w = length($0) } END { print w + 0 }')
    check "$1: lines fit 79 columns (widest $w)" [ "$w" -le 79 ]
}

# seven FILE: 7-bit only
seven() {
    check "$1: 7-bit ASCII" sh -c "! tr -d '\r' < \"$1\" | LC_ALL=C grep -q '[^ -~]'"
}

# The report's lines, as patterns.  Sizes and names depend on the
# emulator, so they are checked for form.
SIZE='[0-9]+(B|K|\.[0-9][MGT])'
REPORT_LINES="
[^ ]
(^|   )-+\$
OS: DOS [0-9]\.[0-9][0-9] \(OEM: [^)]+\)\$
Kernel: DOS [0-9]\.[0-9][0-9]
Machine: [^ ]
CPU: [^ ]
FPU: [^ ]
Memory: $SIZE free / ${SIZE}\$
Video: (MDA|Hercules|CGA|EGA|MCGA|VGA)(, VBE [0-9.]+ $SIZE)?, [0-9]+x[0-9]+ (mono|color)\$
Floppy: ([0-9] drives?|none)\$
Disk \(C:\): $SIZE / $SIZE \([0-9]+%\) [#.]{10}\$
"

report_ok() {
    f=$1
    echo "$REPORT_LINES" | while IFS= read -r re; do
        [ -n "$re" ] || continue
        if tr -d '\r' < "$f" | grep -Eq -- "$re"; then
            echo ok
        else
            echo "FAIL: $f: no line matches '$re'"
        fi
    done > report.out
    pass=$((pass + $(grep -c '^ok$' report.out)))
    fail=$((fail + $(grep -c '^FAIL' report.out)))
    grep '^FAIL' report.out
}

# the report: plain text when redirected, CP437 art by default
rc 1 0
crlf O1.TXT
narrow O1.TXT
lacks O1.TXT "$(printf '\033')"
check "O1.TXT: code page 437 art" \
    sh -c "LC_ALL=C grep -q \"\$(printf '\\332\\304\\304')\" O1.TXT"
rc 4 0
check "--plain is the redirected default" cmp -s O1.TXT O4.TXT

# --ascii, which the patterns read
rc 2 0
seven O2.TXT
narrow O2.TXT
report_ok O2.TXT
has O2.TXT '^\+------------------\+   [^ ]' '^\| C:\\>ver          \|'
lacks O2.TXT 'Disk \([AB]:\)'

# --ansi: colors, a reset at each line's end, and the palette
rc 3 0
has O3.TXT "$(printf '\033')\[1;32mOS: $(printf '\033')\[0;37mDOS" \
    "$(printf '\033')\[0;30m$(printf '\333\333\333')"
crlf O3.TXT
narrow O3.TXT

# --logo=none: the report at the left margin
rc 5 0
seven O5.TXT
has O5.TXT '^OS: DOS' '^CPU: '
lacks O5.TXT '^ '

# --logo=grdos: the GR-DOS art, whatever the kernel; the OS line still
# says what the kernel is
rc 6 0
has O6.TXT '^ _####_   ######_     [^ ]' '^------ D\.O\.S ------   ' \
    'OS: DOS [0-9]'

# -a: A: and B: too (empty floppy drives: not ready, without a prompt)
rc 7 0
has O7.TXT '^Disk \(A:\): ' '^Disk \(B:\): ' '^Disk \(C:\): '

# help wins over the other options
rc 8 0
check "step 8: help" grep -q '^Usage: retrofch' O8.TXT

# help and version
rc 9 0
check "step 9: help" grep -q '^Usage: retrofch' O9.TXT
for n in 10 11; do
    rc $n 0
    check "step $n: same as --help" cmp -s O$n.TXT O9.TXT
done
check "step 8: same as --help" cmp -s O8.TXT O9.TXT
rc 12 0
expect 12 <<EOF
retrofch (retrofetch16) $(cat "$top/VERSION")
EOF

# errors: status 1, nothing on standard output
for n in 13 14 15; do
    rc $n 1
    expect $n < /dev/null
done

# ------------------------------------------------------------------
# CPU matrix

# cpu NAME ERE...: the matrix run NAME's output matches every pattern
cpu() {
    name=$1
    shift
    [ -f "$work/$name/O.TXT" ] || { not_ok "$name: no output"; return; }
    has "$work/$name/O.TXT" "$@"
}

cpu c8086     '^CPU: 8088/8086$' '^FPU: none$'
cpu c8088     '^CPU: Intel 8088$' '^FPU: 8087$'
cpu c186      '^CPU: 80186/80188$'
cpu c188      '^CPU: 80188$'
cpu c286      '^CPU: 80286$' '^FPU: 80287$' '^XMS: '
cpu c286nofpu '^CPU: 80286$' '^FPU: none$'
cpu c386      '^CPU: 80386 \(SX/DX not told apart\)$' '^FPU: 80387$'
cpu c486old   '^CPU: 486-class \(no CPUID\)$' '^FPU: x87, integrated or 487$'
cpu c486      '^CPU: Intel 486$' '^FPU: x87, integrated$'
cpu c586      '^CPU: Intel Pentium @ ~[0-9]+ MHz$'
cpu c586mmx   '^CPU: Intel Pentium MMX @ ~[0-9]+ MHz$'
# no extended memory on an 8086, so no such line
lacks "$work/c8086/O.TXT" '^(XMS|Extended): '

# ------------------------------------------------------------------
# GR-DOS

has "$work/grgood/O.TXT" '^ _####_   ######_     [^ ]' \
    'OS: GR-DOS 0\.04$' 'Kernel: DOS [0-9.]+, GR-DOS API rev 1$'
has "$work/grbad/O.TXT" '^\+------------------\+   [^ ]' 'OS: DOS [0-9]'
lacks "$work/grbad/O.TXT" 'GR-DOS'

# ------------------------------------------------------------------
# the screen

# cells FILE: one line per screen row, "char attr char attr ..." in
# decimal
cells() {
    od -An -v -tu1 -w160 "$1"
}

# attr_at FILE TEXT OFFSET: the attribute of the cell OFFSET after the
# start of TEXT, on the first row that holds TEXT (printable ASCII)
attr_at() {
    cells "$1" | awk -v pat="$2" -v off="$3" '{
        t = ""
        for (i = 1; i <= NF; i += 2)
            t = t sprintf("%c", ($i >= 32 && $i < 127) ? $i : 63)
        p = index(t, pat)
        if (p) { print $((p + off - 1) * 2 + 2); exit }
    }'
}

# attrs FILE CHAR: the distinct attributes of the cells holding CHAR
attrs() {
    cells "$1" | awk -v ch="$2" '{
        for (i = 1; i < NF; i += 2) if ($i == ch) print $(i + 1)
    }' | sort -n -u | tr '\n' ' '
}

scr=$work/scrcolor/SCREEN.BIN
if [ -f "$scr" ]; then
    ok
    # the DOS logo's frame, top left, light gray (07h)
    check "screen: logo at the top left" \
        [ "$(cells "$scr" | awk 'NR == 1 { print $1, $2; exit }')" = "218 7" ]
    # labels in the logo's accent (light green, 0Ah), values light gray
    check "screen: label color" [ "$(attr_at "$scr" "OS: " 0)" = 10 ]
    check "screen: value color" [ "$(attr_at "$scr" "OS: " 4)" = 7 ]
    check "screen: prompt in the logo" [ "$(attr_at "$scr" "C:" 0)" = 10 ]
    # the palette: full blocks in all 16 colors
    got=$(attrs "$scr" 219)
    for a in 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15; do
        case " $got" in
        *" $a "*) ok ;;
        *) not_ok "screen: no palette block in color $a (have: $got)" ;;
        esac
    done
else
    not_ok "screen: no SCREEN.BIN"
fi

scr=$work/scrmono/SCREEN.BIN
if [ -f "$scr" ]; then
    ok
    # monochrome: normal (07h) and bright (0Fh) only, and no palette
    got=$(cells "$scr" | awk '{ for (i = 2; i <= NF; i += 2) print $i }' |
          sort -n -u | tr '\n' ' ')
    check "screen --mono: attributes 7 and 15 only (have: $got)" \
        [ "$got" = "7 15 " ]
    check "screen --mono: label bright" [ "$(attr_at "$scr" "OS: " 0)" = 15 ]
    check "screen --mono: no palette" [ -z "$(attrs "$scr" 219 |
        tr -d ' ' | grep -v '^\(7\|15\|715\)$')" ]
else
    not_ok "screen --mono: no SCREEN.BIN"
fi

# ------------------------------------------------------------------

echo "$pass passed, $fail failed"
if [ "$fail" -ne 0 ] || [ "${KEEP:-0}" = 1 ]; then
    echo "work directory: $work"
else
    rm -rf "$work"
fi
[ "$fail" -eq 0 ]
