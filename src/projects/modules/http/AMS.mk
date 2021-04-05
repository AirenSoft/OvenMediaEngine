LOCAL_PATH := $(call get_local_path)
include $(DEFAULT_VARIABLES)

LOCAL_TARGET := http_server

LOCAL_SOURCE_FILES := $(LOCAL_SOURCE_FILES) \
    $(call get_sub_source_list_r,server) \
    $(call get_sub_source_list_r,client)

LOCAL_HEADER_FILES := $(LOCAL_HEADER_FILES) \
    $(call get_sub_header_list_r,server) \
    $(call get_sub_source_list_r,client)

$(call add_pkg_config,srt)
$(call add_pkg_config,openssl)

include $(BUILD_STATIC_LIBRARY)
