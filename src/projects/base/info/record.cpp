#include <base/ovlibrary/ovlibrary.h>
#include <random>

#include "record.h"
#include "stream.h"

namespace info
{
	Record::Record() : _stream(nullptr)
	{
		_created_time = std::chrono::system_clock::now();
		_id = "";
		_stream = nullptr;

		_file_path = "";
		_tmp_path = "";
		_fileinfo_path = "";

		_record_bytes = 0;
		_record_time = 0;

		_record_total_bytes = 0;
		_record_total_time = 0;
		_sequence = 0;
	}

	void Record::SetId(ov::String record_id)
	{
		_id = record_id;
	}

	ov::String Record::GetId() const
	{
		return _id;
	}

	void Record::SetStream(const info::Stream &stream)
	{
		_stream = std::make_shared<info::Stream>(stream);		
	}

	const std::chrono::system_clock::time_point &Record::GetCreatedTime() const
	{
		return _created_time;
	}

	void Record::SetFilePath(ov::String file_path)
	{
		_file_path = file_path;
	}

	void Record::SetTmpPath(ov::String tmp_path)
	{
		_tmp_path = tmp_path;
	}
	void Record::SetFileInfoPath(ov::String fileinfo_path)
	{
		_fileinfo_path = fileinfo_path;
	}
	void Record::IncreaseRecordBytes(uint64_t bytes)
	{
		_record_bytes += bytes;
		_record_total_bytes += bytes;
	}
	void Record::IncreaseRecordTime(uint64_t time)
	{
		_record_time += time;
		_record_total_time += time;		
	}
	void Record::IncreaseSequence()
	{
		_sequence++;
	}
	void Record::UpdateRecordStartTime()
	{
		_record_start_time = std::chrono::system_clock::now();
	}
	void Record::UpdateRecordStopTime()
	{
		_record_stop_time = std::chrono::system_clock::now();
	}

	ov::String Record::GetFilePath()
	{
		return _file_path;
	}
	ov::String Record::GetTmpPath()
	{
		return _tmp_path;
	}
	ov::String Record::GetFileInfoPath()
	{
		return _fileinfo_path;
	}
	uint64_t Record::GetRecordBytes()
	{
		return _record_bytes;
	}
	uint64_t Record::GetRecordTime()
	{
		return _record_time;
	}
	uint64_t Record::GetRecordTotalBytes()
	{
		return _record_total_bytes;
	}
	uint64_t Record::GetRecordTotalTime()
	{
		return _record_total_time;
	}
	uint64_t Record::GetSequence()
	{
		return _sequence;
	}
	std::chrono::system_clock::time_point Record::GetRecordStartTime()
	{
		return _record_start_time;
	}
	std::chrono::system_clock::time_point Record::GetRecordStopTime()
	{
		return _record_stop_time;
	}

}  // namespace info