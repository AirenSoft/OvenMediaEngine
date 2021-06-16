LOCAL_PATH := $(call get_local_path)
include $(DEFAULT_VARIABLES)

LOCAL_TARGET := ovlibrary

LOCAL_HEADER_FILES := $(LOCAL_HEADER_FILES)
LOCAL_SOURCE_FILES := $(LOCAL_SOURCE_FILES)

$(call add_pkg_config,libpcre2-8)

include $(BUILD_STATIC_LIBRARY)
