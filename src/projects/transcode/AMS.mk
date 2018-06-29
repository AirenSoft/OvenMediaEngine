LOCAL_PATH := $(call get_local_path)
include $(DEFAULT_VARIABLES)

LOCAL_STATIC_LIBRARIES := \
	application \
	ovlibrary \

LOCAL_LDFLAGS := \
	`pkg-config --libs libavformat` \
	`pkg-config --libs libavfilter` \
	`pkg-config --libs libavcodec` \
	`pkg-config --libs libswresample` \
	`pkg-config --libs libswscale` \
	`pkg-config --libs libavutil` \
	`pkg-config --libs vpx` \
	`pkg-config --libs opus`

LOCAL_TARGET := transcoder

LOCAL_SOURCE_FILES := $(LOCAL_SOURCE_FILES) $(call get_sub_source_list,codec) $(call get_sub_source_list,filter)
LOCAL_HEADER_FILES := $(LOCAL_HEADER_FILES) $(call get_sub_source_list,codec) $(call get_sub_source_list,filter)

include $(BUILD_STATIC_LIBRARY)

