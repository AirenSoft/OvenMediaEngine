LOCAL_PATH := $(call get_local_path)
include $(DEFAULT_VARIABLES)

LOCAL_TARGET := web_console

include $(BUILD_STATIC_LIBRARY)
