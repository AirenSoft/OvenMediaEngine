SUB_DIRECTORIES := $(call get_sub_directories,$(LOCAL_PATH))
SUB_AMS_FILES := $(strip $(foreach DIR,$(SUB_DIRECTORIES),$(DIR)/AMS.mk))

ifneq ($(SUB_AMS_FILES),)
-include $(SUB_AMS_FILES)
endif
