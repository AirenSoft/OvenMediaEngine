LOCAL_PATH := $(call get_local_path)
include $(DEFAULT_VARIABLES)

LOCAL_TARGET := provider

LOCAL_SOURCE_FILES := $(LOCAL_SOURCE_FILES) \
    $(call get_sub_source_list,pull_provider) \
    $(call get_sub_source_list,push_provider)

LOCAL_HEADER_FILES := $(LOCAL_HEADER_FILES) \
    $(call get_sub_header_list,pull_provider) \
    $(call get_sub_header_list,push_provider)

$(call add_pkg_config,srt)

include $(BUILD_STATIC_LIBRARY)
