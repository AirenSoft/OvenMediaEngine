LOCAL_PATH := $(call get_local_path)
include $(DEFAULT_VARIABLES)

LOCAL_STATIC_LIBRARIES := \
	application \
	ovlibrary \
	provider

LOCAL_PREBUILT_LIBRARIES := \

LOCAL_LDFLAGS := \
    -lpthread \
    -ldl \
    -lz
 
LOCAL_TARGET := rtsp_provider

LOCAL_SOURCE_FILES := $(LOCAL_SOURCE_FILES) \
    $(call get_sub_source_list,rtp) \
    $(call get_sub_source_list,rtcp)
LOCAL_HEADER_FILES := $(LOCAL_HEADER_FILES) \
    $(call get_sub_header_list,rtp) \
    $(call get_sub_header_list,rtcp)

$(call add_pkg_config,openssl)
$(call add_pkg_config,srt)

include $(BUILD_STATIC_LIBRARY)
