LOCAL_PATH := $(call get_local_path)

PROJECT_C_INCLUDES := \
	projects \
	projects/third_party

PROJECT_CXX_INCLUDES := \
	$(PROJECT_C_INCLUDES)

# -Wfatal-errors: build 중 첫 번째 오류를 만나면 멈춤
PROJECT_CFLAGS := \
	-D__STDC_CONSTANT_MACROS \
	-Wfatal-errors \
	-Wno-unused-function

PROJECT_CXXFLAGS := \
	$(PROJECT_CFLAGS) \
	-Wliteral-suffix \
	-std=c++17

PROJECT_LDFLAGS := \
	-ldl -lz

include $(CLEAR_VARIABLES)
include $(BUILD_SUB_AMS)
