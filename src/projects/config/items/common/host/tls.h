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
	namespace cmn
	{
		struct Tls : public Item
		{
			CFG_DECLARE_REF_GETTER_OF(GetCertPath, _cert_path)
			CFG_DECLARE_REF_GETTER_OF(GetKeyPath, _key_path)
			CFG_DECLARE_REF_GETTER_OF(GetChainCertPath, _chain_cert_path)

		protected:
			void MakeParseList() override
			{
				RegisterValue<ResolvePath>("CertPath", &_cert_path);
				RegisterValue<ResolvePath>("KeyPath", &_key_path);
				RegisterValue<Optional, ResolvePath>("ChainCertPath", &_chain_cert_path);
			}

			ov::String _cert_path;
			ov::String _key_path;
			ov::String _chain_cert_path;
		};
	}  // namespace cmn
}  // namespace cfg