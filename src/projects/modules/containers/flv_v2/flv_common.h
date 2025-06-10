//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2025 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/ovlibrary/ovlibrary.h>

#include "./flv_datastructure.h"

namespace modules
{
	namespace flv
	{
		struct CommonData
		{
			CommonData(bool from_ex_header)
				: from_ex_header(from_ex_header)
			{
			}

			virtual ~CommonData() = default;

			bool from_ex_header = false;
			uint32_t track_id = 0;
			std::shared_ptr<const ov::Data> payload = nullptr;
		};

		class ParserCommon
		{
		public:
			ParserCommon(int track_id_if_legacy)
				: _track_id_if_legacy(track_id_if_legacy)
			{
			}

			virtual bool Parse(ov::BitReader &reader) = 0;

			OV_DEFINE_CONST_GETTER(IsExHeader, _is_ex_header, noexcept);

			OV_DEFINE_CONST_GETTER(IsMultitrack, _is_multitrack, noexcept);
			OV_DEFINE_CONST_GETTER(GetMultitrackType, _multitrack_type, noexcept);

		protected:
			std::shared_ptr<const ov::Data> GetPayload(bool is_multi_track, ov::BitReader &reader, uint24_t size_of_track, size_t size_of_track_offset)
			{
				static auto EMPTY_DATA = std::make_shared<ov::Data>();

				size_t payload_size = 0;

				if ((is_multi_track == false) || (size_of_track == 0))
				{
					payload_size = reader.GetRemainingBytes();
				}
				else
				{
					// How many bytes have been read since the sizeOfVideoTrack field
					auto read_bytes_since_size_of_track = reader.GetByteOffset() - size_of_track_offset;
					payload_size = size_of_track - read_bytes_since_size_of_track;
				}

				return (payload_size > 0) ? reader.ReadBytes(payload_size) : EMPTY_DATA;
			}

		protected:
			uint32_t _track_id_if_legacy;

			bool _is_ex_header = false;

			bool _is_multitrack = false;
			AvMultitrackType _multitrack_type = AvMultitrackType::OneTrack;
		};
	}  // namespace flv
}  // namespace modules
