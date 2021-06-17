#!/bin/bash
set -e
set +f

#	Set the "print" outputs
NASM_FILE="$1/asm_defines_nasm.h"
GAS_FILE="$1/asm_defines_gas.h"
rm -f "$1/asm_defines_*"

#	Adaptation of 'gen_asm_defines.awk' / 'gen_asm_script.cmd':
#
#	1. Display 'asm_defines.o' as a list, looking only for
#	   patterns '@ASM_DEFINE'
#	2. Sort for easy reading, the result is interpreted as an array or
#	   worksheet with ' ' as a self-imposed delimiter in the 'cut' command
#	3. The cut's '-f2,3' discards column "1" (@ASM_DEFINE) and anything
#	   beyond "3", take "2" (offset* column), the delimiters and
#	   "3" (hex* column) to be kept
#	4. After setting 'GAS_OBJSTR', all the next lines (LF) are degraded
#	   to ' ' and thus all patterns in columns "2" and "3" become a
#	   predictable text sequence
GAS_OBJSTR=$(LC_ALL=C grep -a "@ASM_DEFINE" "$2" | sort | cut -d ' ' -f2,3)
for GAS_CURSTR in ${GAS_OBJSTR}; do

#	5. If the current value doesn't contain a hex glob (pattern matching),
#	   it must be a "offset* value"
	if [ "${GAS_CURSTR:0:2}" != "0x" ]; then

#		6. Save current value and repeat step 5 on the next value
		GAS_OFFSET=${GAS_CURSTR}
	else

#		7. Print current values and repeat steps 5-7 on the next value
#		   until EOL is reached
		echo "%define ${GAS_OFFSET} (${GAS_CURSTR})" >> "$NASM_FILE"
		echo "#define ${GAS_OFFSET} (${GAS_CURSTR})" >> "$GAS_FILE"
	fi
done
