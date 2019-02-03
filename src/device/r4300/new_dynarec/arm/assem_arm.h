#ifndef M64P_DEVICE_R4300_NEW_DYNAREC_ARM_ASSEM_ARM_H
#define M64P_DEVICE_R4300_NEW_DYNAREC_ARM_ASSEM_ARM_H

#define HOST_REGS 13
#define HOST_CCREG 10
#define HOST_BTREG 8
#define EXCLUDE_REG 11

#define HOST_IMM8 1
#define HAVE_CMOV_IMM 1
#define CORTEX_A8_BRANCH_PREDICTION_HACK 1
#define USE_MINI_HT 1
//#define REG_PREFETCH 1
#define HAVE_CONDITIONAL_CALL 1
#define RAM_OFFSET 1

/* ARM calling convention:
   r0-r3, r12: caller-save
   r4-r11: callee-save */

#define ARG1_REG 0
#define ARG2_REG 1
#define ARG3_REG 2
#define ARG4_REG 3

/* GCC register naming convention:
   r10 = sl (base)
   r11 = fp (frame pointer)
   r12 = ip (scratch)
   r13 = sp (stack pointer)
   r14 = lr (link register)
   r15 = pc (program counter) */

#define FP 11
#define LR 14
#define CALLER_SAVED_REGS 0x100f
#define HOST_TEMPREG 14

// Note: FP is set to &dynarec_local when executing generated code.
// Thus the local variables are actually global and not on the stack.

#define TARGET_SIZE_2 25 // 2^25 = 32 megabytes
#define JUMP_TABLE_SIZE (sizeof(jump_table_symbols)*2)

#endif /* M64P_DEVICE_R4300_NEW_DYNAREC_ARM_ASSEM_ARM_H */
