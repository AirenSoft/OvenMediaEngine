#pragma once


#include <stdint.h>
#include <memory>
#include <vector>

#include "base/ovlibrary/ovlibrary.h"

class MediaRouteApplicationObserver;
class MediaRouteApplicationConnector;
namespace info{class Stream;}
class MediaPacket;
class MediaRouteStream;

class RelayClient;

class MediaRouteApplicationInterface : public ov::EnableSharedFromThis<MediaRouteApplicationInterface>
{
public:
	virtual bool OnCreateStream(const std::shared_ptr<MediaRouteApplicationConnector> &application, const std::shared_ptr<info::Stream> &stream) = 0;
	virtual bool OnDeleteStream(const std::shared_ptr<MediaRouteApplicationConnector> &application, const std::shared_ptr<info::Stream> &stream) = 0;
	virtual bool OnReceiveBuffer(const std::shared_ptr<MediaRouteApplicationConnector> &application, const std::shared_ptr<info::Stream> &stream, const std::shared_ptr<MediaPacket> &packet) = 0;
};

