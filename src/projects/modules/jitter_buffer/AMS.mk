LOCAL_PATH := $(call get_local_path)
include $(DEFAULT_VARIABLES)

LOCAL_TARGET := jitter_buffer

include $(BUILD_STATIC_LIBRARY)
