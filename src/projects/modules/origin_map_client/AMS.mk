LOCAL_PATH := $(call get_local_path)
include $(DEFAULT_VARIABLES)

LOCAL_TARGET := origin_map_client

$(call add_pkg_config,srt)
$(call add_pkg_config,hiredis)

include $(BUILD_STATIC_LIBRARY)
