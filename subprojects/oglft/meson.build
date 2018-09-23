project('oglft', 'cpp',
    version : '1.15')

src = ['OGLFT.cpp']

inc = include_directories('.')

cc = meson.get_compiler('c')
host_cpu = host_machine.cpu_family()
host_system = host_machine.system()

# Provide pre-compiled fallback for windows
# TODO: provide proper wrap
freetype2_dep = dependency('freetype2', fallback : ['freetype2', 'freetype_dep'], required : false)
if not freetype2_dep.found()

  if host_system == 'windows'
    ftdir = get_option('ftdir')

    if host_cpu == 'x86'
      ftlibdir = join_paths(ftdir, 'lib', 'x86')
    elif host_cpu == 'x86_64'
      ftlibdir = join_paths(ftdir, 'lib', 'x64')
    else
      error('Can\'t use pre-build binaries on ' + host_cpu + ' architecture')
    endif

    message('Trying to find freetype26 in :' + ftlibdir)

    freetype2_dep = declare_dependency(
      dependencies : cc.find_library('freetype26', dirs : ftlibdir, required : true),
      include_directories : include_directories([ join_paths(ftdir, 'include') ]))
  else
    error('Can\'t satisfy freetype2 dependency')
  endif
endif

gl_dep = dependency('gl', required : false)
if not gl_dep.found()
  if host_system == 'windows'
    gl_dep = cc.find_library('opengl32', required : true)
  else
    error('Can\'t satisfy gl dependency')
  endif
endif

glu_dep = dependency('glu', required : false)
if not glu_dep.found()
  if host_system == 'windows'
    glu_dep = cc.find_library('glu32', required : true)
  else
    error('Can\'t satisfy glu dependency')
  endif
endif

sdl_dep = dependency('sdl2', fallback : ['sdl2', 'sdl2_dep'], required : false)
if not sdl_dep.found()
  sdl_dep = dependency('sdl')
endif

liboglft = static_library('oglft',
    src,
    include_directories : inc,
    dependencies : [freetype2_dep, gl_dep, glu_dep, sdl_dep])

oglft_dep = declare_dependency(
    include_directories : inc,
    link_with : liboglft,
    dependencies : [freetype2_dep, glu_dep, sdl_dep])
