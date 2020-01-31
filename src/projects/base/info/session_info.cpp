#include <base/ovlibrary/ovlibrary.h>
#include <random>

#include "session_info.h"
#include "stream_info.h"

namespace info
{
	SessionInfo::SessionInfo(const info::StreamInfo &stream_info)
	{
		_id = ov::Random::GenerateUInt32();
		_stream_info = std::make_shared<info::StreamInfo>(stream_info);
	}

	SessionInfo::SessionInfo(const info::StreamInfo &stream_info, session_id_t session_id)
	{
		_id = session_id;
		_stream_info = std::make_shared<info::StreamInfo>(stream_info);
	}

	SessionInfo::SessionInfo(const info::StreamInfo &stream_info, const SessionInfo &T)
	{
		_id = T._id;
		_stream_info = std::make_shared<info::StreamInfo>(stream_info);
	}

	session_id_t SessionInfo::GetId() const
	{
		return _id;
	}
}  // namespace info