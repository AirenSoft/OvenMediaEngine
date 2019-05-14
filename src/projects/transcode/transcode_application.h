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

// 미디어 라우터 구조체
#include "base/media_route/media_route_application_observer.h"
#include "base/media_route/media_route_application_connector.h"
#include "base/media_route/media_buffer.h"

// 공옹 구조체
#include "base/application/stream_info.h"

#include "transcode_stream.h"

#include <base/ovlibrary/ovlibrary.h>


class TranscodeApplication : public MediaRouteApplicationConnector, public MediaRouteApplicationObserver
{
public:
	static std::shared_ptr<TranscodeApplication> Create(const info::Application *application_info);

	explicit TranscodeApplication(const info::Application *application_info);
	~TranscodeApplication() override;

	MediaRouteApplicationObserver::ObserverType GetObserverType()
	{
		return MediaRouteApplicationObserver::ObserverType::Transcoder;
	}

	MediaRouteApplicationConnector::ConnectorType GetConnectorType()
	{
		return MediaRouteApplicationConnector::ConnectorType::Transcoder;
	}

	////////////////////////////////////////////////////////////////////////////////////////////////
	// 트랜스코드 어플리케이션 관련 모듈
	////////////////////////////////////////////////////////////////////////////////////////////////
	bool OnCreateStream(
		std::shared_ptr<StreamInfo> stream_info) override;

	bool OnDeleteStream(
		std::shared_ptr<StreamInfo> stream_info) override;

	bool OnSendVideoFrame(
		std::shared_ptr<StreamInfo> stream_info,
		std::shared_ptr<MediaTrack> track,
		std::unique_ptr<EncodedFrame> encoded_frame,
		std::unique_ptr<CodecSpecificInfo> codec_info,
		std::unique_ptr<FragmentationHeader> fragmentation) override;

	bool OnSendAudioFrame(std::shared_ptr<StreamInfo> stream, std::shared_ptr<MediaTrack> track, std::unique_ptr<EncodedFrame> encoded_frame, std::unique_ptr<CodecSpecificInfo> codec_info, std::unique_ptr<FragmentationHeader> fragmentation) override;

	bool OnSendFrame(
		std::shared_ptr<StreamInfo> stream_info,
		std::unique_ptr<MediaPacket> packet
	) override;

private:
	std::map<int32_t, std::shared_ptr<TranscodeStream>> _streams;
	std::mutex _mutex;

	const info::Application *_application_info;
};

