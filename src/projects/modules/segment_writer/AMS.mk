LOCAL_PATH := $(call get_local_path)
include $(DEFAULT_VARIABLES)

LOCAL_TARGET := segment_writer

include $(BUILD_STATIC_LIBRARY)
