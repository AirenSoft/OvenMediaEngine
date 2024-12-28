//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2024 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/info/media_track.h>
#include <base/ovsocket/socket.h>
#include <base/publisher/session.h>

namespace pub
{
	class SrtSession final : public Session
	{
	public:
		static std::shared_ptr<SrtSession> Create(const std::shared_ptr<Application> &application,
												  const std::shared_ptr<Stream> &stream,
												  uint32_t srt_session_id,
												  const std::shared_ptr<ov::Socket> &connector);

		SrtSession(const info::Session &session_info,
				   const std::shared_ptr<Application> &application,
				   const std::shared_ptr<Stream> &stream,
				   const std::shared_ptr<ov::Socket> &connector);
		~SrtSession() override final;

		//--------------------------------------------------------------------
		// Overriding of Session
		//--------------------------------------------------------------------
		bool Start() override;
		bool Stop() override;

		// Called by Stream::BroadcastPacket in SrtStream
		void SendOutgoingData(const std::any &packet) override;
		void OnMessageReceived(const std::any &message) override;
		//--------------------------------------------------------------------

		void SetRequestedUrl(const std::shared_ptr<const ov::Url> &url)
		{
			_requested_url = url;
		}

		std::shared_ptr<const ov::Url> GetRequestedUrl()
		{
			return _requested_url;
		}

		void SetFinalUrl(const std::shared_ptr<const ov::Url> &url)
		{
			_final_url = url;
		}

		std::shared_ptr<const ov::Url> GetFinalUrl()
		{
			return _final_url;
		}

		const std::shared_ptr<ov::Socket> GetConnector();

	private:
		ov::String GetAppStreamName() const;

	private:
		std::shared_ptr<ov::Socket> _connector;

		bool _need_to_send_psi = true;
		bool _is_keyframe_sent = false;

		std::shared_ptr<const ov::Url> _requested_url;
		std::shared_ptr<const ov::Url> _final_url;
	};
}  // namespace pub
