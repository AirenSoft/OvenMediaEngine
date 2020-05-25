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
	
	class PushProviderStream : public Stream
	{
	public:
		PushProviderStream(const std::shared_ptr<PushProvider> &provider);

	protected:
		// app name, stream name, tracks 가 모두 준비되면 호출한다. 
		// provider->AssignStream (app)
		// app-> NotifyStreamReady(this)
		bool NotifyStreamReady();

	private:

	};
}