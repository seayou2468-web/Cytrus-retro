import os
import sys
import re

STD_HEADERS = [
    'atomic', 'thread', 'mutex', 'condition_variable', 'future',
    'vector', 'string', 'map', 'set', 'unordered_map', 'unordered_set',
    'list', 'deque', 'stack', 'queue', 'array', 'span',
    'memory', 'functional', 'algorithm', 'utility', 'type_traits',
    'chrono', 'random', 'complex', 'valarray', 'numeric',
    'iostream', 'fstream', 'sstream', 'iomanip',
    'cstdio', 'cstdlib', 'cstring', 'cmath', 'ctime', 'climits', 'cstdint', 'cstddef', 'cassert'
]

def fix_includes(directory):
    for root, dirs, files in os.walk(directory):
        for file in files:
            if file.endswith(('.cpp', '.h', '.cc')):
                filepath = os.path.join(root, file)
                with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
                    content = f.read()

                new_content = content

                # Fix standard headers with double quotes
                for std in STD_HEADERS:
                    if f'#include "{std}"' in content:
                        print(f"Fixing standard header {std} in {filepath}")
                        new_content = new_content.replace(f'#include "{std}"', f'#include <{std}>')

                # Find #include "something.h" where something.h is not in the same directory
                includes = re.findall(r'#include\s+"([^"]+)"', new_content)
                for inc in includes:
                    if '/' in inc: continue
                    if inc == file.replace('.cpp', '.h'): continue
                    if os.path.exists(os.path.join(root, inc)): continue

                    found_path = None
                    for sroot, sdirs, sfiles in os.walk(directory):
                        if inc in sfiles:
                            rel_path = os.path.relpath(os.path.join(sroot, inc), directory)
                            found_path = rel_path
                            break

                    if found_path:
                        print(f"Fixing {inc} -> {found_path} in {filepath}")
                        new_content = new_content.replace(f'#include "{inc}"', f'#include "{found_path}"')

                if new_content != content:
                    with open(filepath, 'w', encoding='utf-8') as f:
                        f.write(new_content)

if __name__ == "__main__":
    fix_includes(sys.argv[1])
