//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2024 AirenSoft. All rights reserved.
//
//==============================================================================
#include "srt_session.h"

#include <base/info/stream.h>
#include <base/ovlibrary/byte_io.h>
#include <base/publisher/stream.h>
#include <monitoring/monitoring.h>

#include "srt_application.h"
#include "srt_private.h"
#include "srt_stream.h"

#define logap(format, ...) logtp("[%s#%u] " format, GetAppStreamName().CStr(), GetId(), ##__VA_ARGS__)
#define logad(format, ...) logtd("[%s#%u] " format, GetAppStreamName().CStr(), GetId(), ##__VA_ARGS__)
#define logas(format, ...) logts("[%s#%u] " format, GetAppStreamName().CStr(), GetId(), ##__VA_ARGS__)

#define logai(format, ...) logti("[%s#%u] " format, GetAppStreamName().CStr(), GetId(), ##__VA_ARGS__)
#define logaw(format, ...) logtw("[%s#%u] " format, GetAppStreamName().CStr(), GetId(), ##__VA_ARGS__)
#define logae(format, ...) logte("[%s#%u] " format, GetAppStreamName().CStr(), GetId(), ##__VA_ARGS__)
#define logac(format, ...) logtc("[%s#%u] " format, GetAppStreamName().CStr(), GetId(), ##__VA_ARGS__)

namespace pub
{
	std::shared_ptr<SrtSession> SrtSession::Create(const std::shared_ptr<Application> &application,
												   const std::shared_ptr<Stream> &stream,
												   uint32_t session_id,
												   const std::shared_ptr<ov::Socket> &connector)
	{
		auto session_info = info::Session(*std::static_pointer_cast<info::Stream>(stream), session_id);
		auto session = std::make_shared<SrtSession>(session_info, application, stream, connector);

		if (session->Start() == false)
		{
			return nullptr;
		}

		return session;
	}

	SrtSession::SrtSession(const info::Session &session_info,
						   const std::shared_ptr<Application> &application,
						   const std::shared_ptr<Stream> &stream,
						   const std::shared_ptr<ov::Socket> &connector)
		: Session(session_info, application, stream)
	{
		_connector = connector;

		MonitorInstance->OnSessionConnected(*GetStream(), PublisherType::Srt);
	}

	SrtSession::~SrtSession()
	{
		Stop();

		logad("SrtSession has finally been terminated");

		MonitorInstance->OnSessionDisconnected(*GetStream(), PublisherType::Srt);
	}

	ov::String SrtSession::GetAppStreamName() const
	{
		const auto &application = GetApplication();
		const auto &stream = GetStream();

		return ov::String::FormatString(
			"%s/%s(%u)",
			application->GetVHostAppName().CStr(),
			stream->GetName().CStr(),
			stream->GetId());
	}

	bool SrtSession::Start()
	{
		logad("SrtSession will be started");
		return Session::Start();
	}

	bool SrtSession::Stop()
	{
		logad("SrtSession has stopped");
		_connector->Close();
		return Session::Stop();
	}

	void SrtSession::SendOutgoingData(const std::any &packet)
	{
		std::shared_ptr<const ov::Data> mpegts_data;

		if (_need_to_send_psi)
		{
			_need_to_send_psi = false;

			auto stream = std::dynamic_pointer_cast<SrtStream>(GetStream());

			if (stream != nullptr)
			{
				_connector->Send(stream->GetPsiData());
			}
			else
			{
				OV_ASSERT2(false);
			}
		}

		try
		{
			mpegts_data = std::any_cast<std::shared_ptr<const ov::Data>>(packet);

			if (mpegts_data == nullptr)
			{
				OV_ASSERT2("SrtStream::BroadcastPacket never sends a null packet");
				return;
			}
		}
		catch (const std::bad_any_cast &e)
		{
			logad("An incorrect type of packet was input from the stream. (%s)", e.what());
			OV_ASSERT2(false);
			return;
		}

		_connector->Send(mpegts_data);
	}

	const std::shared_ptr<ov::Socket> SrtSession::GetConnector()
	{
		return _connector;
	}

	void SrtSession::OnMessageReceived(const std::any &message)
	{
		// Client does not send any message
		OV_ASSERT2(false);
	}
}  // namespace pub