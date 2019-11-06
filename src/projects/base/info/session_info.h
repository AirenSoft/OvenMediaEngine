#pragma once

#include <string>
#include "base/ovlibrary/enable_shared_from_this.h"
#include "base/common_types.h"

// 세션 ID 타입
typedef int session_id_t;

class SessionInfo : public ov::EnableSharedFromThis<SessionInfo>
{
public:
	SessionInfo();
	explicit SessionInfo(session_id_t session_id);
	SessionInfo(const SessionInfo &T);
	SessionInfo(SessionInfo &&T) = default;
	~SessionInfo() override = default;

	session_id_t GetId() const;

private:
	// 세션 ID
	session_id_t _id;
};