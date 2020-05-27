//==============================================================================
//
//  PushProvider Stream Base Class 
//
//  Created by Getroot
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include "base/provider/stream.h"

namespace pvd
{
	class PushProvider;
	
	class PushStream : public Stream
	{
	public:
		

	protected:
		PushStream(const std::shared_ptr<pvd::Application> &application, const info::Stream &stream_info);
		PushStream(StreamSourceType source_type, const std::shared_ptr<PushProvider> &provider);


		// app name, stream name, tracks 가 모두 준비되면 호출한다. 
		// provider->AssignStream (app)
		// app-> NotifyStreamReady(this)
		bool NotifyStreamReady();

	private:

	};
}