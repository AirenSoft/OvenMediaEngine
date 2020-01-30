#pragma once


#include <stdint.h>
#include <memory>
#include <vector>

#include "base/ovlibrary/ovlibrary.h"

class MediaRouteApplicationObserver;
class MediaRouteApplicationConnector;
namespace info{class StreamInfo;}
class MediaPacket;
class MediaRouteStream;

class RelayClient;

class MediaRouteApplicationInterface : public ov::EnableSharedFromThis<MediaRouteApplicationInterface>
{
public:
	virtual bool OnCreateStream(const std::shared_ptr<MediaRouteApplicationConnector> &application, const std::shared_ptr<info::StreamInfo> &stream) = 0;
	virtual bool OnDeleteStream(const std::shared_ptr<MediaRouteApplicationConnector> &application, const std::shared_ptr<info::StreamInfo> &stream) = 0;
	virtual bool OnReceiveBuffer(const std::shared_ptr<MediaRouteApplicationConnector> &application, const std::shared_ptr<info::StreamInfo> &stream, const std::shared_ptr<MediaPacket> &packet) = 0;

	virtual const std::map<uint32_t, std::shared_ptr<MediaRouteStream>> GetStreams() const = 0;
};

