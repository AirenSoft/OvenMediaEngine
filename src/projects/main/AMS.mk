LOCAL_PATH := $(call get_local_path)
include $(DEFAULT_VARIABLES)

LOCAL_STATIC_LIBRARIES := \
	webrtc_publisher \
	llhls_publisher \
	hls_publisher \
	ovt_publisher \
	file_publisher \
	push_publisher \
	thumbnail_publisher \
	ovt_provider \
	rtmp_provider \
	srt_provider \
	mpegts_provider \
	rtspc_provider \
	webrtc_provider \
	scheduled_provider \
	multiplex_provider \
	transcoder \
	rtc_signalling \
	whip \
	address_utilities \
	ice \
	api_server \
	json_serdes \
	bitstream \
	http \
	dtls_srtp \
	rtp_rtcp \
	sdp \
	id3v2 \
	segment_writer \
	web_console \
	mediarouter \
	rtsp_module \
	jitter_buffer \
	ovt_packetizer \
	orchestrator \
	origin_map_client \
	publisher \
	application \
	access_controller \
	physical_port \
	socket \
	ovcrypto \
	config \
	ovlibrary \
	monitoring \
	jsoncpp \
	dump \
	srt \
	file_provider \
	managed_queue \
	ffmpeg_wrapper \
	

# rtsp_provider 

LOCAL_PREBUILT_LIBRARIES := \
	libpugixml.a

LOCAL_LDFLAGS := -lpthread -luuid



$(call add_pkg_config,srt)
$(call add_pkg_config,libavformat)
$(call add_pkg_config,libavfilter)
$(call add_pkg_config,libavcodec)
$(call add_pkg_config,libswresample)
$(call add_pkg_config,libswscale)
$(call add_pkg_config,libavutil)
$(call add_pkg_config,openssl)
$(call add_pkg_config,vpx)
$(call add_pkg_config,opus)
$(call add_pkg_config,libsrtp2)
$(call add_pkg_config,libpcre2-8)
$(call add_pkg_config,hiredis)

ifeq ($(call chk_pkg_exist,ffnvcodec),0)
$(call add_pkg_config,ffnvcodec)
endif

# Enable Xilinx Media SDK
ifeq ($(call chk_pkg_exist,libxma2api),0)
$(call add_pkg_config,libxma2api)
$(call add_pkg_config,xvbm)
$(call add_pkg_config,libxrm)
HWACCELS_XMA_ENABLED := true
PROJECT_CXXFLAGS += -DHWACCELS_XMA_ENABLED
endif

# Enable NVidia Accelerator
ifeq ($(call chk_lib_exist,libcuda.so), 0) 
ifeq ($(call chk_lib_exist,libnvidia-ml.so), 0)
HWACCELS_NVIDIA_ENABLED := true
LOCAL_LDFLAGS += -L/usr/local/cuda/lib64 -lcuda -lnvidia-ml
PROJECT_CXXFLAGS += -I/usr/local/cuda/include -DHWACCELS_NVIDIA_ENABLED 
endif
endif

# Enable Netint Accelerator
ifeq ($(call chk_lib_exist,libxcoder_logan.so), 0)
$(info $(ANSI_YELLOW)- Netint Accelerator is enabled$(ANSI_RESET))
PROJECT_CXXFLAGS += -DHWACCELS_NILOGAN_ENABLED
endif

ifeq ($(shell echo $${OSTYPE}),linux-musl) 
# For alpine linux
LOCAL_LDFLAGS += -lexecinfo
endif


# Enable jemalloc 
ifeq ($(MAKECMDGOALS),release)
$(call add_pkg_config,jemalloc)
endif

LOCAL_TARGET := OvenMediaEngine

# Update git information
PRIVATE_MAIN_PATH := $(LOCAL_PATH)
$(shell "$(PRIVATE_MAIN_PATH)/update_git_info.sh" >/dev/null 2>&1)

BUILD_FILES_TO_CLEAN += "$(PRIVATE_MAIN_PATH)/git_info.h"

include $(BUILD_EXECUTABLE)
