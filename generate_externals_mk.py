import os

def get_files(directory, extensions):
    found_files = []
    if not os.path.exists(directory):
        return []
    for root, dirs, files in os.walk(directory):
        if 'examples' in root: continue
        for file in files:
            if any(file.endswith(ext) for ext in extensions):
                found_files.append(os.path.join(root, file))
    return found_files

def to_obj(path):
    base = os.path.splitext(path)[0]
    return f"$(CORE_DIR)/{base}.o"

# Dynarmic
dynarmic_dir = 'libretro/dynarmic/src'
dynarmic_src = get_files(dynarmic_dir, ['.cpp'])
dynarmic_src = [f for f in dynarmic_src if not (
    '/backend/' in f or '/A64/' in f or '/x64/' in f or
    'disassemble' in f or 'spin_lock_x64' in f or
    'x64_disassemble' in f or 'spin_lock_arm64' in f or
    'a64_' in f or 'A64' in f
)]

# MCL
mcl_src = ['libretro/dynarmic/externals/mcl/src/assert.cpp']

# Teakra
teakra_dir = 'libretro/azahar/externals/teakra/src'
teakra_src = get_files(teakra_dir, ['.cpp'])
teakra_src = [f for f in teakra_src if not (f.endswith('/main.cpp') or '/dsp1_reader/' in f or '/test_generator/' in f or '/step2_test_generator/' in f or '/mod_test_generator/' in f or '/coff_reader/' in f or '/test_verifier/' in f)]

# Fmt
fmt_dir = 'libretro/azahar/externals/fmt/src'
fmt_src = [os.path.join(fmt_dir, 'format.cc'), os.path.join(fmt_dir, 'os.cc')]

# Zstd
zstd_dir = 'libretro/azahar/externals/zstd/lib'
zstd_src = get_files(os.path.join(zstd_dir, 'common'), ['.c']) + \
           get_files(os.path.join(zstd_dir, 'compress'), ['.c']) + \
           get_files(os.path.join(zstd_dir, 'decompress'), ['.c'])
zstd_seekable_src = [
    'libretro/azahar/externals/zstd/contrib/seekable_format/zstdseek_compress.c',
    'libretro/azahar/externals/zstd/contrib/seekable_format/zstdseek_decompress.c'
]

# Lodepng
lodepng_dir = 'libretro/azahar/externals/lodepng/lodepng'
lodepng_src = [os.path.join(lodepng_dir, 'lodepng.cpp'), os.path.join(lodepng_dir, 'lodepng_util.cpp')]

# Boost Serialization & Iostreams
boost_serialization_dir = 'libretro/azahar/externals/boost/libs/serialization/src'
boost_serialization_src = get_files(boost_serialization_dir, ['.cpp'])
boost_iostreams_src = ['libretro/azahar/externals/boost/libs/iostreams/src/file_descriptor.cpp']

# CryptoPP
cryptopp_dir = 'libretro/azahar/externals/cryptopp'
cryptopp_src = get_files(cryptopp_dir, ['.cpp'])
cryptopp_src = [f for f in cryptopp_src if not (
    'test.cpp' in f or 'bench' in f or 'validat' in f or
    'adhoc' in f or 'regtest' in f or 'TestScripts' in f or 'TestPrograms' in f or
    '_avx.cpp' in f or '_sse.cpp' in f or '_arm64.cpp' in f or
    'ppc_simd.cpp' in f or 'power7_ppc.cpp' in f or 'power8_ppc.cpp' in f or 'power9_ppc.cpp' in f or
    'neon_simd.cpp' in f
)]
# We include cpu.cpp as it's needed for detection variables.

# SoundTouch
soundtouch_dir = 'libretro/azahar/externals/soundtouch/source/SoundTouch'
soundtouch_src = [
    os.path.join(soundtouch_dir, 'SoundTouch.cpp'),
    os.path.join(soundtouch_dir, 'FIFOSampleBuffer.cpp'),
    os.path.join(soundtouch_dir, 'RateTransposer.cpp'),
    os.path.join(soundtouch_dir, 'TDStretch.cpp'),
    os.path.join(soundtouch_dir, 'InterpolateLinear.cpp'),
    os.path.join(soundtouch_dir, 'InterpolateCubic.cpp'),
    os.path.join(soundtouch_dir, 'InterpolateShannon.cpp'),
    os.path.join(soundtouch_dir, 'AAFilter.cpp'),
    os.path.join(soundtouch_dir, 'PeakFinder.cpp'),
    os.path.join(soundtouch_dir, 'BPMDetect.cpp'),
    os.path.join(soundtouch_dir, 'FIRFilter.cpp'),
    os.path.join(soundtouch_dir, 'cpu_detect_x86.cpp'),
    os.path.join(soundtouch_dir, 'sse_optimized.cpp'),
    os.path.join(soundtouch_dir, 'mmx_optimized.cpp'),
]

# Libretro-common
libretro_comm_dir = 'libretro/libretro-common'
libretro_comm_src = [
    os.path.join(libretro_comm_dir, 'file/file_path.c'),
    os.path.join(libretro_comm_dir, 'file/file_path_io.c'),
    os.path.join(libretro_comm_dir, 'streams/file_stream.c'),
    os.path.join(libretro_comm_dir, 'vfs/vfs_implementation.c'),
    os.path.join(libretro_comm_dir, 'compat/fopen_utf8.c'),
    os.path.join(libretro_comm_dir, 'encodings/encoding_utf.c'),
    os.path.join(libretro_comm_dir, 'file/retro_dirent.c'),
    os.path.join(libretro_comm_dir, 'streams/file_stream_transforms.c'),
    os.path.join(libretro_comm_dir, 'string/stdstring.c'),
    os.path.join(libretro_comm_dir, 'time/rtime.c'),
    os.path.join(libretro_comm_dir, 'compat/compat_strl.c'),
]

with open('externals_objects.mk', 'w') as f:
    f.write('EXTERNAL_OBJECTS := \\\n')
    sources = dynarmic_src + mcl_src + teakra_src + fmt_src + zstd_src + zstd_seekable_src + lodepng_src + \
              boost_serialization_src + boost_iostreams_src + cryptopp_src + soundtouch_src + libretro_comm_src
    for s in sources:
        f.write(f'    {to_obj(s)} \\\n')
    f.write('\n')
