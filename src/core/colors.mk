################################################################################
# ANSI color set
################################################################################
ifeq ($(ANSI_SUPPORTED), true)
ifeq ($(OS_VERSION), darwin)
ANSI_RESET:=$(shell echo -e "\033[0m")
ANSI_BLACK:=$(shell echo -e "\033[30m")
ANSI_RED:=$(shell echo -e "\033[31m")
ANSI_GREEN:=$(shell echo -e "\033[32m")
ANSI_YELLOW:=$(shell echo -e "\033[33m")
ANSI_BLUE:=$(shell echo -e "\033[34m")
ANSI_PURPLE:=$(shell echo -e "\033[35m")
ANSI_CYAN:=$(shell echo -e "\033[36m")
ANSI_WHITE:=$(shell echo -e "\033[37m")
else # darwin
ANSI_RESET:=$(shell echo -e "\E[0m")
ANSI_BLACK:=$(shell echo -e "\E[30m")
ANSI_RED:=$(shell echo -e "\E[31m")
ANSI_GREEN:=$(shell echo -e "\E[32m")
ANSI_YELLOW:=$(shell echo -e "\E[33m")
ANSI_BLUE:=$(shell echo -e "\E[34m")
ANSI_PURPLE:=$(shell echo -e "\E[35m")
ANSI_CYAN:=$(shell echo -e "\E[36m")
ANSI_WHITE:=$(shell echo -e "\E[37m")
endif
else
ANSI_RESET:=
ANSI_BLACK:=
ANSI_RED:=
ANSI_GREEN:=
ANSI_YELLOW:=
ANSI_BLUE:=
ANSI_PURPLE:=
ANSI_CYAN:=
ANSI_WHITE:=
endif
