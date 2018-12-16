LOCAL_PATH := $(call get_local_path)
include $(DEFAULT_VARIABLES)

LOCAL_TARGET := ovlibrary

LOCAL_HEADER_FILES := $(LOCAL_HEADER_FILES) \
	projects/base/ovlibrary/ovlibrary.h.gch

LOCAL_SOURCE_FILES := $(LOCAL_SOURCE_FILES) \
	projects/base/ovlibrary/ovlibrary.h.gch

# =============
# Precompiled Header (temporary solution)
OVLIBRARY_PATH := $(LOCAL_PATH)/ovlibrary.h
OVLIBRARY_GCH := $(OVLIBRARY_PATH).gch
$(LOCAL_PATH)/log.cpp: $(OVLIBRARY_GCH)

OVLIBRARY_DEPS := ovlibrary.h.P
$(OVLIBRARY_GCH): $(OVLIBRARY_DEPS)
$(OVLIBRARY_DEPS):
	@g++ -I../ -I../third_party -MM $(OVLIBRARY_PATH) -MT $(OVLIBRARY_GCH) -o $@
$(OVLIBRARY_GCH) : $(OVLIBRARY_DEPS)
	@echo "Creating precompiled header $@..."
	@g++ -o $@ $(OVLIBRARY_PATH) -Iprojects/ -Iprojects/third_party
include $(OVLIBRARY_DEPS)
# =============

include $(BUILD_STATIC_LIBRARY)
