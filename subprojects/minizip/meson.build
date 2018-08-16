project('minizip', 'c',
    version : '1.1')

src = [ 'ioapi.c',
        'zip.c',
        'unzip.c' ]

inc = include_directories('.')

cc = meson.get_compiler('c')

# Disable crypt and uncrypt modules
c_args = ['-DNOCRYPT', '-DNOUNCRYPT']

# Add some warnings
foreach x : ['-Wno-unused-parameter']
    if cc.has_argument(x)
        c_args += x
    endif
endforeach

zlib_dep = dependency('zlib', fallback : ['zlib', 'zlib_dep'])

if host_machine.system() == 'openbsd'
    c_args += ['-DIOAPI_NO_64']
endif

libminizip = static_library('minizip',
    src,
    include_directories : inc,
    dependencies : [zlib_dep],
    c_args : c_args)

minizip_dep = declare_dependency(
    include_directories : inc,
    link_with : libminizip)
