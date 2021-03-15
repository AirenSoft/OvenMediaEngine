LOCAL_PATH := $(call get_local_path)
include $(DEFAULT_VARIABLES)

LOCAL_TARGET := ice

LOCAL_SOURCE_FILES := $(LOCAL_SOURCE_FILES) \
    $(call get_sub_source_list,stun) \
    $(call get_sub_source_list,stun/attributes) \
	$(call get_sub_source_list,stun/attributes/templates)

LOCAL_HEADER_FILES := $(LOCAL_HEADER_FILES) \
    $(call get_sub_header_list,stun) \
    $(call get_sub_header_list,stun/attributes) \
	$(call get_sub_header_list,stun/attributes/templates)

$(call add_pkg_config,srt)

include $(BUILD_STATIC_LIBRARY)
