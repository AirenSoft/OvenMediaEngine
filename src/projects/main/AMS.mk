LOCAL_PATH := $(call get_local_path)
include $(DEFAULT_VARIABLES)

LOCAL_STATIC_LIBRARIES := \
	webrtc \
	transcoder \
	rtc_signalling \
	ice \
	jsoncpp \
	http_server \
	relay \
	dtls_srtp \
	rtp_rtcp \
	sdp \
	publisher \
	web_console \
	mediarouter \
	application \
	physical_port \
	socket \
	ovcrypto \
	config \
	ovlibrary \
	rtmpprovider \
	hls \
	dash \
	segment_stream \
	monitoring \
	jsoncpp \
	sqlite

LOCAL_PREBUILT_LIBRARIES := \
	libpugixml.a

LOCAL_LDFLAGS := \
	-lpthread \
	-ldl \
	`pkg-config --libs srt` \
	`pkg-config --libs libavformat` \
	`pkg-config --libs libavfilter` \
	`pkg-config --libs libavcodec` \
	`pkg-config --libs libswresample` \
	`pkg-config --libs libswscale` \
	`pkg-config --libs libavutil` \
	`pkg-config --libs openssl` \
	`pkg-config --libs vpx` \
	`pkg-config --libs opus` \
	`pkg-config --libs libsrtp2`

LOCAL_TARGET := OvenMediaEngine

include $(BUILD_EXECUTABLE)
