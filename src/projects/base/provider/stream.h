//==============================================================================
//
//  Provider Base Class 
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "base/common_types.h"
#include "base/info/stream.h"
#include "monitoring/monitoring.h"

namespace pvd
{
	class Application;

	class Stream : public info::Stream, public ov::EnableSharedFromThis<Stream>
	{
	public:
		enum class State
		{
			IDLE,
			CONNECTED,
			DESCRIBED,
			PLAYING,
			STOPPING,
			STOPPED,
			ERROR
		};

		enum class ProcessMediaResult
		{
			PROCESS_MEDIA_SUCCESS,
			PROCESS_MEDIA_FAILURE,
			PROCESS_MEDIA_FINISH,
			PROCESS_MEDIA_TRY_AGAIN
		};

		State GetState(){return _state;};

		const std::shared_ptr<pvd::Application> &GetApplication()
		{
			return _application;
		}

		std::shared_ptr<const pvd::Application> GetApplication() const
		{
			return _application;
		}

		virtual bool Start();
		virtual bool Play(); // For PullProvider only, It is called after all publishers create stream
		virtual bool Stop();

		// It is used to detect event by StreamMotor and then StreamMotor calls ProcessMediaPacket
		// Internally it is used for epoll
		virtual int GetFileDescriptorForDetectingEvent()
		{
			return -1;
		}

		// If this stream belongs to the Pull provider, 
		// this function is called periodically by the StreamMotor of application. 
		// Media data has to be processed here.
		virtual ProcessMediaResult ProcessMediaPacket()
		{
			// 0.01 sec
			usleep(10000 * 1);
			return ProcessMediaResult::PROCESS_MEDIA_SUCCESS;
		}

	protected:
		Stream(const std::shared_ptr<pvd::Application> &application, StreamSourceType source_type);
		Stream(const std::shared_ptr<pvd::Application> &application, info::stream_id_t stream_id, StreamSourceType source_type);
		Stream(const std::shared_ptr<pvd::Application> &application, const info::Stream &stream_info);

		virtual ~Stream();

		State 	_state = State::IDLE;

		std::shared_ptr<pvd::Application> _application;
	};
}