#pragma once


#include <stdint.h>
#include <memory>
#include <vector>

#include <base/ovlibrary/enable_shared_from_this.h>
#include <base/info/stream_info.h>

#include "media_route_interface.h"
#include "media_route_application_interface.h"

class MediaRouteApplicationObserver : public ov::EnableSharedFromThis<MediaRouteApplicationObserver>
{
public:
	enum class ObserverType : int8_t
	{
		Publisher = 0,
		Transcoder,
		Relay
	};

	////////////////////////////////////////////////////////////////////////////////////////////////
	// Interface
	////////////////////////////////////////////////////////////////////////////////////////////////
	virtual bool OnCreateStream(std::shared_ptr<StreamInfo> info) = 0;
	virtual bool OnDeleteStream(std::shared_ptr<StreamInfo> info) = 0;

	// Delivery encoded video frame
	virtual bool OnSendVideoFrame(std::shared_ptr<StreamInfo> stream, std::shared_ptr<MediaTrack> track, std::unique_ptr<EncodedFrame> encoded_frame, std::unique_ptr<CodecSpecificInfo> codec_info, std::unique_ptr<FragmentationHeader> fragmentation) = 0;

	// Delivery encoded audio frame
	virtual bool OnSendAudioFrame(std::shared_ptr<StreamInfo> stream, std::shared_ptr<MediaTrack> track, std::unique_ptr<EncodedFrame> encoded_frame, std::unique_ptr<CodecSpecificInfo> codec_info, std::unique_ptr<FragmentationHeader> fragmentation) = 0;

	// Provider 등에서 전달 받은 비디오/오디오 프레임 전달
	virtual bool OnSendFrame(std::shared_ptr<StreamInfo> info, std::unique_ptr<MediaPacket> packet)
	{
		return false;
	}

	virtual ObserverType GetObserverType()
	{
		return ObserverType::Publisher;
	}
};

