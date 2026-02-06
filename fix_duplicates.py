import sys
import re

def fix_execute_block(content):
    match = re.search(r'(void ARM_Hybrid::ExecuteBlock\(const TranslatedBlock& block\) \{.*?)(switch \(inst.op\) \{)(.*?)(\}\s+// insts)', content, re.DOTALL)
    if not match:
        print("Could not find ExecuteBlock switch")
        return content

    header = match.group(1) + match.group(2)
    cases_content = match.group(3)
    footer = match.group(4)

    # Split by 'case ' but keep the delimiter
    parts = re.split(r'(case IR::Opcode::)', cases_content)

    new_parts = [parts[0]] # Everything before first case
    seen_opcodes = set()

    for i in range(1, len(parts), 2):
        case_marker = parts[i]
        case_rest = parts[i+1]

        # Extract opcode name
        opcode_match = re.match(r'([A-Za-z0-9]+):', case_rest)
        if opcode_match:
            opcode = opcode_match.group(1)
            if opcode in seen_opcodes:
                print(f"Removing duplicate opcode: {opcode}")
                # Skip until break or next case/default at same brace level
                # This is hard because of nested braces.
                # For this specific file, most cases end with 'break;'
                continue
            seen_opcodes.add(opcode)
            new_parts.append(case_marker)
            new_parts.append(case_rest)
        else:
            new_parts.append(case_marker)
            new_parts.append(case_rest)

    return content[:match.start()] + header + "".join(new_parts) + footer + content[match.end():]

filename = 'Core/core/arm/dynarmic/arm_hybrid.cpp'
with open(filename, 'r') as f:
    content = f.read()

new_content = fix_execute_block(content)

with open(filename, 'w') as f:
    f.write(new_content)
