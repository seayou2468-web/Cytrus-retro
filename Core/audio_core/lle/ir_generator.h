// Generated stubs from decoder.h
    void add(Ab a0, Bx a1) { AddInst(IROp::TeakGeneric); }
    void add(Bx a0, Ax a1) { AddInst(IROp::TeakGeneric); }
    void add(Px a0, Bx a1) { AddInst(IROp::TeakGeneric); }
    void add_add(ArpRn1 a0, ArpStep1 a1, ArpStep1 a2, Ab a3) { AddInst(IROp::TeakGeneric); }
    void add_p1(Ax a0) { AddInst(IROp::TeakGeneric); }
    void add_sub(ArpRn1 a0, ArpStep1 a1, ArpStep1 a2, Ab a3) { AddInst(IROp::TeakGeneric); }
    void add_sub_i_mov_j(ArpRn1 a0, ArpStep1 a1, ArpStep1 a2, Ab a3) { AddInst(IROp::TeakGeneric); }
    void add_sub_j_mov_i(ArpRn1 a0, ArpStep1 a1, ArpStep1 a2, Ab a3) { AddInst(IROp::TeakGeneric); }
    void add_sub_sv(ArRn1 a0, ArStep1 a1, Ab a2) { AddInst(IROp::TeakGeneric); }
    void addhp(ArRn2 a0, ArStep2 a1, Px a2, Ax a3) { AddInst(IROp::TeakGeneric); }
    void alb(Alb a0, Imm16 a1, SttMod a2) { AddInst(IROp::TeakGeneric); }
    void alb(Alb a0, Imm16 a1, Register a2) { AddInst(IROp::TeakGeneric); }
    void alb(Alb a0, Imm16 a1, MemImm8 a2) { AddInst(IROp::TeakGeneric); }
    void alb(Alb a0, Imm16 a1, Rn a2, StepZIDS a3) { AddInst(IROp::TeakGeneric); }
    void alb_r6(Alb a0, Imm16 a1) { AddInst(IROp::TeakGeneric); }
    void alm(Alm a0, MemImm8 a1, Ax a2) { AddInst(IROp::TeakGeneric); }
    void alm(Alm a0, Rn a1, StepZIDS a2, Ax a3) { AddInst(IROp::TeakGeneric); }
    void alm(Alm a0, Register a1, Ax a2) { AddInst(IROp::TeakGeneric); }
    void alm_r6(Alm a0, Ax a1) { AddInst(IROp::TeakGeneric); }
    void alu(Alu a0, Imm16 a1, Ax a2) { AddInst(IROp::TeakGeneric); }
    void alu(Alu a0, Imm8 a1, Ax a2) { AddInst(IROp::TeakGeneric); }
    void alu(Alu a0, MemR7Imm7s a1, Ax a2) { AddInst(IROp::TeakGeneric); }
    void alu(Alu a0, MemImm16 a1, Ax a2) { AddInst(IROp::TeakGeneric); }
    void alu(Alu a0, MemR7Imm16 a1, Ax a2) { AddInst(IROp::TeakGeneric); }
    void and_(Ab a0, Ab a1, Ax a2) { AddInst(IROp::TeakGeneric); }
    void app(Ab a0, SumBase a1, bool a2, bool a3, bool a4, bool a5) { AddInst(IROp::TeakGeneric); }
    void banke(BankFlags a0) { AddInst(IROp::TeakGeneric); }
    void bankr(Ar a0, Arp a1) { AddInst(IROp::TeakGeneric); }
    void bankr(Ar a0) { AddInst(IROp::TeakGeneric); }
    void bankr(Arp a0) { AddInst(IROp::TeakGeneric); }
    void bitrev(Rn a0) { AddInst(IROp::TeakGeneric); }
    void bitrev_dbrv(Rn a0) { AddInst(IROp::TeakGeneric); }
    void bitrev_ebrv(Rn a0) { AddInst(IROp::TeakGeneric); }
    void bkrep(Register a0, Address18_16 a1, Address18_2 a2) { AddInst(IROp::TeakGeneric); }
    void bkrep(Imm8 a0, Address16 a1) { AddInst(IROp::TeakGeneric); }
    void bkrep_r6(Address18_16 a0, Address18_2 a1) { AddInst(IROp::TeakGeneric); }
    void bkreprst(ArRn2 a0) { AddInst(IROp::TeakGeneric); }
    void bkreprst_memsp() { AddInst(IROp::TeakGeneric); }
    void bkrepsto(ArRn2 a0) { AddInst(IROp::TeakGeneric); }
    void bkrepsto_memsp() { AddInst(IROp::TeakGeneric); }
    void br(Address18_16 a0, Address18_2 a1, Cond a2) { AddInst(IROp::TeakGeneric); }
    void brr(RelAddr7 a0, Cond a1) { AddInst(IROp::TeakGeneric); }
    void call(Address18_16 a0, Address18_2 a1, Cond a2) { AddInst(IROp::TeakGeneric); }
    void calla(Ax a0) { AddInst(IROp::TeakGeneric); }
    void calla(Axl a0) { AddInst(IROp::TeakGeneric); }
    void callr(RelAddr7 a0, Cond a1) { AddInst(IROp::TeakGeneric); }
    void cbs(Axh a0, Bxh a1, CbsCond a2) { AddInst(IROp::TeakGeneric); }
    void cbs(Axh a0, CbsCond a1) { AddInst(IROp::TeakGeneric); }
    void cbs(ArpRn1 a0, ArpStep1 a1, ArpStep1 a2, CbsCond a3) { AddInst(IROp::TeakGeneric); }
    void clr(Ab a0, Ab a1) { AddInst(IROp::TeakGeneric); }
    void clrr(Ab a0, Ab a1) { AddInst(IROp::TeakGeneric); }
    void cmp(Bx a0, Ax a1) { AddInst(IROp::TeakGeneric); }
    void cmp(Ax a0, Bx a1) { AddInst(IROp::TeakGeneric); }
    void cmp_p1_to(Ax a0) { AddInst(IROp::TeakGeneric); }
    void divs(MemImm8 a0, Ax a1) { AddInst(IROp::TeakGeneric); }
    void exchange_iaj(Axh a0, ArpRn2 a1, ArpStep2 a2, ArpStep2 a3) { AddInst(IROp::TeakGeneric); }
    void exchange_jai(Axh a0, ArpRn2 a1, ArpStep2 a2, ArpStep2 a3) { AddInst(IROp::TeakGeneric); }
    void exchange_riaj(Axh a0, ArpRn2 a1, ArpStep2 a2, ArpStep2 a3) { AddInst(IROp::TeakGeneric); }
    void exchange_rjai(Axh a0, ArpRn2 a1, ArpStep2 a2, ArpStep2 a3) { AddInst(IROp::TeakGeneric); }
    void exp(Rn a0, StepZIDS a1) { AddInst(IROp::TeakGeneric); }
    void exp(Bx a0) { AddInst(IROp::TeakGeneric); }
    void exp(Register a0) { AddInst(IROp::TeakGeneric); }
    void exp(Rn a0, StepZIDS a1, Ax a2) { AddInst(IROp::TeakGeneric); }
    void exp(Bx a0, Ax a1) { AddInst(IROp::TeakGeneric); }
    void exp(Register a0, Ax a1) { AddInst(IROp::TeakGeneric); }
    void exp_r6(Ax a0) { AddInst(IROp::TeakGeneric); }
    void lim(Ax a0, Ax a1) { AddInst(IROp::TeakGeneric); }
    void load_modi(Imm9 a0) { AddInst(IROp::TeakGeneric); }
    void load_modj(Imm9 a0) { AddInst(IROp::TeakGeneric); }
    void load_movpd(Imm2 a0) { AddInst(IROp::TeakGeneric); }
    void load_page(Imm8 a0) { AddInst(IROp::TeakGeneric); }
    void load_ps(Imm2 a0) { AddInst(IROp::TeakGeneric); }
    void load_ps01(Imm4 a0) { AddInst(IROp::TeakGeneric); }
    void load_stepi(Imm7s a0) { AddInst(IROp::TeakGeneric); }
    void load_stepj(Imm7s a0) { AddInst(IROp::TeakGeneric); }
    void mac1(ArpRn1 a0, ArpStep1 a1, ArpStep1 a2, Ax a3) { AddInst(IROp::TeakGeneric); }
    void mac_x1to0(Ax a0) { AddInst(IROp::TeakGeneric); }
    void max2_vtr(Ax a0) { AddInst(IROp::TeakGeneric); }
    void max2_vtr(Ax a0, Bx a1) { AddInst(IROp::TeakGeneric); }
    void max2_vtr_movh(Bx a0, Ax a1, ArRn1 a2, ArStep1 a3) { AddInst(IROp::TeakGeneric); }
    void max2_vtr_movh(Ax a0, Bx a1, ArRn1 a2, ArStep1 a3) { AddInst(IROp::TeakGeneric); }
    void max2_vtr_movij(Ax a0, Bx a1, ArpRn1 a2, ArpStep1 a3, ArpStep1 a4) { AddInst(IROp::TeakGeneric); }
    void max2_vtr_movji(Ax a0, Bx a1, ArpRn1 a2, ArpStep1 a3, ArpStep1 a4) { AddInst(IROp::TeakGeneric); }
    void max2_vtr_movl(Bx a0, Ax a1, ArRn1 a2, ArStep1 a3) { AddInst(IROp::TeakGeneric); }
    void max2_vtr_movl(Ax a0, Bx a1, ArRn1 a2, ArStep1 a3) { AddInst(IROp::TeakGeneric); }
    void max_ge(Ax a0, StepZIDS a1) { AddInst(IROp::TeakGeneric); }
    void max_ge_r0(Ax a0, StepZIDS a1) { AddInst(IROp::TeakGeneric); }
    void max_gt(Ax a0, StepZIDS a1) { AddInst(IROp::TeakGeneric); }
    void max_gt_r0(Ax a0, StepZIDS a1) { AddInst(IROp::TeakGeneric); }
    void min2_vtr(Ax a0) { AddInst(IROp::TeakGeneric); }
    void min2_vtr(Ax a0, Bx a1) { AddInst(IROp::TeakGeneric); }
    void min2_vtr_movh(Bx a0, Ax a1, ArRn1 a2, ArStep1 a3) { AddInst(IROp::TeakGeneric); }
    void min2_vtr_movh(Ax a0, Bx a1, ArRn1 a2, ArStep1 a3) { AddInst(IROp::TeakGeneric); }
    void min2_vtr_movij(Ax a0, Bx a1, ArpRn1 a2, ArpStep1 a3, ArpStep1 a4) { AddInst(IROp::TeakGeneric); }
    void min2_vtr_movji(Ax a0, Bx a1, ArpRn1 a2, ArpStep1 a3, ArpStep1 a4) { AddInst(IROp::TeakGeneric); }
    void min2_vtr_movl(Bx a0, Ax a1, ArRn1 a2, ArStep1 a3) { AddInst(IROp::TeakGeneric); }
    void min2_vtr_movl(Ax a0, Bx a1, ArRn1 a2, ArStep1 a3) { AddInst(IROp::TeakGeneric); }
    void min_le(Ax a0, StepZIDS a1) { AddInst(IROp::TeakGeneric); }
    void min_le_r0(Ax a0, StepZIDS a1) { AddInst(IROp::TeakGeneric); }
    void min_lt(Ax a0, StepZIDS a1) { AddInst(IROp::TeakGeneric); }
    void min_lt_r0(Ax a0, StepZIDS a1) { AddInst(IROp::TeakGeneric); }
    void mma(RegName a0, bool a1, bool a2, bool a3, bool a4, SumBase a5, bool a6, bool a7, bool a8, bool a9) { AddInst(IROp::TeakGeneric); }
    void mma(ArpRn2 a0, ArpStep2 a1, ArpStep2 a2, bool a3, bool a4, RegName a5, bool a6, bool a7, bool a8, bool a9, SumBase a10, bool a11, bool a12, bool a13, bool a14) { AddInst(IROp::TeakGeneric); }
    void mma(ArpRn1 a0, ArpStep1 a1, ArpStep1 a2, bool a3, bool a4, RegName a5, bool a6, bool a7, bool a8, bool a9, SumBase a10, bool a11, bool a12, bool a13, bool a14) { AddInst(IROp::TeakGeneric); }
    void mma_mov(Axh a0, Bxh a1, ArRn1 a2, ArStep1 a3, RegName a4, bool a5, bool a6, bool a7, bool a8, SumBase a9, bool a10, bool a11, bool a12, bool a13) { AddInst(IROp::TeakGeneric); }
    void mma_mov(ArRn2 a0, ArStep1 a1, RegName a2, bool a3, bool a4, bool a5, bool a6, SumBase a7, bool a8, bool a9, bool a10, bool a11) { AddInst(IROp::TeakGeneric); }
    void mma_mx_xy(ArRn1 a0, ArStep1 a1, RegName a2, bool a3, bool a4, bool a5, bool a6, SumBase a7, bool a8, bool a9, bool a10, bool a11) { AddInst(IROp::TeakGeneric); }
    void mma_my_my(ArRn1 a0, ArStep1 a1, RegName a2, bool a3, bool a4, bool a5, bool a6, SumBase a7, bool a8, bool a9, bool a10, bool a11) { AddInst(IROp::TeakGeneric); }
    void mma_xy_mx(ArRn1 a0, ArStep1 a1, RegName a2, bool a3, bool a4, bool a5, bool a6, SumBase a7, bool a8, bool a9, bool a10, bool a11) { AddInst(IROp::TeakGeneric); }
    void moda3(Moda3 a0, Bx a1, Cond a2) { AddInst(IROp::TeakGeneric); }
    void moda4(Moda4 a0, Ax a1, Cond a2) { AddInst(IROp::TeakGeneric); }
    void modr(Rn a0, StepZIDS a1) { AddInst(IROp::TeakGeneric); }
    void modr_d2(Rn a0) { AddInst(IROp::TeakGeneric); }
    void modr_d2_dmod(Rn a0) { AddInst(IROp::TeakGeneric); }
    void modr_ddmod(ArpRn2 a0, ArpStep2 a1, ArpStep2 a2) { AddInst(IROp::TeakGeneric); }
    void modr_demod(ArpRn2 a0, ArpStep2 a1, ArpStep2 a2) { AddInst(IROp::TeakGeneric); }
    void modr_dmod(Rn a0, StepZIDS a1) { AddInst(IROp::TeakGeneric); }
    void modr_edmod(ArpRn2 a0, ArpStep2 a1, ArpStep2 a2) { AddInst(IROp::TeakGeneric); }
    void modr_eemod(ArpRn2 a0, ArpStep2 a1, ArpStep2 a2) { AddInst(IROp::TeakGeneric); }
    void modr_i2(Rn a0) { AddInst(IROp::TeakGeneric); }
    void modr_i2_dmod(Rn a0) { AddInst(IROp::TeakGeneric); }
    void mov(MemImm8 a0, Ablh a1) { AddInst(IROp::TeakGeneric); }
    void mov(Imm16 a0, SttMod a1) { AddInst(IROp::TeakGeneric); }
    void mov(Register a0, Bx a1) { AddInst(IROp::TeakGeneric); }
    void mov(Axl a0, MemR7Imm16 a1) { AddInst(IROp::TeakGeneric); }
    void mov(ArArp a0, ArRn1 a1, ArStep1 a2) { AddInst(IROp::TeakGeneric); }
    void mov(ArArpSttMod a0, MemR7Imm16 a1) { AddInst(IROp::TeakGeneric); }
    void mov(Imm8 a0, Axl a1) { AddInst(IROp::TeakGeneric); }
    void mov(Abl a0, ArArp a1) { AddInst(IROp::TeakGeneric); }
    void mov(Imm16 a0, Bx a1) { AddInst(IROp::TeakGeneric); }
    void mov(Axl a0, MemImm16 a1) { AddInst(IROp::TeakGeneric); }
    void mov(SttMod a0, ArRn1 a1, ArStep1 a2) { AddInst(IROp::TeakGeneric); }
    void mov(MemR7Imm16 a0, Ax a1) { AddInst(IROp::TeakGeneric); }
    void mov(MemR7Imm16 a0, ArArpSttMod a1) { AddInst(IROp::TeakGeneric); }
    void mov(Abl a0, SttMod a1) { AddInst(IROp::TeakGeneric); }
    void mov(Axl a0, MemR7Imm7s a1) { AddInst(IROp::TeakGeneric); }
    void mov(ArArp a0, Abl a1) { AddInst(IROp::TeakGeneric); }
    void mov(MemImm8 a0, Ab a1) { AddInst(IROp::TeakGeneric); }
    void mov(Register a0, Rn a1, StepZIDS a2) { AddInst(IROp::TeakGeneric); }
    void mov(Imm8s a0, Axh a1) { AddInst(IROp::TeakGeneric); }
    void mov(Rn a0, StepZIDS a1, Bx a2) { AddInst(IROp::TeakGeneric); }
    void mov(ArRn1 a0, ArStep1 a1, ArArp a2) { AddInst(IROp::TeakGeneric); }
    void mov(Register a0, Register a1) { AddInst(IROp::TeakGeneric); }
    void mov(MemR7Imm7s a0, Ax a1) { AddInst(IROp::TeakGeneric); }
    void mov(Ablh a0, MemImm8 a1) { AddInst(IROp::TeakGeneric); }
    void mov(MemImm8 a0, RnOld a1) { AddInst(IROp::TeakGeneric); }
    void mov(SttMod a0, Abl a1) { AddInst(IROp::TeakGeneric); }
    void mov(ArRn1 a0, ArStep1 a1, SttMod a2) { AddInst(IROp::TeakGeneric); }
    void mov(Ab a0, Ab a1) { AddInst(IROp::TeakGeneric); }
    void mov(Imm16 a0, Register a1) { AddInst(IROp::TeakGeneric); }
    void mov(RnOld a0, MemImm8 a1) { AddInst(IROp::TeakGeneric); }
    void mov(MemImm16 a0, Ax a1) { AddInst(IROp::TeakGeneric); }
    void mov(Imm16 a0, ArArp a1) { AddInst(IROp::TeakGeneric); }
    void mov(Imm8s a0, RnOld a1) { AddInst(IROp::TeakGeneric); }
    void mov(Rn a0, StepZIDS a1, Register a2) { AddInst(IROp::TeakGeneric); }
    void mov2(Px a0, ArRn2 a1, ArStep2 a2) { AddInst(IROp::TeakGeneric); }
    void mov2(ArRn2 a0, ArStep2 a1, Px a2) { AddInst(IROp::TeakGeneric); }
    void mov2_abh_m(Abh a0, Abh a1, ArRn1 a2, ArStep1 a3) { AddInst(IROp::TeakGeneric); }
    void mov2_ax_mij(Ab a0, ArpRn1 a1, ArpStep1 a2, ArpStep1 a3) { AddInst(IROp::TeakGeneric); }
    void mov2_ax_mji(Ab a0, ArpRn1 a1, ArpStep1 a2, ArpStep1 a3) { AddInst(IROp::TeakGeneric); }
    void mov2_axh_m_y0_m(Axh a0, ArRn2 a1, ArStep2 a2) { AddInst(IROp::TeakGeneric); }
    void mov2_mij_ax(ArpRn1 a0, ArpStep1 a1, ArpStep1 a2, Ab a3) { AddInst(IROp::TeakGeneric); }
    void mov2_mji_ax(ArpRn1 a0, ArpStep1 a1, ArpStep1 a2, Ab a3) { AddInst(IROp::TeakGeneric); }
    void mov2s(Px a0, ArRn2 a1, ArStep2 a2) { AddInst(IROp::TeakGeneric); }
    void mov_dvm(Abl a0) { AddInst(IROp::TeakGeneric); }
    void mov_dvm_to(Ab a0) { AddInst(IROp::TeakGeneric); }
    void mov_eu(MemImm8 a0, Axh a1) { AddInst(IROp::TeakGeneric); }
    void mov_ext0(Imm8s a0) { AddInst(IROp::TeakGeneric); }
    void mov_ext1(Imm8s a0) { AddInst(IROp::TeakGeneric); }
    void mov_ext2(Imm8s a0) { AddInst(IROp::TeakGeneric); }
    void mov_ext3(Imm8s a0) { AddInst(IROp::TeakGeneric); }
    void mov_icr(Imm5 a0) { AddInst(IROp::TeakGeneric); }
    void mov_icr(Register a0) { AddInst(IROp::TeakGeneric); }
    void mov_icr_to(Ab a0) { AddInst(IROp::TeakGeneric); }
    void mov_memsp_r6() { AddInst(IROp::TeakGeneric); }
    void mov_memsp_to(Register a0) { AddInst(IROp::TeakGeneric); }
    void mov_mixp(Register a0) { AddInst(IROp::TeakGeneric); }
    void mov_mixp_to(Bx a0) { AddInst(IROp::TeakGeneric); }
    void mov_mixp_to(Register a0) { AddInst(IROp::TeakGeneric); }
    void mov_p0(Ab a0) { AddInst(IROp::TeakGeneric); }
    void mov_p0h_to(Bx a0) { AddInst(IROp::TeakGeneric); }
    void mov_p0h_to(Register a0) { AddInst(IROp::TeakGeneric); }
    void mov_p1_to(Ab a0) { AddInst(IROp::TeakGeneric); }
    void mov_pc(Ax a0) { AddInst(IROp::TeakGeneric); }
    void mov_pc(Bx a0) { AddInst(IROp::TeakGeneric); }
    void mov_prpage(Abl a0) { AddInst(IROp::TeakGeneric); }
    void mov_prpage(Imm4 a0) { AddInst(IROp::TeakGeneric); }
    void mov_prpage_to(Abl a0) { AddInst(IROp::TeakGeneric); }
    void mov_r6(Rn a0, StepZIDS a1) { AddInst(IROp::TeakGeneric); }
    void mov_r6(Register a0) { AddInst(IROp::TeakGeneric); }
    void mov_r6(Imm16 a0) { AddInst(IROp::TeakGeneric); }
    void mov_r6_to(Rn a0, StepZIDS a1) { AddInst(IROp::TeakGeneric); }
    void mov_r6_to(Bx a0) { AddInst(IROp::TeakGeneric); }
    void mov_r6_to(Register a0) { AddInst(IROp::TeakGeneric); }
    void mov_repc(ArRn1 a0, ArStep1 a1) { AddInst(IROp::TeakGeneric); }
    void mov_repc(MemR7Imm16 a0) { AddInst(IROp::TeakGeneric); }
    void mov_repc(Abl a0) { AddInst(IROp::TeakGeneric); }
    void mov_repc(Imm16 a0) { AddInst(IROp::TeakGeneric); }
    void mov_repc_to(ArRn1 a0, ArStep1 a1) { AddInst(IROp::TeakGeneric); }
    void mov_repc_to(Ab a0) { AddInst(IROp::TeakGeneric); }
    void mov_repc_to(Abl a0) { AddInst(IROp::TeakGeneric); }
    void mov_repc_to(MemR7Imm16 a0) { AddInst(IROp::TeakGeneric); }
    void mov_stepi0(Imm16 a0) { AddInst(IROp::TeakGeneric); }
    void mov_stepj0(Imm16 a0) { AddInst(IROp::TeakGeneric); }
    void mov_sv(MemImm8 a0) { AddInst(IROp::TeakGeneric); }
    void mov_sv(Imm8s a0) { AddInst(IROp::TeakGeneric); }
    void mov_sv_app(ArRn1 a0, ArStep1 a1, Bx a2, SumBase a3, bool a4, bool a5, bool a6, bool a7) { AddInst(IROp::TeakGeneric); }
    void mov_sv_app(ArRn1 a0, ArStep1Alt a1, Bx a2, SumBase a3, bool a4, bool a5, bool a6, bool a7) { AddInst(IROp::TeakGeneric); }
    void mov_sv_to(MemImm8 a0) { AddInst(IROp::TeakGeneric); }
    void mov_x0(Abl a0) { AddInst(IROp::TeakGeneric); }
    void mov_x0_to(Ab a0) { AddInst(IROp::TeakGeneric); }
    void mov_x1(Abl a0) { AddInst(IROp::TeakGeneric); }
    void mov_x1_to(Ab a0) { AddInst(IROp::TeakGeneric); }
    void mov_y1(Abl a0) { AddInst(IROp::TeakGeneric); }
    void mov_y1_to(Ab a0) { AddInst(IROp::TeakGeneric); }
    void mova(Ab a0, ArRn2 a1, ArStep2 a2) { AddInst(IROp::TeakGeneric); }
    void mova(ArRn2 a0, ArStep2 a1, Ab a2) { AddInst(IROp::TeakGeneric); }
    void movd(R0123 a0, StepZIDS a1, R45 a2, StepZIDS a3) { AddInst(IROp::TeakGeneric); }
    void movp(Ax a0, Register a1) { AddInst(IROp::TeakGeneric); }
    void movp(Axl a0, Register a1) { AddInst(IROp::TeakGeneric); }
    void movp(Rn a0, StepZIDS a1, R0123 a2, StepZIDS a3) { AddInst(IROp::TeakGeneric); }
    void movpdw(Ax a0) { AddInst(IROp::TeakGeneric); }
    void movr(Rn a0, StepZIDS a1, Ax a2) { AddInst(IROp::TeakGeneric); }
    void movr(Register a0, Ax a1) { AddInst(IROp::TeakGeneric); }
    void movr(Bx a0, Ax a1) { AddInst(IROp::TeakGeneric); }
    void movr(ArRn2 a0, ArStep2 a1, Abh a2) { AddInst(IROp::TeakGeneric); }
    void movr_r6_to(Ax a0) { AddInst(IROp::TeakGeneric); }
    void movs(Register a0, Ab a1) { AddInst(IROp::TeakGeneric); }
    void movs(Rn a0, StepZIDS a1, Ab a2) { AddInst(IROp::TeakGeneric); }
    void movs(MemImm8 a0, Ab a1) { AddInst(IROp::TeakGeneric); }
    void movs_r6_to(Ax a0) { AddInst(IROp::TeakGeneric); }
    void movsi(RnOld a0, Ab a1, Imm5s a2) { AddInst(IROp::TeakGeneric); }
    void mpyi(Imm8s a0) { AddInst(IROp::TeakGeneric); }
    void msu(Rn a0, StepZIDS a1, Imm16 a2, Ax a3) { AddInst(IROp::TeakGeneric); }
    void msu(R45 a0, StepZIDS a1, R0123 a2, StepZIDS a3, Ax a4) { AddInst(IROp::TeakGeneric); }
    void msusu(ArRn2 a0, ArStep2 a1, Ax a2) { AddInst(IROp::TeakGeneric); }
    void mul(Mul3 a0, R45 a1, StepZIDS a2, R0123 a3, StepZIDS a4, Ax a5) { AddInst(IROp::TeakGeneric); }
    void mul(Mul3 a0, Rn a1, StepZIDS a2, Imm16 a3, Ax a4) { AddInst(IROp::TeakGeneric); }
    void mul_y0(Mul3 a0, Register a1, Ax a2) { AddInst(IROp::TeakGeneric); }
    void mul_y0(Mul3 a0, Rn a1, StepZIDS a2, Ax a3) { AddInst(IROp::TeakGeneric); }
    void mul_y0(Mul2 a0, MemImm8 a1, Ax a2) { AddInst(IROp::TeakGeneric); }
    void mul_y0_r6(Mul3 a0, Ax a1) { AddInst(IROp::TeakGeneric); }
    void norm(Ax a0, Rn a1, StepZIDS a2) { AddInst(IROp::TeakGeneric); }
    void or_(Ab a0, Ax a1, Ax a2) { AddInst(IROp::TeakGeneric); }
    void or_(Ax a0, Bx a1, Ax a2) { AddInst(IROp::TeakGeneric); }
    void or_(Bx a0, Bx a1, Ax a2) { AddInst(IROp::TeakGeneric); }
    void pacr1(Ax a0) { AddInst(IROp::TeakGeneric); }
    void pop(Px a0) { AddInst(IROp::TeakGeneric); }
    void pop(Bx a0) { AddInst(IROp::TeakGeneric); }
    void pop(Register a0) { AddInst(IROp::TeakGeneric); }
    void pop(Abe a0) { AddInst(IROp::TeakGeneric); }
    void pop(ArArpSttMod a0) { AddInst(IROp::TeakGeneric); }
    void pop_prpage() { AddInst(IROp::TeakGeneric); }
    void pop_r6() { AddInst(IROp::TeakGeneric); }
    void pop_repc() { AddInst(IROp::TeakGeneric); }
    void pop_y1() { AddInst(IROp::TeakGeneric); }
    void popa(Ab a0) { AddInst(IROp::TeakGeneric); }
    void push(Px a0) { AddInst(IROp::TeakGeneric); }
    void push(Register a0) { AddInst(IROp::TeakGeneric); }
    void push(Imm16 a0) { AddInst(IROp::TeakGeneric); }
    void push(Abe a0) { AddInst(IROp::TeakGeneric); }
    void push(ArArpSttMod a0) { AddInst(IROp::TeakGeneric); }
    void push_prpage() { AddInst(IROp::TeakGeneric); }
    void push_r6() { AddInst(IROp::TeakGeneric); }
    void push_repc() { AddInst(IROp::TeakGeneric); }
    void push_x0() { AddInst(IROp::TeakGeneric); }
    void push_x1() { AddInst(IROp::TeakGeneric); }
    void push_y1() { AddInst(IROp::TeakGeneric); }
    void pusha(Ax a0) { AddInst(IROp::TeakGeneric); }
    void pusha(Bx a0) { AddInst(IROp::TeakGeneric); }
    void rep(Imm8 a0) { AddInst(IROp::TeakGeneric); }
    void rep(Register a0) { AddInst(IROp::TeakGeneric); }
    void rep_r6() { AddInst(IROp::TeakGeneric); }
    void ret(Cond a0) { AddInst(IROp::TeakGeneric); }
    void reti(Cond a0) { AddInst(IROp::TeakGeneric); }
    void retic(Cond a0) { AddInst(IROp::TeakGeneric); }
    void rets(Imm8 a0) { AddInst(IROp::TeakGeneric); }
    void shfc(Ab a0, Ab a1, Cond a2) { AddInst(IROp::TeakGeneric); }
    void shfi(Ab a0, Ab a1, Imm6s a2) { AddInst(IROp::TeakGeneric); }
    void sqr_mpysu_add3a(Ab a0, Ab a1) { AddInst(IROp::TeakGeneric); }
    void sqr_sqr_add3(Ab a0, Ab a1) { AddInst(IROp::TeakGeneric); }
    void sqr_sqr_add3(ArRn2 a0, ArStep2 a1, Ab a2) { AddInst(IROp::TeakGeneric); }
    void sub(Ab a0, Bx a1) { AddInst(IROp::TeakGeneric); }
    void sub(Bx a0, Ax a1) { AddInst(IROp::TeakGeneric); }
    void sub(Px a0, Bx a1) { AddInst(IROp::TeakGeneric); }
    void sub_add(ArpRn1 a0, ArpStep1 a1, ArpStep1 a2, Ab a3) { AddInst(IROp::TeakGeneric); }
    void sub_add_i_mov_j_sv(ArpRn1 a0, ArpStep1 a1, ArpStep1 a2, Ab a3) { AddInst(IROp::TeakGeneric); }
    void sub_add_j_mov_i_sv(ArpRn1 a0, ArpStep1 a1, ArpStep1 a2, Ab a3) { AddInst(IROp::TeakGeneric); }
    void sub_add_sv(ArRn1 a0, ArStep1 a1, Ab a2) { AddInst(IROp::TeakGeneric); }
    void sub_p1(Ax a0) { AddInst(IROp::TeakGeneric); }
    void sub_sub(ArpRn1 a0, ArpStep1 a1, ArpStep1 a2, Ab a3) { AddInst(IROp::TeakGeneric); }
    void swap(SwapType a0) { AddInst(IROp::TeakGeneric); }
    void tst4b(ArRn2 a0, ArStep2 a1) { AddInst(IROp::TeakGeneric); }
    void tst4b(ArRn2 a0, ArStep2 a1, Ax a2) { AddInst(IROp::TeakGeneric); }
    void tstb(MemImm8 a0, Imm4 a1) { AddInst(IROp::TeakGeneric); }
    void tstb(Register a0, Imm4 a1) { AddInst(IROp::TeakGeneric); }
    void tstb(SttMod a0, Imm16 a1) { AddInst(IROp::TeakGeneric); }
    void tstb(Rn a0, StepZIDS a1, Imm4 a2) { AddInst(IROp::TeakGeneric); }
    void tstb_r6(Imm4 a0) { AddInst(IROp::TeakGeneric); }
    void vtrmov(Axl a0) { AddInst(IROp::TeakGeneric); }
    void vtrmov0(Axl a0) { AddInst(IROp::TeakGeneric); }
    void vtrmov1(Axl a0) { AddInst(IROp::TeakGeneric); }
