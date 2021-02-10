LOCAL_PATH := $(call get_local_path)
include $(DEFAULT_VARIABLES)

LOCAL_STATIC_LIBRARIES := \
    monitoring \
	application \
	ovlibrary \

LOCAL_TARGET := config

LOCAL_SOURCE_FILES := $(LOCAL_SOURCE_FILES) \
    $(call get_sub_source_list,engine) \
    $(call get_sub_source_list,items) \
    $(call get_sub_source_list,utilities)

LOCAL_HEADER_FILES := $(LOCAL_HEADER_FILES) \
    $(call get_sub_header_list,engine) \
    $(call get_sub_header_list,items) \
    $(call get_sub_header_list,utilities)

$(call add_pkg_config,srt)

include $(BUILD_STATIC_LIBRARY)
