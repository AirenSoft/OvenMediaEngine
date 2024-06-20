LOCAL_PATH := $(call get_local_path)
include $(DEFAULT_VARIABLES)

LOCAL_STATIC_LIBRARIES := \
	application \
	ovlibrary \
	flv_container \
	provider
	
LOCAL_PREBUILT_LIBRARIES := \

LOCAL_LDFLAGS := \
    -lpthread \
    -ldl \
    -lz
 
LOCAL_TARGET := rtmp_provider

LOCAL_SOURCE_FILES := $(LOCAL_SOURCE_FILES) $(call get_sub_source_list,chunk) $(call get_sub_source_list,amf0)
LOCAL_HEADER_FILES := $(LOCAL_HEADER_FILES) $(call get_sub_source_list,chunk) $(call get_sub_source_list,amf0)

$(call add_pkg_config,srt)

include $(BUILD_STATIC_LIBRARY)
# include $(BUILD_EXECUTABLE)
