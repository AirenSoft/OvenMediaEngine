GCC_VERSION := $(shell gcc -v 2>&1 | tail -1 | awk ' { print $3 } ')
GCC_VERSION := $(word 3, $(GCC_VERSION))
GCC_VERSION := $(subst ., ,$(GCC_VERSION))

GCC_VERSION_MAJOR := $(word 1, $(GCC_VERSION))
GCC_VERSION_MINOR := $(word 2, $(GCC_VERSION))
GCC_VERSION_PATCH := $(word 3, $(GCC_VERSION))

# <major> + <00 + minor> + <00 + patch>
GCC_VERSION := $(shell expr $(GCC_VERSION_MAJOR) "*" 1000000 "+" $(GCC_VERSION_MINOR) "*" 1000 "+" $(GCC_VERSION_PATCH))

# gcc 4.9 이상부터는 -fdiagnostics-color 옵션을 지원함
# 4.9 = 4009000
ifeq ($(shell test $(GCC_VERSION) -gt 4009000; echo $$?),0)
# 4.9 이상
GLOBAL_CC := gcc
GLOBAL_CXX := g++
GLOBAL_LD := g++
GCC_COLOR_OPTION := -fdiagnostics-color
else
GLOBAL_CC := core/colorgcc
GLOBAL_CXX := core/colorg++
GLOBAL_LD := core/colorg++
GCC_COLOR_OPTION :=
endif

GLOBAL_CFLAGS_COMMON := $(GCC_COLOR_OPTION)
GLOBAL_CXXFLAGS_COMMON := $(GCC_COLOR_OPTION)
GLOBAL_LDFLAGS_COMMON := $(GCC_COLOR_OPTION)

GLOBAL_CFLAGS_RELEASE := -g -O3 $(GLOBAL_CFLAGS_COMMON) -Wall
GLOBAL_CXXFLAGS_RELEASE := -g -O3 $(GLOBAL_CXXFLAGS_COMMON) -Wall
GLOBAL_LDFLAGS_RELEASE := -O3 $(GLOBAL_LDFLAGS_COMMON)

GLOBAL_CFLAGS_DEBUG := -g -DDEBUG -D_DEBUG $(GLOBAL_CFLAGS_COMMON)
GLOBAL_CXXFLAGS_DEBUG := -g -DDEBUG -D_DEBUG $(GLOBAL_CXXFLAGS_COMON)
GLOBAL_LDFLAGS_DEBUG := $(GLOBAL_LDFLAGS_COMMON)

ifneq ($(OS_VERSION), darwin)
# -Wl,--export-dynamic: backtrace 출력가능
GLOBAL_LDFLAGS_DEBUG := $(GLOBAL_LDFLAGS_DEBUG) -Wl,--export-dynamic
endif

