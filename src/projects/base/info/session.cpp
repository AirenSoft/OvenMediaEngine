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
	}

	Session::Session(const info::Stream &stream, session_id_t session_id)
	{
		_id = session_id;
		_stream = std::make_shared<info::Stream>(stream);
	}

	Session::Session(const info::Stream &stream, const Session &T)
	{
		_id = T._id;
		_stream = std::make_shared<info::Stream>(stream);
	}

	session_id_t Session::GetId() const
	{
		return _id;
	}
}  // namespace info