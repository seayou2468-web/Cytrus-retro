used = set(open('used_opcodes.txt').read().splitlines())
defined = set(open('defined_opcodes.txt').read().splitlines())
# Note: defined opcodes might need to be prefixed with A32 or A64 depending on how they are used in the switch.
# But in our code, we use IR::Opcode::A32GetRegister etc.
# Wait, let's look at defining more accurately.

definitions = open('libretro/azahar/externals/dynarmic/src/dynarmic/ir/opcodes.inc').read().splitlines()
real_defined = set()
for line in definitions:
    if 'OPCODE(' in line:
        real_defined.add(line.split('(')[1].split(',')[0].strip())
    elif 'A32OPC(' in line:
        real_defined.add('A32' + line.split('(')[1].split(',')[0].strip())
    elif 'A64OPC(' in line:
        real_defined.add('A64' + line.split('(')[1].split(',')[0].strip())

invalid = used - real_defined
if invalid:
    print("Invalid opcodes used in switch:", invalid)
else:
    print("All used opcodes are valid.")
