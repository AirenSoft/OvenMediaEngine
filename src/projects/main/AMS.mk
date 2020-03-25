LOCAL_PATH := $(call get_local_path)
include $(DEFAULT_VARIABLES)

LOCAL_STATIC_LIBRARIES := \
	webrtc_publisher \
	segment_publishers \
	ovt_publisher \
	ovt_provider \
	rtmp_provider \
	rtsp_provider \
	rtspc_provider \
	transcoder \
	rtc_signalling \
	ice \
	jsoncpp \
	http_server \
	signed_url \
	dtls_srtp \
	rtp_rtcp \
	sdp \
	h264 \
	web_console \
	mediarouter \
	ovt_packetizer \
	orchestrator \
	publisher \
	application \
	physical_port \
	socket \
	ovcrypto \
	config \
	ovlibrary \
	monitoring \
	jsoncpp \
	sqlite

LOCAL_PREBUILT_LIBRARIES := \
	libpugixml.a

LOCAL_LDFLAGS := -lpthread

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

LOCAL_TARGET := OvenMediaEngine

# Update git information
.PHONY: update_git_info
projects/main/git_info.h: update_git_info
projects/main/main.h: projects/main/git_info.h
update_git_info:
	@cd $(PRIVATE_LOCAL_PATH) && ./update_git_info.sh

include $(BUILD_EXECUTABLE)
