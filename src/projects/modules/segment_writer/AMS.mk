LOCAL_PATH := $(call get_local_path)
include $(DEFAULT_VARIABLES)

LOCAL_TARGET := segment_writer

$(call add_pkg_config,libavformat)

include $(BUILD_STATIC_LIBRARY)
