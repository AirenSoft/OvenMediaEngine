LOCAL_PATH := $(call get_local_path)
include $(DEFAULT_VARIABLES)

LOCAL_STATIC_LIBRARIES := \
	webrtc_publisher \
	segment_publishers \
	ovt_publisher \
	file_publisher \
	mpegtspush_publisher \
	rtmppush_publisher \
	thumbnail_publisher \
	ovt_provider \
	rtmp_provider \
	srt_provider \
	mpegts_provider \
	rtspc_provider \
	webrtc_provider \
	transcoder \
	rtc_signalling \
	address_utilities \
	ice \
	api_server \
	json_serdes \
	bitstream \
	containers \
	http \
	dtls_srtp \
	rtp_rtcp \
	sdp \
	segment_writer \
	web_console \
	mediarouter \
	rtsp_module \
	jitter_buffer \
	ovt_packetizer \
	orchestrator \
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
	file \
	rtmp \

# rtsp_provider 

LOCAL_PREBUILT_LIBRARIES := \
	libpugixml.a

LOCAL_LDFLAGS := -lpthread -luuid

ifeq ($(shell echo $${OSTYPE}),linux-musl) 
# For alpine linux
LOCAL_LDFLAGS += -lexecinfo
endif

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

# Temporarily stop using JEMALLOC. We will test it more and use it again.
#ifeq ($(MAKECMDGOALS),release)
	# Enable jemalloc 
	# $(call add_pkg_config,jemalloc)
#endif

LOCAL_TARGET := OvenMediaEngine

# Update git information
PRIVATE_MAIN_PATH := $(LOCAL_PATH)
$(shell "$(PRIVATE_MAIN_PATH)/update_git_info.sh" >/dev/null 2>&1)

BUILD_FILES_TO_CLEAN += "$(PRIVATE_MAIN_PATH)/git_info.h"

include $(BUILD_EXECUTABLE)
