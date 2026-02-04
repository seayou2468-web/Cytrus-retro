import re

content = open('Core/core/arm/dynarmic/arm_static_ir.cpp').read()

calls = re.findall(r'Handle([a-zA-Z0-9_]+)\(', content)
defs = re.findall(r'OP_HANDLER\(([a-zA-Z0-9_]+)\)', content)

calls = set(calls)
defs = set(defs)

missing = calls - defs
if missing:
    print("Missing handlers:", missing)
else:
    print("All handlers defined.")
