#pragma once


#include <stdint.h>
#include <memory>
#include <vector>

#include <base/ovlibrary/enable_shared_from_this.h>
#include <base/info/stream.h>

#include "media_route_interface.h"
#include "media_route_application_interface.h"

class MediaRouteApplicationObserver : public ov::EnableSharedFromThis<MediaRouteApplicationObserver>
{
public:
	enum class ObserverType : int8_t
	{
		Publisher = 0,
		Transcoder,
		Relay,

		// Temporarily used until Orchestrator takes stream management
		Orchestrator
	};

	////////////////////////////////////////////////////////////////////////////////////////////////
	// Interface
	////////////////////////////////////////////////////////////////////////////////////////////////
	virtual bool OnCreateStream(const std::shared_ptr<info::Stream> &info) = 0;
	virtual bool OnDeleteStream(const std::shared_ptr<info::Stream> &info) = 0;

	// Delivery encoded video frame
	virtual bool OnSendVideoFrame(const std::shared_ptr<info::Stream> &stream,
									const std::shared_ptr<MediaPacket> &media_packet) = 0;

	// Delivery encoded audio frame
	virtual bool OnSendAudioFrame(const std::shared_ptr<info::Stream> &stream,
									const std::shared_ptr<MediaPacket> &media_packet) = 0;

	// Provider 등에서 전달 받은 비디오/오디오 프레임 전달
	virtual bool OnSendFrame(const std::shared_ptr<info::Stream> &info,
								const std::shared_ptr<MediaPacket> &packet)
	{
		return false;
	}

	virtual ObserverType GetObserverType()
	{
		return ObserverType::Publisher;
	}
};

