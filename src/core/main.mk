#===============================================================================
# Configure variables
#===============================================================================
SHELL := /bin/bash
BUILD_ROOT := .
BUILD_SYSTEM_DIRECTORY := core

#===============================================================================
# Include makefiles
#===============================================================================
include $(BUILD_SYSTEM_DIRECTORY)/pre_processing.mk
include $(BUILD_SYSTEM_DIRECTORY)/os_version.mk
include $(BUILD_SYSTEM_DIRECTORY)/colors.mk
include $(BUILD_SYSTEM_DIRECTORY)/global_config.mk
include $(BUILD_SYSTEM_DIRECTORY)/config.mk
include $(BUILD_SYSTEM_DIRECTORY)/environments.mk
include $(BUILD_SYSTEM_DIRECTORY)/utilities.mk
include $(BUILD_SYSTEM_DIRECTORY)/progress.mk

#===============================================================================
# OS detection for use utilties
#===============================================================================
ifeq ($(OS_VERSION),)
    $(error Unable to determine OS_VERSION from uname -sm: $(UNAME)!)
endif

#===============================================================================
# Rules
#===============================================================================
BUILD_TARGET_LIST :=

.PHONY: all help release
all: directories_to_prepare build_target_list
release: all
help:
	@echo ""
	@echo " $(ANSI_GREEN)* AMS Help Page$(ANSI_RESET)"
	@echo ""
	@echo "   Usage: make $(ANSI_YELLOW)[COMMANDS]$(ANSI_RESET)"
	@echo ""
	@echo "   Commands:"
	@echo "       $(ANSI_YELLOW)help$(ANSI_RESET): show this page"
	@echo "       $(ANSI_YELLOW)release$(ANSI_RESET): make project to release"
	@echo ""

# clean할 때 target이 삭제될 수 있도록 함
# ifneq ($(MAKECMDGOALS),clean)
include $(BUILD_PROJECTS_DIRECTORY)/AMS.mk
# endif

include $(BUILD_SYSTEM_DIRECTORY)/informations.mk

BUILD_TARGET_LIST := $(strip $(BUILD_TARGET_LIST))
BUILD_BUILT_COUNT := 1
BUILD_TOTAL_PROJECTS_COUNT := $(words $(BUILD_TARGET_LIST))

.PHONY: build_target_list
build_target_list: $(BUILD_TARGET_LIST)
	@$(TARGET_COUNTER)
	@echo $(CURRENT_PROGRESS)"$(CONFIG_COMPLETE_COLOR)Completed.$(ANSI_RESET)"$(INCREASE_COUNT)

.PHONY: directories_to_prepare
directories_to_prepare:
	@$(TARGET_COUNTER)
	@echo $(CURRENT_PROGRESS)"$(CONFIG_CREATE_COLOR)Preparing directories$(ANSI_RESET)..."$(INCREASE_COUNT)
	@for dir in $(BUILD_DIRECTORIES_TO_PREPARE); \
	do \
		if [ ! -d $$dir ]; \
		then \
			echo "    $(CONFIG_CREATE_COLOR)Preparing directory$(ANSI_RESET) $$dir..."; \
			mkdir -p $$dir; \
		fi \
	done

.PHONY: clean clean_internal
clean_internal:
	@$(TARGET_COUNTER)
	@echo $(CURRENT_PROGRESS)"$(CONFIG_CLEAN_COLOR)Deleting directories$(ANSI_RESET)..."$(INCREASE_COUNT)
	@for dir in $(BUILD_DIRECTORIES_TO_CLEAN); \
	do \
		echo "    $(CONFIG_CLEAN_COLOR)Deleting directory$(ANSI_RESET) $$dir..."; \
		rm -rf $$dir; \
	done
	@for target in $(BUILD_TARGET_LIST); \
	do \
		echo "    $(CONFIG_CLEAN_COLOR)Deleting target$(ANSI_RESET) $$target..."; \
		rm -rf $$target; \
	done

clean: clean_internal
	@$(TARGET_COUNTER)
	@echo $(CURRENT_PROGRESS)"$(CONFIG_CLEAN_COLOR)Cleaned.$(ANSI_RESET)"$(INCREASE_COUNT)

