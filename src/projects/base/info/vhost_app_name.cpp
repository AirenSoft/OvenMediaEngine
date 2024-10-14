//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#include "vhost_app_name.h"

#include "application_private.h"

namespace info
{
	VHostAppName::VHostAppName()
		: _vhost_app_name("?InvalidApp?")
	{
	}

	VHostAppName::VHostAppName(const ov::String &vhost_name, const ov::String &app_name)
		: _is_valid(true),

		  _vhost_name(vhost_name),
		  _app_name(app_name)
	{
		_vhost_app_name.Format("#%s#%s", vhost_name.Replace("#", "_").CStr(), app_name.Replace("#", "_").CStr());
	}

	VHostAppName::VHostAppName(const ov::String &vhost_app_name)
		: _vhost_app_name(vhost_app_name)
	{
		auto tokens = vhost_app_name.Split("#");

		if (tokens.size() == 3)
		{
			_vhost_name = tokens[1];
			_app_name = tokens[2];
			_is_valid = true;
		}
		else
		{
			_is_valid = false;
		}
	}

	VHostAppName VHostAppName::InvalidVHostAppName()
	{
		static VHostAppName vhost_app_name;
		return vhost_app_name;
	}

#if 0
	bool VHostAppName::Parse(ov::String *vhost_name, ov::String *app_name) const
	{
		if(IsInvalid())
		{
			return false;
		}

		auto tokens = Split("#");

		if (tokens.size() == 3)
		{
			// #<vhost_name>#<app_name>
			OV_ASSERT2(tokens[0] == "");

			if (vhost_name != nullptr)
			{
				*vhost_name = tokens[1];
			}

			if (app_name != nullptr)
			{
				*app_name = tokens[2];
			}
			return true;
		}

		OV_ASSERT2(false);
		return false;
	}
#endif

	bool VHostAppName::IsValid() const
	{
		return _is_valid;
	}

	bool VHostAppName::operator==(const VHostAppName &another) const
	{
		return (_is_valid == another._is_valid) && (_vhost_app_name == another._vhost_app_name);
	}

	bool VHostAppName::operator<(const VHostAppName &another) const
	{
		return _vhost_app_name < another._vhost_app_name;
	}

	const ov::String &VHostAppName::GetVHostName() const
	{
		return _vhost_name;
	}

	const ov::String &VHostAppName::GetAppName() const
	{
		return _app_name;
	}

	const ov::String &VHostAppName::ToString() const
	{
		return _vhost_app_name;
	}

	const char *VHostAppName::CStr() const
	{
		return _vhost_app_name.CStr();
	}
}  // namespace info
