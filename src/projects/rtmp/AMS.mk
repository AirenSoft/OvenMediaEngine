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
 
LOCAL_TARGET := rtmpprovider

LOCAL_SOURCE_FILES := $(LOCAL_SOURCE_FILES) $(call get_sub_source_list,chunk)
LOCAL_HEADER_FILES := $(LOCAL_HEADER_FILES) $(call get_sub_source_list,chunk)

include $(BUILD_STATIC_LIBRARY)
# include $(BUILD_EXECUTABLE)
