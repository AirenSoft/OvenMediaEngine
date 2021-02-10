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
	virtual bool OnStreamCreated(const std::shared_ptr<info::Stream> &info) = 0;
	virtual bool OnStreamDeleted(const std::shared_ptr<info::Stream> &info) = 0;
	virtual bool OnStreamPrepared(const std::shared_ptr<info::Stream> &info) = 0;

	// Delivery encoded video/audio frame
	virtual bool OnSendFrame(const std::shared_ptr<info::Stream> &info,
								const std::shared_ptr<MediaPacket> &packet) = 0;

	virtual ObserverType GetObserverType()
	{
		return ObserverType::Publisher;
	}
};

