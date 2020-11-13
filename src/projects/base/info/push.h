#pragma once

#include <string>
#include "base/common_types.h"
#include "base/ovlibrary/enable_shared_from_this.h"
#include "stream.h"
#include "vhost_app_name.h"
#include "session.h"

namespace info
{
	class Stream;

	class Push : public ov::EnableSharedFromThis<info::Push>
	{
	public:
		Push();
		~Push() override = default;

		void SetId(ov::String id);
		ov::String GetId() const;

		void SetStream(const info::Stream &stream);
		const info::Stream &GetStream() const
		{
			return *_stream;
		}
		
		void SetEnable(bool eanble) ;
		bool GetEnable();

		void SetVhost(ov::String value);
		ov::String GetVhost();

		void SetApplication(ov::String value);
		ov::String GetApplication();

		void SetRemove(bool value);
		bool GetRemove();

		ov::String GetStreamName();

		void SetSessionId(session_id_t id);
		session_id_t GetSessionId();
		
		void SetProtocol(ov::String protocol);
		ov::String GetProtocol();

		void SetUrl(ov::String url);
		ov::String GetUrl();

		void SetStreamKey(ov::String stream_key);
		ov::String GetStreamKey();

		void IncreasePushBytes(uint64_t bytes);
		uint64_t GetPushBytes();
		uint64_t GetPushTotalBytes();

		void UpdatePushTime();
		uint64_t GetPushTime();
		uint64_t GetPushTotalTime();

		void IncreaseSequence();

		void UpdatePushStartTime();
		void UpdatePushStopTime();

		uint64_t GetSequence();

		const std::chrono::system_clock::time_point &GetCreatedTime() const;
		const std::chrono::system_clock::time_point GetPushStartTime() const;
		const std::chrono::system_clock::time_point GetPushStopTime() const;

		enum class PushState : int8_t
		{
			Ready,
			Pushing,
			Stopping,
			Stopped,
			Error
		};

		PushState GetState();
		void SetState(PushState state);
		ov::String GetStateString();

		const ov::String GetInfoString();

	private:
		ov::String _id;
	
		// Enabled/Disabled Flag
		bool _enable;

		// Remove Flag
		bool _remove;

		// Virtual Host
		ov::String _vhost_name;

		// Application
		ov::String _aplication_name;

		std::shared_ptr<info::Stream> _stream;

		ov::String _protocol;
		ov::String _url;
		ov::String _stream_key;

		uint64_t _Push_bytes;
		uint64_t _Push_time;

		uint64_t _Push_total_bytes;
		uint64_t _Push_total_time;

		uint32_t _sequence;

		std::chrono::system_clock::time_point _created_time;

		std::chrono::system_clock::time_point _Push_start_time;		
		std::chrono::system_clock::time_point _Push_stop_time;	

		PushState _state;

		// File Session Id
		session_id_t _session_id;		
	};
}  // namespace info