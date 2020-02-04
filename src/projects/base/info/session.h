#pragma once

#include <string>
#include "base/common_types.h"
#include "base/ovlibrary/enable_shared_from_this.h"

typedef uint32_t session_id_t;

namespace info
{
	class Stream;

	class Session : public ov::EnableSharedFromThis<info::Session>
	{
	public:
		Session(const info::Stream &stream);
		explicit Session(const info::Stream &stream, session_id_t session_id);
		Session(const info::Stream &stream, const Session &T);
		Session(Session &&T) = default;
		~Session() override = default;

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