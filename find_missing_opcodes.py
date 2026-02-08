import re
import os

OPCODES_INC = 'libretro/dynarmic/src/dynarmic/ir/opcodes.inc'
ARM_HYBRID_CPP = 'Core/core/arm/dynarmic/arm_hybrid.cpp'

def get_all_opcodes():
    opcodes = set()
    with open(OPCODES_INC, 'r') as f:
        for line in f:
            # Match OPCODE(Name, ...)
            m = re.match(r'^\s*OPCODE\s*\(\s*(\w+)', line)
            if m:
                opcodes.add(m.group(1))
            # Match A32OPC(Name, ...)
            m = re.match(r'^\s*A32OPC\s*\(\s*(\w+)', line)
            if m:
                opcodes.add('A32' + m.group(1))
    return opcodes

def get_implemented_opcodes():
    implemented = set()
    with open(ARM_HYBRID_CPP, 'r') as f:
        content = f.read()
        # Find all IR::Opcode::Name
        matches = re.findall(r'IR::Opcode::(\w+)', content)
        for m in matches:
            implemented.add(m)
    return implemented

def main():
    all_opcodes = get_all_opcodes()
    implemented_opcodes = get_implemented_opcodes()

    missing = sorted(list(all_opcodes - implemented_opcodes))

    print(f"Total opcodes in Dynarmic (Generic + A32): {len(all_opcodes)}")
    print(f"Implemented opcodes in ARM_Hybrid: {len(implemented_opcodes)}")
    print(f"Missing opcodes ({len(missing)}):")
    for op in missing:
        # Ignore NUM_OPCODE and Void if they aren't meant to be implemented
        if op in ['NUM_OPCODE', 'Void', 'Identity']:
            continue
        print(f"  {op}")

if __name__ == "__main__":
    main()
