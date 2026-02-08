import re

with open('libretro/azahar/externals/teakra/src/interpreter.h', 'r') as f:
    content = f.read()

# Match function definitions like: void nop() {
# or: void add(Ab a, Bx b) {
matches = re.finditer(r'void\s+(\w+)\s*\(([^)]*)\)\s*\{', content)

methods = []
for m in matches:
    name = m.group(1)
    args = m.group(2)
    if name not in ['PushPC', 'PopPC', 'SetPC', 'Run', 'SignalInterrupt', 'SignalVectoredInterrupt', 'ContextStore', 'ContextRestore', 'Repeat', 'Exp', 'ExpStore', 'CodebookSearch', 'ShiftBus40', 'SaturateAccNoFlag', 'SaturateAcc', 'GetAndSatAcc', 'GetAndSatAccNoFlag', 'RegToBus16', 'SetAccFlag', 'SetAcc', 'SatAndSetAccAndFlag', 'SetAccAndFlag', 'RegFromBus16', 'GetArRnUnit', 'GetArpRnUnit', 'ConvertArStep', 'GetArStep', 'GetArpStep', 'GetArOffset', 'GetArpOffset', 'RnAddress', 'RnAddressAndModify', 'OffsetAddress', 'StepAddress', 'RnAndModify', 'ProductToBus32_NoShift', 'ProductToBus40', 'ProductFromBus32', 'CounterAcc']:
        methods.append((name, args))

# Print as C++ methods for IRGenerator
for name, args in methods:
    print(f"    void {name}({args}) {{")
    print(f"        // STUB")
    print(f"    }}")
