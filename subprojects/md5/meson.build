project('md5', 'c',
    version : '1.6')

src = ['md5.c']

inc = include_directories('.')

libmd5 = static_library('md5',
    src,
    include_directories : inc)

md5_dep = declare_dependency(
    include_directories : inc,
    link_with : libmd5)
