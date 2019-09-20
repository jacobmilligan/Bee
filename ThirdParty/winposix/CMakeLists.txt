bee_new_source_root()
bee_add_sources(
    unistd.h
    getopt.h    getopt.c
)
bee_library(winposix)
target_compile_options(
    winposix
    PRIVATE
    /wd4131  # 'function' : uses old-style declarator
)
