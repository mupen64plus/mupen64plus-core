#!/bin/bash
set -e
set +f

#	Set the "print" outputs
NASM_FILE="$1/asm_defines_nasm.h"
GAS_FILE="$1/asm_defines_gas.h"
rm -f "$1/asm_defines_*"

#	Adaptation of 'gen_asm_defines.awk' / 'gen_asm_script.cmd':
#
#	1. Display 'asm_defines.o' as a list
#	2. Look for the '@ASM_DEFINE' pattern
#	3. Sort for easy reading
#	4. Discard "1" ('@ASM_DEFINE') pattern
#	5. Take "2" (offset*) and "3" (0x*) patterns
#	6. Exploit LF to space (U+0020) behavior and save all patterns "2" and "3"
#
GAS_OBJSTR=$(LC_ALL=C grep -a "@ASM_DEFINE" "$2" | sort | cut -d ' ' -f2,3)
for GAS_CURSTR in ${GAS_OBJSTR}; do

	#	7. If current 'GAS_CURSTR' has a "hex" glob pattern, print
	if [ "${GAS_CURSTR:0:2}" = "0x" ]; then

		#	9. Print current values, repeat steps 7-9 until EOL
		echo "%define ${GAS_OFFSET} (${GAS_CURSTR})" >> "$NASM_FILE"
		echo "#define ${GAS_OFFSET} (${GAS_CURSTR})" >> "$GAS_FILE"
	else

		#	8. Save current "offset"
		GAS_OFFSET=${GAS_CURSTR}
	fi
done
