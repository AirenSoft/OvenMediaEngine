//==============================================================================
//
//  TranscodeApplication
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <algorithm>
#include <thread>
#include "base/media_route/media_route_application_observer.h"
#include "base/media_route/media_route_application_connector.h"
#include "base/media_route/media_buffer.h"
#include "base/info/stream.h"
#include "transcode_stream.h"
#include <base/ovlibrary/ovlibrary.h>

class TranscodeApplication : public MediaRouteApplicationConnector, public MediaRouteApplicationObserver
{
public:
	static std::shared_ptr<TranscodeApplication> Create(const info::Application &application_info);

	explicit TranscodeApplication(const info::Application &application_info);
	~TranscodeApplication() override;

	bool Start();
	bool Stop();

	MediaRouteApplicationObserver::ObserverType GetObserverType() override
	{
		return MediaRouteApplicationObserver::ObserverType::Transcoder;
	}

	MediaRouteApplicationConnector::ConnectorType GetConnectorType() override
	{
		return MediaRouteApplicationConnector::ConnectorType::Transcoder;
	}

	////////////////////////////////////////////////////////////////////////////////////////////////
	// MediaRouteApplicationObserver Implementation 
	////////////////////////////////////////////////////////////////////////////////////////////////
	bool OnCreateStream(const std::shared_ptr<info::Stream> &stream) override;
	bool OnDeleteStream(const std::shared_ptr<info::Stream> &stream) override;

	bool OnSendVideoFrame(const std::shared_ptr<info::Stream> &stream, const std::shared_ptr<MediaPacket> &media_packet) override
	{
		return true;
	}

	bool OnSendAudioFrame(const std::shared_ptr<info::Stream> &stream, const std::shared_ptr<MediaPacket> &media_packet) override
	{
		return true;
	}

	bool OnSendFrame(const std::shared_ptr<info::Stream> &stream, const std::shared_ptr<MediaPacket> &packet) override;

private:
	std::map<int32_t, std::shared_ptr<TranscodeStream>> _streams;
	std::mutex _mutex;

	const info::Application _application_info;
};

