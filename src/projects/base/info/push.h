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

		void SetEnable(bool eanble) ;
		bool GetEnable();

		void SetVhost(ov::String value);
		ov::String GetVhost();

		void SetApplication(ov::String value);
		ov::String GetApplication();

		// set by user
		void SetStreamName(ov::String stream_name);
		ov::String GetStreamName();

		// set by user
		void AddTrackId(uint32_t selected_id);
		void AddTrackName(ov::String selected_name);

		void SetTrackIds(const std::vector<uint32_t>& ids);
		void SetTrackNames(const std::vector<ov::String>& names);

		const std::vector<uint32_t>& GetTrackIds();
		const std::vector<ov::String>& GetTrackNames();

		void SetRemove(bool value);
		bool GetRemove();

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
		ov::String _application_name;

		// Stream
		ov::String _stream_name;

		// The stream target for the Outbound that you want to record
		std::vector<uint32_t> _selected_track_ids;
		std::vector<ov::String> _selected_track_names;

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