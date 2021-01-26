//==============================================================================
//
//  TranscodeApplication
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include <base/ovlibrary/ovlibrary.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

#include "base/info/stream.h"
#include "base/mediarouter/media_buffer.h"
#include "base/mediarouter/media_route_application_connector.h"
#include "base/mediarouter/media_route_application_observer.h"
#include "transcode_stream.h"

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
	bool OnStreamCreated(const std::shared_ptr<info::Stream> &stream) override;
	bool OnStreamDeleted(const std::shared_ptr<info::Stream> &stream) override;
	bool OnStreamPrepared(const std::shared_ptr<info::Stream> &stream) override;

	bool OnSendFrame(const std::shared_ptr<info::Stream> &stream, const std::shared_ptr<MediaPacket> &packet) override;

private:
	const info::Application _application_info;

private:
	std::map<int32_t, std::shared_ptr<TranscodeStream>> _streams;
	std::mutex _mutex;
};
