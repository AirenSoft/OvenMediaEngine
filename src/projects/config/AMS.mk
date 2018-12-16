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
OVCONFIG_PATH := $(LOCAL_PATH)/config.h
OVCONFIG_GCH := $(OVCONFIG_PATH).gch
OVCONFIG_DEPS := config.h.P

$(LOCAL_PATH)/config_loader.cpp: $(OVCONFIG_GCH)

$(OVCONFIG_GCH): $(OVCONFIG_DEPS)

$(OVCONFIG_DEPS):
	@g++ -I../ -I../third_party -MM $(OVCONFIG_PATH) -MT $(OVCONFIG_GCH) -o $@

$(OVCONFIG_GCH) : $(OVCONFIG_DEPS)
	@echo "Creating precompiled header $@..."
	@g++ -o $@ $(OVCONFIG_PATH) -Iprojects/ -Iprojects/third_party
include $(OVCONFIG_DEPS)
# =============

include $(BUILD_STATIC_LIBRARY)
