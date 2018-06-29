#pragma once


#include <stdint.h>
#include <memory>
#include <vector>


#include "base/ovlibrary/enable_shared_from_this.h"
#include "base/application/stream_info.h"
#include "base/application/application_info.h"
#include "base/common_video.h"

#include "media_route_interface.h"
#include "media_route_application_interface.h"
#include "media_buffer.h"

class MediaRouteApplicationConnector : public ov::EnableSharedFromThis<MediaRouteApplicationConnector>
{
public:
	enum ConnectorType {
		CONNECTOR_TYPE_PROVIDER = 0,
		CONNECTOR_TYPE_TRANSCODER
	};

	////////////////////////////////////////////////////////////////////////////////////////////////
	// 인터페이스
	////////////////////////////////////////////////////////////////////////////////////////////////
public:
	// MediaRouteApplication -> Stream 생성
	inline bool CreateStream(
		std::shared_ptr<StreamInfo> stream_info) 
	{
		if(!GetMediaRouteApplication())
			return false;
		return GetMediaRouteApplication()->OnCreateStream(this->GetSharedPtr(), stream_info);
	}

	// MediaRouteApplication -> Stream 삭제
	inline bool DeleteStream(
		std::shared_ptr<StreamInfo> stream_info)
	{
		if(!GetMediaRouteApplication())
			return false;
		return GetMediaRouteApplication()->OnDeleteStream(this->GetSharedPtr(), stream_info);
	}

	// MediaRouteApplication -> Stream-> Frame
	inline bool SendFrame(
		std::shared_ptr<StreamInfo> stream_info, 
		std::unique_ptr<MediaBuffer> buffer)
	{
		if(!GetMediaRouteApplication())
			return false;
		return GetMediaRouteApplication()->OnReceiveBuffer(this->GetSharedPtr(), stream_info, std::move(buffer));
	}

	virtual ConnectorType GetConnectorType() {
		return CONNECTOR_TYPE_PROVIDER;
	}
	
	////////////////////////////////////////////////////////////////////////////////////////////////
	// 연동 모듈
	////////////////////////////////////////////////////////////////////////////////////////////////
public:
	// @see: media_router_application.cpp / MediaRouteApplication::RegisterConnectorApp
	inline void SetMediaRouterApplication(std::shared_ptr<MediaRouteApplicationInterface> route_application) 
	{
		_media_route_application = route_application;
	}
	inline std::shared_ptr<MediaRouteApplicationInterface>& GetMediaRouteApplication() 
	{ 
		return _media_route_application; 
	}

private:
	std::shared_ptr<MediaRouteApplicationInterface>	_media_route_application;
};

