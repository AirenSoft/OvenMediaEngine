//==============================================================================
//
//  PullProvider Base Class 
//
//  Created by Getroot
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "base/common_types.h"
#include "base/info/stream.h"
#include "base/provider/stream.h"
#include "monitoring/monitoring.h"

namespace pvd
{
	class Application;

	class PullStream : public Stream
	{
	public:
		enum class ProcessMediaResult
		{
			PROCESS_MEDIA_SUCCESS,
			PROCESS_MEDIA_FAILURE,
			PROCESS_MEDIA_FINISH,
			PROCESS_MEDIA_TRY_AGAIN
		};

		virtual bool Start() override;
		virtual bool Stop() override;
		virtual bool Play();

		// It is used to detect event by StreamMotor and then StreamMotor calls ProcessMediaPacket
		// Internally it is used for epoll
		virtual int GetFileDescriptorForDetectingEvent() = 0;

		// If this stream belongs to the Pull provider, 
		// this function is called periodically by the StreamMotor of application. 
		// Media data has to be processed here.
		virtual ProcessMediaResult ProcessMediaPacket() = 0;

	protected:
		PullStream(const std::shared_ptr<pvd::Application> &application, const info::Stream &stream_info);
	};
}