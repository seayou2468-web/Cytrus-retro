import sys

filename = 'Core/core/hle/service/http/http_c.cpp'
with open(filename, 'r') as f:
    lines = f.readlines()

new_lines = []
in_ssl_func = False
brace_level = 0

for line in lines:
    if 'void Context::MakeRequestSSL' in line:
        in_ssl_func = True
        new_lines.append(line)
        new_lines.append('{\n')
        new_lines.append('    LOG_ERROR(Service_HTTP, "SSL not supported");\n')
        new_lines.append('}\n')
        continue

    if in_ssl_func:
        if '{' in line: brace_level += 1
        if '}' in line:
            brace_level -= 1
            if brace_level == 0:
                in_ssl_func = False
        continue

    new_lines.append(line)

with open(filename, 'w') as f:
    f.writelines(new_lines)
