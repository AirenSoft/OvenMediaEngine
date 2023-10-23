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
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <thread>
#include <vector>

#include "base/info/stream.h"
#include "base/mediarouter/media_buffer.h"
#include "base/mediarouter/mediarouter_application_connector.h"
#include "base/mediarouter/mediarouter_application_observer.h"
#include "modules/transcode_webhook/transcode_webhook.h"
#include "transcoder_stream.h"

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
	bool OnStreamUpdated(const std::shared_ptr<info::Stream> &stream) override;

	bool OnSendFrame(const std::shared_ptr<info::Stream> &stream, const std::shared_ptr<MediaPacket> &packet) override;

private:
	bool ValidateAppConfiguration();

private:
	const info::Application _application_info;
	std::map<int32_t, std::shared_ptr<TranscoderStream>> _streams;
	std::mutex _mutex;
	TranscodeWebhook _transcode_webhook;
};
