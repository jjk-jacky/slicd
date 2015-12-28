BIN_TARGETS := \
slicd-exec \
slicd-parser \
slicd-sched \
setuid \
miniexec

DOC_TARGETS := \
slicd.1 \
slicd-parser.1 \
slicd-sched.1 \
slicd-exec.1 \
setuid.1 \
miniexec.1

ifdef DO_ALLSTATIC
LIBSLICD := libslicd.a
else
LIBSLICD := libslicd.so
endif

ifdef DO_SHARED
SHARED_LIBS := libslicd.so
endif

ifdef DO_STATIC
STATIC_LIBS := libslicd.a
endif
