ifeq ($(LOCAL_PATH),)
$(error LOCAL_PATH is not declared)
endif

include $(CLEAR_VARIABLES)

# 여기서 사용할 수 있는 값들은 clear_variables.mk 참고

LOCAL_SOURCE_FILES := $(call get_local_source_list)
LOCAL_HEADER_FILES := $(call get_local_header_list)

