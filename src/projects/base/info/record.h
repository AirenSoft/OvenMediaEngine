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

	class Record : public ov::EnableSharedFromThis<info::Record>
	{
	public:
		Record();
		~Record() override = default;

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
		
		void SetFilePath(ov::String file_path);
		ov::String GetFilePath();

		void SetTmpPath(ov::String tmp_path);
		ov::String GetTmpPath();

		void SetFileInfoPath(ov::String fileinfo_path);
		ov::String GetFileInfoPath();

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

		ov::String _file_path;
		ov::String _tmp_path;
		ov::String _fileinfo_path;

		uint64_t _record_bytes;
		uint64_t _record_time;

		uint64_t _record_total_bytes;
		uint64_t _record_total_time;

		uint32_t _sequence;

		std::chrono::system_clock::time_point _created_time;

		std::chrono::system_clock::time_point _record_start_time;		
		std::chrono::system_clock::time_point _record_stop_time;	

		RecordState _state;

		// File Session Id
		session_id_t _session_id;		
	};
}  // namespace info