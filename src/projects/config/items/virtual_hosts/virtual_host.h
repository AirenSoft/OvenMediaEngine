//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "../common/host/host.h"
#include "applications/applications.h"
#include "origins/origins.h"
#include "signature/signed_policy.h"
#include "signature/signed_token.h"

namespace cfg
{
	namespace vhost
	{
		struct VirtualHost : public Item
		{
			CFG_DECLARE_REF_GETTER_OF(GetName, _name)

			CFG_DECLARE_REF_GETTER_OF(GetHost, _host)

			CFG_DECLARE_REF_GETTER_OF(GetSignedPolicy, _signed_policy)
			CFG_DECLARE_REF_GETTER_OF(GetSignedToken, _signed_token)

			CFG_DECLARE_REF_GETTER_OF(GetOrigins, _origins)
			CFG_DECLARE_REF_GETTER_OF(GetOriginList, _origins.GetOriginList())
			CFG_DECLARE_REF_GETTER_OF(GetApplicationList, _applications.GetApplicationList())

		protected:
			void MakeParseList() override
			{
				RegisterValue("Name", &_name);

				RegisterValue<Optional>("Host", &_host);

				RegisterValue<Optional>("SignedPolicy", &_signed_policy);
				RegisterValue<Optional>("SignedToken", &_signed_token);

				RegisterValue<CondOptional>("Origins", &_origins, [this]() -> bool {
					// <Origins> is not optional when the host type is edge
					// return (_type != HostType::Edge);
					return true;
				});
				RegisterValue<CondOptional>("Applications", &_applications, [this]() -> bool {
					// <Applications> is not optional when the host type is origin
					// return (_type != HostType::Origin);
					return true;
				});
			}

			ov::String _name;

			cmn::Host _host;
			sig::SignedPolicy _signed_policy;
			sig::SignedToken _signed_token;
			orgn::Origins _origins;
			app::Applications _applications;
		};
	}  // namespace vhost
}  // namespace cfg
