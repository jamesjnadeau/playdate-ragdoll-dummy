#!/bin/sh
# Fails if the ELF links any double-precision soft-float helpers.
elf="${1:-build/pdex.elf}"
bad=$(arm-none-eabi-nm "$elf" | grep -E '__aeabi_d|__adddf3|__subdf3|__muldf3|__divdf3|__extendsfdf2|__truncdfsf2|__floatsidf|__fixdfsi')
if [ -n "$bad" ]; then
    echo "double-precision helpers found in $elf:"
    echo "$bad"
    exit 1
fi
echo "no double-precision helpers"
