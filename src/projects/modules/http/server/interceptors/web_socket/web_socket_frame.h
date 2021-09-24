//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "./web_socket_datastructure.h"

namespace http
{
	namespace svr
	{
		namespace ws
		{
			enum class FrameParseStatus
			{
				ParseHeader,
				ParseLength,
				ParseMask,
				ParsePayload,
				Completed,
				Error,
			};

			class Frame
			{
			public:
				Frame();

				const std::shared_ptr<const ov::Data> GetPayload() const noexcept
				{
					return _payload;
				}

				// @returns true if frame is completed, false if not
				bool Process(const std::shared_ptr<const ov::Data> &data, ssize_t *read_bytes);
				FrameParseStatus GetStatus() const noexcept;

				const FrameHeader &GetHeader();

				void Reset();

				ov::String ToString() const;

			protected:
				// This function determines whether _previous_data and input_data have as much data as size_to_parse.
				// If there is sufficient data, output_data is filled with the data and true is returned.
				//
				// If this function returns false, we must verify that the read_bytes value is negative to check if an error has occurred
				bool ParseData(const std::shared_ptr<const ov::Data> &input_data, void *output_data, size_t size_to_parse, ssize_t *read_bytes);

				ssize_t ProcessHeader(const std::shared_ptr<const ov::Data> &data);
				ssize_t ProcessLength(const std::shared_ptr<const ov::Data> &data);
				ssize_t ProcessMask(const std::shared_ptr<const ov::Data> &data);
				ssize_t ProcessPayload(const std::shared_ptr<const ov::Data> &data);

				FrameHeader _header;
				int _header_read_bytes = 0;

				uint64_t _remained_payload_length = 0UL;
				uint64_t _payload_length = 0UL;
				uint32_t _frame_masking_key = 0U;

				FrameParseStatus _last_status = FrameParseStatus::ParseHeader;

				// Temporary buffer used only in steps ParseHeader, ParseLength, and ParseMask
				std::shared_ptr<ov::Data> _previous_data;

				std::shared_ptr<ov::Data> _payload;
			};
		}  // namespace ws
	}	   // namespace svr
}  // namespace http
