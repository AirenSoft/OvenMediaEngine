//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "tls.h"

namespace cfg
{
	struct WebConsole : public Item
	{
		const Tls &GetTls() const
		{
			return _tls;
		}

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

		ov::String GetDocumentPath() const
		{
			return _document_path;
		}

	protected:
		void MakeParseList() const override
		{
			RegisterValue("TLS", &_tls);
			RegisterValue<Optional>("ListenPort", &_listen_port);
			RegisterValue<Optional>("LoginID", &_login_id);
			RegisterValue("LoginPW", &_login_pw);
			RegisterValue("DocumentPath", &_document_path);
		}

		Tls _tls;
		int _listen_port = 8888;
		ov::String _login_id = "admin";
		ov::String _login_pw;
		ov::String _document_path;
	};
}