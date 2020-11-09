#pragma once

#include <string>
#include "base/common_types.h"
#include "base/ovlibrary/enable_shared_from_this.h"
#include "stream.h"

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

		const std::chrono::system_clock::time_point &GetCreatedTime() const;

		void SetFilePath(ov::String file_path);
		void SetTmpPath(ov::String tmp_path);
		void SetFileInfoPath(ov::String fileinfo_path);
		void IncreaseRecordBytes(uint64_t bytes);
		void IncreaseRecordTime(uint64_t time);
		void IncreaseSequence();
		void UpdateRecordStartTime();
		void UpdateRecordStopTime();

		ov::String GetFilePath();
		ov::String GetTmpPath();
		ov::String GetFileInfoPath();
		uint64_t GetRecordBytes();
		uint64_t GetRecordTime();
		uint64_t GetRecordTotalBytes();
		uint64_t GetRecordTotalTime();
		uint64_t GetSequence();
		std::chrono::system_clock::time_point GetRecordStartTime();
		std::chrono::system_clock::time_point GetRecordStopTime();

		inline ov::String GetInfoString() {
			ov::String info = "";

			info.AppendFormat("_id=%s\n", _id.CStr());
			info.AppendFormat("_stream=%s\n", _stream->GetName().CStr());
			info.AppendFormat("_file_path=%s\n", _file_path.CStr());
			info.AppendFormat("_tmp_path=%s\n", _tmp_path.CStr());
			info.AppendFormat("_fileinfo_path=%s\n", _fileinfo_path.CStr());
			info.AppendFormat("_record_bytes=%lld\n", _record_bytes);
			info.AppendFormat("_record_bytes=%lld\n", _record_bytes);
			info.AppendFormat("_record_total_bytes=%lld\n", _record_total_bytes);
			info.AppendFormat("_record_total_time=%lld\n", _record_total_time);
			info.AppendFormat("_sequence=%d\n", _sequence);
			info.AppendFormat("_created_time=%s\n", ov::Converter::ToString(_created_time).CStr());
			info.AppendFormat("_record_start_time=%s\n", ov::Converter::ToString(_record_start_time).CStr());
			info.AppendFormat("_record_stop_time=%s", ov::Converter::ToString(_record_stop_time).CStr());

			return info;
		}

	private:
		ov::String 								_id;
		std::shared_ptr<info::Stream>			_stream;

		ov::String 								_file_path;
		ov::String  							_tmp_path;
		ov::String 								_fileinfo_path;

		uint64_t								_record_bytes;
		uint64_t								_record_time;

		uint64_t 								_record_total_bytes;
		uint64_t 								_record_total_time;

		uint32_t 								_sequence;

		std::chrono::system_clock::time_point	_created_time;

		std::chrono::system_clock::time_point	_record_start_time;		
		std::chrono::system_clock::time_point	_record_stop_time;
	};
}  // namespace info