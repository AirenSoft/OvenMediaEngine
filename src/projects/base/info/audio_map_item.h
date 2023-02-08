//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "base/common_types.h"

namespace info
{

	class AudioMapItem
	{
	public:
		AudioMapItem(int index, const ov::String &name, const ov::String &language)
			: _index(index),
			  _name(name),
			  _language(language)
		{
		}

		// Setter
		void SetIndex(int index)
		{
			_index = index;
		}

		void SetName(const ov::String &name)
		{
			_name = name;
		}

		void SetLanguage(const ov::String &language)
		{
			_language = language;
		}

		// Gettter
		int GetIndex() const
		{
			return _index;
		}

		const ov::String &GetName() const
		{
			return _name;
		}

		const ov::String &GetLanguage() const
		{
			return _language;
		}

	private:
		int _index;
		ov::String _name;
		ov::String _language;
	};
} // namespace info