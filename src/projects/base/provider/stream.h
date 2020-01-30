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
#include "base/info/stream_info.h"

namespace pvd
{
	class Application;

	class Stream : public info::StreamInfo, public ov::EnableSharedFromThis<Stream>
	{
	public:
		enum class State
		{
			IDLE,
			CONNECTED,
			DESCRIBED,
			PLAYING,
			STOPPED,
			ERROR
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

		virtual bool Start() {return true;}
		virtual bool Stop() {return true;}

	protected:
		Stream(const std::shared_ptr<pvd::Application> &application, StreamSourceType source_type);
		Stream(const std::shared_ptr<pvd::Application> &application, info::stream_id_t stream_id, StreamSourceType source_type);
		Stream(const std::shared_ptr<pvd::Application> &application, const StreamInfo &stream_info);

		virtual ~Stream();

		State 	_state;

		std::shared_ptr<pvd::Application> _application;
	};
}