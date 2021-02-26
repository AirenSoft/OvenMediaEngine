#pragma once

#include <string>

#include "base/common_types.h"
#include "base/ovlibrary/enable_shared_from_this.h"
#include "session.h"
#include "stream.h"
#include "vhost_app_name.h"

namespace info
{
	class Stream;

	class Record : public ov::EnableSharedFromThis<info::Record>
	{
	public:
		Record();
		~Record() override = default;

		void SetTransactionId(ov::String transaction_id);
		ov::String GetTransactionId();

		void SetId(ov::String id);
		ov::String GetId() const;

		void SetMetadata(ov::String metadata);
		ov::String GetMetadata() const;

		void SetStream(const info::Stream &stream);
		const info::Stream &GetStream() const
		{
			return *_stream;
		}

		void SetEnable(bool eanble);
		bool GetEnable();

		void SetVhost(ov::String vhost_name);
		ov::String GetVhost();

		void SetApplication(ov::String app_name);
		ov::String GetApplication();

		void SetRemove(bool value);
		bool GetRemove();

		ov::String GetStreamName();

		void SetSessionId(session_id_t id);
		session_id_t GetSessionId();

		void SetInterval(int32_t interval);
		int32_t GetInterval();

		void SetFilePath(ov::String file_path);
		ov::String GetFilePath();

		void SetInfoPath(ov::String info_path);
		ov::String GetInfoPath();

		void SetFilePathSetByUser(bool by_user);
		bool IsFilePathSetByUser();
		void SetInfoPathSetByUser(bool by_user);
		bool IsInfoPathSetByUser();

		void SetTmpPath(ov::String tmp_path);
		ov::String GetTmpPath();

		void IncreaseRecordBytes(uint64_t bytes);
		uint64_t GetRecordBytes();
		uint64_t GetRecordTotalBytes();

		void UpdateRecordTime();
		uint64_t GetRecordTime();
		uint64_t GetRecordTotalTime();

		void IncreaseSequence();

		void UpdateRecordStartTime();
		void UpdateRecordStopTime();

		uint64_t GetSequence();

		const std::chrono::system_clock::time_point &GetCreatedTime() const;
		const std::chrono::system_clock::time_point GetRecordStartTime() const;
		const std::chrono::system_clock::time_point GetRecordStopTime() const;

		enum class RecordState : int8_t
		{
			Ready,
			Recording,
			Stopping,
			Stopped,
			Error
		};

		RecordState GetState();
		void SetState(RecordState state);
		ov::String GetStateString();

		const ov::String GetInfoString();

	private:
		ov::String _transaction_id;

		// User custom id
		ov::String _id;

		// It is used as additional information for recording session. It's not essential information.
		ov::String _metadata;

		// Enabled/Disabled Flag
		bool _enable;

		// Remove Flag
		bool _remove;

		// Virtual Host
		ov::String _vhost_name;

		// Application
		ov::String _aplication_name;

		// The stream target for the Outbound that you want to record
		std::shared_ptr<info::Stream> _stream;

		// Path
		ov::String _file_path;
		bool _file_path_by_user;

		ov::String _info_path;
		bool _info_path_by_user;

		ov::String _tmp_path;

		// Recording interval
		//  0 : infinite
		//  > 0 : seconds
		int32_t _interval;

		// Recorded (Accumulated) file size
		uint64_t _record_bytes;
		uint64_t _record_total_bytes;

		// Recorded (Accumulated) Time
		uint64_t _record_time;
		uint64_t _record_total_time;

		// Sequence number of the recorded file
		uint32_t _sequence;

		// Event Occurrence Time
		std::chrono::system_clock::time_point _created_time;
		std::chrono::system_clock::time_point _record_start_time;
		std::chrono::system_clock::time_point _record_stop_time;

		RecordState _state;

		// File Session Id
		session_id_t _session_id;
	};
}  // namespace info