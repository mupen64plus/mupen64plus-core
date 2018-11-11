project('mupen64plus-core', 'c', 'cpp',
    version : '2.5.0',
    license : 'GPLv2+',
    default_options : ['warning_level=3',
                       'buildtype=release',
                       'b_lto=true',
                       'c_std=gnu11',
                       'cpp_std=gnu++11'])

libmupen64plus_version = meson.project_version()
version_arr = libmupen64plus_version.split('.')
libmupen64plus_version_major = version_arr[0]
libmupen64plus_version_minor = version_arr[1]
libmupen64plus_version_patch = version_arr[2]

# Headers directories
inc = include_directories('src')

# Shared files
shared_files = [
    'data/mupen64plus.ini',
    'data/mupencheat.txt',
    'data/font.ttf']

# Public headers
public_headers = [
    'src/api/m64p_common.h',
    'src/api/m64p_config.h',
    'src/api/m64p_debugger.h',
    'src/api/m64p_frontend.h',
    'src/api/m64p_plugin.h',
    'src/api/m64p_types.h',
    'src/api/m64p_vidext.h']

# C/C++/GAS sources files
src = [
    'src/api/callbacks.c',
    'src/api/common.c',
    'src/api/config.c',
    'src/api/debugger.c',
    'src/api/frontend.c',
    'src/api/vidext.c',
    'src/backends/api/video_capture_backend.c',
    'src/backends/plugins_compat/audio_plugin_compat.c',
    'src/backends/plugins_compat/input_plugin_compat.c',
    'src/backends/clock_ctime_plus_delta.c',
    'src/backends/dummy_video_capture.c',
    'src/backends/file_storage.c',
    'src/device/cart/cart.c',
    'src/device/cart/af_rtc.c',
    'src/device/cart/cart_rom.c',
    'src/device/cart/eeprom.c',
    'src/device/cart/flashram.c',
    'src/device/cart/sram.c',
    'src/device/controllers/game_controller.c',
    'src/device/controllers/paks/biopak.c',
    'src/device/controllers/paks/mempak.c',
    'src/device/controllers/paks/rumblepak.c',
    'src/device/controllers/paks/transferpak.c',
    'src/device/dd/dd_controller.c',
    'src/device/device.c',
    'src/device/gb/gb_cart.c',
    'src/device/gb/mbc3_rtc.c',
    'src/device/gb/m64282fp.c',
    'src/device/memory/memory.c',
    'src/device/pif/bootrom_hle.c',
    'src/device/pif/cic.c',
    'src/device/pif/n64_cic_nus_6105.c',
    'src/device/pif/pif.c',
    'src/device/r4300/cached_interp.c',
    'src/device/r4300/cp0.c',
    'src/device/r4300/cp1.c',
    'src/device/r4300/idec.c',
    'src/device/r4300/interrupt.c',
    'src/device/r4300/pure_interp.c',
    'src/device/r4300/r4300_core.c',
    'src/device/r4300/tlb.c',
    'src/device/rcp/ai/ai_controller.c',
    'src/device/rcp/mi/mi_controller.c',
    'src/device/rcp/pi/pi_controller.c',
    'src/device/rcp/rdp/fb.c',
    'src/device/rcp/rdp/rdp_core.c',
    'src/device/rcp/ri/ri_controller.c',
    'src/device/rcp/rsp/rsp_core.c',
    'src/device/rcp/si/si_controller.c',
    'src/device/rcp/vi/vi_controller.c',
    'src/device/rdram/rdram.c',
    'src/main/main.c',
    'src/main/util.c',
    'src/main/cheat.c',
    'src/main/eventloop.c',
    'src/main/rom.c',
    'src/main/savestates.c',
    'src/main/screenshot.c',
    'src/main/sdl_key_converter.c',
    'src/main/workqueue.c',
    'src/plugin/plugin.c',
    'src/plugin/dummy_video.c',
    'src/plugin/dummy_audio.c',
    'src/plugin/dummy_input.c',
    'src/plugin/dummy_rsp.c']

# API export script
vscript = 'src/api/api_export.ver'

# Can be usefull on windows to find bundled win32-deps
m64pwindeppath = join_paths(meson.current_source_dir(), '../mupen64plus-win32-deps')

# NASM Assembler files (handled separately)
src_nasm = []
# Extra dependencies
extra_dep = []

# Define some base flags
warnflags = ['-Wno-unused-function', '-Wno-unused-parameter']
commonflags = ['-ffast-math', '-fno-strict-aliasing', '-fvisibility=hidden']
cflags = ['-DM64P_PARALLEL']
vflag = ['-Wl,--version-script,@0@/@1@'.format(meson.current_source_dir(), vscript)]


# Get compiler and host machine infos
cc = meson.get_compiler('c')
cxx = meson.get_compiler('cpp')

host_cpu = host_machine.cpu_family()
host_system = host_machine.system()
host_endian = host_machine.endian()


# Handle options
with_asm = not get_option('no_asm')
with_dbg_compare = get_option('dbg_compare')
with_dbg_core = get_option('dbg_core')
with_dbg_count = get_option('dbg_count')
with_dbg_timing = get_option('dbg_timing')
with_dbg_profile = get_option('dbg_profile')
with_debugger = get_option('debugger')
with_lirc = get_option('lirc')
with_vc = get_option('vc')
with_gles = with_vc or get_option('gles') # VC option implies GLES
with_osd = not with_gles and get_option('osd') # OSD is not compatible with GLES
with_new_dynarec = (host_cpu == 'arm') or get_option('new_dynarec') # Force new_dynarec on ARM host
with_opencv = get_option('opencv')

# Define M64P_BIG_ENDIAN on big endian host
if host_endian == 'big'
  add_project_arguments('-DM64P_BIG_ENDIAN')
endif

# Define WIN32 on windows host
if host_system == 'windows'
  cflags += '-DWIN32'
endif

# Needed for asm_defines as MSVC doesn't define that on x64
if cc.get_id() == 'msvc' and host_cpu == 'x86_64'
  cflags += '-D__x86_64__'
endif

# Disable 'unsafe' functions warning on msvc
if cc.get_id() == 'msvc'
  cflags += '-D_CRT_SECURE_NO_WARNINGS'
endif


# Add some architectures defines
armflags = []
if host_cpu == 'arm'
  arm_fpu = get_option('arm_fpu')
  if arm_fpu == ''
  elif arm_fpu == 'neon'
    armflags = ['-mfpu=neon', '-mfloat-abi=hard']
  elif arm_fpu == 'vfp-hard'
    armflags = ['-mfpu=vfp', '-mfloat-abi=hard']
  elif arm_fpu == 'vfp-soft'
    armflags = ['-mfpu=vfp', '-mfloat-abi=softfp']
  else
    error('invalid arm-fpu option')
  endif
endif

if host_machine.cpu().startswith('armv5') or host_machine.cpu().startswith('armv6')
  cflags += '-DARMv5_ONLY'
  message('Using ARMv5_ONLY')
endif

# Pass some compiler arguments
foreach x : commonflags + warnflags + armflags
  if cc.has_argument(x)
    add_project_arguments(x, language: 'c')
  endif
endforeach
foreach x : commonflags + warnflags + armflags + ['-fvisibility-inlines-hidden']
  if cxx.has_argument(x)
    add_project_arguments(x, language: 'cpp')
  endif
endforeach

# If custom sharedir is specified add a define
# otherwise use plateform specific sharedir
sharedir = get_option('sharedir')
if not (sharedir == '')
  cflags += ['-DSHAREDIR=' + sharedir]
else
  # Also check that installation directory matches osal/files_{unix,win32}.c
  if host_system == 'windows'
    # Use the same location that installed binaries
    sharedir = join_paths(get_option('prefix'), get_option('libdir'))
  else
    sharedir = join_paths(get_option('prefix'), get_option('datadir'), 'mupen64plus')
  endif
endif


# declare required dependencies
# always use the embedded version because md5 project is not a "real" library
md5_dep = subproject('md5').get_variable('md5_dep')
zlib_dep = dependency('zlib', fallback : ['zlib', 'zlib_dep'])
# XXX: use wrap file instead
minizip_dep = dependency('minizip', fallback : ['minizip', 'minizip_dep'])
png_dep = dependency('libpng', fallback : ['libpng', 'png_dep'])
# XXX: use wrap file instead
xxhash_dep = dependency('xxhash', fallback : ['xxhash', 'xxhash_dep'])
# try sdl2 first, and fallback to sdl1 otherwise
sdl_dep = dependency('sdl2', fallback : ['sdl2', 'sdl2_dep'], required : false)
if not sdl_dep.found()
  sdl_dep = dependency('sdl')
endif
# Add math library portably (see meson FAQ)
libm = cc.find_library('m', required : false)

if host_system == 'mingw'
  pthreads = dependency('threads')
  extra_dep += pthreads
endif

# Add OSAL sources
if host_system == 'windows'
  src += [
    'src/osal/dynamiclib_win32.c',
    'src/osal/files_win32.c']
elif host_system == 'linux'
  src += [
    'src/osal/dynamiclib_unix.c',
    'src/osal/files_unix.c']
  # On linux we also need libdl
  libdl = cc.find_library('dl')
  extra_dep += libdl
elif host_system == 'darwin'
  src += [
    'src/osal/dynamiclib_unix.c',
    'src/osal/files_macos.c']
  # On macos we also need libdl
  libdl = cc.find_library('dl')
  extra_dep += libdl
endif

# Add various core debugging possibilities
if with_dbg_compare
  cflags += ['-DCOMPARE_CORE']
endif
if with_dbg_core
  cflags += ['-DCORE_DBG']
endif
if with_dbg_count
  cflags += ['-DCOUNT_INSTR']
  src += ['src/device/r4300/instr_counters.c']
endif
if with_dbg_timing
  cflags += ['-DPROFILE']
  src += ['src/main/profile.c']
endif
if with_dbg_profile
  cflags += ['-DPROFILE_R4300']
endif


# Add OSD support
if with_osd
  oglft_dep = dependency('oglft', fallback : ['oglft', 'oglft_dep'],
      default_options : ['ftdir=' + join_paths(m64pwindeppath, 'freetype-2.6')])
  extra_dep += [oglft_dep]
  src += [
    'src/osd/osd.c',
    'src/osd/oglft_c.cpp'
  ]
  cflags += ['-DM64P_OSD']
endif


# Add LIRC support
if with_lirc
  lirc_dep = dependency('lirc', required : false)
  # Seems like Ubuntu has a different pkg-config file
  if not lirc_dep.found()
    lirc_dep = dependency('liblircclient0')
  endif
  extra_dep += lirc_dep
  src += ['src/main/lirc.c']
  cflags += ['-DWITH_LIRC']
endif


# Add debugger support
if with_debugger
  libopcodes = cc.find_library('opcodes')
  libbfd = cc.find_library('bfd')
  extra_dep += [libopcodes, libbfd]
  src += [
    'src/debugger/dbg_debugger.c',
    'src/debugger/dbg_decoder.c',
    'src/debugger/dbg_memory.c',
    'src/debugger/dbg_breakpoints.c']
  cflags += ['-DDBG']
endif


# Add OpenCV video capture support
if with_opencv
  opencv_dep = dependency('opencv', fallback : ['opencv', 'opencv_dep'])
  extra_dep += [opencv_dep]
  src += ['src/backends/opencv_video_capture.cpp']
  cflags += ['-DM64P_OPENCV']
endif

# Add dynarec support
if with_asm
  cflags += '-DDYNAREC'

  if with_new_dynarec
    # New Dynarec
    dynarec_src = [
        ['new_dynarec', ['new_dynarec.c']]]
    dynarec_asm = []
    if host_cpu == 'x86'
      cflags += '-DNEW_DYNAREC=1'
      dynarec_asm += [['new_dynarec/x86', ['linkage_x86.asm']]]
    elif host_cpu == 'arm'
      cflags += '-DNEW_DYNAREC=3'
      dynarec_src += [['new_dynarec/arm', ['linkage_arm.S', 'arm_cpu_features.c']]]
    else
      error('New Dynarec is only supported on 32 bit x86 and 32 bit armel')
    endif
  else
    # Hacktarux dynarec
    if not 'x86 x86_64'.contains(host_cpu)
      error('Unsupported architecture for dynarec: ' + host_cpu)
    endif
    dynarec_src = [
      ['', ['recomp.c']],
      [host_cpu, ['assemble.c', 'dynarec.c', 'regcache.c']]]
    dynarec_asm = [[host_cpu, ['dyna_start.asm']]]
  endif

else
  # dynarec is disabled
  cflags += '-DNO_ASM'

  dynarec_src = []
  dynarec_asm = []
endif
r4300_dir = 'src/device/r4300'
foreach x : dynarec_src
  foreach f : x[1]
    src += join_paths(r4300_dir, x[0], f)
  endforeach
endforeach
foreach x : dynarec_asm
  foreach f : x[1]
    src_nasm += join_paths(r4300_dir, x[0], f)
  endforeach
endforeach


# Add GLES support
if with_gles
  cflags += ['-DUSE_GLES']
  libgles = cc.find_library('GLESv2')

  # Setup VideoCore support
  if with_vc
    vc_dir = '/opt/vc/'
    vc_lib_dir = join_paths(vc_dir, 'lib')
    vc_inc_dir = join_paths(vc_dir, 'include')

    inc += include_directories([
      vc_inc_dir,
      join_paths(vc_inc_dir, 'interface/vcos/pthreads'),
      join_paths(vc_inc_dir, 'vmcs_host/linux')])

    foreach x : ['brcmEGL', 'bcm_host', 'vcos', 'vchiq_arm']
      extra_dep += cc.find_library(x, dirs : vc_lib_dir)
    endforeach
    # override libgles implementation
    libgles = cc.find_library('brcmGLESv2', dirs : vc_lib_dir)
  endif

  extra_dep += libgles
endif

# Assembler step
if src_nasm.length() > 0 or src.contains('.S')
  # Compile asm_defines.c to extract compiler/os-specific offsets
  # Use the same compilation flags as the other files of the project
  # except for LTO which must be disabled for effective creation of required symbols
  asm_defines_lib = static_library('asm_defines',
    'src/asm_defines/asm_defines.c',
    include_directories : inc,
    c_args : cflags,
    override_options : 'b_lto=false')

  # Generate headers using an awk parser directly on the asm_defines_lib binary
  awk = find_program('awk', join_paths(m64pwindeppath, 'gawk-3.1.6-1/bin/gawk.exe'), native : true)
  asm_defines_headers = custom_target('asm_defines_headers',
      input : ['tools/gen_asm_defines.awk', asm_defines_lib],
      output : ['asm_defines_nasm.h', 'asm_defines_gas.h'],
      command : [awk, '-v', 'dest_dir=@OUTDIR@', '-f', '@INPUT0@', '@INPUT1@'])

  # If we have some GAS style assembler files,
  # add asm_defines_headers to src
  if src.contains('.S')
    src += asm_defines_headers
  endif

  asflags = []

  # Add include directory where asm_defines headers are generated
  # According to NASM documentation, a trailing slash is desirable
  asflags += ['-i', join_paths(meson.current_build_dir(), '')]

  # forward PIC mode to assembler files
  if not get_option('b_staticpic')
    asflags += ['-d', 'PIC']
  endif

  # Select appropriate format
  # Assume ELF{,64} format by default
  if host_system == 'darwin'
    nasm_fmt = 'macho' + (host_cpu.endswidth('64') ? '64' : '')
  elif host_system == 'windows'
    nasm_fmt = 'win' + (host_cpu.endswith('64') ? '64' : '32')
  else
    nasm_fmt = 'elf' + (host_cpu.endswith('64') ? '64' : '')
  endif

  # Forward leading underscore requirement to assembler files
  if cc.symbols_have_underscore_prefix()
    asflags += ['-d', 'LEADING_UNDERSCORE']
  endif

  # Some more per-host tweaks
  if host_system == 'windows'
    if host_cpu == 'x86_64'
      asflags += ['-d', 'WIN64']
    endif
  endif

  # Get NASM and print it's version
  nasm = find_program('nasm', join_paths(m64pwindeppath, 'nasm-2.13.03/nasm.exe'), native : true)

  # Assemble each assembly file
  # BUG?: Can't make it work with generator because depends : asm_defines_headers is not honored
  # So we use a custom_target
#  nasm_gen = generator(nasm,
#      output : '@BASENAME@.obj',
#      depfile : '@BASENAME@.dep',
#      arguments : ['-o', '@OUTPUT@', '-f', nasm_fmt, '-MD', '@DEPFILE@', '@EXTRA_ARGS@', '@INPUT@'])
#  src += nasm_gen.process(src_nasm, extra_args : asflags, depends : asm_defines_headers)
  foreach x : src_nasm
    src += custom_target(x.underscorify() + '.o',
        output : ['@BASENAME@.o'],
        input : x,
        depfile : '@BASENAME@.dep',
        depends : asm_defines_headers,
        command : [nasm, '-o', '@OUTPUT@', '-f', nasm_fmt, '-MD', '@DEPFILE@', asflags, '@INPUT@'])
  endforeach
endif

mupen64plus = library('mupen64plus',
    sources : src,
    include_directories : inc,
    link_args : vflag,
    link_depends : vscript,
    dependencies : [libm, zlib_dep, png_dep, sdl_dep, minizip_dep, md5_dep, xxhash_dep, extra_dep],
    version : libmupen64plus_version,
    install : true,
    install_dir : get_option('libdir'),
    c_args : cflags,
    cpp_args : cflags)

mupen64plus_dep = declare_dependency(
  include_directories : include_directories('src/api'),
  link_with : mupen64plus)

# generate pkg-config files
pkg = import('pkgconfig')
pkg.generate(
  name : 'mupen64plus',
  description : 'Core module of the Mupen64Plus project',
  url : 'https://www.mupen64plus.org/',
  version : libmupen64plus_version,
  filebase : 'libmupen64plus',
  libraries : mupen64plus,
  subdirs : 'mupen64plus',
  variables : [
    # XXX: not sure what to put here ? It would be best if sharedir is relative to ${prefix}
    #'sharedir=' + sharedir
    'sharedir=' + join_paths('${prefix}', get_option('datadir'), 'mupen64plus')
  ])

install_headers(public_headers, subdir : 'mupen64plus')

install_data(shared_files, install_dir : sharedir)
