#include <base/ovlibrary/ovlibrary.h>
#include <random>

#include "record.h"
#include "stream.h"

namespace info
{
	Record::Record() : _stream(nullptr)
	{
		_created_time = std::chrono::system_clock::now();
	}

	void Record::SetId(ov::String record_id)
	{
		_record_id = record_id;
	}

	ov::String Record::GetId() const
	{
		return _record_id;
	}

	const std::chrono::system_clock::time_point &Record::GetCreatedTime() const
	{
		return _created_time;
	}

	void Record::SetStream(const info::Stream &stream)
	{
		_stream = std::make_shared<info::Stream>(stream);		
	}

}  // namespace info