//==============================================================================
//
//  Provider Base Class 
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================


#include "stream.h"
#include "application.h"
#include "base/info/application.h"
#include "provider_private.h"

namespace pvd
{
	Stream::Stream(StreamSourceType source_type)
		:info::Stream(source_type),
		_application(nullptr)
	{
	}

	Stream::Stream(const std::shared_ptr<pvd::Application> &application, StreamSourceType source_type)
		:info::Stream(*(std::static_pointer_cast<info::Application>(application)), source_type),
		_application(application)
	{
	}

	Stream::Stream(const std::shared_ptr<pvd::Application> &application, info::stream_id_t stream_id, StreamSourceType source_type)
		:info::Stream((*std::static_pointer_cast<info::Application>(application)), stream_id, source_type),
		_application(application)
	{
	}

	Stream::Stream(const std::shared_ptr<pvd::Application> &application, const info::Stream &stream_info)
		:info::Stream(stream_info),
		_application(application)
	{
	}

	Stream::~Stream()
	{

	}

	bool Stream::Start() 
	{
		logti("%s/%s(%u) has been started stream", GetApplicationName(), GetName().CStr(), GetId());
		return true;
	}
	
	bool Stream::Stop() 
	{
		logti("%s/%s(%u) has been stopped playing stream", GetApplicationName(), GetName().CStr(), GetId());
		return true;
	}

	const char* Stream::GetApplicationTypeName()
	{
		if(GetApplication() == nullptr)
		{
			return "Unknown";
		}

		return GetApplication()->GetApplicationTypeName();
	}

	bool Stream::SendFrame(const std::shared_ptr<MediaPacket> &packet)
	{
		if(_application == nullptr)
		{
			return false;
		}

		if(packet->GetPacketType() == cmn::PacketType::Unknwon)
		{
			logte("The packet type must be specified. %s/%s(%u)", GetApplicationName(), GetName().CStr(), GetId());
			return false;
		}

		if(packet->GetPacketType() != cmn::PacketType::OVT && 
			packet->GetBitstreamFormat() == cmn::BitstreamFormat::Unknwon)
		{
			logte("The bitstream format must be specified. %s/%s(%u)", GetApplicationName(), GetName().CStr(), GetId());
			return false;
		}

		// Statistics
		auto stream_metrics = StreamMetrics(*GetSharedPtrAs<info::Stream>());
		if(stream_metrics != nullptr)
		{
			stream_metrics->IncreaseBytesIn(packet->GetData()->GetLength());
		}

		return _application->SendFrame(GetSharedPtr(), packet);
	}

	bool Stream::SetState(State state)
	{
		_state = state;
		return true;
	}
}