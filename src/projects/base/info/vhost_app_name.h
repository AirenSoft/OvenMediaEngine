//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <config/config.h>

namespace info
{
	/// VHostAppName is a name that consists of the same form as "#vhost#app_name"
	/// This is a combination of the VHost name and app_name.
	/// VHostAppName is immutable object
	class VHostAppName
	{
		friend class Application;

	public:
		VHostAppName(const ov::String &vhost_name, const ov::String &app_name);

		static VHostAppName InvalidVHostAppName();

		bool IsValid() const;
		bool operator==(const VHostAppName &another) const;

		const ov::String &GetVHostName() const;
		const ov::String &GetAppName() const;

		const ov::String &ToString() const;
		const char *CStr() const;

	protected:
		VHostAppName();

		bool _is_valid = false;

		ov::String _vhost_app_name;
		ov::String _vhost_name;
		ov::String _app_name;
	};
}  // namespace info