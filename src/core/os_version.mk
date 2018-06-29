UNAME := $(shell uname -sm)

ifneq (,$(findstring Solaris,$(UNAME)))
	OS_VERSION := solaris
	ANSI_SUPPORTED := false
endif
ifneq (,$(findstring SunOS,$(UNAME)))
	OS_VERSION := sunos
	ANSI_SUPPORTED := false
endif
ifneq (,$(findstring Linux,$(UNAME)))
	OS_VERSION := linux
	ANSI_SUPPORTED := true
endif
ifneq (,$(findstring Darwin,$(UNAME)))
	OS_VERSION := darwin
	ANSI_SUPPORTED := true
endif
ifneq (,$(findstring Macintosh,$(UNAME)))
	OS_VERSION := darwin
	ANSI_SUPPORTED := true
endif
ifneq (,$(findstring CYGWIN,$(UNAME)))
	OS_VERSION := windows
	ANSI_SUPPORTED := true
endif
