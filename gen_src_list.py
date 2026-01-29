import os

def find_sources(directory):
    sources = []
    for root, dirs, files in os.walk(directory):
        for file in files:
            if file.endswith('.cpp') or file.endswith('.c') or file.endswith('.mm'):
                path = os.path.join(root, file)
                # Skip some unnecessary directories
                if 'tests' in path or 'renderer_vulkan' in path or 'renderer_opengl' in path:
                    continue
                sources.append(path)
    return sources

def main():
    core_sources = find_sources('Core')
    libretro_sources = [
        'libretro/libretro.cpp'
    ]
    input_sources = find_sources('InputManager')

    # Filter core_sources to exclude reference directories
    core_sources = [s for s in core_sources if 'cytrusをirjitとhleに対応させさらに動作を改善させるるのに役に立つ参考にすべき物達' not in s]

    all_sources = core_sources + libretro_sources + input_sources

    with open('sources.mk', 'w') as f:
        f.write('SOURCES_CXX := ')
        f.write(' \\\n\t'.join([s for s in all_sources if s.endswith('.cpp')]))
        f.write('\n\nSOURCES_C := ')
        f.write(' \\\n\t'.join([s for s in all_sources if s.endswith('.c')]))
        f.write('\n\nSOURCES_OBJC := ')
        f.write(' \\\n\t'.join([s for s in all_sources if s.endswith('.mm')]))
        f.write('\n')

if __name__ == '__main__':
    main()
