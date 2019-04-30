//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "../item.h"

namespace cfg
{
	struct WebConsole : public Item
	{
		int GetListenPort() const
		{
			return _listen_port;
		}

		ov::String GetLoginId() const
		{
			return _login_id;
		}

		ov::String GetLoginPw() const
		{
			return _login_pw;
		}

	protected:
		void MakeParseList() const override
		{
			RegisterValue<Optional>("ListenPort", &_listen_port);
			RegisterValue<Optional>("LoginID", &_login_id);
			RegisterValue<Optional>("LoginPW", &_login_pw);
		}

		int _listen_port = 8888;
		ov::String _login_id;
		ov::String _login_pw;
	};
}