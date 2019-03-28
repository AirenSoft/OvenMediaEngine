LOCAL_PATH := $(call get_local_path)
include $(DEFAULT_VARIABLES)

LOCAL_STATIC_LIBRARIES := \
	application \
	ovlibrary \

LOCAL_TARGET := config

LOCAL_SOURCE_FILES := $(LOCAL_SOURCE_FILES) \
    $(call get_sub_source_list,items) \
    $(call get_sub_source_list,utilities)

LOCAL_HEADER_FILES := $(LOCAL_HEADER_FILES) \
    $(call get_sub_header_list,items) \
    $(call get_sub_header_list,utilities) \
    config.h.gch

# =============
# Precompiled Header (temporary solution)
LOCAL_PRECOMPILE_FILE := $(LOCAL_PATH)/item.h
LOCAL_PRECOMPILE_TEMP_DEP := $(LOCAL_PATH)/config_loader.cpp

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

# =============
# Precompiled Header (temporary solution)
LOCAL_PRECOMPILE_FILE := $(LOCAL_PATH)/config.h
LOCAL_PRECOMPILE_TEMP_DEP := $(LOCAL_PATH)/config_loader.cpp

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
