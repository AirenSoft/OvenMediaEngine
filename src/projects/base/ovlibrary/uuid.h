//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <uuid/uuid.h>

#include "./ovlibrary.h"
#include "./string.h"

namespace ov
{
	class UUID
	{
	public:
		UUID()
		{
			::uuid_generate(_uuid);
		}

		UUID(const UUID &other)		= default;
		UUID(UUID &&other) noexcept = default;
		~UUID()						= default;

		String ToString() const
		{
			// The UUID string consist of 36 characters including the NULL character.
			char uuid_cstr[UUID_STR_LEN];

			::uuid_unparse(_uuid, uuid_cstr);

			return uuid_cstr;
		}

		bool operator==(const UUID &other) const
		{
			return (::uuid_compare(_uuid, other._uuid) == 0);
		}

	private:
		uuid_t _uuid;
	};
}  // namespace ov
