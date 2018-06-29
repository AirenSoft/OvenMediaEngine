LOCAL_PATH := $(call get_local_path)

include $(CLEAR_VARIABLES)

SOURCE_PATH := jsoncpp-1.8.4

LOCAL_TARGET := jsoncpp
LOCAL_SOURCE_FILES := $(call get_sub_source_list,$(SOURCE_PATH))
LOCAL_HEADER_FILES := $(call get_sub_header_list,$(SOURCE_PATH))

include $(BUILD_STATIC_LIBRARY)
