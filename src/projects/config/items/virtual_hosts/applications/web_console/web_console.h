//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

namespace cfg
{
	namespace vhost
	{
		namespace app
		{
			namespace wc
			{
				struct WebConsole : public Item
				{
					CFG_DECLARE_GETTER_OF(GetListenPort, _listen_port)
					CFG_DECLARE_REF_GETTER_OF(GetLoginId, _login_id)
					CFG_DECLARE_REF_GETTER_OF(GetLoginPw, _login_pw)
					CFG_DECLARE_REF_GETTER_OF(GetDocumentPath, _document_path)

				protected:
					void MakeParseList() override
					{
						RegisterValue<Optional>("ListenPort", &_listen_port);
						RegisterValue<Optional>("LoginID", &_login_id);
						RegisterValue("LoginPW", &_login_pw);
						RegisterValue("DocumentPath", &_document_path);
					}

					int _listen_port = 8888;
					ov::String _login_id = "admin";
					ov::String _login_pw;
					ov::String _document_path;
				};
			}  // namespace wc
		}	   // namespace app
	}		   // namespace vhost
}  // namespace cfg