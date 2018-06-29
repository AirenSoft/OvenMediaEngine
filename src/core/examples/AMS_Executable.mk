LOCAL_PATH := $(call get_local_path)
include $(DEFAULT_VARIABLES)

LOCAL_STATIC_LIBRARIES := \
	hlibrary

LOCAL_SHARED_LIBRARIES := \
	test

LOCAL_PREBUILT_LIBRARIES := \
	libprebuilt.a

LOCAL_TARGET := main

include $(BUILD_EXECUTABLE)
