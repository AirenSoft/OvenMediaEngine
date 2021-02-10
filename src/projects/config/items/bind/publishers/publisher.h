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
	namespace bind
	{
		namespace pub
		{
			template <typename Tport>
			struct Publisher : public Item
			{
			protected:
				Tport _port;
				Tport _tls_port;

			public:
				explicit Publisher(const char *port)
					: _port(port)
				{
				}

				Publisher(const char *port, const char *tls_port)
					: _port(port),
					  _tls_port(port)
				{
				}

				CFG_DECLARE_REF_GETTER_OF(GetPort, _port);
				CFG_DECLARE_REF_GETTER_OF(GetTlsPort, _tls_port);

			protected:
				void MakeList() override
				{
					Register<Optional>("Port", &_port);
					Register<Optional>({"TLSPort", "tlsPort"}, &_tls_port);
				};
			};
		}  // namespace pub
	}	   // namespace bind
}  // namespace cfg