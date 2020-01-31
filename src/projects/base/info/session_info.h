#pragma once

#include <string>
#include "base/common_types.h"
#include "base/ovlibrary/enable_shared_from_this.h"

typedef uint32_t session_id_t;

namespace info
{
	class StreamInfo;

	class SessionInfo : public ov::EnableSharedFromThis<info::SessionInfo>
	{
	public:
		SessionInfo(const info::StreamInfo &stream_info);
		explicit SessionInfo(const info::StreamInfo &stream_info, session_id_t session_id);
		SessionInfo(const info::StreamInfo &stream_info, const SessionInfo &T);
		SessionInfo(SessionInfo &&T) = default;
		~SessionInfo() override = default;

		session_id_t GetId() const;

		const StreamInfo &GetStreamInfo() const
		{
			return *_stream_info;
		}

	private:
		// 세션 ID
		session_id_t _id;
		std::shared_ptr<info::StreamInfo>		_stream_info;
	};
}  // namespace info