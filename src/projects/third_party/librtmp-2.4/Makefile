VERSION=v2.4

prefix=/usr/local

incdir=$(prefix)/include/librtmp
bindir=$(prefix)/bin
libdir=$(prefix)/lib
mandir=$(prefix)/man
BINDIR=$(DESTDIR)$(bindir)
INCDIR=$(DESTDIR)$(incdir)
LIBDIR=$(DESTDIR)$(libdir)
MANDIR=$(DESTDIR)$(mandir)

CC=$(CROSS_COMPILE)gcc
LD=$(CROSS_COMPILE)ld
AR=$(CROSS_COMPILE)ar

SYS=posix
#CRYPTO=OPENSSL
#CRYPTO=GNUTLS
DEF_POLARSSL=-DUSE_POLARSSL
DEF_OPENSSL=-DUSE_OPENSSL
DEF_GNUTLS=-DUSE_GNUTLS
DEF_=-DNO_CRYPTO
REQ_GNUTLS=gnutls,hogweed,nettle
REQ_OPENSSL=libssl,libcrypto
PUB_GNUTLS=-lgmp
LIBZ=-lz
LIBS_posix=
LIBS_darwin=
LIBS_mingw=-lws2_32 -lwinmm -lgdi32
LIB_GNUTLS=-lgnutls -lhogweed -lnettle -lgmp $(LIBZ)
LIB_OPENSSL=-lssl -lcrypto $(LIBZ)
LIB_POLARSSL=-lpolarssl $(LIBZ)
PRIVATE_LIBS=$(LIBS_$(SYS))
CRYPTO_LIB=$(LIB_$(CRYPTO)) $(PRIVATE_LIBS)
CRYPTO_REQ=$(REQ_$(CRYPTO))
CRYPTO_DEF=$(DEF_$(CRYPTO))
PUBLIC_LIBS=$(PUB_$(CRYPTO))

SO_VERSION=1
SOX_posix=so
SOX_darwin=dylib
SOX_mingw=dll
SOX=$(SOX_$(SYS))
SO_posix=.$(SOX).$(SO_VERSION)
SO_darwin=.$(SO_VERSION).$(SOX)
SO_mingw=-$(SO_VERSION).$(SOX)
SO_EXT=$(SO_$(SYS))

SODIR_posix=$(LIBDIR)
SODIR_darwin=$(LIBDIR)
SODIR_mingw=$(BINDIR)
SODIR=$(SODIR_$(SYS))

SO_LDFLAGS_posix=-shared -Wl,-soname,$@
SO_LDFLAGS_darwin=-dynamiclib -twolevel_namespace -undefined dynamic_lookup \
	-fno-common -headerpad_max_install_names -install_name $(libdir)/$@
SO_LDFLAGS_mingw=-shared -Wl,--out-implib,librtmp.dll.a
SO_LDFLAGS=$(SO_LDFLAGS_$(SYS))

INSTALL_IMPLIB_posix=
INSTALL_IMPLIB_darwin=
INSTALL_IMPLIB_mingw=cp librtmp.dll.a $(LIBDIR)
INSTALL_IMPLIB=$(INSTALL_IMPLIB_$(SYS))

SHARED=yes
SODEF_yes=-fPIC
SOLIB_yes=librtmp$(SO_EXT)
SOINST_yes=install_so
SO_DEF=$(SODEF_$(SHARED))
SO_LIB=$(SOLIB_$(SHARED))
SO_INST=$(SOINST_$(SHARED))

DEF=-DRTMPDUMP_VERSION=\"$(VERSION)\" $(CRYPTO_DEF) $(XDEF)
OPT=-O2
CFLAGS=-Wall $(XCFLAGS) $(INC) $(DEF) $(OPT) $(SO_DEF)
LDFLAGS=$(XLDFLAGS)


OBJS=rtmp.o log.o amf.o hashswf.o parseurl.o

all:	librtmp.a $(SO_LIB)

clean:
	rm -f *.o *.a *.$(SOX) *$(SO_EXT) librtmp.pc

librtmp.a: $(OBJS)
	$(AR) rs $@ $?

librtmp$(SO_EXT): $(OBJS)
	$(CC) $(SO_LDFLAGS) $(LDFLAGS) -o $@ $^ $> $(CRYPTO_LIB)
	ln -sf $@ librtmp.$(SOX)

log.o: log.c log.h Makefile
rtmp.o: rtmp.c rtmp.h rtmp_sys.h handshake.h dh.h log.h amf.h Makefile
amf.o: amf.c amf.h bytes.h log.h Makefile
hashswf.o: hashswf.c http.h rtmp.h rtmp_sys.h Makefile
parseurl.o: parseurl.c rtmp.h rtmp_sys.h log.h Makefile

librtmp.pc: librtmp.pc.in Makefile
	sed -e "s;@prefix@;$(prefix);" -e "s;@libdir@;$(libdir);" \
		-e "s;@VERSION@;$(VERSION);" \
		-e "s;@CRYPTO_REQ@;$(CRYPTO_REQ);" \
		-e "s;@PUBLIC_LIBS@;$(PUBLIC_LIBS);" \
		-e "s;@PRIVATE_LIBS@;$(PRIVATE_LIBS);" librtmp.pc.in > $@

install:	install_base $(SO_INST)

install_base:	librtmp.a librtmp.pc
	-mkdir -p $(INCDIR) $(LIBDIR)/pkgconfig $(MANDIR)/man3 $(SODIR)
	cp amf.h http.h log.h rtmp.h $(INCDIR)
	cp librtmp.a $(LIBDIR)
	cp librtmp.pc $(LIBDIR)/pkgconfig
	cp librtmp.3 $(MANDIR)/man3

install_so:	librtmp$(SO_EXT)
	cp librtmp$(SO_EXT) $(SODIR)
	$(INSTALL_IMPLIB)
	cd $(SODIR); ln -sf librtmp$(SO_EXT) librtmp.$(SOX)

