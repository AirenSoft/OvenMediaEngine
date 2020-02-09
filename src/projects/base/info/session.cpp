#include <base/ovlibrary/ovlibrary.h>
#include <random>

#include "session.h"
#include "stream.h"

namespace info
{
	Session::Session(const info::Stream &stream)
	{
		_id = ov::Random::GenerateUInt32();
		_stream = std::make_shared<info::Stream>(stream);
		_created_time = std::chrono::system_clock::now();
	}

	Session::Session(const info::Stream &stream, session_id_t session_id)
	{
		_id = session_id;
		_stream = std::make_shared<info::Stream>(stream);
		_created_time = std::chrono::system_clock::now();
	}

	Session::Session(const info::Stream &stream, const Session &T)
	{
		_id = T._id;
		_stream = std::make_shared<info::Stream>(stream);
		_created_time = std::chrono::system_clock::now();
	}

	session_id_t Session::GetId() const
	{
		return _id;
	}

	const std::chrono::system_clock::time_point &Session::GetCreatedTime() const
	{
		return _created_time;
	}
	uint64_t Session::GetSentBytes()
	{
		return _sent_bytes;
	}

}  // namespace info