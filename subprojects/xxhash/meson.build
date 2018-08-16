project('xxhash', 'c',
    version : '0.6.5')

src = ['xxhash.c']

inc = include_directories('.')

libxxhash = static_library('xxhash',
    src,
    include_directories : inc)

xxhash_dep = declare_dependency(
    include_directories : inc,
    link_with : libxxhash)
