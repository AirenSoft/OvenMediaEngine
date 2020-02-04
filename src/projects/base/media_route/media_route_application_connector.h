#pragma once

#include <stdint.h>
#include <memory>
#include <utility>
#include <vector>

#include "base/ovlibrary/enable_shared_from_this.h"
#include "base/info/stream.h"
#include "base/common_types.h"

#include "media_route_interface.h"
#include "media_route_application_interface.h"
#include "media_buffer.h"

class MediaRouteApplicationConnector : public ov::EnableSharedFromThis<MediaRouteApplicationConnector>
{
public:
	enum class ConnectorType : int8_t
	{
		Provider = 0,
		Transcoder,
		Relay
	};

	// MediaRouteApplication -> Stream 생성
	inline bool CreateStream(const std::shared_ptr<info::Stream> &stream)
	{
		if(GetMediaRouteApplication() == nullptr)
		{
			return false;
		}

		return GetMediaRouteApplication()->OnCreateStream(this->GetSharedPtr(), stream);
	}

	// MediaRouteApplication -> Stream 삭제
	inline bool DeleteStream(const std::shared_ptr<info::Stream> &stream)
	{
		if(GetMediaRouteApplication() == nullptr)
		{
			return false;
		}

		return GetMediaRouteApplication()->OnDeleteStream(this->GetSharedPtr(), stream);
	}

	// MediaRouteApplication -> Stream-> Frame
	inline bool SendFrame(const std::shared_ptr<info::Stream> &stream, const std::shared_ptr<MediaPacket> &packet)
	{
		if(GetMediaRouteApplication() == nullptr)
		{
			OV_ASSERT(false, "MediaRouteAppplication MUST NOT BE NULL");
			return false;
		}

		return GetMediaRouteApplication()->OnReceiveBuffer(this->GetSharedPtr(), stream, packet);
	}

	virtual ConnectorType GetConnectorType()
	{
		return ConnectorType::Provider;
	}

public:
	// @see: media_router_application.cpp / MediaRouteApplication::RegisterConnectorApp
	inline void SetMediaRouterApplication(const std::shared_ptr<MediaRouteApplicationInterface> &route_application)
	{
		_media_route_application = route_application;
	}

	inline std::shared_ptr<MediaRouteApplicationInterface> &GetMediaRouteApplication()
	{
		return _media_route_application;
	}

private:
	std::shared_ptr<MediaRouteApplicationInterface> _media_route_application;
};

