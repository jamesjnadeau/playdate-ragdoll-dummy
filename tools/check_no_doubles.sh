#!/bin/sh
# Fails if the ELF links any double-precision soft-float helpers.
# Also fails (never passes vacuously) if the ELF or toolchain is missing.
elf="${1:-build/pdex.elf}"
if [ ! -f "$elf" ]; then
    echo "check_no_doubles: $elf not found" >&2
    exit 1
fi
syms=$(arm-none-eabi-nm "$elf") || {
    echo "check_no_doubles: arm-none-eabi-nm failed on $elf" >&2
    exit 1
}
if [ -z "$syms" ]; then
    echo "check_no_doubles: no symbols in $elf" >&2
    exit 1
fi
bad=$(printf '%s\n' "$syms" | grep -E '__aeabi_d|__adddf3|__subdf3|__muldf3|__divdf3|__extendsfdf2|__truncdfsf2|__floatsidf|__fixdfsi')
if [ -n "$bad" ]; then
    echo "double-precision helpers found in $elf:"
    echo "$bad"
    exit 1
fi
echo "no double-precision helpers"
