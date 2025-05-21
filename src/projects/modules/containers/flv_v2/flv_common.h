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
			virtual ~CommonData() = default;

			std::optional<uint8_t> track_id;
			
			std::shared_ptr<const ov::Data> payload;
		};

		class ParserBase
		{
		public:
			virtual bool Parse(ov::BitReader &reader) = 0;

			OV_DEFINE_CONST_GETTER(IsExHeader, _is_ex_header, noexcept);

			OV_DEFINE_CONST_GETTER(IsMultitrack, _is_multitrack, noexcept);
			OV_DEFINE_CONST_GETTER(GetMultitrackType, _multitrack_type, noexcept);

			OV_DEFINE_CONST_GETTER(GetDataList, _data_list, noexcept);

		protected:
			bool _is_ex_header = false;

			bool _is_multitrack = false;
			AvMultitrackType _multitrack_type = AvMultitrackType::OneTrack;

			std::vector<std::shared_ptr<CommonData>> _data_list;
		};
	}  // namespace flv
}  // namespace modules
