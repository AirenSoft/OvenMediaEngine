//==============================================================================
//
//  PushProvider Stream Base Class 
//
//  Created by Getroot
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include <base/ovlibrary/stop_watch.h>
#include "base/provider/stream.h"

namespace pvd
{
	class PushProvider;
	
	class PushStream : public Stream
	{
	public:
		enum class PushStreamType : uint8_t
		{
			UNKNOWN,
			SIGNALLING, 
			DATA,
			INTERLEAVED
		};

		virtual bool OnDataReceived(const std::shared_ptr<const ov::Data> &data) = 0;
		uint32_t GetChannelId();
		bool DoesBelongApplication();
		virtual PushStreamType GetPushStreamType() = 0;

		uint32_t GetRelatedChannelId();
		void SetRelatedChannelId(uint32_t related_channel_id);

		bool IsReadyToReceiveStreamData();
		bool IsPublished();

		void UpdateLastReceivedTime();
		void SetTimeoutSec(time_t seconds);
		time_t GetElapsedSecSinceLastReceived();
		bool IsTimedOut();

	protected:
		PushStream(StreamSourceType source_type, uint32_t channel_id, const std::shared_ptr<PushProvider> &provider);

		// app name, stream name, tracks 가 모두 준비되면 호출한다. 
		// provider->AssignStream (app)
		// app-> NotifyStreamReady(this)
		bool PublishInterleavedChannel(const info::VHostAppName &vhost_app_name);
		bool PublishDataChannel(const info::VHostAppName &vhost_app_name, const std::shared_ptr<PushStream> &data_channel);

	private:
		uint32_t 		_channel_id = 0;
		// If it's type is DATA, related channel is Signalling, or vice versa. 
		uint32_t		_related_channel_id = 0; 
		// Published?
		bool			_is_published = false;
		// Time elapsed since the last OnDataReceived function was called
		ov::StopWatch 	_stop_watch;
		time_t			_timeout_sec = 0;

		// Push Provider
		std::shared_ptr<PushProvider>	_provider;
	};
}