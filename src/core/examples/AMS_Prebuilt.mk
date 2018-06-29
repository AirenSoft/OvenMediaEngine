LOCAL_PATH := $(call get_local_path)
include $(DEFAULT_VARIABLES)

LOCAL_TARGET := libprebuilt.a
LOCAL_PREBUILT_TARGET := $(LOCAL_PATH)/prebuilt/libpre.a

LIBPRE_PATH := $(LOCAL_PATH)/prebuilt

# libpre.a를 생성하기 위한 make rule 지정
$(LOCAL_PREBUILT_TARGET):
	@cd "$(LIBPRE_PATH)" && cmake . && $(MAKE) fmt

include $(BUILD_PREBUILT)
