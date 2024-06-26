# Makefile

# Compiler
CC = gcc

# Flags
CFLAGS = -Wall -fPIC
LDFLAGS = -shared

# Source files
SOURCES = libnvidia-ml_stub.c libcuda_stub.c libxma2api_stub.c libxrm_stub.c libxrt_core_stub.c libnppig_stub.c libnppicc_stub.c

# Object files
OBJECTS = $(SOURCES:.c=.o)

# Output libraries
TARGETS = libnvidia-ml.so.1 libcuda.so.1 libxma2api.so.2 \
		  libxrm.so.1 libxrt_core.so.2 libnppig.so.10 libnppig.so.11 libnppig.so.12 libnppicc.so.10 libnppicc.so.11 libnppicc.so.12 

# Install path prefix (default: /opt/ovenmediaengine)
PREFIX ?= /opt/ovenmediaengine
INSTALL_PATH = $(PREFIX)/lib/stubs

# Build rules
all: $(TARGETS)

###############################################
# Stubs library for nvidia driver
###############################################
libnvidia-ml.so.1: libnvidia-ml_stub.o
	$(CC) $(LDFLAGS) -o $@ $^

libcuda.so.1: libcuda_stub.o
	$(CC) $(LDFLAGS) -o $@ $^

libnppig.so.10: libnppig_stub.o
	$(CC) $(LDFLAGS) -o $@ $^

libnppig.so.11: libnppig_stub.o
	$(CC) $(LDFLAGS) -o $@ $^

libnppig.so.12: libnppig_stub.o
	$(CC) $(LDFLAGS) -o $@ $^

libnppicc.so.10: libnppicc_stub.o
	$(CC) $(LDFLAGS) -o $@ $^

libnppicc.so.11: libnppicc_stub.o
	$(CC) $(LDFLAGS) -o $@ $^

libnppicc.so.12: libnppicc_stub.o
	$(CC) $(LDFLAGS) -o $@ $^

###############################################
# Stubs library for xilinx driver
###############################################
libxma2api.so.2: libxma2api_stub.o
	$(CC) $(LDFLAGS) -o $@ $^

libxrm.so.1: libxrm_stub.o
	$(CC) $(LDFLAGS) -o $@ $^

libxrt_core.so.2: libxrt_core_stub.o
	$(CC) $(LDFLAGS) -o $@ $^


%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

install: $(TARGETS)
	mkdir -p $(INSTALL_PATH)
	cp $(TARGETS) $(INSTALL_PATH)

clean:
	rm -f $(OBJECTS) $(TARGETS)

.PHONY: all install clean
