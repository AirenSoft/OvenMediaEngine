#include <base/ovlibrary/ovlibrary.h>
#include <random>

#include "session_info.h"
#include "stream.h"

namespace info
{
	SessionInfo::SessionInfo(const info::Stream &stream)
	{
		_id = ov::Random::GenerateUInt32();
		_stream = std::make_shared<info::Stream>(stream);
	}

	SessionInfo::SessionInfo(const info::Stream &stream, session_id_t session_id)
	{
		_id = session_id;
		_stream = std::make_shared<info::Stream>(stream);
	}

	SessionInfo::SessionInfo(const info::Stream &stream, const SessionInfo &T)
	{
		_id = T._id;
		_stream = std::make_shared<info::Stream>(stream);
	}

	session_id_t SessionInfo::GetId() const
	{
		return _id;
	}
}  // namespace info