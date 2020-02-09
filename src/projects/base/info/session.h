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
		const std::chrono::system_clock::time_point &GetCreatedTime() const;
		uint64_t GetSentBytes();

		const Stream &GetStream() const
		{
			return *_stream;
		}

	protected:
		uint64_t								_sent_bytes = 0;
		uint64_t								_received_bytes = 0;

	private:
		session_id_t 							_id;
		std::chrono::system_clock::time_point 	_created_time;
		std::shared_ptr<info::Stream>			_stream;
	};
}  // namespace info