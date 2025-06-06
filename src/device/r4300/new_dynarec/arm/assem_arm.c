/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - assem_arm.c                                             *
 *   Copyright (C) 2009-2011 Ari64                                         *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* ease access to fp-addressed variables */
#define fp_cycle_count         (offsetof(struct new_dynarec_hot_state, cycle_count))
#define fp_invc_ptr            (offsetof(struct new_dynarec_hot_state, invc_ptr))
#define fp_fcr31               (offsetof(struct new_dynarec_hot_state, cp1_fcr31))
#define fp_regs                (offsetof(struct new_dynarec_hot_state, regs))
#define fp_hi                  (offsetof(struct new_dynarec_hot_state, hi))
#define fp_lo                  (offsetof(struct new_dynarec_hot_state, lo))
#define fp_cp0_regs(x)         ((offsetof(struct new_dynarec_hot_state, cp0_regs)) + (x)*sizeof(uint32_t))
#define fp_rounding_modes      (offsetof(struct new_dynarec_hot_state, rounding_modes))
#define fp_fake_pc             (offsetof(struct new_dynarec_hot_state, fake_pc))
#define fp_ram_offset          (offsetof(struct new_dynarec_hot_state, ram_offset))
#define fp_mini_ht(x,y)        ((offsetof(struct new_dynarec_hot_state, mini_ht)) + 4*((y)*32+(x)))
#define fp_memory_map          (offsetof(struct new_dynarec_hot_state, memory_map))

void jump_vaddr_r0(void);
void jump_vaddr_r1(void);
void jump_vaddr_r2(void);
void jump_vaddr_r3(void);
void jump_vaddr_r4(void);
void jump_vaddr_r5(void);
void jump_vaddr_r6(void);
void jump_vaddr_r7(void);
void jump_vaddr_r8(void);
void jump_vaddr_r9(void);
void jump_vaddr_r10(void);
void jump_vaddr_r12(void);
void invalidate_addr_r0(void);
void invalidate_addr_r1(void);
void invalidate_addr_r2(void);
void invalidate_addr_r3(void);
void invalidate_addr_r4(void);
void invalidate_addr_r5(void);
void invalidate_addr_r6(void);
void invalidate_addr_r7(void);
void invalidate_addr_r8(void);
void invalidate_addr_r9(void);
void invalidate_addr_r10(void);
void invalidate_addr_r12(void);
void breakpoint(void);
static void invalidate_addr(u_int addr);

static u_int literals[1024][2];
static unsigned int needs_clear_cache[1<<(TARGET_SIZE_2-17)];

static const u_int jump_vaddr_reg[16] = {
  (int)jump_vaddr_r0,
  (int)jump_vaddr_r1,
  (int)jump_vaddr_r2,
  (int)jump_vaddr_r3,
  (int)jump_vaddr_r4,
  (int)jump_vaddr_r5,
  (int)jump_vaddr_r6,
  (int)jump_vaddr_r7,
  (int)jump_vaddr_r8,
  (int)jump_vaddr_r9,
  (int)jump_vaddr_r10,
  (int)breakpoint,
  (int)jump_vaddr_r12,
  (int)breakpoint,
  (int)breakpoint,
  (int)breakpoint};

static const u_int invalidate_addr_reg[16] = {
  (int)invalidate_addr_r0,
  (int)invalidate_addr_r1,
  (int)invalidate_addr_r2,
  (int)invalidate_addr_r3,
  (int)invalidate_addr_r4,
  (int)invalidate_addr_r5,
  (int)invalidate_addr_r6,
  (int)invalidate_addr_r7,
  (int)invalidate_addr_r8,
  (int)invalidate_addr_r9,
  (int)invalidate_addr_r10,
  (int)breakpoint,
  (int)invalidate_addr_r12,
  (int)breakpoint,
  (int)breakpoint,
  (int)breakpoint};

static u_int jump_table_symbols[] = {
  (int)NULL /*TLBR*/,
  (int)NULL /*TLBP*/,
  (int)NULL /*MULT*/,
  (int)NULL /*MULTU*/,
  (int)NULL /*DIV*/,
  (int)NULL /*DIVU*/,
  (int)NULL /*DMULT*/,
  (int)NULL /*DMULTU*/,
  (int)NULL /*DDIV*/,
  (int)NULL /*DDIVU*/,
  (int)invalidate_addr,
  (int)dyna_linker,
  (int)dyna_linker_ds,
  (int)verify_code,
  (int)cc_interrupt,
  (int)fp_exception,
  (int)jump_syscall,
  (int)jump_eret,
  (int)do_interrupt,
  (int)TLBWI_new,
  (int)TLBWR_new,
  (int)MFC0_new,
  (int)MTC0_new,
  (int)jump_vaddr_r0,
  (int)jump_vaddr_r1,
  (int)jump_vaddr_r2,
  (int)jump_vaddr_r3,
  (int)jump_vaddr_r4,
  (int)jump_vaddr_r5,
  (int)jump_vaddr_r6,
  (int)jump_vaddr_r7,
  (int)jump_vaddr_r8,
  (int)jump_vaddr_r9,
  (int)jump_vaddr_r10,
  (int)jump_vaddr_r12,
  (int)invalidate_addr_r0,
  (int)invalidate_addr_r1,
  (int)invalidate_addr_r2,
  (int)invalidate_addr_r3,
  (int)invalidate_addr_r4,
  (int)invalidate_addr_r5,
  (int)invalidate_addr_r6,
  (int)invalidate_addr_r7,
  (int)invalidate_addr_r8,
  (int)invalidate_addr_r9,
  (int)invalidate_addr_r10,
  (int)invalidate_addr_r12,
  (int)cvt_s_w,
  (int)cvt_d_w,
  (int)cvt_s_l,
  (int)cvt_d_l,
  (int)cvt_w_s,
  (int)cvt_w_d,
  (int)cvt_l_s,
  (int)cvt_l_d,
  (int)cvt_d_s,
  (int)cvt_s_d,
  (int)round_l_s,
  (int)round_w_s,
  (int)trunc_l_s,
  (int)trunc_w_s,
  (int)ceil_l_s,
  (int)ceil_w_s,
  (int)floor_l_s,
  (int)floor_w_s,
  (int)round_l_d,
  (int)round_w_d,
  (int)trunc_l_d,
  (int)trunc_w_d,
  (int)ceil_l_d,
  (int)ceil_w_d,
  (int)floor_l_d,
  (int)floor_w_d,
  (int)c_f_s,
  (int)c_un_s,
  (int)c_eq_s,
  (int)c_ueq_s,
  (int)c_olt_s,
  (int)c_ult_s,
  (int)c_ole_s,
  (int)c_ule_s,
  (int)c_sf_s,
  (int)c_ngle_s,
  (int)c_seq_s,
  (int)c_ngl_s,
  (int)c_lt_s,
  (int)c_nge_s,
  (int)c_le_s,
  (int)c_ngt_s,
  (int)c_f_d,
  (int)c_un_d,
  (int)c_eq_d,
  (int)c_ueq_d,
  (int)c_olt_d,
  (int)c_ult_d,
  (int)c_ole_d,
  (int)c_ule_d,
  (int)c_sf_d,
  (int)c_ngle_d,
  (int)c_seq_d,
  (int)c_ngl_d,
  (int)c_lt_d,
  (int)c_nge_d,
  (int)c_le_d,
  (int)c_ngt_d,
  (int)add_s,
  (int)sub_s,
  (int)mul_s,
  (int)div_s,
  (int)sqrt_s,
  (int)abs_s,
  (int)mov_s,
  (int)neg_s,
  (int)add_d,
  (int)sub_d,
  (int)mul_d,
  (int)div_d,
  (int)sqrt_d,
  (int)abs_d,
  (int)mov_d,
  (int)neg_d,
  (int)read_byte_new,
  (int)read_hword_new,
  (int)read_word_new,
  (int)read_dword_new,
  (int)write_byte_new,
  (int)write_hword_new,
  (int)write_word_new,
  (int)write_dword_new,
  (int)LWL_new,
  (int)LWR_new,
  (int)LDL_new,
  (int)LDR_new,
  (int)SWL_new,
  (int)SWR_new,
  (int)SDL_new,
  (int)SDR_new,
  (int)breakpoint
};

static void cache_flush(char* start, char* end)
{
    __clear_cache(start, end);
}

/* Linker */
static void set_jump_target(int addr,u_int target)
{
  u_char *ptr=(u_char *)addr;
  u_int *ptr2=(u_int *)ptr;
  if(ptr==NULL) return;
  if(ptr[3]==0xe2) {
    assert((target-(u_int)ptr2-8)<1024);
    assert((addr&3)==0);
    assert((target&3)==0);
    *ptr2=(*ptr2&0xFFFFF000)|((target-(u_int)ptr2-8)>>2)|0xF00;
    //DebugMessage(M64MSG_VERBOSE, "target=%x addr=%x insn=%x",target,addr,*ptr2);
  }
  else if(ptr[3]==0x72) {
    // generated by emit_jno_unlikely
    if((target-(u_int)ptr2-8)<1024) {
      assert((addr&3)==0);
      assert((target&3)==0);
      *ptr2=(*ptr2&0xFFFFF000)|((target-(u_int)ptr2-8)>>2)|0xF00;
    }
    else if((target-(u_int)ptr2-8)<4096&&!((target-(u_int)ptr2-8)&15)) {
      assert((addr&3)==0);
      assert((target&3)==0);
      *ptr2=(*ptr2&0xFFFFF000)|((target-(u_int)ptr2-8)>>4)|0xE00;
    }
    else *ptr2=(0x7A000000)|(((target-(u_int)ptr2-8)<<6)>>8);
  }
  else {
    assert((ptr[3]&0x0e)==0xa);
    *ptr2=(*ptr2&0xFF000000)|(((target-(u_int)ptr2-8)<<6)>>8);
  }
}

// This optionally copies the instruction from the target of the branch into
// the space before the branch.  Works, but the difference in speed is
// usually insignificant.
/*
static void set_jump_target_fillslot(int addr,u_int target,int copy)
{
  u_char *ptr=(u_char *)addr;
  u_int *ptr2=(u_int *)ptr;
  assert(!copy||ptr2[-1]==0xe28dd000);
  if(ptr[3]==0xe2) {
    assert(!copy);
    assert((target-(u_int)ptr2-8)<4096);
    *ptr2=(*ptr2&0xFFFFF000)|(target-(u_int)ptr2-8);
  }
  else {
    assert((ptr[3]&0x0e)==0xa);
    u_int target_insn=*(u_int *)target;
    if((target_insn&0x0e100000)==0) { // ALU, no immediate, no flags
      copy=0;
    }
    if((target_insn&0x0c100000)==0x04100000) { // Load
      copy=0;
    }
    if(target_insn&0x08000000) {
      copy=0;
    }
    if(copy) {
      ptr2[-1]=target_insn;
      target+=4;
    }
    *ptr2=(*ptr2&0xFF000000)|(((target-(u_int)ptr2-8)<<6)>>8);
  }
}
*/

/* Literal pool */
static void add_literal(int addr,int val)
{
  literals[literalcount][0]=addr;
  literals[literalcount][1]=val;
  literalcount++;
}

static void *add_pointer(void *src, void* addr)
{
  int *ptr=(int*)src;
  assert((*ptr&0x0f000000)==0x0a000000); //jmp
  int offset=(int)(((u_int)*ptr+2)<<8)>>6;
  void *ptr2=(void*)((u_int)ptr+(u_int)offset);
#ifdef ARMv5_ONLY
  assert((*(int*)((u_int)ptr2)&0x0ff00000)==0x05900000); //ldr
  assert((*(int*)((u_int)ptr2+4)&0x0ff00000)==0x05900000); //ldr
#else
  assert((*(int*)((u_int)ptr2)&0x0ff00000)==0x03000000); //movw
  assert((*(int*)((u_int)ptr2+4)&0x0ff00000)==0x03400000); //movt
  assert((*(int*)((u_int)ptr2+8)&0x0ff00000)==0x03000000); //movw
  assert((*(int*)((u_int)ptr2+12)&0x0ff00000)==0x03400000); //movt
#endif
  *ptr=(*ptr&0xFF000000)|((((u_int)addr-(u_int)ptr-8)<<6)>>8);
  cache_flush((void*)ptr, (void*)((u_int)ptr+4));
  return ptr2;
}

static void *kill_pointer(void *stub)
{
#ifdef ARMv5_ONLY
  int *ptr=(int *)(stub+4);
  assert((*ptr&0x0ff00000)==0x05900000); //ldr
  u_int offset=*ptr&0xfff;
  int **l_ptr=(void *)ptr+offset+8;
  int *i_ptr=*l_ptr;
#else
  int *ptr=(int *)((int)stub+8);
  int *ptr2=(int *)((int)stub+12);
  assert((*ptr&0x0ff00000)==0x03000000); //movw
  assert((*ptr2&0x0ff00000)==0x03400000); //movt
  int *i_ptr=(int*)((*ptr&0xfff)|((*ptr>>4)&0xf000)|((*ptr2&0xfff)<<16)|((*ptr2&0xf0000)<<12));
#endif
  assert((*i_ptr&0x0f000000)==0x0a000000); //jmp
  set_jump_target((int)i_ptr,(int)stub);
  return i_ptr;
}

static int get_pointer(void *stub)
{
#ifdef ARMv5_ONLY
  int *ptr=(int *)(stub+4);
  assert((*ptr&0x0ff00000)==0x05900000); //ldr
  u_int offset=*ptr&0xfff;
  int **l_ptr=(void *)ptr+offset+8;
  int *i_ptr=*l_ptr;
#else
  int *ptr=(int *)((int)stub+8);
  int *ptr2=(int *)((int)stub+12);
  assert((*ptr&0x0ff00000)==0x03000000); //movw
  assert((*ptr2&0x0ff00000)==0x03400000); //movt
  int *i_ptr=(int*)((*ptr&0xfff)|((*ptr>>4)&0xf000)|((*ptr2&0xfff)<<16)|((*ptr2&0xf0000)<<12));
#endif
  assert((*i_ptr&0x0f000000)==0x0a000000); //jmp
  return (int)i_ptr+((*i_ptr<<8)>>6)+8;
}

/* Register allocation */

// Note: registers are allocated clean (unmodified state)
// if you intend to modify the register, you must call dirty_reg().
static void alloc_reg(struct regstat *cur,int i,signed char reg)
{
  int r,hr;
  int preferred_reg = (reg&7);
  if(reg==CCREG) preferred_reg=HOST_CCREG;
  if(reg==PTEMP||reg==FTEMP) preferred_reg=12;

  // Don't allocate unused registers
  if((cur->u>>reg)&1) return;

  // see if it's already allocated
  for(hr=0;hr<HOST_REGS;hr++)
  {
    if(cur->regmap[hr]==reg) return;
  }

  // Keep the same mapping if the register was already allocated in a loop
  preferred_reg = loop_reg(i,reg,preferred_reg);

  // Try to allocate the preferred register
  if(cur->regmap[preferred_reg]==-1) {
    cur->regmap[preferred_reg]=reg;
    cur->dirty&=~(1<<preferred_reg);
    cur->isconst&=~(1<<preferred_reg);
    return;
  }
  r=cur->regmap[preferred_reg];
  if(r<64&&((cur->u>>r)&1)) {
    cur->regmap[preferred_reg]=reg;
    cur->dirty&=~(1<<preferred_reg);
    cur->isconst&=~(1<<preferred_reg);
    return;
  }
  if(r>=64&&((cur->uu>>(r&63))&1)) {
    cur->regmap[preferred_reg]=reg;
    cur->dirty&=~(1<<preferred_reg);
    cur->isconst&=~(1<<preferred_reg);
    return;
  }

  // Clear any unneeded registers
  // We try to keep the mapping consistent, if possible, because it
  // makes branches easier (especially loops).  So we try to allocate
  // first (see above) before removing old mappings.  If this is not
  // possible then go ahead and clear out the registers that are no
  // longer needed.
  for(hr=0;hr<HOST_REGS;hr++)
  {
    r=cur->regmap[hr];
    if(r>=0) {
      if(r<64) {
        if((cur->u>>r)&1) {cur->regmap[hr]=-1;break;}
      }
      else
      {
        if((cur->uu>>(r&63))&1) {cur->regmap[hr]=-1;break;}
      }
    }
  }
  // Try to allocate any available register, but prefer
  // registers that have not been used recently.
  if(i>0) {
    for(hr=0;hr<HOST_REGS;hr++) {
      if(hr!=EXCLUDE_REG&&cur->regmap[hr]==-1) {
        if(regs[i-1].regmap[hr]!=rs1[i-1]&&regs[i-1].regmap[hr]!=rs2[i-1]&&regs[i-1].regmap[hr]!=rt1[i-1]&&regs[i-1].regmap[hr]!=rt2[i-1]) {
          cur->regmap[hr]=reg;
          cur->dirty&=~(1<<hr);
          cur->isconst&=~(1<<hr);
          return;
        }
      }
    }
  }
  // Try to allocate any available register
  for(hr=0;hr<HOST_REGS;hr++) {
    if(hr!=EXCLUDE_REG&&cur->regmap[hr]==-1) {
      cur->regmap[hr]=reg;
      cur->dirty&=~(1<<hr);
      cur->isconst&=~(1<<hr);
      return;
    }
  }

  // Ok, now we have to evict someone
  // Pick a register we hopefully won't need soon
  u_char hsn[MAXREG+1];
  memset(hsn,10,sizeof(hsn));
  int j;
  lsn(hsn,i,&preferred_reg);
  if(i>0) {
    // Don't evict the cycle count at entry points, otherwise the entry
    // stub will have to write it.
    if(bt[i]&&hsn[CCREG]>2) hsn[CCREG]=2;
    if(i>1&&hsn[CCREG]>2&&(itype[i-2]==RJUMP||itype[i-2]==UJUMP||itype[i-2]==CJUMP||itype[i-2]==SJUMP||itype[i-2]==FJUMP)) hsn[CCREG]=2;
    for(j=10;j>=3;j--)
    {
      // Alloc preferred register if available
      if(hsn[r=cur->regmap[preferred_reg]&63]==j) {
        for(hr=0;hr<HOST_REGS;hr++) {
          // Evict both parts of a 64-bit register
          if((cur->regmap[hr]&63)==r) {
            cur->regmap[hr]=-1;
            cur->dirty&=~(1<<hr);
            cur->isconst&=~(1<<hr);
          }
        }
        cur->regmap[preferred_reg]=reg;
        return;
      }
      for(r=1;r<=MAXREG;r++)
      {
        if(hsn[r]==j&&r!=rs1[i-1]&&r!=rs2[i-1]&&r!=rt1[i-1]&&r!=rt2[i-1]) {
          for(hr=0;hr<HOST_REGS;hr++) {
            if(hr!=HOST_CCREG||j<hsn[CCREG]) {
              if(cur->regmap[hr]==r+64) {
                cur->regmap[hr]=reg;
                cur->dirty&=~(1<<hr);
                cur->isconst&=~(1<<hr);
                return;
              }
            }
          }
          for(hr=0;hr<HOST_REGS;hr++) {
            if(hr!=HOST_CCREG||j<hsn[CCREG]) {
              if(cur->regmap[hr]==r) {
                cur->regmap[hr]=reg;
                cur->dirty&=~(1<<hr);
                cur->isconst&=~(1<<hr);
                return;
              }
            }
          }
        }
      }
    }
  }
  for(j=10;j>=0;j--)
  {
    for(r=1;r<=MAXREG;r++)
    {
      if(hsn[r]==j) {
        for(hr=0;hr<HOST_REGS;hr++) {
          if(cur->regmap[hr]==r+64) {
            cur->regmap[hr]=reg;
            cur->dirty&=~(1<<hr);
            cur->isconst&=~(1<<hr);
            return;
          }
        }
        for(hr=0;hr<HOST_REGS;hr++) {
          if(cur->regmap[hr]==r) {
            cur->regmap[hr]=reg;
            cur->dirty&=~(1<<hr);
            cur->isconst&=~(1<<hr);
            return;
          }
        }
      }
    }
  }
  DebugMessage(M64MSG_ERROR, "This shouldn't happen (alloc_reg)");exit(1);
}

static void alloc_reg64(struct regstat *cur,int i,signed char reg)
{
  int preferred_reg = 8+(reg&1);
  int r,hr;

  // allocate the lower 32 bits
  alloc_reg(cur,i,reg);

  // Don't allocate unused registers
  if((cur->uu>>reg)&1) return;

  // see if the upper half is already allocated
  for(hr=0;hr<HOST_REGS;hr++)
  {
    if(cur->regmap[hr]==reg+64) return;
  }

  // Keep the same mapping if the register was already allocated in a loop
  preferred_reg = loop_reg(i,reg,preferred_reg);

  // Try to allocate the preferred register
  if(cur->regmap[preferred_reg]==-1) {
    cur->regmap[preferred_reg]=reg|64;
    cur->dirty&=~(1<<preferred_reg);
    cur->isconst&=~(1<<preferred_reg);
    return;
  }
  r=cur->regmap[preferred_reg];
  if(r<64&&((cur->u>>r)&1)) {
    cur->regmap[preferred_reg]=reg|64;
    cur->dirty&=~(1<<preferred_reg);
    cur->isconst&=~(1<<preferred_reg);
    return;
  }
  if(r>=64&&((cur->uu>>(r&63))&1)) {
    cur->regmap[preferred_reg]=reg|64;
    cur->dirty&=~(1<<preferred_reg);
    cur->isconst&=~(1<<preferred_reg);
    return;
  }

  // Clear any unneeded registers
  // We try to keep the mapping consistent, if possible, because it
  // makes branches easier (especially loops).  So we try to allocate
  // first (see above) before removing old mappings.  If this is not
  // possible then go ahead and clear out the registers that are no
  // longer needed.
  for(hr=HOST_REGS-1;hr>=0;hr--)
  {
    r=cur->regmap[hr];
    if(r>=0) {
      if(r<64) {
        if((cur->u>>r)&1) {cur->regmap[hr]=-1;break;}
      }
      else
      {
        if((cur->uu>>(r&63))&1) {cur->regmap[hr]=-1;break;}
      }
    }
  }
  // Try to allocate any available register, but prefer
  // registers that have not been used recently.
  if(i>0) {
    for(hr=0;hr<HOST_REGS;hr++) {
      if(hr!=EXCLUDE_REG&&cur->regmap[hr]==-1) {
        if(regs[i-1].regmap[hr]!=rs1[i-1]&&regs[i-1].regmap[hr]!=rs2[i-1]&&regs[i-1].regmap[hr]!=rt1[i-1]&&regs[i-1].regmap[hr]!=rt2[i-1]) {
          cur->regmap[hr]=reg|64;
          cur->dirty&=~(1<<hr);
          cur->isconst&=~(1<<hr);
          return;
        }
      }
    }
  }
  // Try to allocate any available register
  for(hr=0;hr<HOST_REGS;hr++) {
    if(hr!=EXCLUDE_REG&&cur->regmap[hr]==-1) {
      cur->regmap[hr]=reg|64;
      cur->dirty&=~(1<<hr);
      cur->isconst&=~(1<<hr);
      return;
    }
  }

  // Ok, now we have to evict someone
  // Pick a register we hopefully won't need soon
  u_char hsn[MAXREG+1];
  memset(hsn,10,sizeof(hsn));
  int j;
  lsn(hsn,i,&preferred_reg);
  if(i>0) {
    // Don't evict the cycle count at entry points, otherwise the entry
    // stub will have to write it.
    if(bt[i]&&hsn[CCREG]>2) hsn[CCREG]=2;
    if(i>1&&hsn[CCREG]>2&&(itype[i-2]==RJUMP||itype[i-2]==UJUMP||itype[i-2]==CJUMP||itype[i-2]==SJUMP||itype[i-2]==FJUMP)) hsn[CCREG]=2;
    for(j=10;j>=3;j--)
    {
      // Alloc preferred register if available
      if(hsn[r=cur->regmap[preferred_reg]&63]==j) {
        for(hr=0;hr<HOST_REGS;hr++) {
          // Evict both parts of a 64-bit register
          if((cur->regmap[hr]&63)==r) {
            cur->regmap[hr]=-1;
            cur->dirty&=~(1<<hr);
            cur->isconst&=~(1<<hr);
          }
        }
        cur->regmap[preferred_reg]=reg|64;
        return;
      }
      for(r=1;r<=MAXREG;r++)
      {
        if(hsn[r]==j&&r!=rs1[i-1]&&r!=rs2[i-1]&&r!=rt1[i-1]&&r!=rt2[i-1]) {
          for(hr=0;hr<HOST_REGS;hr++) {
            if(hr!=HOST_CCREG||j<hsn[CCREG]) {
              if(cur->regmap[hr]==r+64) {
                cur->regmap[hr]=reg|64;
                cur->dirty&=~(1<<hr);
                cur->isconst&=~(1<<hr);
                return;
              }
            }
          }
          for(hr=0;hr<HOST_REGS;hr++) {
            if(hr!=HOST_CCREG||j<hsn[CCREG]) {
              if(cur->regmap[hr]==r) {
                cur->regmap[hr]=reg|64;
                cur->dirty&=~(1<<hr);
                cur->isconst&=~(1<<hr);
                return;
              }
            }
          }
        }
      }
    }
  }
  for(j=10;j>=0;j--)
  {
    for(r=1;r<=MAXREG;r++)
    {
      if(hsn[r]==j) {
        for(hr=0;hr<HOST_REGS;hr++) {
          if(cur->regmap[hr]==r+64) {
            cur->regmap[hr]=reg|64;
            cur->dirty&=~(1<<hr);
            cur->isconst&=~(1<<hr);
            return;
          }
        }
        for(hr=0;hr<HOST_REGS;hr++) {
          if(cur->regmap[hr]==r) {
            cur->regmap[hr]=reg|64;
            cur->dirty&=~(1<<hr);
            cur->isconst&=~(1<<hr);
            return;
          }
        }
      }
    }
  }
  DebugMessage(M64MSG_ERROR, "This shouldn't happen");exit(1);
}

// Allocate a temporary register.  This is done without regard to
// dirty status or whether the register we request is on the unneeded list
// Note: This will only allocate one register, even if called multiple times
static void alloc_reg_temp(struct regstat *cur,int i,signed char reg)
{
  int r,hr;
  int preferred_reg = -1;

  // see if it's already allocated
  for(hr=0;hr<HOST_REGS;hr++)
  {
    if(hr!=EXCLUDE_REG&&cur->regmap[hr]==reg) return;
  }

  // Try to allocate any available register
  for(hr=HOST_REGS-1;hr>=0;hr--) {
    if(hr!=EXCLUDE_REG&&cur->regmap[hr]==-1) {
      cur->regmap[hr]=reg;
      cur->dirty&=~(1<<hr);
      cur->isconst&=~(1<<hr);
      return;
    }
  }

  // Find an unneeded register
  for(hr=HOST_REGS-1;hr>=0;hr--)
  {
    r=cur->regmap[hr];
    if(r>=0) {
      if(r<64) {
        if((cur->u>>r)&1) {
          if(i==0||((unneeded_reg[i-1]>>r)&1)) {
            cur->regmap[hr]=reg;
            cur->dirty&=~(1<<hr);
            cur->isconst&=~(1<<hr);
            return;
          }
        }
      }
      else
      {
        if((cur->uu>>(r&63))&1) {
          if(i==0||((unneeded_reg_upper[i-1]>>(r&63))&1)) {
            cur->regmap[hr]=reg;
            cur->dirty&=~(1<<hr);
            cur->isconst&=~(1<<hr);
            return;
          }
        }
      }
    }
  }

  // Ok, now we have to evict someone
  // Pick a register we hopefully won't need soon
  // TODO: we might want to follow unconditional jumps here
  // TODO: get rid of dupe code and make this into a function
  u_char hsn[MAXREG+1];
  memset(hsn,10,sizeof(hsn));
  int j;
  lsn(hsn,i,&preferred_reg);
  if(i>0) {
    // Don't evict the cycle count at entry points, otherwise the entry
    // stub will have to write it.
    if(bt[i]&&hsn[CCREG]>2) hsn[CCREG]=2;
    if(i>1&&hsn[CCREG]>2&&(itype[i-2]==RJUMP||itype[i-2]==UJUMP||itype[i-2]==CJUMP||itype[i-2]==SJUMP||itype[i-2]==FJUMP)) hsn[CCREG]=2;
    for(j=10;j>=3;j--)
    {
      for(r=1;r<=MAXREG;r++)
      {
        if(hsn[r]==j&&r!=rs1[i-1]&&r!=rs2[i-1]&&r!=rt1[i-1]&&r!=rt2[i-1]) {
          for(hr=0;hr<HOST_REGS;hr++) {
            if(hr!=HOST_CCREG||hsn[CCREG]>2) {
              if(cur->regmap[hr]==r+64) {
                cur->regmap[hr]=reg;
                cur->dirty&=~(1<<hr);
                cur->isconst&=~(1<<hr);
                return;
              }
            }
          }
          for(hr=0;hr<HOST_REGS;hr++) {
            if(hr!=HOST_CCREG||hsn[CCREG]>2) {
              if(cur->regmap[hr]==r) {
                cur->regmap[hr]=reg;
                cur->dirty&=~(1<<hr);
                cur->isconst&=~(1<<hr);
                return;
              }
            }
          }
        }
      }
    }
  }
  for(j=10;j>=0;j--)
  {
    for(r=1;r<=MAXREG;r++)
    {
      if(hsn[r]==j) {
        for(hr=0;hr<HOST_REGS;hr++) {
          if(cur->regmap[hr]==r+64) {
            cur->regmap[hr]=reg;
            cur->dirty&=~(1<<hr);
            cur->isconst&=~(1<<hr);
            return;
          }
        }
        for(hr=0;hr<HOST_REGS;hr++) {
          if(cur->regmap[hr]==r) {
            cur->regmap[hr]=reg;
            cur->dirty&=~(1<<hr);
            cur->isconst&=~(1<<hr);
            return;
          }
        }
      }
    }
  }
  DebugMessage(M64MSG_ERROR, "This shouldn't happen");exit(1);
}
// Allocate a specific ARM register.
static void alloc_arm_reg(struct regstat *cur,int i,signed char reg,int hr)
{
  int n;
  int dirty=0;

  // see if it's already allocated (and dealloc it)
  for(n=0;n<HOST_REGS;n++)
  {
    if(n!=EXCLUDE_REG&&cur->regmap[n]==reg) {
      dirty=(cur->dirty>>n)&1;
      cur->regmap[n]=-1;
    }
  }

  cur->regmap[hr]=reg;
  cur->dirty&=~(1<<hr);
  cur->dirty|=dirty<<hr;
  cur->isconst&=~(1<<hr);
}

// Alloc cycle count into dedicated register
static void alloc_cc(struct regstat *cur,int i)
{
  alloc_arm_reg(cur,i,CCREG,HOST_CCREG);
}

/* Special alloc */


/* Assembler */

static char regname[16][4] = {
 "r0",
 "r1",
 "r2",
 "r3",
 "r4",
 "r5",
 "r6",
 "r7",
 "r8",
 "r9",
 "r10",
 "fp",
 "r12",
 "sp",
 "lr",
 "pc"};

static void output_byte(u_char byte)
{
  *(out++)=byte;
}
static void output_modrm(u_char mod,u_char rm,u_char ext)
{
  assert(mod<4);
  assert(rm<8);
  assert(ext<8);
  u_char byte=(mod<<6)|(ext<<3)|rm;
  *(out++)=byte;
}

static void output_w32(u_int word)
{
  *((u_int *)out)=word;
  out+=4;
}
static u_int rd_rn_rm(u_int rd, u_int rn, u_int rm)
{
  assert(rd<16);
  assert(rn<16);
  assert(rm<16);
  return((rn<<16)|(rd<<12)|rm);
}
static u_int rd_rn_imm_shift(u_int rd, u_int rn, u_int imm, u_int shift)
{
  assert(rd<16);
  assert(rn<16);
  assert(imm<256);
  assert((shift&1)==0);
  return((rn<<16)|(rd<<12)|(((32-shift)&30)<<7)|imm);
}
static u_int genimm(u_int imm,u_int *encoded)
{
  if(imm==0) {*encoded=0;return 1;}
  int i=32;
  while(i>0)
  {
    if(imm<256) {
      *encoded=((i&30)<<7)|imm;
      return 1;
    }
    imm=(imm>>2)|(imm<<30);i-=2;
  }
  return 0;
}
static u_int genjmp(u_int addr)
{
  if(addr<4) return 0;
  int offset=addr-(int)out-8;
  if(offset<-33554432||offset>=33554432) {
    int n;
    for (n=0;n<sizeof(jump_table_symbols)/4;n++)
    {
      if(addr==jump_table_symbols[n])
      {
        offset=(int)base_addr+(1<<TARGET_SIZE_2)-JUMP_TABLE_SIZE+n*8-(int)out-8;
        break;
      }
    }
  }
  assert(offset>=-33554432&&offset<33554432);
  return ((u_int)offset>>2)&0xffffff;
}

static void emit_mov(int rs,int rt)
{
  assem_debug("mov %s,%s",regname[rt],regname[rs]);
  output_w32(0xe1a00000|rd_rn_rm(rt,0,rs));
}

static void emit_movs(int rs,int rt)
{
  assem_debug("movs %s,%s",regname[rt],regname[rs]);
  output_w32(0xe1b00000|rd_rn_rm(rt,0,rs));
}

static void emit_add(int rs1,int rs2,int rt)
{
  assem_debug("add %s,%s,%s",regname[rt],regname[rs1],regname[rs2]);
  output_w32(0xe0800000|rd_rn_rm(rt,rs1,rs2));
}

static void emit_addne(int rs1,int rs2,int rt)
{
  assem_debug("addne %s,%s,%s",regname[rt],regname[rs1],regname[rs2]);
  output_w32(0x12800000|rd_rn_rm(rt,rs1,rs2));
}

static void emit_adcsarimm(int rs1,int rs2,int rt,int imm)
{
  assert(imm>0);
  assert(imm<32);
  assem_debug("adc %s,%s,%s,ASR#%d",regname[rt],regname[rs1],regname[rs2],imm);
  output_w32(0xe0a00000|rd_rn_rm(rt,rs1,rs2)|0x40|(imm<<7));
}

static void emit_adds(int rs1,int rs2,int rt)
{
  assem_debug("adds %s,%s,%s",regname[rt],regname[rs1],regname[rs2]);
  output_w32(0xe0900000|rd_rn_rm(rt,rs1,rs2));
}

static void emit_adc(int rs1,int rs2,int rt)
{
  assem_debug("adc %s,%s,%s",regname[rt],regname[rs1],regname[rs2]);
  output_w32(0xe0a00000|rd_rn_rm(rt,rs1,rs2));
}

static void emit_adcs(int rs1,int rs2,int rt)
{
  assem_debug("adcs %s,%s,%s",regname[rt],regname[rs1],regname[rs2]);
  output_w32(0xe0b00000|rd_rn_rm(rt,rs1,rs2));
}

static void emit_sbc(int rs1,int rs2,int rt)
{
  assem_debug("sbc %s,%s,%s",regname[rt],regname[rs1],regname[rs2]);
  output_w32(0xe0c00000|rd_rn_rm(rt,rs1,rs2));
}

static void emit_sbcs(int rs1,int rs2,int rt)
{
  assem_debug("sbcs %s,%s,%s",regname[rt],regname[rs1],regname[rs2]);
  output_w32(0xe0d00000|rd_rn_rm(rt,rs1,rs2));
}

static void emit_neg(int rs, int rt)
{
  assem_debug("rsb %s,%s,#0",regname[rt],regname[rs]);
  output_w32(0xe2600000|rd_rn_rm(rt,rs,0));
}

static void emit_negs(int rs, int rt)
{
  assem_debug("rsbs %s,%s,#0",regname[rt],regname[rs]);
  output_w32(0xe2700000|rd_rn_rm(rt,rs,0));
}

static void emit_sub(int rs1,int rs2,int rt)
{
  assem_debug("sub %s,%s,%s",regname[rt],regname[rs1],regname[rs2]);
  output_w32(0xe0400000|rd_rn_rm(rt,rs1,rs2));
}

static void emit_subs(int rs1,int rs2,int rt)
{
  assem_debug("subs %s,%s,%s",regname[rt],regname[rs1],regname[rs2]);
  output_w32(0xe0500000|rd_rn_rm(rt,rs1,rs2));
}

static void emit_zeroreg(int rt)
{
  assem_debug("mov %s,#0",regname[rt]);
  output_w32(0xe3a00000|rd_rn_rm(rt,0,0));
}

static void emit_loadlp(u_int imm,u_int rt)
{
  add_literal((int)out,imm);
  assem_debug("ldr %s,pc+? [=%x]",regname[rt],imm);
  output_w32(0xe5900000|rd_rn_rm(rt,15,0));
}
static void emit_movw(u_int imm,u_int rt)
{
  assert(imm<65536);
  assem_debug("movw %s,#%d (0x%x)",regname[rt],imm,imm);
  output_w32(0xe3000000|rd_rn_rm(rt,0,0)|(imm&0xfff)|((imm<<4)&0xf0000));
}
static void emit_movt(u_int imm,u_int rt)
{
  assem_debug("movt %s,#%d (0x%x)",regname[rt],imm&0xffff0000,imm&0xffff0000);
  output_w32(0xe3400000|rd_rn_rm(rt,0,0)|((imm>>16)&0xfff)|((imm>>12)&0xf0000));
}
static void emit_movimm(u_int imm,u_int rt)
{
  u_int armval;
  if(genimm(imm,&armval)) {
    assem_debug("mov %s,#%d",regname[rt],imm);
    output_w32(0xe3a00000|rd_rn_rm(rt,0,0)|armval);
  }else if(genimm(~imm,&armval)) {
    assem_debug("mvn %s,#%d",regname[rt],imm);
    output_w32(0xe3e00000|rd_rn_rm(rt,0,0)|armval);
  }else if(imm<65536) {
    #ifdef ARMv5_ONLY
    assem_debug("mov %s,#%d",regname[rt],imm&0xFF00);
    output_w32(0xe3a00000|rd_rn_imm_shift(rt,0,imm>>8,8));
    assem_debug("add %s,%s,#%d",regname[rt],regname[rt],imm&0xFF);
    output_w32(0xe2800000|rd_rn_imm_shift(rt,rt,imm&0xff,0));
    #else
    emit_movw(imm,rt);
    #endif
  }else{
    #ifdef ARMv5_ONLY
    emit_loadlp(imm,rt);
    #else
    emit_movw(imm&0x0000FFFF,rt);
    emit_movt(imm&0xFFFF0000,rt);
    #endif
  }
}
static void emit_pcreladdr(u_int rt)
{
  assem_debug("add %s,pc,#?",regname[rt]);
  output_w32(0xe2800000|rd_rn_rm(rt,15,0));
}

static void emit_loadreg(int r, int hr)
{
  if((r&63)==0)
    emit_zeroreg(hr);
  else if(r==MMREG)
    emit_movimm(fp_memory_map>>2,hr);
  else {
    u_int offset = fp_regs+((r&63)<<3)+((r&64)>>4);
    if((r&63)==HIREG) offset=fp_hi+((r&64)>>4);
    if((r&63)==LOREG) offset=fp_lo+((r&64)>>4);
    if(r==CCREG) offset=fp_cycle_count;
    if(r==CSREG) offset=fp_cp0_regs(CP0_STATUS_REG);
    if(r==FSREG) offset=fp_fcr31;
    if(r==INVCP) offset=fp_invc_ptr;
    if(r==ROREG) offset=fp_ram_offset;
    assert(offset<4096);
    assem_debug("ldr %s,fp+%d",regname[hr],offset);
    output_w32(0xe5900000|rd_rn_rm(hr,FP,0)|offset);
  }
}
static void emit_storereg(int r, int hr)
{
  u_int offset = fp_regs+((r&63)<<3)+((r&64)>>4);
  if((r&63)==HIREG) offset=fp_hi+((r&64)>>4);
  if((r&63)==LOREG) offset=fp_lo+((r&64)>>4);
  if(r==CCREG) offset=fp_cycle_count;
  if(r==FSREG) offset=fp_fcr31;
  assert((r&63)!=CSREG);
  assert((r&63)!=0);
  assert((r&63)<=CCREG);
  assert(offset<4096);
  assem_debug("str %s,fp+%d",regname[hr],offset);
  output_w32(0xe5800000|rd_rn_rm(hr,FP,0)|offset);
}

static void emit_test(int rs, int rt)
{
  assem_debug("tst %s,%s",regname[rs],regname[rt]);
  output_w32(0xe1100000|rd_rn_rm(0,rs,rt));
}

static void emit_testimm(int rs,int imm)
{
  u_int armval, ret;
  assem_debug("tst %s,#%d",regname[rs],imm);
  ret = genimm(imm,&armval);
  assert(ret);
  output_w32(0xe3100000|rd_rn_rm(0,rs,0)|armval);
}

static void emit_not(int rs,int rt)
{
  assem_debug("mvn %s,%s",regname[rt],regname[rs]);
  output_w32(0xe1e00000|rd_rn_rm(rt,0,rs));
}

static void emit_and(u_int rs1,u_int rs2,u_int rt)
{
  assem_debug("and %s,%s,%s",regname[rt],regname[rs1],regname[rs2]);
  output_w32(0xe0000000|rd_rn_rm(rt,rs1,rs2));
}

static void emit_or(u_int rs1,u_int rs2,u_int rt)
{
  assem_debug("orr %s,%s,%s",regname[rt],regname[rs1],regname[rs2]);
  output_w32(0xe1800000|rd_rn_rm(rt,rs1,rs2));
}
static void emit_or_and_set_flags(int rs1,int rs2,int rt)
{
  assem_debug("orrs %s,%s,%s",regname[rt],regname[rs1],regname[rs2]);
  output_w32(0xe1900000|rd_rn_rm(rt,rs1,rs2));
}

static void emit_xor(u_int rs1,u_int rs2,u_int rt)
{
  assem_debug("eor %s,%s,%s",regname[rt],regname[rs1],regname[rs2]);
  output_w32(0xe0200000|rd_rn_rm(rt,rs1,rs2));
}

static void emit_addimm(u_int rs,int imm,u_int rt)
{
  assert(rs<16);
  assert(rt<16);
  if(imm!=0) {
    assert(imm>-65536&&imm<65536);
    u_int armval;
    if(genimm(imm,&armval)) {
      assem_debug("add %s,%s,#%d",regname[rt],regname[rs],imm);
      output_w32(0xe2800000|rd_rn_rm(rt,rs,0)|armval);
    }else if(genimm(-imm,&armval)) {
      assem_debug("sub %s,%s,#%d",regname[rt],regname[rs],imm);
      output_w32(0xe2400000|rd_rn_rm(rt,rs,0)|armval);
    }else if(imm<0) {
      assem_debug("sub %s,%s,#%d",regname[rt],regname[rs],(-imm)&0xFF00);
      assem_debug("sub %s,%s,#%d",regname[rt],regname[rt],(-imm)&0xFF);
      output_w32(0xe2400000|rd_rn_imm_shift(rt,rs,(-imm)>>8,8));
      output_w32(0xe2400000|rd_rn_imm_shift(rt,rt,(-imm)&0xff,0));
    }else{
      assem_debug("add %s,%s,#%d",regname[rt],regname[rs],imm&0xFF00);
      assem_debug("add %s,%s,#%d",regname[rt],regname[rt],imm&0xFF);
      output_w32(0xe2800000|rd_rn_imm_shift(rt,rs,imm>>8,8));
      output_w32(0xe2800000|rd_rn_imm_shift(rt,rt,imm&0xff,0));
    }
  }
  else if(rs!=rt) emit_mov(rs,rt);
}

static void emit_addimm_and_set_flags(int imm,int rt)
{
  assert(imm>-65536&&imm<65536);
  u_int armval;
  if(genimm(imm,&armval)) {
    assem_debug("adds %s,%s,#%d",regname[rt],regname[rt],imm);
    output_w32(0xe2900000|rd_rn_rm(rt,rt,0)|armval);
  }else if(genimm(-imm,&armval)) {
    assem_debug("subs %s,%s,#%d",regname[rt],regname[rt],imm);
    output_w32(0xe2500000|rd_rn_rm(rt,rt,0)|armval);
  }else if(imm<0) {
    assem_debug("sub %s,%s,#%d",regname[rt],regname[rt],(-imm)&0xFF00);
    assem_debug("subs %s,%s,#%d",regname[rt],regname[rt],(-imm)&0xFF);
    output_w32(0xe2400000|rd_rn_imm_shift(rt,rt,(-imm)>>8,8));
    output_w32(0xe2500000|rd_rn_imm_shift(rt,rt,(-imm)&0xff,0));
  }else{
    assem_debug("add %s,%s,#%d",regname[rt],regname[rt],imm&0xFF00);
    assem_debug("adds %s,%s,#%d",regname[rt],regname[rt],imm&0xFF);
    output_w32(0xe2800000|rd_rn_imm_shift(rt,rt,imm>>8,8));
    output_w32(0xe2900000|rd_rn_imm_shift(rt,rt,imm&0xff,0));
  }
}

#ifndef RAM_OFFSET
static void emit_addimm_no_flags(u_int imm,u_int rt)
{
  emit_addimm(rt,imm,rt);
}
#endif

static void emit_addnop(u_int r)
{
  assert(r<16);
  assem_debug("add %s,%s,#0 (nop)",regname[r],regname[r]);
  output_w32(0xe2800000|rd_rn_rm(r,r,0));
}

static void emit_adcimm(u_int rs,int imm,u_int rt)
{
  u_int armval, ret;
  ret = genimm(imm,&armval);
  assert(ret);
  assem_debug("adc %s,%s,#%d",regname[rt],regname[rs],imm);
  output_w32(0xe2a00000|rd_rn_rm(rt,rs,0)|armval);
}
static void emit_sbcimm(u_int rs,int imm,u_int rt)
{
  u_int armval, ret;
  ret = genimm(imm,&armval);
  assert(ret);
  assem_debug("sbc %s,%s,#%d",regname[rt],regname[rs],imm);
  output_w32(0xe2c00000|rd_rn_rm(rt,rs,0)|armval);
}

static void emit_rscimm(int rs,int imm,u_int rt)
{
  u_int armval, ret;
  ret = genimm(imm,&armval);
  assert(ret);
  assem_debug("rsc %s,%s,#%d",regname[rt],regname[rs],imm);
  output_w32(0xe2e00000|rd_rn_rm(rt,rs,0)|armval);
}

static void emit_addimm64_32(int rsh,int rsl,int imm,int rth,int rtl)
{
  u_int armval;
  if(imm>0&&genimm(imm,&armval)) {
    assem_debug("adds %s,%s,#%d",regname[rtl],regname[rsl],imm);
    output_w32(0xe2900000|rd_rn_rm(rtl,rsl,0)|armval);
    emit_adcimm(rsh,0,rth);
  }else if(imm<0&&genimm(-imm,&armval)) {
    assem_debug("subs %s,%s,#%d",regname[rtl],regname[rsl],imm);
    output_w32(0xe2500000|rd_rn_rm(rtl,rsl,0)|armval);
    emit_sbcimm(rsh,0,rth);
  }else if(imm<0) {
    assert(rsl!=HOST_TEMPREG);
    emit_movimm(-imm,HOST_TEMPREG);
    emit_subs(rsl,HOST_TEMPREG,rtl);
    emit_sbcimm(rsh,0,rth);
  }else if(imm>0) {
    assert(rsl!=HOST_TEMPREG);
    emit_movimm(imm,HOST_TEMPREG);
    emit_adds(rsl,HOST_TEMPREG,rtl);
    emit_adcimm(rsh,0,rth);
  }
  else {
    assert(imm==0);
    if(rsl!=rtl) {
      assert(rsh!=rth);
      emit_mov(rsl,rtl);
      emit_mov(rsh,rth);
    }
  }
}
#ifdef INVERTED_CARRY
static void emit_sbb(int rs1,int rs2)
{
  assem_debug("sbb %%%s,%%%s",regname[rs2],regname[rs1]);
  output_byte(0x19);
  output_modrm(3,rs1,rs2);
}
#endif

static void emit_andimm(int rs,int imm,int rt)
{
  u_int armval;
  if(imm==0) {
    emit_zeroreg(rt);
  }else if(genimm(imm,&armval)) {
    assem_debug("and %s,%s,#%d",regname[rt],regname[rs],imm);
    output_w32(0xe2000000|rd_rn_rm(rt,rs,0)|armval);
  }else if(genimm(~imm,&armval)) {
    assem_debug("bic %s,%s,#%d",regname[rt],regname[rs],imm);
    output_w32(0xe3c00000|rd_rn_rm(rt,rs,0)|armval);
  }else if(imm==65535) {
    #ifdef ARMv5_ONLY
    assem_debug("bic %s,%s,#FF000000",regname[rt],regname[rs]);
    output_w32(0xe3c00000|rd_rn_rm(rt,rs,0)|0x4FF);
    assem_debug("bic %s,%s,#00FF0000",regname[rt],regname[rt]);
    output_w32(0xe3c00000|rd_rn_rm(rt,rt,0)|0x8FF);
    #else
    assem_debug("uxth %s,%s",regname[rt],regname[rs]);
    output_w32(0xe6ff0070|rd_rn_rm(rt,0,rs));
    #endif
  }else{
    assert(rs!=HOST_TEMPREG);
    assert(imm>0&&imm<65535);
    #ifdef ARMv5_ONLY
    assem_debug("mov r14,#%d",imm&0xFF00);
    output_w32(0xe3a00000|rd_rn_imm_shift(HOST_TEMPREG,0,imm>>8,8));
    assem_debug("add r14,r14,#%d",imm&0xFF);
    output_w32(0xe2800000|rd_rn_imm_shift(HOST_TEMPREG,HOST_TEMPREG,imm&0xff,0));
    #else
    emit_movw(imm,HOST_TEMPREG);
    #endif
    assem_debug("and %s,%s,r14",regname[rt],regname[rs]);
    output_w32(0xe0000000|rd_rn_rm(rt,rs,HOST_TEMPREG));
  }
}

static void emit_orimm(int rs,int imm,int rt)
{
  u_int armval;
  if(imm==0) {
    if(rs!=rt) emit_mov(rs,rt);
  }else if(genimm(imm,&armval)) {
    assem_debug("orr %s,%s,#%d",regname[rt],regname[rs],imm);
    output_w32(0xe3800000|rd_rn_rm(rt,rs,0)|armval);
  }else{
    assert(imm>0&&imm<65536);
    assem_debug("orr %s,%s,#%d",regname[rt],regname[rs],imm&0xFF00);
    assem_debug("orr %s,%s,#%d",regname[rt],regname[rs],imm&0xFF);
    output_w32(0xe3800000|rd_rn_imm_shift(rt,rs,imm>>8,8));
    output_w32(0xe3800000|rd_rn_imm_shift(rt,rt,imm&0xff,0));
  }
}

static void emit_xorimm(int rs,int imm,int rt)
{
  u_int armval;
  if(imm==0) {
    if(rs!=rt) emit_mov(rs,rt);
  }else if(genimm(imm,&armval)) {
    assem_debug("eor %s,%s,#%d",regname[rt],regname[rs],imm);
    output_w32(0xe2200000|rd_rn_rm(rt,rs,0)|armval);
  }else{
    assert(imm>0&&imm<65536);
    assem_debug("eor %s,%s,#%d",regname[rt],regname[rs],imm&0xFF00);
    assem_debug("eor %s,%s,#%d",regname[rt],regname[rs],imm&0xFF);
    output_w32(0xe2200000|rd_rn_imm_shift(rt,rs,imm>>8,8));
    output_w32(0xe2200000|rd_rn_imm_shift(rt,rt,imm&0xff,0));
  }
}

static void emit_shlimm(int rs,u_int imm,int rt)
{
  assert(imm>0);
  assert(imm<32);
  //if(imm==1) ...
  assem_debug("lsl %s,%s,#%d",regname[rt],regname[rs],imm);
  output_w32(0xe1a00000|rd_rn_rm(rt,0,rs)|(imm<<7));
}

static void emit_shrimm(int rs,u_int imm,int rt)
{
  assert(imm>0);
  assert(imm<32);
  assem_debug("lsr %s,%s,#%d",regname[rt],regname[rs],imm);
  output_w32(0xe1a00000|rd_rn_rm(rt,0,rs)|0x20|(imm<<7));
}

static void emit_sarimm(int rs,u_int imm,int rt)
{
  assert(imm>0);
  assert(imm<32);
  assem_debug("asr %s,%s,#%d",regname[rt],regname[rs],imm);
  output_w32(0xe1a00000|rd_rn_rm(rt,0,rs)|0x40|(imm<<7));
}

static void emit_rorimm(int rs,u_int imm,int rt)
{
  assert(imm>0);
  assert(imm<32);
  assem_debug("ror %s,%s,#%d",regname[rt],regname[rs],imm);
  output_w32(0xe1a00000|rd_rn_rm(rt,0,rs)|0x60|(imm<<7));
}

static void emit_shldimm(int rs,int rs2,u_int imm,int rt)
{
  assem_debug("shld %%%s,%%%s,%d",regname[rt],regname[rs2],imm);
  assert(imm>0);
  assert(imm<32);
  //if(imm==1) ...
  assem_debug("lsl %s,%s,#%d",regname[rt],regname[rs],imm);
  output_w32(0xe1a00000|rd_rn_rm(rt,0,rs)|(imm<<7));
  assem_debug("orr %s,%s,%s,lsr #%d",regname[rt],regname[rt],regname[rs2],32-imm);
  output_w32(0xe1800020|rd_rn_rm(rt,rt,rs2)|((32-imm)<<7));
}

static void emit_shrdimm(int rs,int rs2,u_int imm,int rt)
{
  assem_debug("shrd %%%s,%%%s,%d",regname[rt],regname[rs2],imm);
  assert(imm>0);
  assert(imm<32);
  //if(imm==1) ...
  assem_debug("lsr %s,%s,#%d",regname[rt],regname[rs],imm);
  output_w32(0xe1a00020|rd_rn_rm(rt,0,rs)|(imm<<7));
  assem_debug("orr %s,%s,%s,lsl #%d",regname[rt],regname[rt],regname[rs2],32-imm);
  output_w32(0xe1800000|rd_rn_rm(rt,rt,rs2)|((32-imm)<<7));
}

static void emit_shl(u_int rs,u_int shift,u_int rt)
{
  assert(rs<16);
  assert(rt<16);
  assert(shift<16);
  //if(imm==1) ...
  assem_debug("lsl %s,%s,%s",regname[rt],regname[rs],regname[shift]);
  output_w32(0xe1a00000|rd_rn_rm(rt,0,rs)|0x10|(shift<<8));
}
static void emit_shr(u_int rs,u_int shift,u_int rt)
{
  assert(rs<16);
  assert(rt<16);
  assert(shift<16);
  assem_debug("lsr %s,%s,%s",regname[rt],regname[rs],regname[shift]);
  output_w32(0xe1a00000|rd_rn_rm(rt,0,rs)|0x30|(shift<<8));
}
static void emit_sar(u_int rs,u_int shift,u_int rt)
{
  assert(rs<16);
  assert(rt<16);
  assert(shift<16);
  assem_debug("asr %s,%s,%s",regname[rt],regname[rs],regname[shift]);
  output_w32(0xe1a00000|rd_rn_rm(rt,0,rs)|0x50|(shift<<8));
}

static void emit_orrshl(u_int rs,u_int shift,u_int rt)
{
  assert(rs<16);
  assert(rt<16);
  assert(shift<16);
  assem_debug("orr %s,%s,%s,lsl %s",regname[rt],regname[rt],regname[rs],regname[shift]);
  output_w32(0xe1800000|rd_rn_rm(rt,rt,rs)|0x10|(shift<<8));
}
static void emit_orrshr(u_int rs,u_int shift,u_int rt)
{
  assert(rs<16);
  assert(rt<16);
  assert(shift<16);
  assem_debug("orr %s,%s,%s,lsr %s",regname[rt],regname[rt],regname[rs],regname[shift]);
  output_w32(0xe1800000|rd_rn_rm(rt,rt,rs)|0x30|(shift<<8));
}

static void emit_cmpimm(int rs,int imm)
{
  u_int armval;
  if(genimm(imm,&armval)) {
    assem_debug("cmp %s,#%d",regname[rs],imm);
    output_w32(0xe3500000|rd_rn_rm(0,rs,0)|armval);
  }else if(genimm(-imm,&armval)) {
    assem_debug("cmn %s,#%d",regname[rs],imm);
    output_w32(0xe3700000|rd_rn_rm(0,rs,0)|armval);
  }else if(imm>0) {
    assert(rs!=HOST_TEMPREG);
    assert(imm<65536);
    #ifdef ARMv5_ONLY
    emit_movimm(imm,HOST_TEMPREG);
    #else
    emit_movw(imm,HOST_TEMPREG);
    #endif
    assem_debug("cmp %s,r14",regname[rs]);
    output_w32(0xe1500000|rd_rn_rm(0,rs,HOST_TEMPREG));
  }else{
    assert(rs!=HOST_TEMPREG);
    assert(imm>-65536);
    #ifdef ARMv5_ONLY
    emit_movimm(-imm,HOST_TEMPREG);
    #else
    emit_movw(-imm,HOST_TEMPREG);
    #endif
    assem_debug("cmn %s,r14",regname[rs]);
    output_w32(0xe1700000|rd_rn_rm(0,rs,HOST_TEMPREG));
  }
}

static void emit_cmovne_imm(int imm,int rt)
{
  assem_debug("movne %s,#%d",regname[rt],imm);
  u_int armval, ret;
  ret = genimm(imm,&armval);
  assert(ret);
  output_w32(0x13a00000|rd_rn_rm(rt,0,0)|armval);
}
static void emit_cmovl_imm(int imm,int rt)
{
  assem_debug("movlt %s,#%d",regname[rt],imm);
  u_int armval, ret;
  ret = genimm(imm,&armval);
  assert(ret);
  output_w32(0xb3a00000|rd_rn_rm(rt,0,0)|armval);
}
static void emit_cmovb_imm(int imm,int rt)
{
  assem_debug("movcc %s,#%d",regname[rt],imm);
  u_int armval, ret;
  ret = genimm(imm,&armval);
  assert(ret);
  output_w32(0x33a00000|rd_rn_rm(rt,0,0)|armval);
}
static void emit_cmovs_imm(int imm,int rt)
{
  assem_debug("movmi %s,#%d",regname[rt],imm);
  u_int armval, ret;
  ret = genimm(imm,&armval);
  assert(ret);
  output_w32(0x43a00000|rd_rn_rm(rt,0,0)|armval);
}
static void emit_cmove_reg(int rs,int rt)
{
  assem_debug("moveq %s,%s",regname[rt],regname[rs]);
  output_w32(0x01a00000|rd_rn_rm(rt,0,rs));
}
static void emit_cmovne_reg(int rs,int rt)
{
  assem_debug("movne %s,%s",regname[rt],regname[rs]);
  output_w32(0x11a00000|rd_rn_rm(rt,0,rs));
}
static void emit_cmovl_reg(int rs,int rt)
{
  assem_debug("movlt %s,%s",regname[rt],regname[rs]);
  output_w32(0xb1a00000|rd_rn_rm(rt,0,rs));
}
static void emit_cmovs_reg(int rs,int rt)
{
  assem_debug("movmi %s,%s",regname[rt],regname[rs]);
  output_w32(0x41a00000|rd_rn_rm(rt,0,rs));
}

static void emit_slti32(int rs,int imm,int rt)
{
  if(rs!=rt) emit_zeroreg(rt);
  emit_cmpimm(rs,imm);
  if(rs==rt) emit_movimm(0,rt);
  emit_cmovl_imm(1,rt);
}
static void emit_sltiu32(int rs,int imm,int rt)
{
  if(rs!=rt) emit_zeroreg(rt);
  emit_cmpimm(rs,imm);
  if(rs==rt) emit_movimm(0,rt);
  emit_cmovb_imm(1,rt);
}
static void emit_slti64_32(int rsh,int rsl,int imm,int rt)
{
  assert(rsh!=rt);
  emit_slti32(rsl,imm,rt);
  if(imm>=0)
  {
    emit_test(rsh,rsh);
    emit_cmovne_imm(0,rt);
    emit_cmovs_imm(1,rt);
  }
  else
  {
    emit_cmpimm(rsh,-1);
    emit_cmovne_imm(0,rt);
    emit_cmovl_imm(1,rt);
  }
}
static void emit_sltiu64_32(int rsh,int rsl,int imm,int rt)
{
  assert(rsh!=rt);
  emit_sltiu32(rsl,imm,rt);
  if(imm>=0)
  {
    emit_test(rsh,rsh);
    emit_cmovne_imm(0,rt);
  }
  else
  {
    emit_cmpimm(rsh,-1);
    emit_cmovne_imm(1,rt);
  }
}

static void emit_cmp(int rs,int rt)
{
  assem_debug("cmp %s,%s",regname[rs],regname[rt]);
  output_w32(0xe1500000|rd_rn_rm(0,rs,rt));
}
static void emit_set_gz32(int rs, int rt)
{
  //assem_debug("set_gz32");
  emit_cmpimm(rs,1);
  emit_movimm(1,rt);
  emit_cmovl_imm(0,rt);
}
static void emit_set_nz32(int rs, int rt)
{
  //assem_debug("set_nz32");
  if(rs!=rt) emit_movs(rs,rt);
  else emit_test(rs,rs);
  emit_cmovne_imm(1,rt);
}
static void emit_set_gz64_32(int rsh, int rsl, int rt)
{
  //assem_debug("set_gz64");
  emit_set_gz32(rsl,rt);
  emit_test(rsh,rsh);
  emit_cmovne_imm(1,rt);
  emit_cmovs_imm(0,rt);
}
static void emit_set_nz64_32(int rsh, int rsl, int rt)
{
  //assem_debug("set_nz64");
  emit_or_and_set_flags(rsh,rsl,rt);
  emit_cmovne_imm(1,rt);
}
static void emit_set_if_less32(int rs1, int rs2, int rt)
{
  //assem_debug("set if less (%%%s,%%%s),%%%s",regname[rs1],regname[rs2],regname[rt]);
  if(rs1!=rt&&rs2!=rt) emit_zeroreg(rt);
  emit_cmp(rs1,rs2);
  if(rs1==rt||rs2==rt) emit_movimm(0,rt);
  emit_cmovl_imm(1,rt);
}
static void emit_set_if_carry32(int rs1, int rs2, int rt)
{
  //assem_debug("set if carry (%%%s,%%%s),%%%s",regname[rs1],regname[rs2],regname[rt]);
  if(rs1!=rt&&rs2!=rt) emit_zeroreg(rt);
  emit_cmp(rs1,rs2);
  if(rs1==rt||rs2==rt) emit_movimm(0,rt);
  emit_cmovb_imm(1,rt);
}
static void emit_set_if_less64_32(int u1, int l1, int u2, int l2, int rt)
{
  //assem_debug("set if less64 (%%%s,%%%s,%%%s,%%%s),%%%s",regname[u1],regname[l1],regname[u2],regname[l2],regname[rt]);
  assert(u1!=rt);
  assert(u2!=rt);
  emit_cmp(l1,l2);
  emit_movimm(0,rt);
  emit_sbcs(u1,u2,HOST_TEMPREG);
  emit_cmovl_imm(1,rt);
}
static void emit_set_if_carry64_32(int u1, int l1, int u2, int l2, int rt)
{
  //assem_debug("set if carry64 (%%%s,%%%s,%%%s,%%%s),%%%s",regname[u1],regname[l1],regname[u2],regname[l2],regname[rt]);
  assert(u1!=rt);
  assert(u2!=rt);
  emit_cmp(l1,l2);
  emit_movimm(0,rt);
  emit_sbcs(u1,u2,HOST_TEMPREG);
  emit_cmovb_imm(1,rt);
}

static void emit_call(int a)
{
  assem_debug("bl %x (%x+%x)",a,(int)out,a-(int)out-8);
  u_int offset=genjmp(a);
  output_w32(0xeb000000|offset);
}
static void emit_jmp(int a)
{
  assem_debug("b %x (%x+%x)",a,(int)out,a-(int)out-8);
  u_int offset=genjmp(a);
  output_w32(0xea000000|offset);
}
static void emit_jne(int a)
{
  assem_debug("bne %x",a);
  u_int offset=genjmp(a);
  output_w32(0x1a000000|offset);
}
static void emit_jeq(int a)
{
  assem_debug("beq %x",a);
  u_int offset=genjmp(a);
  output_w32(0x0a000000|offset);
}
static void emit_js(int a)
{
  assem_debug("bmi %x",a);
  u_int offset=genjmp(a);
  output_w32(0x4a000000|offset);
}
static void emit_jns(int a)
{
  assem_debug("bpl %x",a);
  u_int offset=genjmp(a);
  output_w32(0x5a000000|offset);
}
static void emit_jl(int a)
{
  assem_debug("blt %x",a);
  u_int offset=genjmp(a);
  output_w32(0xba000000|offset);
}
static void emit_jge(int a)
{
  assem_debug("bge %x",a);
  u_int offset=genjmp(a);
  output_w32(0xaa000000|offset);
}
static void emit_jno(int a)
{
  assem_debug("bvc %x",a);
  u_int offset=genjmp(a);
  output_w32(0x7a000000|offset);
}

static void emit_jcc(int a)
{
  assem_debug("bcc %x",a);
  u_int offset=genjmp(a);
  output_w32(0x3a000000|offset);
}
static void emit_jae(int a)
{
  assem_debug("bcs %x",a);
  u_int offset=genjmp(a);
  output_w32(0x2a000000|offset);
}
static void emit_jb(int a)
{
  assem_debug("bcc %x",a);
  u_int offset=genjmp(a);
  output_w32(0x3a000000|offset);
}

static void emit_pushreg(u_int r)
{
  assem_debug("push %%%s",regname[r]);
  assert(0);
}
static void emit_popreg(u_int r)
{
  assem_debug("pop %%%s",regname[r]);
  assert(0);
}
/*
static void emit_callreg(u_int r)
{
  assem_debug("call *%%%s",regname[r]);
  assert(0);
}*/
static void emit_jmpreg(u_int r)
{
  assem_debug("mov pc,%s",regname[r]);
  output_w32(0xe1a00000|rd_rn_rm(15,0,r));
}
static void emit_readword_indexed(int offset, int rs, int rt)
{
  assert(offset>-4096&&offset<4096);
  assem_debug("ldr %s,%s+%d",regname[rt],regname[rs],offset);
  if(offset>=0) {
    output_w32(0xe5900000|rd_rn_rm(rt,rs,0)|offset);
  }else{
    output_w32(0xe5100000|rd_rn_rm(rt,rs,0)|(-offset));
  }
}
static void emit_readword_dualindexedx4(int rs1, int rs2, int rt)
{
  assem_debug("ldr %s,%s,%s lsl #2",regname[rt],regname[rs1],regname[rs2]);
  output_w32(0xe7900000|rd_rn_rm(rt,rs1,rs2)|0x100);
}
static void emit_readword_indexed_tlb(int addr, int rs, int map, int rt)
{
  if(map<0) emit_readword_indexed(addr, rs, rt);
  else {
    assert(addr==0);
    emit_readword_dualindexedx4(rs, map, rt);
  }
}
static void emit_readdword_indexed_tlb(int addr, int rs, int map, int rh, int rl)
{
  if(map<0) {
    if(rh>=0) emit_readword_indexed(addr, rs, rh);
    emit_readword_indexed(addr+4, rs, rl);
  }else{
    assert(rh!=rs);
    if(rh>=0) emit_readword_indexed_tlb(addr, rs, map, rh);
    emit_addimm(map,1,HOST_TEMPREG);
    emit_readword_indexed_tlb(addr, rs, HOST_TEMPREG, rl);
  }
}
static void emit_movsbl_indexed(int offset, int rs, int rt)
{
  assert(offset>-256&&offset<256);
  assem_debug("ldrsb %s,%s+%d",regname[rt],regname[rs],offset);
  if(offset>=0) {
    output_w32(0xe1d000d0|rd_rn_rm(rt,rs,0)|((offset<<4)&0xf00)|(offset&0xf));
  }else{
    output_w32(0xe15000d0|rd_rn_rm(rt,rs,0)|(((-offset)<<4)&0xf00)|((-offset)&0xf));
  }
}
static void emit_movsbl_indexed_tlb(int addr, int rs, int map, int rt)
{
  if(map<0) emit_movsbl_indexed(addr, rs, rt);
  else {
    if(addr==0) {
      emit_shlimm(map,2,HOST_TEMPREG);
      assem_debug("ldrsb %s,%s+%s",regname[rt],regname[rs],regname[HOST_TEMPREG]);
      output_w32(0xe19000d0|rd_rn_rm(rt,rs,HOST_TEMPREG));
    }else{
      assert(addr>-256&&addr<256);
      assem_debug("add %s,%s,%s,lsl #2",regname[rt],regname[rs],regname[map]);
      output_w32(0xe0800000|rd_rn_rm(rt,rs,map)|(2<<7));
      emit_movsbl_indexed(addr, rt, rt);
    }
  }
}
static void emit_movswl_indexed(int offset, int rs, int rt)
{
  assert(offset>-256&&offset<256);
  assem_debug("ldrsh %s,%s+%d",regname[rt],regname[rs],offset);
  if(offset>=0) {
    output_w32(0xe1d000f0|rd_rn_rm(rt,rs,0)|((offset<<4)&0xf00)|(offset&0xf));
  }else{
    output_w32(0xe15000f0|rd_rn_rm(rt,rs,0)|(((-offset)<<4)&0xf00)|((-offset)&0xf));
  }
}
static void emit_movswl_indexed_tlb(int addr, int rs, int map, int rt)
{
  if(map<0) emit_movswl_indexed(addr,rs,rt);
  else {
    if(addr==0) {
      emit_shlimm(map,2,HOST_TEMPREG);
      assem_debug("ldrsh %s,%s+%s",regname[rt],regname[rs],regname[HOST_TEMPREG]);
      output_w32(0xe19000f0|rd_rn_rm(rt,rs,HOST_TEMPREG));
    }else{
      assert(addr>-256&&addr<256);
      assem_debug("add %s,%s,%s,lsl #2",regname[rt],regname[rs],regname[map]);
      output_w32(0xe0800000|rd_rn_rm(rt,rs,map)|(2<<7));
      emit_movswl_indexed(addr, rt, rt);
    }
  }
}
static void emit_movzbl_indexed(int offset, int rs, int rt)
{
  assert(offset>-4096&&offset<4096);
  assem_debug("ldrb %s,%s+%d",regname[rt],regname[rs],offset);
  if(offset>=0) {
    output_w32(0xe5d00000|rd_rn_rm(rt,rs,0)|offset);
  }else{
    output_w32(0xe5500000|rd_rn_rm(rt,rs,0)|(-offset));
  }
}
static void emit_movzbl_dualindexedx4(int rs1, int rs2, int rt)
{
  assem_debug("ldrb %s,%s,%s lsl #2",regname[rt],regname[rs1],regname[rs2]);
  output_w32(0xe7d00000|rd_rn_rm(rt,rs1,rs2)|0x100);
}
static void emit_movzbl_indexed_tlb(int addr, int rs, int map, int rt)
{
  if(map<0) emit_movzbl_indexed(addr, rs, rt);
  else {
    if(addr==0) {
      emit_movzbl_dualindexedx4(rs, map, rt);
    }else{
      emit_addimm(rs,addr,rt);
      emit_movzbl_dualindexedx4(rt, map, rt);
    }
  }
}
static void emit_movzwl_indexed(int offset, int rs, int rt)
{
  assert(offset>-256&&offset<256);
  assem_debug("ldrh %s,%s+%d",regname[rt],regname[rs],offset);
  if(offset>=0) {
    output_w32(0xe1d000b0|rd_rn_rm(rt,rs,0)|((offset<<4)&0xf00)|(offset&0xf));
  }else{
    output_w32(0xe15000b0|rd_rn_rm(rt,rs,0)|(((-offset)<<4)&0xf00)|((-offset)&0xf));
  }
}
static void emit_movzwl_indexed_tlb(int addr, int rs, int map, int rt)
{
  if(map<0) emit_movzwl_indexed(addr,rs,rt);
  else {
    if(addr==0) {
      emit_shlimm(map,2,HOST_TEMPREG);
      assem_debug("ldrh %s,%s+%s",regname[rt],regname[rs],regname[HOST_TEMPREG]);
      output_w32(0xe19000b0|rd_rn_rm(rt,rs,HOST_TEMPREG));
    }else{
      assert(addr>-256&&addr<256);
      assem_debug("add %s,%s,%s,lsl #2",regname[rt],regname[rs],regname[map]);
      output_w32(0xe0800000|rd_rn_rm(rt,rs,map)|(2<<7));
      emit_movzwl_indexed(addr, rt, rt);
    }
  }
}
static void emit_readword(int addr, int rt)
{
  u_int offset = addr-(u_int)&g_dev.r4300.new_dynarec_hot_state;
  assert(offset<4096);
  assem_debug("ldr %s,fp+%d",regname[rt],offset);
  output_w32(0xe5900000|rd_rn_rm(rt,FP,0)|offset);
}
static void emit_readptr(int addr, int rt)
{
  emit_readword(addr,rt);
}
static void emit_movsbl(int addr, int rt)
{
  u_int offset = addr-(u_int)&g_dev.r4300.new_dynarec_hot_state;
  assert(offset<256);
  assem_debug("ldrsb %s,fp+%d",regname[rt],offset);
  output_w32(0xe1d000d0|rd_rn_rm(rt,FP,0)|((offset<<4)&0xf00)|(offset&0xf));
}
static void emit_movswl(int addr, int rt)
{
  u_int offset = addr-(u_int)&g_dev.r4300.new_dynarec_hot_state;
  assert(offset<256);
  assem_debug("ldrsh %s,fp+%d",regname[rt],offset);
  output_w32(0xe1d000f0|rd_rn_rm(rt,FP,0)|((offset<<4)&0xf00)|(offset&0xf));
}
static void emit_movzbl(int addr, int rt)
{
  u_int offset = addr-(u_int)&g_dev.r4300.new_dynarec_hot_state;
  assert(offset<4096);
  assem_debug("ldrb %s,fp+%d",regname[rt],offset);
  output_w32(0xe5d00000|rd_rn_rm(rt,FP,0)|offset);
}
static void emit_movzwl(int addr, int rt)
{
  u_int offset = addr-(u_int)&g_dev.r4300.new_dynarec_hot_state;
  assert(offset<256);
  assem_debug("ldrh %s,fp+%d",regname[rt],offset);
  output_w32(0xe1d000b0|rd_rn_rm(rt,FP,0)|((offset<<4)&0xf00)|(offset&0xf));
}

/*
static void emit_movzwl_reg(int rs, int rt)
{
  assem_debug("movzwl %%%s,%%%s",regname[rs]+1,regname[rt]);
  assert(0);
}
*/

static void emit_writeword_indexed(int rt, int offset, int rs)
{
  assert(offset>-4096&&offset<4096);
  assem_debug("str %s,%s+%d",regname[rt],regname[rs],offset);
  if(offset>=0) {
    output_w32(0xe5800000|rd_rn_rm(rt,rs,0)|offset);
  }else{
    output_w32(0xe5000000|rd_rn_rm(rt,rs,0)|(-offset));
  }
}
static void emit_writeword_dualindexedx4(int rt, int rs1, int rs2)
{
  assem_debug("str %s,%s,%s lsl #2",regname[rt],regname[rs1],regname[rs2]);
  output_w32(0xe7800000|rd_rn_rm(rt,rs1,rs2)|0x100);
}
static void emit_writeword_indexed_tlb(int rt, int addr, int rs, int map)
{
  if(map<0) emit_writeword_indexed(rt, addr, rs);
  else {
    if(addr==0) {
      emit_writeword_dualindexedx4(rt, rs, map);
    }else{
      assem_debug("add %s,%s,%s,lsl #2",regname[HOST_TEMPREG],regname[rs],regname[map]);
      output_w32(0xe0800000|rd_rn_rm(HOST_TEMPREG,rs,map)|(2<<7));
      emit_writeword_indexed(rt,addr,HOST_TEMPREG);
    }
  }
}
static void emit_writedword_indexed_tlb(int rh, int rl, int addr, int rs, int map)
{
  //emit_writeword_indexed_tlb modifies HOST_TEMPREG when addr!=0
  if(map==HOST_TEMPREG) assert(addr==0);
  assert(rh>=0);
  emit_writeword_indexed_tlb(rh, addr, rs, map);
  emit_writeword_indexed_tlb(rl, addr+4, rs, map);
}
static void emit_writehword_indexed(int rt, int offset, int rs)
{
  assert(offset>-256&&offset<256);
  assem_debug("strh %s,%s+%d",regname[rt],regname[rs],offset);
  if(offset>=0) {
    output_w32(0xe1c000b0|rd_rn_rm(rt,rs,0)|((offset<<4)&0xf00)|(offset&0xf));
  }else{
    output_w32(0xe14000b0|rd_rn_rm(rt,rs,0)|(((-offset)<<4)&0xf00)|((-offset)&0xf));
  }
}
static void emit_writehword_indexed_tlb(int rt, int addr, int rs, int map)
{
  if(map<0) emit_writehword_indexed(rt, addr, rs);
  else {
    assem_debug("add %s,%s,%s,lsl #2",regname[HOST_TEMPREG],regname[rs],regname[map]);
    output_w32(0xe0800000|rd_rn_rm(HOST_TEMPREG,rs,map)|(2<<7));
    emit_writehword_indexed(rt,addr,HOST_TEMPREG);
  }
}
static void emit_writebyte_indexed(int rt, int offset, int rs)
{
  assert(offset>-4096&&offset<4096);
  assem_debug("strb %s,%s+%d",regname[rt],regname[rs],offset);
  if(offset>=0) {
    output_w32(0xe5c00000|rd_rn_rm(rt,rs,0)|offset);
  }else{
    output_w32(0xe5400000|rd_rn_rm(rt,rs,0)|(-offset));
  }
}
static void emit_writebyte_dualindexedx4(int rt, int rs1, int rs2)
{
  assem_debug("strb %s,%s,%s lsl #2",regname[rt],regname[rs1],regname[rs2]);
  output_w32(0xe7c00000|rd_rn_rm(rt,rs1,rs2)|0x100);
}
static void emit_writebyte_indexed_tlb(int rt, int addr, int rs, int map)
{
  if(map<0) emit_writebyte_indexed(rt, addr, rs);
  else {
    if(addr==0) {
      emit_writebyte_dualindexedx4(rt, rs, map);
    }else{
      assem_debug("add %s,%s,%s,lsl #2",regname[HOST_TEMPREG],regname[rs],regname[map]);
      output_w32(0xe0800000|rd_rn_rm(HOST_TEMPREG,rs,map)|(2<<7));
      emit_writebyte_indexed(rt,addr,HOST_TEMPREG);
    }
  }
}
static void emit_writeword(int rt, int addr)
{
  if(rt<0) return;
  u_int offset = addr-(u_int)&g_dev.r4300.new_dynarec_hot_state;
  assert(offset<4096);
  assem_debug("str %s,fp+%d",regname[rt],offset);
  output_w32(0xe5800000|rd_rn_rm(rt,FP,0)|offset);
}
static void emit_writehword(int rt, int addr)
{
  u_int offset = addr-(u_int)&g_dev.r4300.new_dynarec_hot_state;
  assert(offset<256);
  assem_debug("strh %s,fp+%d",regname[rt],offset);
  output_w32(0xe1c000b0|rd_rn_rm(rt,FP,0)|((offset<<4)&0xf00)|(offset&0xf));
}
static void emit_writebyte(int rt, int addr)
{
  u_int offset = addr-(u_int)&g_dev.r4300.new_dynarec_hot_state;
  assert(offset<4096);
  assem_debug("strb %s,fp+%d",regname[rt],offset);
  output_w32(0xe5c00000|rd_rn_rm(rt,FP,0)|offset);
}

static void emit_mul(u_int rs1,u_int rs2,u_int rt)
{
  assem_debug("mul %s,%s,%s",regname[rt],regname[rs1],regname[rs2]);
  output_w32(0xe0000090|(rt<<16)|(rs2<<8)|rs1);
}
static void emit_umull(u_int rs1, u_int rs2, u_int hi, u_int lo)
{
  assem_debug("umull %s, %s, %s, %s",regname[lo],regname[hi],regname[rs1],regname[rs2]);
  assert(rs1<16);
  assert(rs2<16);
  assert(hi<16);
  assert(lo<16);
  output_w32(0xe0800090|(hi<<16)|(lo<<12)|(rs2<<8)|rs1);
}
static void emit_umlal(u_int rs1, u_int rs2, u_int hi, u_int lo)
{
  assem_debug("umlal %s, %s, %s, %s",regname[lo],regname[hi],regname[rs1],regname[rs2]);
  assert(rs1<16);
  assert(rs2<16);
  assert(hi<16);
  assert(lo<16);
  output_w32(0xe0a00090|(hi<<16)|(lo<<12)|(rs2<<8)|rs1);
}
static void emit_smull(u_int rs1, u_int rs2, u_int hi, u_int lo)
{
  assem_debug("smull %s, %s, %s, %s",regname[lo],regname[hi],regname[rs1],regname[rs2]);
  assert(rs1<16);
  assert(rs2<16);
  assert(hi<16);
  assert(lo<16);
  output_w32(0xe0c00090|(hi<<16)|(lo<<12)|(rs2<<8)|rs1);
}
static void emit_smlal(u_int rs1, u_int rs2, u_int hi, u_int lo)
{
  assem_debug("smlal %s, %s, %s, %s",regname[lo],regname[hi],regname[rs1],regname[rs2]);
  assert(rs1<16);
  assert(rs2<16);
  assert(hi<16);
  assert(lo<16);
  output_w32(0xe0e00090|(hi<<16)|(lo<<12)|(rs2<<8)|rs1);
}

static void emit_sdiv(u_int rs1,u_int rs2,u_int rt)
{
  assert(arm_cpu_features.IDIVa);
  assem_debug("sdiv %s,%s,%s",regname[rt],regname[rs1],regname[rs2]);
  output_w32(0xe710f010|(rt<<16)|(rs2<<8)|rs1);
}
static void emit_udiv(u_int rs1,u_int rs2,u_int rt)
{
  assert(arm_cpu_features.IDIVa);
  assem_debug("udiv %s,%s,%s",regname[rt],regname[rs1],regname[rs2]);
  output_w32(0xe730f010|(rt<<16)|(rs2<<8)|rs1);
}

static void emit_clz(int rs,int rt)
{
  assem_debug("clz %s,%s",regname[rt],regname[rs]);
  output_w32(0xe16f0f10|rd_rn_rm(rt,0,rs));
}

static void emit_subcs(int rs1,int rs2,int rt)
{
  assem_debug("subcs %s,%s,%s",regname[rt],regname[rs1],regname[rs2]);
  output_w32(0x20400000|rd_rn_rm(rt,rs1,rs2));
}

static void emit_shrcc_imm(int rs,u_int imm,int rt)
{
  assert(imm>0);
  assert(imm<32);
  assem_debug("lsrcc %s,%s,#%d",regname[rt],regname[rs],imm);
  output_w32(0x31a00000|rd_rn_rm(rt,0,rs)|0x20|(imm<<7));
}

static void emit_negmi(int rs, int rt)
{
  assem_debug("rsbmi %s,%s,#0",regname[rt],regname[rs]);
  output_w32(0x42600000|rd_rn_rm(rt,rs,0));
}

static void emit_orreq(u_int rs1,u_int rs2,u_int rt)
{
  assem_debug("orreq %s,%s,%s",regname[rt],regname[rs1],regname[rs2]);
  output_w32(0x01800000|rd_rn_rm(rt,rs1,rs2));
}

static void emit_orrne(u_int rs1,u_int rs2,u_int rt)
{
  assem_debug("orrne %s,%s,%s",regname[rt],regname[rs1],regname[rs2]);
  output_w32(0x11800000|rd_rn_rm(rt,rs1,rs2));
}

static void emit_bic_lsl(u_int rs1,u_int rs2,u_int shift,u_int rt)
{
  assem_debug("bic %s,%s,%s lsl %s",regname[rt],regname[rs1],regname[rs2],regname[shift]);
  output_w32(0xe1C00000|rd_rn_rm(rt,rs1,rs2)|0x10|(shift<<8));
}

static void emit_biceq_lsl(u_int rs1,u_int rs2,u_int shift,u_int rt)
{
  assem_debug("biceq %s,%s,%s lsl %s",regname[rt],regname[rs1],regname[rs2],regname[shift]);
  output_w32(0x01C00000|rd_rn_rm(rt,rs1,rs2)|0x10|(shift<<8));
}

static void emit_bicne_lsl(u_int rs1,u_int rs2,u_int shift,u_int rt)
{
  assem_debug("bicne %s,%s,%s lsl %s",regname[rt],regname[rs1],regname[rs2],regname[shift]);
  output_w32(0x11C00000|rd_rn_rm(rt,rs1,rs2)|0x10|(shift<<8));
}

static void emit_bic_lsr(u_int rs1,u_int rs2,u_int shift,u_int rt)
{
  assem_debug("bic %s,%s,%s lsr %s",regname[rt],regname[rs1],regname[rs2],regname[shift]);
  output_w32(0xe1C00000|rd_rn_rm(rt,rs1,rs2)|0x30|(shift<<8));
}

static void emit_biceq_lsr(u_int rs1,u_int rs2,u_int shift,u_int rt)
{
  assem_debug("biceq %s,%s,%s lsr %s",regname[rt],regname[rs1],regname[rs2],regname[shift]);
  output_w32(0x01C00000|rd_rn_rm(rt,rs1,rs2)|0x30|(shift<<8));
}

static void emit_bicne_lsr(u_int rs1,u_int rs2,u_int shift,u_int rt)
{
  assem_debug("bicne %s,%s,%s lsr %s",regname[rt],regname[rs1],regname[rs2],regname[shift]);
  output_w32(0x11C00000|rd_rn_rm(rt,rs1,rs2)|0x30|(shift<<8));
}

static void emit_teq(int rs, int rt)
{
  assem_debug("teq %s,%s",regname[rs],regname[rt]);
  output_w32(0xe1300000|rd_rn_rm(0,rs,rt));
}

static void emit_rsbimm(int rs, int imm, int rt)
{
  u_int armval, ret;
  ret = genimm(imm,&armval);
  assert(ret);
  assem_debug("rsb %s,%s,#%d",regname[rt],regname[rs],imm);
  output_w32(0xe2600000|rd_rn_rm(rt,rs,0)|armval);
}

// Load 2 immediates optimizing for small code size
static void emit_mov2imm_compact(int imm1,u_int rt1,int imm2,u_int rt2)
{
  emit_movimm(imm1,rt1);
  u_int armval;
  if(genimm(imm2-imm1,&armval)) {
    assem_debug("add %s,%s,#%d",regname[rt2],regname[rt1],imm2-imm1);
    output_w32(0xe2800000|rd_rn_rm(rt2,rt1,0)|armval);
  }else if(genimm(imm1-imm2,&armval)) {
    assem_debug("sub %s,%s,#%d",regname[rt2],regname[rt1],imm1-imm2);
    output_w32(0xe2400000|rd_rn_rm(rt2,rt1,0)|armval);
  }
  else emit_movimm(imm2,rt2);
}

// Conditionally select one of two immediates, optimizing for small code size
// This will only be called if HAVE_CMOV_IMM is defined
static void emit_cmov2imm_e_ne_compact(int imm1,int imm2,u_int rt)
{
  u_int armval;
  if(genimm(imm2-imm1,&armval)) {
    emit_movimm(imm1,rt);
    assem_debug("addne %s,%s,#%d",regname[rt],regname[rt],imm2-imm1);
    output_w32(0x12800000|rd_rn_rm(rt,rt,0)|armval);
  }else if(genimm(imm1-imm2,&armval)) {
    emit_movimm(imm1,rt);
    assem_debug("subne %s,%s,#%d",regname[rt],regname[rt],imm1-imm2);
    output_w32(0x12400000|rd_rn_rm(rt,rt,0)|armval);
  }
  else {
    #ifdef ARMv5_ONLY
    emit_movimm(imm1,rt);
    add_literal((int)out,imm2);
    assem_debug("ldrne %s,pc+? [=%x]",regname[rt],imm2);
    output_w32(0x15900000|rd_rn_rm(rt,15,0));
    #else
    emit_movw(imm1&0x0000FFFF,rt);
    if((imm1&0xFFFF)!=(imm2&0xFFFF)) {
      assem_debug("movwne %s,#%d (0x%x)",regname[rt],imm2&0xFFFF,imm2&0xFFFF);
      output_w32(0x13000000|rd_rn_rm(rt,0,0)|(imm2&0xfff)|((imm2<<4)&0xf0000));
    }
    emit_movt(imm1&0xFFFF0000,rt);
    if((imm1&0xFFFF0000)!=(imm2&0xFFFF0000)) {
      assem_debug("movtne %s,#%d (0x%x)",regname[rt],imm2&0xffff0000,imm2&0xffff0000);
      output_w32(0x13400000|rd_rn_rm(rt,0,0)|((imm2>>16)&0xfff)|((imm2>>12)&0xf0000));
    }
    #endif
  }
}

// special case for checking pending_exception
static void emit_cmpmem_imm(int addr, int imm)
{
  assert(imm==0);
  emit_readword(addr,HOST_TEMPREG);
  emit_test(HOST_TEMPREG,HOST_TEMPREG);
}

#if !defined(HOST_IMM8)
// special case for checking invalid_code
static void emit_cmpmem_indexedsr12_imm(int addr,int r,int imm)
{
  assert(0);
}
#endif

// special case for checking invalid_code
static void emit_cmpmem_indexedsr12_reg(int base,int r,int imm)
{
  assert(imm<128&&imm>=0);
  assert(r>=0&&r<16);
  assem_debug("ldrb lr,%s,%s lsr #12",regname[base],regname[r]);
  output_w32(0xe7d00000|rd_rn_rm(HOST_TEMPREG,base,r)|0x620);
  emit_cmpimm(HOST_TEMPREG,imm);
}

// special case for tlb mapping
static void emit_addsr12(int rs1,int rs2,int rt)
{
  assem_debug("add %s,%s,%s lsr #12",regname[rt],regname[rs1],regname[rs2]);
  output_w32(0xe0800620|rd_rn_rm(rt,rs1,rs2));
}

static void emit_addsl2(int rs1,int rs2,int rt)
{
  assem_debug("add %s,%s,%s lsl #2",regname[rt],regname[rs1],regname[rs2]);
  output_w32(0xe0800100|rd_rn_rm(rt,rs1,rs2));
}

static void emit_callne(int a)
{
  assem_debug("blne %x",a);
  u_int offset=genjmp(a);
  output_w32(0x1b000000|offset);
}

#ifdef IMM_PREFETCH
// Used to preload hash table entries
static void emit_prefetch(void *addr)
{
  assem_debug("prefetch %x",(int)addr);
  output_byte(0x0F);
  output_byte(0x18);
  output_modrm(0,5,1);
  output_w32((int)addr);
}
#endif

#ifdef REG_PREFETCH
static void emit_prefetchreg(int r)
{
  assem_debug("pld %s",regname[r]);
  output_w32(0xf5d0f000|rd_rn_rm(0,r,0));
}
#endif

// Special case for mini_ht
static void emit_ldreq_indexed(int rs, u_int offset, int rt)
{
  assert(offset<4096);
  assem_debug("ldreq %s,[%s, #%d]",regname[rt],regname[rs],offset);
  output_w32(0x05900000|rd_rn_rm(rt,rs,0)|offset);
}

static void emit_flds(int r,int sr)
{
  assem_debug("flds s%d,[%s]",sr,regname[r]);
  output_w32(0xed900a00|((sr&14)<<11)|((sr&1)<<22)|(r<<16));
}

static void emit_vldr(int r,int vr)
{
  assem_debug("vldr d%d,[%s]",vr,regname[r]);
  output_w32(0xed900b00|(vr<<12)|(r<<16));
}

static void emit_fsts(int sr,int r)
{
  assem_debug("fsts s%d,[%s]",sr,regname[r]);
  output_w32(0xed800a00|((sr&14)<<11)|((sr&1)<<22)|(r<<16));
}

static void emit_vstr(int vr,int r)
{
  assem_debug("vstr d%d,[%s]",vr,regname[r]);
  output_w32(0xed800b00|(vr<<12)|(r<<16));
}

static void emit_ftosizs(int s,int d)
{
  assem_debug("ftosizs s%d,s%d",d,s);
  output_w32(0xeebd0ac0|((d&14)<<11)|((d&1)<<22)|((s&14)>>1)|((s&1)<<5));
}

static void emit_ftosizd(int s,int d)
{
  assem_debug("ftosizd s%d,d%d",d,s);
  output_w32(0xeebd0bc0|((d&14)<<11)|((d&1)<<22)|(s&7));
}

static void emit_fsitos(int s,int d)
{
  assem_debug("fsitos s%d,s%d",d,s);
  output_w32(0xeeb80ac0|((d&14)<<11)|((d&1)<<22)|((s&14)>>1)|((s&1)<<5));
}

static void emit_fsitod(int s,int d)
{
  assem_debug("fsitod d%d,s%d",d,s);
  output_w32(0xeeb80bc0|((d&7)<<12)|((s&14)>>1)|((s&1)<<5));
}

static void emit_fcvtds(int s,int d)
{
  assem_debug("fcvtds d%d,s%d",d,s);
  output_w32(0xeeb70ac0|((d&7)<<12)|((s&14)>>1)|((s&1)<<5));
}

static void emit_fcvtsd(int s,int d)
{
  assem_debug("fcvtsd s%d,d%d",d,s);
  output_w32(0xeeb70bc0|((d&14)<<11)|((d&1)<<22)|(s&7));
}

static void emit_fsqrts(int s,int d)
{
  assem_debug("fsqrts d%d,s%d",d,s);
  output_w32(0xeeb10ac0|((d&14)<<11)|((d&1)<<22)|((s&14)>>1)|((s&1)<<5));
}

static void emit_fsqrtd(int s,int d)
{
  assem_debug("fsqrtd s%d,d%d",d,s);
  output_w32(0xeeb10bc0|((d&7)<<12)|(s&7));
}

static void emit_fabss(int s,int d)
{
  assem_debug("fabss d%d,s%d",d,s);
  output_w32(0xeeb00ac0|((d&14)<<11)|((d&1)<<22)|((s&14)>>1)|((s&1)<<5));
}

static void emit_fabsd(int s,int d)
{
  assem_debug("fabsd s%d,d%d",d,s);
  output_w32(0xeeb00bc0|((d&7)<<12)|(s&7));
}

static void emit_fnegs(int s,int d)
{
  assem_debug("fnegs d%d,s%d",d,s);
  output_w32(0xeeb10a40|((d&14)<<11)|((d&1)<<22)|((s&14)>>1)|((s&1)<<5));
}

static void emit_fnegd(int s,int d)
{
  assem_debug("fnegd s%d,d%d",d,s);
  output_w32(0xeeb10b40|((d&7)<<12)|(s&7));
}

static void emit_fadds(int s1,int s2,int d)
{
  assem_debug("fadds s%d,s%d,s%d",d,s1,s2);
  output_w32(0xee300a00|((d&14)<<11)|((d&1)<<22)|((s1&14)<<15)|((s1&1)<<7)|((s2&14)>>1)|((s2&1)<<5));
}

static void emit_faddd(int s1,int s2,int d)
{
  assem_debug("faddd d%d,d%d,d%d",d,s1,s2);
  output_w32(0xee300b00|((d&7)<<12)|((s1&7)<<16)|(s2&7));
}

static void emit_fsubs(int s1,int s2,int d)
{
  assem_debug("fsubs s%d,s%d,s%d",d,s1,s2);
  output_w32(0xee300a40|((d&14)<<11)|((d&1)<<22)|((s1&14)<<15)|((s1&1)<<7)|((s2&14)>>1)|((s2&1)<<5));
}

static void emit_fsubd(int s1,int s2,int d)
{
  assem_debug("fsubd d%d,d%d,d%d",d,s1,s2);
  output_w32(0xee300b40|((d&7)<<12)|((s1&7)<<16)|(s2&7));
}

static void emit_fmuls(int s1,int s2,int d)
{
  assem_debug("fmuls s%d,s%d,s%d",d,s1,s2);
  output_w32(0xee200a00|((d&14)<<11)|((d&1)<<22)|((s1&14)<<15)|((s1&1)<<7)|((s2&14)>>1)|((s2&1)<<5));
}

static void emit_fmuld(int s1,int s2,int d)
{
  assem_debug("fmuld d%d,d%d,d%d",d,s1,s2);
  output_w32(0xee200b00|((d&7)<<12)|((s1&7)<<16)|(s2&7));
}

static void emit_fdivs(int s1,int s2,int d)
{
  assem_debug("fdivs s%d,s%d,s%d",d,s1,s2);
  output_w32(0xee800a00|((d&14)<<11)|((d&1)<<22)|((s1&14)<<15)|((s1&1)<<7)|((s2&14)>>1)|((s2&1)<<5));
}

static void emit_fdivd(int s1,int s2,int d)
{
  assem_debug("fdivd d%d,d%d,d%d",d,s1,s2);
  output_w32(0xee800b00|((d&7)<<12)|((s1&7)<<16)|(s2&7));
}

static void emit_fcmps(int x,int y)
{
  assem_debug("fcmps s14, s15");
  output_w32(0xeeb47a67);
}

static void emit_fcmpd(int x,int y)
{
  assem_debug("fcmpd d6, d7");
  output_w32(0xeeb46b47);
}

static void emit_fmstat(void)
{
  assem_debug("fmstat");
  output_w32(0xeef1fa10);
}

static void emit_bicne_imm(int rs,int imm,int rt)
{
  u_int armval, ret;
  ret = genimm(imm,&armval);
  assert(ret);
  assem_debug("bicne %s,%s,#%d",regname[rt],regname[rs],imm);
  output_w32(0x13c00000|rd_rn_rm(rt,rs,0)|armval);
}

static void emit_biccs_imm(int rs,int imm,int rt)
{
  u_int armval, ret;
  ret = genimm(imm,&armval);
  assert(ret);
  assem_debug("biccs %s,%s,#%d",regname[rt],regname[rs],imm);
  output_w32(0x23c00000|rd_rn_rm(rt,rs,0)|armval);
}

static void emit_bicvc_imm(int rs,int imm,int rt)
{
  u_int armval, ret;
  ret = genimm(imm,&armval);
  assert(ret);
  assem_debug("bicvc %s,%s,#%d",regname[rt],regname[rs],imm);
  output_w32(0x73c00000|rd_rn_rm(rt,rs,0)|armval);
}

static void emit_bichi_imm(int rs,int imm,int rt)
{
  u_int armval, ret;
  ret = genimm(imm,&armval);
  assert(ret);
  assem_debug("bichi %s,%s,#%d",regname[rt],regname[rs],imm);
  output_w32(0x83c00000|rd_rn_rm(rt,rs,0)|armval);
}

static void emit_orrvs_imm(int rs,int imm,int rt)
{
  u_int armval, ret;
  ret = genimm(imm,&armval);
  assert(ret);
  assem_debug("orrvs %s,%s,#%d",regname[rt],regname[rs],imm);
  output_w32(0x63800000|rd_rn_rm(rt,rs,0)|armval);
}

static void emit_jno_unlikely(int a)
{
  //emit_jno(a);
  assem_debug("addvc pc,pc,#? (%x)",/*a-(int)out-8,*/a);
  output_w32(0x72800000|rd_rn_rm(15,15,0));
}

// Save registers before function call
static void save_regs(u_int reglist)
{
  reglist&=CALLER_SAVED_REGS; // only save the caller-save registers, r0-r3, r12
  if(!reglist) return;
  assem_debug("stmia fp,{");
  if(reglist&1) assem_debug("r0, ");
  if(reglist&2) assem_debug("r1, ");
  if(reglist&4) assem_debug("r2, ");
  if(reglist&8) assem_debug("r3, ");
  if(reglist&0x1000) assem_debug("r12");
  assem_debug("}");
  output_w32(0xe88b0000|reglist);
}
// Restore registers after function call
static void restore_regs(u_int reglist)
{
  reglist&=CALLER_SAVED_REGS; // only restore the caller-save registers, r0-r3, r12
  if(!reglist) return;
  assem_debug("ldmia fp,{");
  if(reglist&1) assem_debug("r0, ");
  if(reglist&2) assem_debug("r1, ");
  if(reglist&4) assem_debug("r2, ");
  if(reglist&8) assem_debug("r3, ");
  if(reglist&0x1000) assem_debug("r12");
  assem_debug("}");
  output_w32(0xe89b0000|reglist);
}

/* Stubs/epilogue */

static void literal_pool(int n)
{
  if(!literalcount) return;
  if(n) {
    if((int)out-literals[0][0]<4096-n) return;
  }
  u_int *ptr;
  int i;
  for(i=0;i<literalcount;i++)
  {
    ptr=(u_int *)literals[i][0];
    u_int offset=(u_int)out-(u_int)ptr-8;
    assert(offset<4096);
    assert(!(offset&3));
    *ptr|=offset;
    output_w32(literals[i][1]);
  }
  literalcount=0;
}

static void literal_pool_jumpover(int n)
{
  if(!literalcount) return;
  if(n) {
    if((int)out-literals[0][0]<4096-n) return;
  }
  int jaddr=(int)out;
  emit_jmp(0);
  literal_pool(0);
  set_jump_target(jaddr,(int)out);
}

static void emit_extjump2(int addr, int target, int linker)
{
  u_char *ptr=(u_char *)addr;
  assert((ptr[3]&0x0e)==0xa);
#ifdef ARMv5_ONLY
  emit_loadlp(target,1);
  emit_loadlp(addr,0);
#else
  emit_movw(target&0x0000FFFF,1);
  emit_movt(target&0xFFFF0000,1);
  emit_movw(addr&0x0000FFFF,0);
  emit_movt(addr&0xFFFF0000,0);
#endif
  emit_jmp(linker);
}

static void do_invstub(int n)
{
  if(stubs[n][4]==-1) return;
  literal_pool(20);
  u_int reglist=stubs[n][3];
  set_jump_target(stubs[n][1],(int)out);
  save_regs(reglist);
  if(stubs[n][4]!=0) emit_mov(stubs[n][4],0);
  emit_call((int)&invalidate_addr);
  restore_regs(reglist);
  emit_jmp(stubs[n][2]); // return address
}

static int do_dirty_stub(int i, struct ll_entry * head)
{
  assem_debug("do_dirty_stub %x",head->vaddr);
  #ifdef ARMv5_ONLY
  emit_loadlp((int)head,ARG1_REG);
  #else
  emit_movw(((u_int)head)&0x0000FFFF,ARG1_REG);
  emit_movt(((u_int)head)&0xFFFF0000,ARG1_REG);
  #endif
  emit_call((int)&verify_code);
  int entry=(int)out;
  load_regs_entry(i);
  if(entry==(int)out) entry=instr_addr[i];
  emit_jmp(instr_addr[i]);
  return entry;
}

static void do_dirty_stub_ds(struct ll_entry * head)
{
  assem_debug("do_dirty_stub_ds %x",head->vaddr);
  #ifdef ARMv5_ONLY
  emit_loadlp((int)head,ARG1_REG);
  #else
  emit_movw(((u_int)head)&0x0000FFFF,ARG1_REG);
  emit_movt(((u_int)head)&0xFFFF0000,ARG1_REG);
  #endif
  emit_call((int)&verify_code);
}

/* TLB */

static int do_tlb_r(int s,int ar,int map,int cache,int x,int c,u_int addr)
{
  if(c) {
    if((signed int)addr>=(signed int)0xC0000000) {
      // address_generation already loaded the const
      emit_readword_dualindexedx4(FP,map,map);
    }
    else
      return -1; // No mapping
  }
  else {
    assert(s!=map);
    if(cache>=0) {
      // Use cached offset to memory map
      emit_addsr12(cache,s,map);
    }else{
      emit_movimm(fp_memory_map>>2,map);
      emit_addsr12(map,s,map);
    }
    // Schedule this while we wait on the load
    //if(x) emit_xorimm(s,x,ar);
    emit_readword_dualindexedx4(FP,map,map);
  }
  return map;
}
static int do_tlb_r_branch(int map, int c, u_int addr, int *jaddr)
{
  if(!c||(signed int)addr>=(signed int)0xC0000000) {
    emit_test(map,map);
    *jaddr=(int)out;
    emit_js(0);
  }
  return map;
}

static int do_tlb_w(int s,int ar,int map,int cache,int x,int c,u_int addr)
{
  if(c) {
    if(addr<0x80800000||addr>=0xC0000000) {
      // address_generation already loaded the const
      emit_readword_dualindexedx4(FP,map,map);
    }
    else
      return -1; // No mapping
  }
  else {
    assert(s!=map);
    if(cache>=0) {
      // Use cached offset to memory map
      emit_addsr12(cache,s,map);
    }else{
      emit_movimm(fp_memory_map>>2,map);
      emit_addsr12(map,s,map);
    }
    // Schedule this while we wait on the load
    //if(x) emit_xorimm(s,x,ar);
    emit_readword_dualindexedx4(FP,map,map);
  }
  return map;
}
static void do_tlb_w_branch(int map, int c, u_int addr, int *jaddr)
{
  if(!c||addr<0x80800000||addr>=0xC0000000) {
    emit_testimm(map,0x40000000);
    *jaddr=(int)out;
    emit_jne(0);
  }
}

// Generate the address of the memory_map entry, relative to dynarec_local
static void generate_map_const(u_int addr,int reg) {
  //DebugMessage(M64MSG_VERBOSE, "generate_map_const(%x,%s)",addr,regname[reg]);
  emit_movimm((addr>>12)+(fp_memory_map>>2),reg);
}

static void set_rounding_mode(int s,int temp)
{
  assert(temp>=0);
  emit_andimm(s,3,temp);
  emit_addimm(FP,fp_rounding_modes,HOST_TEMPREG);
  emit_readword_dualindexedx4(HOST_TEMPREG,temp,temp);
  output_w32(0xeef10a10|rd_rn_rm(HOST_TEMPREG,0,0)); /*Read FPSCR*/
  emit_andimm(HOST_TEMPREG,~0xc00000,HOST_TEMPREG); /*Clear RMode*/
  emit_or(temp,HOST_TEMPREG,HOST_TEMPREG); /*Set RMode*/
  output_w32(0xeee10a10|rd_rn_rm(HOST_TEMPREG,0,0)); /*Write FPSCR*/
}

/* Special assem */

static void shift_assemble_arm(int i,struct regstat *i_regs)
{
  if(rt1[i]) {
    if(opcode2[i]<=0x07) // SLLV/SRLV/SRAV
    {
      signed char s,t,shift;
      t=get_reg(i_regs->regmap,rt1[i]);
      s=get_reg(i_regs->regmap,rs1[i]);
      shift=get_reg(i_regs->regmap,rs2[i]);
      if(t>=0){
        if(rs1[i]==0)
        {
          emit_zeroreg(t);
        }
        else if(rs2[i]==0)
        {
          assert(s>=0);
          if(s!=t) emit_mov(s,t);
        }
        else
        {
          emit_andimm(shift,31,HOST_TEMPREG);
          if(opcode2[i]==4) // SLLV
          {
            emit_shl(s,HOST_TEMPREG,t);
          }
          if(opcode2[i]==6) // SRLV
          {
            emit_shr(s,HOST_TEMPREG,t);
          }
          if(opcode2[i]==7) // SRAV
          {
            emit_sar(s,HOST_TEMPREG,t);
          }
        }
      }
    } else { // DSLLV/DSRLV/DSRAV
      signed char sh,sl,th,tl,shift;
      th=get_reg(i_regs->regmap,rt1[i]|64);
      tl=get_reg(i_regs->regmap,rt1[i]);
      sh=get_reg(i_regs->regmap,rs1[i]|64);
      sl=get_reg(i_regs->regmap,rs1[i]);
      shift=get_reg(i_regs->regmap,rs2[i]);
      if(tl>=0){
        if(rs1[i]==0)
        {
          emit_zeroreg(tl);
          if(th>=0) emit_zeroreg(th);
        }
        else if(rs2[i]==0)
        {
          assert(sl>=0);
          if(sl!=tl) emit_mov(sl,tl);
          if(th>=0&&sh!=th) emit_mov(sh,th);
        }
        else
        {
          int temp=get_reg(i_regs->regmap,-1);
          int real_th=th;
          if(th<0&&opcode2[i]!=0x14) {th=temp;} // DSLLV doesn't need a temporary register
          assert(sl>=0);
          assert(sh>=0);
          emit_testimm(shift,32);
          emit_andimm(shift,31,HOST_TEMPREG);
          if(opcode2[i]==0x14) // DSLLV
          {
            if(th>=0) emit_shl(sh,HOST_TEMPREG,th);
            emit_rsbimm(HOST_TEMPREG,32,HOST_TEMPREG);
            emit_orrshr(sl,HOST_TEMPREG,th);
            emit_rsbimm(HOST_TEMPREG,32,HOST_TEMPREG);
            emit_shl(sl,HOST_TEMPREG,tl);
            if(th>=0) emit_cmovne_reg(tl,th);
            emit_cmovne_imm(0,tl);
          }
          if(opcode2[i]==0x16) // DSRLV
          {
            assert(th>=0);
            emit_shr(sl,HOST_TEMPREG,tl);
            emit_rsbimm(HOST_TEMPREG,32,HOST_TEMPREG);
            emit_orrshl(sh,HOST_TEMPREG,tl);
            emit_rsbimm(HOST_TEMPREG,32,HOST_TEMPREG);
            emit_shr(sh,HOST_TEMPREG,th);
            emit_cmovne_reg(th,tl);
            if(real_th>=0) emit_cmovne_imm(0,th);
          }
          if(opcode2[i]==0x17) // DSRAV
          {
            assert(th>=0);
            emit_shr(sl,HOST_TEMPREG,tl);
            emit_rsbimm(HOST_TEMPREG,32,HOST_TEMPREG);
            if(real_th>=0) {
              assert(temp>=0);
              emit_sarimm(th,31,temp);
            }
            emit_orrshl(sh,HOST_TEMPREG,tl);
            emit_rsbimm(HOST_TEMPREG,32,HOST_TEMPREG);
            emit_sar(sh,HOST_TEMPREG,th);
            emit_cmovne_reg(th,tl);
            if(real_th>=0) emit_cmovne_reg(temp,th);
          }
        }
      }
    }
  }
}
#define shift_assemble shift_assemble_arm

static void loadlr_assemble_arm(int i,struct regstat *i_regs)
{
  signed char s,th,tl,temp,temp2,temp2h,addr,map=-1,cache=-1;
  int offset,type=0,memtarget=0,c=0;
  intptr_t jaddr=0;
  u_int hr,reglist=0;
  th=get_reg(i_regs->regmap,rt1[i]|64);
  tl=get_reg(i_regs->regmap,rt1[i]);
  s=get_reg(i_regs->regmap,rs1[i]);
  temp=get_reg(i_regs->regmap,-1);
  temp2=get_reg(i_regs->regmap,FTEMP);
  temp2h=get_reg(i_regs->regmap,FTEMP|64);
  addr=get_reg(i_regs->regmap,AGEN1+(i&1));
  assert(addr<0);
  assert(temp>=0);
  assert(temp2>=0);
  offset=imm[i];

  for(hr=0;hr<HOST_REGS;hr++) {
    if(i_regs->regmap[hr]>=0) reglist|=1<<hr;
  }
  reglist|=1<<temp;
  if(s>=0) {
    c=(i_regs->wasconst>>s)&1;
    memtarget=c&&((signed int)(constmap[i][s]+offset))<(signed int)0x80800000;
    if(c&&using_tlb&&((signed int)(constmap[i][s]+offset))>=(signed int)0xC0000000) memtarget=1;
  }
  if(offset||s<0||c) addr=temp2;
  else addr=s;
  int dummy=(rt1[i]==0)||(tl!=get_reg(i_regs->regmap,rt1[i])); // ignore loads to r0 and unneeded reg

  switch(opcode[i]) {
    case 0x22: type=LOADWL_STUB; break;
    case 0x26: type=LOADWR_STUB; break;
    case 0x1A: type=LOADDL_STUB; break;
    case 0x1B: type=LOADDR_STUB; break;
  }

#ifndef INTERPRET_LOADLR
  if(!using_tlb) {
    if(!c) {
      emit_cmpimm(addr,0x800000);
      jaddr=(intptr_t)out;
      emit_jno(0);
    }
    #ifdef RAM_OFFSET
    if(!c&&!dummy) {
      map=get_reg(i_regs->regmap,ROREG);
      if(map<0) emit_loadreg(ROREG,map=HOST_TEMPREG);
    }
    #endif
  }else{ // using tlb
    map=get_reg(i_regs->regmap,TLREG);
    cache=get_reg(i_regs->regmap,MMREG);
    assert(map>=0);
    reglist&=~(1<<map);
    map=do_tlb_r(addr,temp2,map,cache,0,c,constmap[i][s]+offset);
    do_tlb_r_branch(map,c,constmap[i][s]+offset,&jaddr);
  }
  if((!c||memtarget)&&!dummy) {
    if(opcode[i]==0x22||opcode[i]==0x26) { // LWL/LWR
      assert(tl>=0);
      if(!c) {
        emit_shlimm(addr,3,temp);
        emit_andimm(addr,~3,temp2);
        emit_readword_indexed_tlb(0,temp2,map,temp2);
        emit_andimm(temp,24,temp);
        if (opcode[i]==0x26) emit_xorimm(temp,24,temp); // LWR
        emit_movimm(-1,HOST_TEMPREG);
        if (opcode[i]==0x26) {
          emit_shr(temp2,temp,temp2);
          emit_bic_lsr(tl,HOST_TEMPREG,temp,tl);
        }else{
          emit_shl(temp2,temp,temp2);
          emit_bic_lsl(tl,HOST_TEMPREG,temp,tl);
        }
        emit_or(temp2,tl,tl);
      }
      else
      {
        int shift=((constmap[i][s]+offset)&3)<<3;
        uint32_t mask=~UINT32_C(0);
        if (opcode[i]==0x26) { //LWR
          shift^=24;
          mask>>=shift;
        } else { //LWL
          mask<<=shift;
        }

        if((constmap[i][s]+offset)&3)
          emit_andimm(addr,~3,temp2);

        if(shift) {
          emit_readword_indexed_tlb(0,temp2,map,temp2);
          if (opcode[i]==0x26) emit_shrimm(temp2,shift,temp2);
          else emit_shlimm(temp2,shift,temp2);
          emit_andimm(tl,~mask,tl);
          emit_or(temp2,tl,tl);
        }
        else
          emit_readword_indexed_tlb(0,temp2,map,tl);
      }
    }
    if(opcode[i]==0x1A||opcode[i]==0x1B) { // LDL/LDR
      assert(tl>=0);
      assert(th>=0);
      assert(temp2h>=0);
      if(!c) {
        emit_shlimm(addr,3,temp);
        emit_andimm(addr,~7,temp2);
        emit_readdword_indexed_tlb(0,temp2,map,temp2h,temp2);
        emit_testimm(temp,32);
        emit_andimm(temp,24,temp);
        if (opcode[i]==0x1A) { // LDL
          emit_rsbimm(temp,32,HOST_TEMPREG);
          emit_shl(temp2h,temp,temp2h);
          emit_orrshr(temp2,HOST_TEMPREG,temp2h);
          emit_movimm(-1,HOST_TEMPREG);
          emit_shl(temp2,temp,temp2);
          emit_cmove_reg(temp2h,th);
          emit_biceq_lsl(tl,HOST_TEMPREG,temp,tl);
          emit_bicne_lsl(th,HOST_TEMPREG,temp,th);
          emit_orreq(temp2,tl,tl);
          emit_orrne(temp2,th,th);
        }
        if (opcode[i]==0x1B) { // LDR
          emit_xorimm(temp,24,temp);
          emit_rsbimm(temp,32,HOST_TEMPREG);
          emit_shr(temp2,temp,temp2);
          emit_orrshl(temp2h,HOST_TEMPREG,temp2);
          emit_movimm(-1,HOST_TEMPREG);
          emit_shr(temp2h,temp,temp2h);
          emit_cmovne_reg(temp2,tl);
          emit_bicne_lsr(th,HOST_TEMPREG,temp,th);
          emit_biceq_lsr(tl,HOST_TEMPREG,temp,tl);
          emit_orrne(temp2h,th,th);
          emit_orreq(temp2h,tl,tl);
        }
      }
      else
      {
        int shift=((constmap[i][s]+offset)&7)<<3;
        uint64_t mask=~UINT64_C(0);
        if (opcode[i]==0x1B) { //LDR
          shift^=56;
          mask>>=shift;
        } else { //LDL
          mask<<=shift;
        }

        if((constmap[i][s]+offset)&7)
          emit_andimm(addr,~7,temp2);

        if(shift) {
          emit_readdword_indexed_tlb(0,temp2,map,temp2h,temp2);
          if (opcode[i]==0x1B) {
            emit_shrdimm(temp2h,temp2,shift,temp2h);
            emit_shrimm(temp2,shift,temp2);
          } else {
            emit_shldimm(temp2h,temp2,shift,temp2h);
            emit_shlimm(temp2,shift,temp2);
          }
          emit_andimm(tl,~mask,tl);
          emit_andimm(th,~mask>>32,th);
          emit_or(temp2,tl,tl);
          emit_or(temp2h,th,th);
        }
        else
          emit_readdword_indexed_tlb(0,temp2,map,th,tl);
      }
    }
  }
  if(jaddr) {
    add_stub(type,jaddr,(intptr_t)out,i,addr,(intptr_t)i_regs,ccadj[i],reglist);
  } else if(c&&!memtarget) {
    inline_readstub(type,i,(constmap[i][s]+offset),addr,i_regs,rt1[i],ccadj[i],reglist);
  }
#else
  inline_readstub(type,i,c?(constmap[i][s]+offset):0,addr,i_regs,rt1[i],ccadj[i],reglist);
#endif
}
#define loadlr_assemble loadlr_assemble_arm

static void fconv_assemble_arm(int i,struct regstat *i_regs)
{
  signed char temp=get_reg(i_regs->regmap,-1);
  assert(temp>=0);
  // Check cop1 unusable
  if(!cop1_usable) {
    signed char rs=get_reg(i_regs->regmap,CSREG);
    assert(rs>=0);
    emit_testimm(rs,CP0_STATUS_CU1);
    int jaddr=(int)out;
    emit_jeq(0);
    add_stub(FP_STUB,jaddr,(int)out,i,rs,(int)i_regs,is_delayslot,0);
    cop1_usable=1;
  }

#ifndef INTERPRET_FCONV
  #if (defined(__VFP_FP__) && !defined(__SOFTFP__))
  if(opcode2[i]==0x10&&(source[i]&0x3f)==0x0d) { // trunc_w_s
    emit_readword((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_simple[(source[i]>>11)&0x1f],temp);
    emit_flds(temp,15);
    emit_ftosizs(15,15); // float->int, truncate
    if(((source[i]>>11)&0x1f)!=((source[i]>>6)&0x1f))
      emit_readword((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_simple[(source[i]>>6)&0x1f],temp);
    emit_fsts(15,temp);
    return;
  }
  if(opcode2[i]==0x11&&(source[i]&0x3f)==0x0d) { // trunc_w_d
    emit_readword((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_double[(source[i]>>11)&0x1f],temp);
    emit_vldr(temp,7);
    emit_ftosizd(7,13); // double->int, truncate
    emit_readword((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_simple[(source[i]>>6)&0x1f],temp);
    emit_fsts(13,temp);
    return;
  }

  if(opcode2[i]==0x14&&(source[i]&0x3f)==0x20) { // cvt_s_w
    emit_readword((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_simple[(source[i]>>11)&0x1f],temp);
    emit_flds(temp,13);
    if(((source[i]>>11)&0x1f)!=((source[i]>>6)&0x1f))
      emit_readword((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_simple[(source[i]>>6)&0x1f],temp);
    emit_fsitos(13,15);
    emit_fsts(15,temp);
    return;
  }
  if(opcode2[i]==0x14&&(source[i]&0x3f)==0x21) { // cvt_d_w
    emit_readword((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_simple[(source[i]>>11)&0x1f],temp);
    emit_flds(temp,13);
    emit_readword((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_double[(source[i]>>6)&0x1f],temp);
    emit_fsitod(13,7);
    emit_vstr(7,temp);
    return;
  }

  if(opcode2[i]==0x10&&(source[i]&0x3f)==0x21) { // cvt_d_s
    emit_readword((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_simple[(source[i]>>11)&0x1f],temp);
    emit_flds(temp,13);
    emit_readword((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_double[(source[i]>>6)&0x1f],temp);
    emit_fcvtds(13,7);
    emit_vstr(7,temp);
    return;
  }
  if(opcode2[i]==0x11&&(source[i]&0x3f)==0x20) { // cvt_s_d
    emit_readword((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_double[(source[i]>>11)&0x1f],temp);
    emit_vldr(temp,7);
    emit_readword((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_simple[(source[i]>>6)&0x1f],temp);
    emit_fcvtsd(7,13);
    emit_fsts(13,temp);
    return;
  }
  #endif
#endif

  // C emulation code

  u_int hr,reglist=0;
  for(hr=0;hr<HOST_REGS;hr++) {
    if(i_regs->regmap[hr]>=0) reglist|=1<<hr;
  }

  signed char fs=get_reg(i_regs->regmap,FSREG);
  save_regs(reglist);

  if(opcode2[i]==0x14&&(source[i]&0x3f)==0x20) {
    emit_addimm(FP,fp_fcr31,ARG1_REG);
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_simple[(source[i]>>11)&0x1f],ARG2_REG);
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_simple[(source[i]>> 6)&0x1f],ARG3_REG);
    emit_call((int)cvt_s_w);
  }
  if(opcode2[i]==0x14&&(source[i]&0x3f)==0x21) {
    emit_addimm(FP,fp_fcr31,ARG1_REG);
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_simple[(source[i]>>11)&0x1f],ARG1_REG);
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_double[(source[i]>> 6)&0x1f],ARG2_REG);
    emit_call((int)cvt_d_w);
  }
  if(opcode2[i]==0x15&&(source[i]&0x3f)==0x20) {
    emit_addimm(FP,fp_fcr31,ARG1_REG);
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_double[(source[i]>>11)&0x1f],ARG2_REG);
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_simple[(source[i]>> 6)&0x1f],ARG3_REG);
    emit_call((int)cvt_s_l);
  }
  if(opcode2[i]==0x15&&(source[i]&0x3f)==0x21) {
    emit_addimm(FP,fp_fcr31,ARG1_REG);
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_double[(source[i]>>11)&0x1f],ARG2_REG);
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_double[(source[i]>> 6)&0x1f],ARG3_REG);
    emit_call((int)cvt_d_l);
  }

  if(opcode2[i]==0x10&&(source[i]&0x3f)==0x21) {
    emit_addimm(FP,fp_fcr31,ARG1_REG);
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_simple[(source[i]>>11)&0x1f],ARG1_REG);
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_double[(source[i]>> 6)&0x1f],ARG2_REG);
    emit_call((int)cvt_d_s);
  }
  if(opcode2[i]==0x10&&(source[i]&0x3f)==0x24) {
    emit_addimm(FP,fp_fcr31,ARG1_REG);
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_simple[(source[i]>>11)&0x1f],ARG2_REG);
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_simple[(source[i]>> 6)&0x1f],ARG3_REG);
    emit_call((int)cvt_w_s);
  }
  if(opcode2[i]==0x10&&(source[i]&0x3f)==0x25) {
    emit_addimm(FP,fp_fcr31,ARG1_REG);
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_simple[(source[i]>>11)&0x1f],ARG2_REG);
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_double[(source[i]>> 6)&0x1f],ARG3_REG);
    emit_call((int)cvt_l_s);
  }

  if(opcode2[i]==0x11&&(source[i]&0x3f)==0x20) {
    emit_addimm(FP,fp_fcr31,ARG1_REG);
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_double[(source[i]>>11)&0x1f],ARG2_REG);
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_simple[(source[i]>> 6)&0x1f],ARG3_REG);
    emit_call((int)cvt_s_d);
  }
  if(opcode2[i]==0x11&&(source[i]&0x3f)==0x24) {
    emit_addimm(FP,fp_fcr31,ARG1_REG);
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_double[(source[i]>>11)&0x1f],ARG2_REG);
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_simple[(source[i]>> 6)&0x1f],ARG3_REG);
    emit_call((int)cvt_w_d);
  }
  if(opcode2[i]==0x11&&(source[i]&0x3f)==0x25) {
    emit_addimm(FP,fp_fcr31,ARG1_REG);
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_double[(source[i]>>11)&0x1f],ARG2_REG);
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_double[(source[i]>> 6)&0x1f],ARG3_REG);
    emit_call((int)cvt_l_d);
  }

  if(opcode2[i]==0x10&&(source[i]&0x3f)==0x08) {
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_simple[(source[i]>>11)&0x1f],ARG1_REG);
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_double[(source[i]>> 6)&0x1f],ARG2_REG);
    emit_call((int)round_l_s);
  }
  if(opcode2[i]==0x10&&(source[i]&0x3f)==0x09) {
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_simple[(source[i]>>11)&0x1f],ARG1_REG);
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_double[(source[i]>> 6)&0x1f],ARG2_REG);
    emit_call((int)trunc_l_s);
  }
  if(opcode2[i]==0x10&&(source[i]&0x3f)==0x0a) {
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_simple[(source[i]>>11)&0x1f],ARG1_REG);
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_double[(source[i]>> 6)&0x1f],ARG2_REG);
    emit_call((int)ceil_l_s);
  }
  if(opcode2[i]==0x10&&(source[i]&0x3f)==0x0b) {
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_simple[(source[i]>>11)&0x1f],ARG1_REG);
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_double[(source[i]>> 6)&0x1f],ARG2_REG);
    emit_call((int)floor_l_s);
  }
  if(opcode2[i]==0x10&&(source[i]&0x3f)==0x0c) {
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_simple[(source[i]>>11)&0x1f],ARG1_REG);
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_simple[(source[i]>> 6)&0x1f],ARG2_REG);
    emit_call((int)round_w_s);
  }
  if(opcode2[i]==0x10&&(source[i]&0x3f)==0x0d) {
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_simple[(source[i]>>11)&0x1f],ARG1_REG);
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_simple[(source[i]>> 6)&0x1f],ARG2_REG);
    emit_call((int)trunc_w_s);
  }
  if(opcode2[i]==0x10&&(source[i]&0x3f)==0x0e) {
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_simple[(source[i]>>11)&0x1f],ARG1_REG);
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_simple[(source[i]>> 6)&0x1f],ARG2_REG);
    emit_call((int)ceil_w_s);
  }
  if(opcode2[i]==0x10&&(source[i]&0x3f)==0x0f) {
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_simple[(source[i]>>11)&0x1f],ARG1_REG);
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_simple[(source[i]>> 6)&0x1f],ARG2_REG);
    emit_call((int)floor_w_s);
  }

  if(opcode2[i]==0x11&&(source[i]&0x3f)==0x08) {
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_double[(source[i]>>11)&0x1f],ARG1_REG);
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_double[(source[i]>> 6)&0x1f],ARG2_REG);
    emit_call((int)round_l_d);
  }
  if(opcode2[i]==0x11&&(source[i]&0x3f)==0x09) {
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_double[(source[i]>>11)&0x1f],ARG1_REG);
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_double[(source[i]>> 6)&0x1f],ARG2_REG);
    emit_call((int)trunc_l_d);
  }
  if(opcode2[i]==0x11&&(source[i]&0x3f)==0x0a) {
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_double[(source[i]>>11)&0x1f],ARG1_REG);
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_double[(source[i]>> 6)&0x1f],ARG2_REG);
    emit_call((int)ceil_l_d);
  }
  if(opcode2[i]==0x11&&(source[i]&0x3f)==0x0b) {
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_double[(source[i]>>11)&0x1f],ARG1_REG);
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_double[(source[i]>> 6)&0x1f],ARG2_REG);
    emit_call((int)floor_l_d);
  }
  if(opcode2[i]==0x11&&(source[i]&0x3f)==0x0c) {
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_double[(source[i]>>11)&0x1f],ARG1_REG);
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_simple[(source[i]>> 6)&0x1f],ARG2_REG);
    emit_call((int)round_w_d);
  }
  if(opcode2[i]==0x11&&(source[i]&0x3f)==0x0d) {
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_double[(source[i]>>11)&0x1f],ARG1_REG);
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_simple[(source[i]>> 6)&0x1f],ARG2_REG);
    emit_call((int)trunc_w_d);
  }
  if(opcode2[i]==0x11&&(source[i]&0x3f)==0x0e) {
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_double[(source[i]>>11)&0x1f],ARG1_REG);
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_simple[(source[i]>> 6)&0x1f],ARG2_REG);
    emit_call((int)ceil_w_d);
  }
  if(opcode2[i]==0x11&&(source[i]&0x3f)==0x0f) {
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_double[(source[i]>>11)&0x1f],ARG1_REG);
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_simple[(source[i]>> 6)&0x1f],ARG2_REG);
    emit_call((int)floor_w_d);
  }

  restore_regs(reglist);
}
#define fconv_assemble fconv_assemble_arm

static void fcomp_assemble(int i,struct regstat *i_regs)
{
  signed char fs=get_reg(i_regs->regmap,FSREG);
  signed char temp=get_reg(i_regs->regmap,-1);
  assert(temp>=0);
  // Check cop1 unusable
  if(!cop1_usable) {
    signed char cs=get_reg(i_regs->regmap,CSREG);
    assert(cs>=0);
    emit_testimm(cs,CP0_STATUS_CU1);
    int jaddr=(int)out;
    emit_jeq(0);
    add_stub(FP_STUB,jaddr,(int)out,i,cs,(int)i_regs,is_delayslot,0);
    cop1_usable=1;
  }

#ifndef INTERPRET_FCOMP
  if((source[i]&0x3f)==0x30) {
    emit_andimm(fs,~0x800000,fs);
    return;
  }

  if((source[i]&0x3e)==0x38) {
    // sf/ngle - these should throw exceptions for NaNs
    emit_andimm(fs,~0x800000,fs);
    return;
  }

  #if (defined(__VFP_FP__) && !defined(__SOFTFP__))
  if(opcode2[i]==0x10) {
    emit_readword((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_simple[(source[i]>>11)&0x1f],temp);
    emit_readword((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_simple[(source[i]>>16)&0x1f],HOST_TEMPREG);
    emit_orimm(fs,0x800000,fs);
    emit_flds(temp,14);
    emit_flds(HOST_TEMPREG,15);
    emit_fcmps(14,15);
    emit_fmstat();
    if((source[i]&0x3f)==0x31) emit_bicvc_imm(fs,0x800000,fs); // c_un_s
    if((source[i]&0x3f)==0x32) emit_bicne_imm(fs,0x800000,fs); // c_eq_s
    if((source[i]&0x3f)==0x33) {emit_bicne_imm(fs,0x800000,fs);emit_orrvs_imm(fs,0x800000,fs);} // c_ueq_s
    if((source[i]&0x3f)==0x34) emit_biccs_imm(fs,0x800000,fs); // c_olt_s
    if((source[i]&0x3f)==0x35) {emit_biccs_imm(fs,0x800000,fs);emit_orrvs_imm(fs,0x800000,fs);} // c_ult_s
    if((source[i]&0x3f)==0x36) emit_bichi_imm(fs,0x800000,fs); // c_ole_s
    if((source[i]&0x3f)==0x37) {emit_bichi_imm(fs,0x800000,fs);emit_orrvs_imm(fs,0x800000,fs);} // c_ule_s
    if((source[i]&0x3f)==0x3a) emit_bicne_imm(fs,0x800000,fs); // c_seq_s
    if((source[i]&0x3f)==0x3b) emit_bicne_imm(fs,0x800000,fs); // c_ngl_s
    if((source[i]&0x3f)==0x3c) emit_biccs_imm(fs,0x800000,fs); // c_lt_s
    if((source[i]&0x3f)==0x3d) emit_biccs_imm(fs,0x800000,fs); // c_nge_s
    if((source[i]&0x3f)==0x3e) emit_bichi_imm(fs,0x800000,fs); // c_le_s
    if((source[i]&0x3f)==0x3f) emit_bichi_imm(fs,0x800000,fs); // c_ngt_s
    return;
  }
  if(opcode2[i]==0x11) {
    emit_readword((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_double[(source[i]>>11)&0x1f],temp);
    emit_readword((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_double[(source[i]>>16)&0x1f],HOST_TEMPREG);
    emit_orimm(fs,0x800000,fs);
    emit_vldr(temp,6);
    emit_vldr(HOST_TEMPREG,7);
    emit_fcmpd(6,7);
    emit_fmstat();
    if((source[i]&0x3f)==0x31) emit_bicvc_imm(fs,0x800000,fs); // c_un_d
    if((source[i]&0x3f)==0x32) emit_bicne_imm(fs,0x800000,fs); // c_eq_d
    if((source[i]&0x3f)==0x33) {emit_bicne_imm(fs,0x800000,fs);emit_orrvs_imm(fs,0x800000,fs);} // c_ueq_d
    if((source[i]&0x3f)==0x34) emit_biccs_imm(fs,0x800000,fs); // c_olt_d
    if((source[i]&0x3f)==0x35) {emit_biccs_imm(fs,0x800000,fs);emit_orrvs_imm(fs,0x800000,fs);} // c_ult_d
    if((source[i]&0x3f)==0x36) emit_bichi_imm(fs,0x800000,fs); // c_ole_d
    if((source[i]&0x3f)==0x37) {emit_bichi_imm(fs,0x800000,fs);emit_orrvs_imm(fs,0x800000,fs);} // c_ule_d
    if((source[i]&0x3f)==0x3a) emit_bicne_imm(fs,0x800000,fs); // c_seq_d
    if((source[i]&0x3f)==0x3b) emit_bicne_imm(fs,0x800000,fs); // c_ngl_d
    if((source[i]&0x3f)==0x3c) emit_biccs_imm(fs,0x800000,fs); // c_lt_d
    if((source[i]&0x3f)==0x3d) emit_biccs_imm(fs,0x800000,fs); // c_nge_d
    if((source[i]&0x3f)==0x3e) emit_bichi_imm(fs,0x800000,fs); // c_le_d
    if((source[i]&0x3f)==0x3f) emit_bichi_imm(fs,0x800000,fs); // c_ngt_d
    return;
  }
  #endif
#endif

  // C only

  u_int hr,reglist=0;
  for(hr=0;hr<HOST_REGS;hr++) {
    if(i_regs->regmap[hr]>=0) reglist|=1<<hr;
  }
  reglist&=~(1<<fs);
  emit_storereg(FSREG, fs);
  save_regs(reglist);
  if(opcode2[i]==0x10) {
    emit_addimm(FP,fp_fcr31,ARG1_REG);
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_simple[(source[i]>>11)&0x1f],ARG2_REG);
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_simple[(source[i]>>16)&0x1f],ARG3_REG);
    if((source[i]&0x3f)==0x30) emit_call((int)c_f_s);
    if((source[i]&0x3f)==0x31) emit_call((int)c_un_s);
    if((source[i]&0x3f)==0x32) emit_call((int)c_eq_s);
    if((source[i]&0x3f)==0x33) emit_call((int)c_ueq_s);
    if((source[i]&0x3f)==0x34) emit_call((int)c_olt_s);
    if((source[i]&0x3f)==0x35) emit_call((int)c_ult_s);
    if((source[i]&0x3f)==0x36) emit_call((int)c_ole_s);
    if((source[i]&0x3f)==0x37) emit_call((int)c_ule_s);
    if((source[i]&0x3f)==0x38) emit_call((int)c_sf_s);
    if((source[i]&0x3f)==0x39) emit_call((int)c_ngle_s);
    if((source[i]&0x3f)==0x3a) emit_call((int)c_seq_s);
    if((source[i]&0x3f)==0x3b) emit_call((int)c_ngl_s);
    if((source[i]&0x3f)==0x3c) emit_call((int)c_lt_s);
    if((source[i]&0x3f)==0x3d) emit_call((int)c_nge_s);
    if((source[i]&0x3f)==0x3e) emit_call((int)c_le_s);
    if((source[i]&0x3f)==0x3f) emit_call((int)c_ngt_s);
  }
  if(opcode2[i]==0x11) {
    emit_addimm(FP,fp_fcr31,ARG1_REG);
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_double[(source[i]>>11)&0x1f],ARG2_REG);
    emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_double[(source[i]>>16)&0x1f],ARG3_REG);
    if((source[i]&0x3f)==0x30) emit_call((int)c_f_d);
    if((source[i]&0x3f)==0x31) emit_call((int)c_un_d);
    if((source[i]&0x3f)==0x32) emit_call((int)c_eq_d);
    if((source[i]&0x3f)==0x33) emit_call((int)c_ueq_d);
    if((source[i]&0x3f)==0x34) emit_call((int)c_olt_d);
    if((source[i]&0x3f)==0x35) emit_call((int)c_ult_d);
    if((source[i]&0x3f)==0x36) emit_call((int)c_ole_d);
    if((source[i]&0x3f)==0x37) emit_call((int)c_ule_d);
    if((source[i]&0x3f)==0x38) emit_call((int)c_sf_d);
    if((source[i]&0x3f)==0x39) emit_call((int)c_ngle_d);
    if((source[i]&0x3f)==0x3a) emit_call((int)c_seq_d);
    if((source[i]&0x3f)==0x3b) emit_call((int)c_ngl_d);
    if((source[i]&0x3f)==0x3c) emit_call((int)c_lt_d);
    if((source[i]&0x3f)==0x3d) emit_call((int)c_nge_d);
    if((source[i]&0x3f)==0x3e) emit_call((int)c_le_d);
    if((source[i]&0x3f)==0x3f) emit_call((int)c_ngt_d);
  }
  restore_regs(reglist);
  emit_loadreg(FSREG,fs);
}

static void float_assemble(int i,struct regstat *i_regs)
{
  signed char temp=get_reg(i_regs->regmap,-1);
  assert(temp>=0);
  // Check cop1 unusable
  if(!cop1_usable) {
    signed char cs=get_reg(i_regs->regmap,CSREG);
    assert(cs>=0);
    emit_testimm(cs,CP0_STATUS_CU1);
    int jaddr=(int)out;
    emit_jeq(0);
    add_stub(FP_STUB,jaddr,(int)out,i,cs,(int)i_regs,is_delayslot,0);
    cop1_usable=1;
  }

#ifndef INTERPRET_FLOAT
  #if (defined(__VFP_FP__) && !defined(__SOFTFP__))
  if((source[i]&0x3f)==6) // mov
  {
    if(((source[i]>>11)&0x1f)!=((source[i]>>6)&0x1f)) {
      if(opcode2[i]==0x10) {
        emit_readword((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_simple[(source[i]>>11)&0x1f],temp);
        emit_readword((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_simple[(source[i]>>6)&0x1f],HOST_TEMPREG);
        emit_readword_indexed(0,temp,temp);
        emit_writeword_indexed(temp,0,HOST_TEMPREG);
      }
      if(opcode2[i]==0x11) {
        emit_readword((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_double[(source[i]>>11)&0x1f],temp);
        emit_readword((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_double[(source[i]>>6)&0x1f],HOST_TEMPREG);
        emit_vldr(temp,7);
        emit_vstr(7,HOST_TEMPREG);
      }
    }
    return;
  }

  if((source[i]&0x3f)>3)
  {
    if(opcode2[i]==0x10) {
      emit_readword((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_simple[(source[i]>>11)&0x1f],temp);
      emit_flds(temp,15);
      if(((source[i]>>11)&0x1f)!=((source[i]>>6)&0x1f)) {
        emit_readword((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_simple[(source[i]>>6)&0x1f],temp);
      }
      if((source[i]&0x3f)==4) // sqrt
        emit_fsqrts(15,15);
      if((source[i]&0x3f)==5) // abs
        emit_fabss(15,15);
      if((source[i]&0x3f)==7) // neg
        emit_fnegs(15,15);
      emit_fsts(15,temp);
    }
    if(opcode2[i]==0x11) {
      emit_readword((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_double[(source[i]>>11)&0x1f],temp);
      emit_vldr(temp,7);
      if(((source[i]>>11)&0x1f)!=((source[i]>>6)&0x1f)) {
        emit_readword((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_double[(source[i]>>6)&0x1f],temp);
      }
      if((source[i]&0x3f)==4) // sqrt
        emit_fsqrtd(7,7);
      if((source[i]&0x3f)==5) // abs
        emit_fabsd(7,7);
      if((source[i]&0x3f)==7) // neg
        emit_fnegd(7,7);
      emit_vstr(7,temp);
    }
    return;
  }
  if((source[i]&0x3f)<4)
  {
    if(opcode2[i]==0x10) {
      emit_readword((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_simple[(source[i]>>11)&0x1f],temp);
    }
    if(opcode2[i]==0x11) {
      emit_readword((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_double[(source[i]>>11)&0x1f],temp);
    }
    if(((source[i]>>11)&0x1f)!=((source[i]>>16)&0x1f)) {
      if(opcode2[i]==0x10) {
        emit_readword((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_simple[(source[i]>>16)&0x1f],HOST_TEMPREG);
        emit_flds(temp,15);
        emit_flds(HOST_TEMPREG,13);
        if(((source[i]>>11)&0x1f)!=((source[i]>>6)&0x1f)) {
          if(((source[i]>>16)&0x1f)!=((source[i]>>6)&0x1f)) {
            emit_readword((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_simple[(source[i]>>6)&0x1f],temp);
          }
        }
        if((source[i]&0x3f)==0) emit_fadds(15,13,15);
        if((source[i]&0x3f)==1) emit_fsubs(15,13,15);
        if((source[i]&0x3f)==2) emit_fmuls(15,13,15);
        if((source[i]&0x3f)==3) emit_fdivs(15,13,15);
        if(((source[i]>>16)&0x1f)==((source[i]>>6)&0x1f)) {
          emit_fsts(15,HOST_TEMPREG);
        }else{
          emit_fsts(15,temp);
        }
      }
      else if(opcode2[i]==0x11) {
        emit_readword((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_double[(source[i]>>16)&0x1f],HOST_TEMPREG);
        emit_vldr(temp,7);
        emit_vldr(HOST_TEMPREG,6);
        if(((source[i]>>11)&0x1f)!=((source[i]>>6)&0x1f)) {
          if(((source[i]>>16)&0x1f)!=((source[i]>>6)&0x1f)) {
            emit_readword((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_double[(source[i]>>6)&0x1f],temp);
          }
        }
        if((source[i]&0x3f)==0) emit_faddd(7,6,7);
        if((source[i]&0x3f)==1) emit_fsubd(7,6,7);
        if((source[i]&0x3f)==2) emit_fmuld(7,6,7);
        if((source[i]&0x3f)==3) emit_fdivd(7,6,7);
        if(((source[i]>>16)&0x1f)==((source[i]>>6)&0x1f)) {
          emit_vstr(7,HOST_TEMPREG);
        }else{
          emit_vstr(7,temp);
        }
      }
    }
    else {
      if(opcode2[i]==0x10) {
        emit_flds(temp,15);
        if(((source[i]>>11)&0x1f)!=((source[i]>>6)&0x1f)) {
          emit_readword((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_simple[(source[i]>>6)&0x1f],temp);
        }
        if((source[i]&0x3f)==0) emit_fadds(15,15,15);
        if((source[i]&0x3f)==1) emit_fsubs(15,15,15);
        if((source[i]&0x3f)==2) emit_fmuls(15,15,15);
        if((source[i]&0x3f)==3) emit_fdivs(15,15,15);
        emit_fsts(15,temp);
      }
      else if(opcode2[i]==0x11) {
        emit_vldr(temp,7);
        if(((source[i]>>11)&0x1f)!=((source[i]>>6)&0x1f)) {
          emit_readword((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_double[(source[i]>>6)&0x1f],temp);
        }
        if((source[i]&0x3f)==0) emit_faddd(7,7,7);
        if((source[i]&0x3f)==1) emit_fsubd(7,7,7);
        if((source[i]&0x3f)==2) emit_fmuld(7,7,7);
        if((source[i]&0x3f)==3) emit_fdivd(7,7,7);
        emit_vstr(7,temp);
      }
    }
    return;
  }
  #endif
#endif

  u_int hr,reglist=0;
  for(hr=0;hr<HOST_REGS;hr++) {
    if(i_regs->regmap[hr]>=0) reglist|=1<<hr;
  }

  signed char fs=get_reg(i_regs->regmap,FSREG);
  if(opcode2[i]==0x10) { // Single precision
    save_regs(reglist);
    switch(source[i]&0x3f)
    {
      case 0x00: case 0x01: case 0x02: case 0x03:
        emit_addimm(FP,fp_fcr31,ARG1_REG);
        emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_simple[(source[i]>>11)&0x1f],ARG2_REG);
        emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_simple[(source[i]>>16)&0x1f],ARG3_REG);
        emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_simple[(source[i]>> 6)&0x1f],ARG4_REG);
        break;
     case 0x04:
        emit_addimm(FP,fp_fcr31,ARG1_REG);
        emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_simple[(source[i]>>11)&0x1f],ARG2_REG);
        emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_simple[(source[i]>> 6)&0x1f],ARG3_REG);
        break;
     case 0x05: case 0x06: case 0x07:
        emit_addimm(FP,fp_fcr31,ARG1_REG);
        emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_simple[(source[i]>>11)&0x1f],ARG1_REG);
        emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_simple[(source[i]>> 6)&0x1f],ARG2_REG);
        break;
    }
    switch(source[i]&0x3f)
    {
      case 0x00: emit_call((int)add_s);break;
      case 0x01: emit_call((int)sub_s);break;
      case 0x02: emit_call((int)mul_s);break;
      case 0x03: emit_call((int)div_s);break;
      case 0x04: emit_call((int)sqrt_s);break;
      case 0x05: emit_call((int)abs_s);break;
      case 0x06: emit_call((int)mov_s);break;
      case 0x07: emit_call((int)neg_s);break;
    }
    restore_regs(reglist);
  }
  if(opcode2[i]==0x11) { // Double precision
    save_regs(reglist);

    switch(source[i]&0x3f)
    {
      case 0x00: case 0x01: case 0x02: case 0x03:
        emit_addimm(FP,fp_fcr31,ARG1_REG);
        emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_double[(source[i]>>11)&0x1f],ARG2_REG);
        emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_double[(source[i]>>16)&0x1f],ARG3_REG);
        emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_double[(source[i]>> 6)&0x1f],ARG4_REG);
        break;
     case 0x04:
        emit_addimm(FP,fp_fcr31,ARG1_REG);
        emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_double[(source[i]>>11)&0x1f],ARG2_REG);
        emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_double[(source[i]>> 6)&0x1f],ARG3_REG);
        break;
     case 0x05: case 0x06: case 0x07:
        emit_addimm(FP,fp_fcr31,ARG1_REG);
        emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_double[(source[i]>>11)&0x1f],ARG1_REG);
        emit_readptr((u_int)&g_dev.r4300.new_dynarec_hot_state.cp1_regs_double[(source[i]>> 6)&0x1f],ARG2_REG);
        break;
    }
    switch(source[i]&0x3f)
    {
      case 0x00: emit_call((int)add_d);break;
      case 0x01: emit_call((int)sub_d);break;
      case 0x02: emit_call((int)mul_d);break;
      case 0x03: emit_call((int)div_d);break;
      case 0x04: emit_call((int)sqrt_d);break;
      case 0x05: emit_call((int)abs_d);break;
      case 0x06: emit_call((int)mov_d);break;
      case 0x07: emit_call((int)neg_d);break;
    }
    restore_regs(reglist);
  }
}

static void multdiv_assemble_arm(int i,struct regstat *i_regs)
{
  //  case 0x18: MULT
  //  case 0x19: MULTU
  //  case 0x1A: DIV
  //  case 0x1B: DIVU
  //  case 0x1C: DMULT
  //  case 0x1D: DMULTU
  //  case 0x1E: DDIV
  //  case 0x1F: DDIVU
  if(rs1[i]&&rs2[i])
  {
    if((opcode2[i]&4)==0) // 32-bit
    {
#ifndef INTERPRET_MULT
      if((opcode2[i]==0x18) || (opcode2[i]==0x19))
      {
        signed char m1=get_reg(i_regs->regmap,rs1[i]);
        signed char m2=get_reg(i_regs->regmap,rs2[i]);
        signed char hi=get_reg(i_regs->regmap,HIREG);
        signed char lo=get_reg(i_regs->regmap,LOREG);
        assert(m1>=0);
        assert(m2>=0);
        assert(hi>=0);
        assert(lo>=0);

        if(opcode2[i]==0x18) //MULT
          emit_smull(m1,m2,hi,lo);
        else if(opcode2[i]==0x19) //MULTU
          emit_umull(m1,m2,hi,lo);
      }
      else
#endif
#ifndef INTERPRET_DIV
      if((opcode2[i]==0x1A) || (opcode2[i]==0x1B))
      {
        signed char d1=get_reg(i_regs->regmap,rs1[i]); // dividend
        signed char d2=get_reg(i_regs->regmap,rs2[i]); // divisor
        assert(d1>=0);
        assert(d2>=0);
        signed char quotient=get_reg(i_regs->regmap,LOREG);
        signed char remainder=get_reg(i_regs->regmap,HIREG);
        assert(quotient>=0);
        assert(remainder>=0);

        if(opcode2[i]==0x1A) //DIV
        {
          if(arm_cpu_features.IDIVa)
          {
            emit_test(d2,d2);
            emit_jeq((int)out+16); // Division by zero
            emit_sdiv(d1,d2,quotient);
            emit_mul(quotient,d2,remainder);
            emit_sub(d1,remainder,remainder);
          }
          else
          {
            emit_movs(d1,remainder);
            emit_negmi(remainder,remainder);
            emit_movs(d2,HOST_TEMPREG);
            emit_jeq((int)out+52); // Division by zero
            emit_negmi(HOST_TEMPREG,HOST_TEMPREG);
            emit_clz(HOST_TEMPREG,quotient);
            emit_shl(HOST_TEMPREG,quotient,HOST_TEMPREG);
            emit_orimm(quotient,1<<31,quotient);
            emit_shr(quotient,quotient,quotient);
            emit_cmp(remainder,HOST_TEMPREG);
            emit_subcs(remainder,HOST_TEMPREG,remainder);
            emit_adcs(quotient,quotient,quotient);
            emit_shrimm(HOST_TEMPREG,1,HOST_TEMPREG);
            emit_jcc((int)out-16); // -4
            emit_teq(d1,d2);
            emit_negmi(quotient,quotient);
            emit_test(d1,d1);
            emit_negmi(remainder,remainder);
          }
        }
        else if(opcode2[i]==0x1B) //DIVU
        {
          emit_test(d2,d2);

          if(arm_cpu_features.IDIVa)
          {
            emit_jeq((int)out+16); // Division by zero
            emit_udiv(d1,d2,quotient);
            emit_mul(quotient,d2,remainder);
            emit_sub(d1,remainder,remainder);
          }
          else
          {
            emit_jeq((int)out+44); // Division by zero
            emit_clz(d2,HOST_TEMPREG);
            emit_movimm(1<<31,quotient);
            emit_shl(d2,HOST_TEMPREG,d2);
            emit_mov(d1,remainder);
            emit_shr(quotient,HOST_TEMPREG,quotient);
            emit_cmp(remainder,d2);
            emit_subcs(remainder,d2,remainder);
            emit_adcs(quotient,quotient,quotient);
            emit_shrcc_imm(d2,1,d2);
            emit_jcc((int)out-16); // -4
          }
        }
      }
      else
#endif
      {
        u_int reglist=0;
        signed char r1=get_reg(i_regs->regmap,rs1[i]);
        signed char r2=get_reg(i_regs->regmap,rs2[i]);
        signed char hi=get_reg(i_regs->regmap,HIREG);
        signed char lo=get_reg(i_regs->regmap,LOREG);
        assert(r1>=0);
        assert(r2>=0);

        for(int hr=0;hr<HOST_REGS;hr++) {
          if(i_regs->regmap[hr]>=0) reglist|=1<<hr;
        }

        //Don't save lo and hi regs are they will be overwritten anyway
        if(hi>=0) reglist&=~(1<<hi);
        if(lo>=0) reglist&=~(1<<lo);

        emit_writeword(r1,(intptr_t)&g_dev.r4300.new_dynarec_hot_state.rs);
        emit_writeword(r2,(intptr_t)&g_dev.r4300.new_dynarec_hot_state.rt);

        save_regs(reglist);

        if(opcode2[i]==0x18)
          emit_call((intptr_t)cached_interp_MULT);
        else if(opcode2[i]==0x19)
          emit_call((intptr_t)cached_interp_MULTU);
        else if(opcode2[i]==0x1A)
          emit_call((intptr_t)cached_interp_DIV);
        else if(opcode2[i]==0x1B)
          emit_call((intptr_t)cached_interp_DIVU);

        restore_regs(reglist);

        if(hi>=0) emit_loadreg(HIREG,hi);
        if(lo>=0) emit_loadreg(LOREG,lo);
      }
    }
    else // 64-bit
    {
#ifndef INTERPRET_MULT64
      if(opcode2[i]==0x1C||opcode2[i]==0x1D)
      {
        signed char b_1=get_reg(i_regs->regmap,rs1[i]|64);
        signed char b_0=get_reg(i_regs->regmap,rs1[i]);
        signed char c_1=get_reg(i_regs->regmap,rs2[i]|64);
        signed char c_0=get_reg(i_regs->regmap,rs2[i]);
        assert(b_1>=0);
        assert(b_0>=0);
        assert(c_1>=0);
        assert(c_0>=0);
        signed char a_3=get_reg(i_regs->regmap,HIREG|64);
        signed char a_2=get_reg(i_regs->regmap,HIREG);
        signed char a_1=get_reg(i_regs->regmap,LOREG|64);
        signed char a_0=get_reg(i_regs->regmap,LOREG);
        assert(a_3>=0);
        assert(a_2>=0);
        assert(a_1>=0);
        assert(a_0>=0);

        if(opcode2[i]==0x1C) // DMULT
        {
          emit_umull(b_0,c_0,a_1,a_0);
          emit_zeroreg(a_2);
          emit_smlal(b_0,c_1,a_2,a_1);
          emit_testimm(b_0,0x80000000);
          emit_addne(a_2,c_1,a_2);
          emit_zeroreg(a_3);
          emit_smlal(b_1,c_0,a_3,a_1);
          emit_testimm(c_0,0x80000000);
          emit_addne(a_3,b_1,a_3);
          emit_sarimm(a_2,31,HOST_TEMPREG);
          emit_adds(a_2,a_3,a_2);
          emit_adcsarimm(HOST_TEMPREG,a_3,a_3,31);
          emit_smlal(b_1,c_1,a_3,a_2);
        }
        else if(opcode2[i]==0x1D) // DMULTU
        {
          emit_umull(b_0,c_0,a_1,a_0);
          emit_zeroreg(a_2);
          emit_umlal(b_0,c_1,a_2,a_1);
          emit_zeroreg(a_3);
          emit_umlal(b_1,c_0,a_3,a_1);
          emit_zeroreg(HOST_TEMPREG);
          emit_adds(a_2,a_3,a_2);
          emit_adcimm(HOST_TEMPREG,0,a_3);
          emit_umlal(b_1,c_1,a_3,a_2);
        }
      }
      else
#endif
      {
        u_int reglist=0;
        signed char r1h=get_reg(i_regs->regmap,rs1[i]|64);
        signed char r1l=get_reg(i_regs->regmap,rs1[i]);
        signed char r2h=get_reg(i_regs->regmap,rs2[i]|64);
        signed char r2l=get_reg(i_regs->regmap,rs2[i]);
        signed char hih=get_reg(i_regs->regmap,HIREG|64);
        signed char hil=get_reg(i_regs->regmap,HIREG);
        signed char loh=get_reg(i_regs->regmap,LOREG|64);
        signed char lol=get_reg(i_regs->regmap,LOREG);
        assert(r1h>=0);
        assert(r2h>=0);
        assert(r1l>=0);
        assert(r2l>=0);

        for(int hr=0;hr<HOST_REGS;hr++) {
          if(i_regs->regmap[hr]>=0) reglist|=1<<hr;
        }

        //Don't save lo and hi regs are they will be overwritten anyway
        if(hih>=0) reglist&=~(1<<hih);
        if(hil>=0) reglist&=~(1<<hil);
        if(loh>=0) reglist&=~(1<<loh);
        if(lol>=0) reglist&=~(1<<lol);

        emit_writeword(r1l,(int)&g_dev.r4300.new_dynarec_hot_state.rs);
        emit_writeword(r1h,((int)&g_dev.r4300.new_dynarec_hot_state.rs)+4);
        emit_writeword(r2l,(int)&g_dev.r4300.new_dynarec_hot_state.rt);
        emit_writeword(r2h,((int)&g_dev.r4300.new_dynarec_hot_state.rt)+4);

        save_regs(reglist);

        if(opcode2[i]==0x1C) // DMULT
          emit_call((int)cached_interp_DMULT);
        else if(opcode2[i]==0x1D) // DMULTU
          emit_call((int)cached_interp_DMULTU);
        else if(opcode2[i]==0x1E) // DDIV
          emit_call((int)cached_interp_DDIV);
        else if(opcode2[i]==0x1F) // DDIVU
          emit_call((int)cached_interp_DDIVU);

        restore_regs(reglist);
        if(hih>=0) emit_loadreg(HIREG|64,hih);
        if(hil>=0) emit_loadreg(HIREG,hil);
        if(loh>=0) emit_loadreg(LOREG|64,loh);
        if(lol>=0) emit_loadreg(LOREG,lol);
      }
    }
  }
  else
  {
    // Multiply by zero is zero.
    // MIPS does not have a divide by zero exception.
    // The result is undefined, we return zero.
    signed char hr=get_reg(i_regs->regmap,HIREG);
    signed char lr=get_reg(i_regs->regmap,LOREG);
    if(hr>=0) emit_zeroreg(hr);
    if(lr>=0) emit_zeroreg(lr);
  }
}
#define multdiv_assemble multdiv_assemble_arm

static void do_preload_rhash(int r) {
  // Don't need this for ARM.  On x86, this puts the value 0xf8 into the
  // register.  On ARM the hash can be done with a single instruction (below)
}

static void do_preload_rhtbl(int ht) {
  emit_addimm(FP,fp_mini_ht(0,0),ht);
}

static void do_rhash(int rs,int rh) {
  emit_andimm(rs,0xf8,rh);
}

static void do_miniht_load(int ht,int rh) {
  assem_debug("ldr %s,[%s,%s]!",regname[rh],regname[ht],regname[rh]);
  output_w32(0xe7b00000|rd_rn_rm(rh,ht,rh));
}

static void do_miniht_jump(int rs,int rh,int ht) {
  emit_cmp(rh,rs);
  emit_ldreq_indexed(ht,4,15);
  #ifdef CORTEX_A8_BRANCH_PREDICTION_HACK
  emit_mov(rs,7);
  emit_jmp(jump_vaddr_reg[7]);
  #else
  emit_jmp(jump_vaddr_reg[rs]);
  #endif
}

static void do_miniht_insert(u_int return_address,int rt,int temp) {
  #ifdef ARMv5_ONLY
  emit_movimm(return_address,rt); // PC into link register
  add_to_linker((int)out,return_address,1);
  emit_pcreladdr(temp);
  emit_writeword(rt,(u_int)&g_dev.r4300.new_dynarec_hot_state.mini_ht[(return_address&0xFF)>>3][0]);
  emit_writeword(temp,(u_int)&g_dev.r4300.new_dynarec_hot_state.mini_ht[(return_address&0xFF)>>3][1]);
  #else
  emit_movw(return_address&0x0000FFFF,rt);
  add_to_linker((int)out,return_address,1);
  emit_pcreladdr(temp);
  emit_writeword(temp,(u_int)&g_dev.r4300.new_dynarec_hot_state.mini_ht[(return_address&0xFF)>>3][1]);
  emit_movt(return_address&0xFFFF0000,rt);
  emit_writeword(rt,(u_int)&g_dev.r4300.new_dynarec_hot_state.mini_ht[(return_address&0xFF)>>3][0]);
  #endif
}

// Clearing the cache is rather slow on ARM Linux, so mark the areas
// that need to be cleared, and then only clear these areas once.
static void do_clear_cache(void)
{
  int i,j;
  for (i=0;i<(1<<(TARGET_SIZE_2-17));i++)
  {
    u_int bitmap=needs_clear_cache[i];
    if(bitmap) {
      u_int start,end;
      for(j=0;j<32;j++)
      {
        if(bitmap&(1<<j)) {
          start=(int)base_addr+i*131072+j*4096;
          end=start+4095;
          j++;
          while(j<32) {
            if(bitmap&(1<<j)) {
              end+=4096;
              j++;
            }else{
              cache_flush((void *)start,(void *)end);
              break;
            }
          }
        }
      }
      needs_clear_cache[i]=0;
    }
  }
}

static void invalidate_addr(u_int addr)
{
  invalidate_block(addr>>12);
}

// CPU-architecture-specific initialization
static void arch_init(void) {

  detect_arm_cpu_features();
  print_arm_cpu_features();

  g_dev.r4300.new_dynarec_hot_state.rounding_modes[0]=0x0<<22; // round
  g_dev.r4300.new_dynarec_hot_state.rounding_modes[1]=0x3<<22; // trunc
  g_dev.r4300.new_dynarec_hot_state.rounding_modes[2]=0x1<<22; // ceil
  g_dev.r4300.new_dynarec_hot_state.rounding_modes[3]=0x2<<22; // floor

  jump_table_symbols[0] = (int) cached_interp_TLBR;
  jump_table_symbols[1] = (int) cached_interp_TLBP;
  jump_table_symbols[2] = (int) cached_interp_MULT;
  jump_table_symbols[3] = (int) cached_interp_MULTU;
  jump_table_symbols[4] = (int) cached_interp_DIV;
  jump_table_symbols[5] = (int) cached_interp_DIVU;
  jump_table_symbols[6] = (int) cached_interp_DMULT;
  jump_table_symbols[7] = (int) cached_interp_DMULTU;
  jump_table_symbols[8] = (int) cached_interp_DDIV;
  jump_table_symbols[9] = (int) cached_interp_DDIVU;

  #ifdef RAM_OFFSET
  g_dev.r4300.new_dynarec_hot_state.ram_offset=((int)g_dev.rdram.dram-(int)0x80000000)>>2;
  #endif

  // Trampolines for jumps >32M
  int *ptr,*ptr2;
  ptr=(int *)jump_table_symbols;
  ptr2=(int *)((char *)base_addr+(1<<TARGET_SIZE_2)-JUMP_TABLE_SIZE);
  while((char *)ptr<(char *)jump_table_symbols+sizeof(jump_table_symbols))
  {
    int offset=*ptr-(int)ptr2-8;
    if(offset>=-33554432&&offset<33554432) {
      *ptr2=0xea000000|((offset>>2)&0xffffff); // direct branch
    }else{
      *ptr2=0xe51ff004; // ldr pc,[pc,#-4]
    }
    ptr2++;
    *ptr2=*ptr;
    ptr++;
    ptr2++;
  }

  // Jumping thru the trampolines created above slows things down by about 1%.
  // If part of the cache is beyond the 32M limit, avoid using this area
  // initially.  It will be used later if the cache gets full.
  /*if((u_int)dyna_linker-33554432>(u_int)base_addr) {
    if((u_int)dyna_linker-33554432<(u_int)base_addr+(1<<(TARGET_SIZE_2-1))) {
      out=(u_char *)(((u_int)dyna_linker-33554432)&~4095);
      expirep=((((int)out-(int)base_addr)>>(TARGET_SIZE_2-16))+16384)&65535;
    }
  }*/
}
