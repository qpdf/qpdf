TARGETS_lib := z
SRCS_lib_z := adler32.c compress.c crc32.c gzio.c uncompr.c deflate.c trees.c \
       zutil.c inflate.c infback.c inftrees.c inffast.c
XCPPFLAGS += -DHAVE_VSNPRINTF
RULES := ccxx
