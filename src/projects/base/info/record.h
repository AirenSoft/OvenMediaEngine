#pragma once

#include <string>
#include "base/common_types.h"
#include "base/ovlibrary/enable_shared_from_this.h"

namespace info
{
	class Stream;

	class Record : public ov::EnableSharedFromThis<info::Record>
	{
	public:
		Record();
		~Record() override = default;


		void SetId(ov::String id);
		ov::String GetId() const;
		const std::chrono::system_clock::time_point &GetCreatedTime() const;

		void SetStream(const info::Stream &stream);
		const info::Stream &GetStream() const
		{
			return *_stream;
		}

	protected:
		uint64_t								_sent_bytes = 0;
		uint64_t								_received_bytes = 0;

	private:
		ov::String 								_record_id;
		std::chrono::system_clock::time_point 	_created_time;
		std::shared_ptr<info::Stream>			_stream;
	};
}  // namespace info