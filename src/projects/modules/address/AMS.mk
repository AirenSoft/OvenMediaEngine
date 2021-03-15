LOCAL_PATH := $(call get_local_path)
include $(DEFAULT_VARIABLES)

LOCAL_STATIC_LIBRARIES := \
    ice \

LOCAL_TARGET := address_utilities

$(call add_pkg_config,srt)

include $(BUILD_STATIC_LIBRARY)