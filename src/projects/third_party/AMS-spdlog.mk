LOCAL_PATH := $(call get_local_path)

SPDLOG_PATH := $(shell realpath "$(LOCAL_PATH)/spdlog-1.15.1")
SPDLOG_BUILD_PATH := $(shell realpath "$(CONFIG_OBJECT_DIRECTORY)")/spdlog
SPDLOG_LIB_PATH := $(SPDLOG_BUILD_PATH)/lib/libspdlog.a

include $(CLEAR_VARIABLES)

LOCAL_TARGET := libspdlog.a
LOCAL_PREBUILT_TARGET := $(SPDLOG_LIB_PATH)

include $(BUILD_PREBUILT_LIBRARY)

include $(CLEAR_VARIABLES)

$(SPDLOG_BUILD_PATH):
	@mkdir -p "$(SPDLOG_BUILD_PATH)"

$(SPDLOG_LIB_PATH): $(SPDLOG_BUILD_PATH)
	@cd "$(SPDLOG_BUILD_PATH)" && \
	cmake "$(SPDLOG_PATH)" \
		"-DCMAKE_INSTALL_PREFIX=$(SPDLOG_BUILD_PATH)" \
		"-DCMAKE_INSTALL_LIBDIR=$(SPDLOG_BUILD_PATH)/lib" && \
	$(MAKE) && \
	$(MAKE) install && \
	touch "$(SPDLOG_LIB_PATH)"
