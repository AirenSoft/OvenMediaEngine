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

				int _worker_count{};

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

				CFG_DECLARE_CONST_REF_GETTER_OF(GetPort, _port);
				CFG_DECLARE_CONST_REF_GETTER_OF(GetTlsPort, _tls_port);

				CFG_DECLARE_CONST_REF_GETTER_OF(GetWorkerCount, _worker_count);

			protected:
				void MakeList() override
				{
					Register<Optional>("Port", &_port);
					Register<Optional>({"TLSPort", "tlsPort"}, &_tls_port);

					Register<Optional>("WorkerCount", &_worker_count);
				}
			};
		}  // namespace mgr
	}	   // namespace bind
}  // namespace cfg