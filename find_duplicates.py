import re

content = open('Core/core/arm/dynarmic/arm_static_ir.cpp').read()
switch_block = re.search(r'switch \(inst\.op\) \{(.*?)\}', content, re.DOTALL).group(1)
cases = re.findall(r'case IR::Opcode::([a-zA-Z0-9_]+):', switch_block)

from collections import Counter
counts = Counter(cases)
duplicates = [item for item, count in counts.items() if count > 1]
if duplicates:
    print("Duplicate cases:", duplicates)
else:
    print("No duplicate cases.")
