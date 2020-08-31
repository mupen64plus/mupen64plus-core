#!/bin/sh
set -e

if [ -z "$TR" ]; then TR=tr; fi
NASM_FILE="$1/asm_defines_nasm.h"
GAS_FILE="$1/asm_defines_gas.h"
rm -f $NASM_FILE $GAS_FILE

GAS_BOOL=0
GAS_LOOP="$(LC_ALL=C grep -a @ASM_DEFINE "$2" | sed 's/@ASM_DEFINE offsetof_struct_//g' | $TR -s '\n' ' ')"
for GAS_CUR in ${GAS_LOOP}; do
	if [ "${GAS_BOOL}" = "0" ]; then
		GAS_BOOL=1
		GAS_STR=${GAS_CUR}
	else
		GAS_BOOL=0
		echo "%define offsetof_struct_${GAS_STR} (${GAS_CUR})">>$NASM_FILE
		echo "#define offsetof_struct_${GAS_STR} (${GAS_CUR})">>$GAS_FILE
	fi
done
