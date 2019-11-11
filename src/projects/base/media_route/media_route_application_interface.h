#pragma once


#include <stdint.h>
#include <memory>
#include <vector>

#include "base/ovlibrary/ovlibrary.h"

class MediaRouteApplicationObserver;
class MediaRouteApplicationConnector;
class StreamInfo;
class MediaPacket;
class MediaRouteStream;

class RelayClient;

class MediaRouteApplicationInterface : public ov::EnableSharedFromThis<MediaRouteApplicationInterface>
{
public:
	virtual bool OnCreateStream(std::shared_ptr<MediaRouteApplicationConnector> application, std::shared_ptr<StreamInfo> stream) = 0;
	virtual bool OnDeleteStream(std::shared_ptr<MediaRouteApplicationConnector> application, std::shared_ptr<StreamInfo> stream) = 0;
	virtual bool OnReceiveBuffer(std::shared_ptr<MediaRouteApplicationConnector> application, std::shared_ptr<StreamInfo> stream, std::unique_ptr<MediaPacket> packet) = 0;

	virtual const std::map<uint32_t, std::shared_ptr<MediaRouteStream>> GetStreams() const = 0;
};

