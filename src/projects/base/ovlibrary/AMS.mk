LOCAL_PATH := $(call get_local_path)
include $(DEFAULT_VARIABLES)

LOCAL_TARGET := ovlibrary

LOCAL_HEADER_FILES := $(LOCAL_HEADER_FILES) \
	projects/base/ovlibrary/ovlibrary.h.gch

LOCAL_SOURCE_FILES := $(LOCAL_SOURCE_FILES) \
	projects/base/ovlibrary/ovlibrary.h.gch

# =============
# Precompiled Header (temporary solution)
LOCAL_PRECOMPILE_FILE := $(LOCAL_PATH)/ovlibrary.h
LOCAL_PRECOMPILE_TEMP_DEP := $(LOCAL_PATH)/log.cpp

# Dummy dependency
LOCAL_PRECOMPILE_TARGET := $(LOCAL_PRECOMPILE_FILE).gch
LOCAL_PRECOMPILE_DEPS := $(LOCAL_PRECOMPILE_FILE).P

$(LOCAL_PRECOMPILE_TEMP_DEP) : $(LOCAL_PRECOMPILE_TARGET)

$(LOCAL_PRECOMPILE_DEPS):
	@echo "Generating precompiled header dependencies for $@..."
	@g++ -I../ -I../third_party -MM $(LOCAL_PRECOMPILE_FILE) -MT $(LOCAL_PRECOMPILE_TARGET) -o $@

$(LOCAL_PRECOMPILE_TARGET) : $(LOCAL_PRECOMPILE_DEPS)
	@echo "Creating precompiled header $@..."
	@g++ -o $@ $(LOCAL_PRECOMPILE_FILE) -Iprojects/ -Iprojects/third_party

include $(LOCAL_PRECOMPILE_DEPS)
# =============

include $(BUILD_STATIC_LIBRARY)
