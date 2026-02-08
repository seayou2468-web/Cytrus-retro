struct TeakVisitorStub {
    using instruction_return_type = void;
     void SignalInterrupt(u32 i) {}
     void SignalVectoredInterrupt(u32 address, bool context_switch) {}
     void nop() {}
     void norm(Ax a, Rn b, StepZIDS bs) {}
     void swap(SwapType swap) {}
     void trap() {}
     void DoMultiplication(u32 unit, bool x_sign, bool y_sign) {}
     void ProductSum(SumBase base, RegName acc, bool sub_p0, bool p0_align, bool sub_p1,
                    bool p1_align) {}
     void AlmGeneric(AlmOp op, u64 a, Ax b) {}
     void alm(Alm op, MemImm8 a, Ax b) {}
     void alm(Alm op, Rn a, StepZIDS as, Ax b) {}
     void alm(Alm op, Register a, Ax b) {}
    void undefined(u16 opcode) {}
};
