//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

namespace cfg
{
	namespace bind
	{
		namespace mgr
		{
			struct API : public Item
			{
			protected:
				cmn::SingularPort _port;
				cmn::SingularPort _tls_port;

			public:
				explicit API(const char *port)
					: _port(port)
				{
				}

				API(const char *port, const char *tls_port)
					: _port(port),
					  _tls_port(tls_port)
				{
				}

				CFG_DECLARE_REF_GETTER_OF(GetPort, _port);
				CFG_DECLARE_REF_GETTER_OF(GetTlsPort, _tls_port);

			protected:
				void MakeParseList() override
				{
					RegisterValue<Optional>("Port", &_port);
					RegisterValue<Optional>("TLSPort", &_tls_port);
				}
			};
		}  // namespace mgr
	}	   // namespace bind
}  // namespace cfg