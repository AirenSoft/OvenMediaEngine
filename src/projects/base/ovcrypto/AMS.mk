LOCAL_PATH := $(call get_local_path)
include $(DEFAULT_VARIABLES)

LOCAL_TARGET := ovcrypto

LOCAL_SOURCE_FILES := $(LOCAL_SOURCE_FILES) \
    $(call get_sub_source_list,openssl)

LOCAL_HEADER_FILES := $(LOCAL_HEADER_FILES) \
    $(call get_sub_header_list,openssl)

LOCAL_CFLAGS := $(shell pkg-config --cflags openssl)
LOCAL_LDFLAGS := $(shell pkg-config --libs openssl)

include $(BUILD_STATIC_LIBRARY)
