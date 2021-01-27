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
		namespace pvd
		{
			template <typename Tport>
			struct Provider : public Item
			{
			protected:
				Tport _port;
				Tport _tls_port;

			public:
				explicit Provider(const char *port)
					: _port(port)
				{
				}

				Provider(const char *port, const char *tls_port)
					: _port(port),
					  _tls_port(tls_port)
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
		}  // namespace pvd
	}	   // namespace bind
}  // namespace cfg