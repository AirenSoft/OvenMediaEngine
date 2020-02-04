#pragma once

#include <string>
#include "base/common_types.h"
#include "base/ovlibrary/enable_shared_from_this.h"

typedef uint32_t session_id_t;

namespace info
{
	class Stream;

	class SessionInfo : public ov::EnableSharedFromThis<info::SessionInfo>
	{
	public:
		SessionInfo(const info::Stream &stream);
		explicit SessionInfo(const info::Stream &stream, session_id_t session_id);
		SessionInfo(const info::Stream &stream, const SessionInfo &T);
		SessionInfo(SessionInfo &&T) = default;
		~SessionInfo() override = default;

		session_id_t GetId() const;

		const Stream &GetStream() const
		{
			return *_stream;
		}

	private:
		// 세션 ID
		session_id_t _id;
		std::shared_ptr<info::Stream>		_stream;
	};
}  // namespace info