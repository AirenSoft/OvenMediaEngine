#include <random>
#include <base/ovlibrary/ovlibrary.h>
#include "session_info.h"

SessionInfo::SessionInfo()
{
	_id = ov::Random::GenerateUInt32();
}

SessionInfo::SessionInfo(session_id_t session_id)
{
	_id = session_id;
}

SessionInfo::SessionInfo(const SessionInfo &T)
{
	_id = T._id;
}

session_id_t SessionInfo::GetId() const
{
	return _id;
}