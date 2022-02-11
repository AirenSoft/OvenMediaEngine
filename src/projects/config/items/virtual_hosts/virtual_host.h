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
#include "access_control/admission_webhooks.h"
#include "access_control/signed_policy.h"
#include "access_control/signed_token.h"
#include "applications/applications.h"
#include "origins/origins.h"

namespace cfg
{
	namespace vhost
	{
		struct VirtualHost : public Item
		{
		protected:
			ov::String _name;
			ov::String _distribution = "ovenmediaengine.com";

			cmn::Host _host;
			sig::SignedPolicy _signed_policy;
			sig::SignedToken _signed_token;
			sig::AdmissionWebhooks _admission_webhooks;
			orgn::Origins _origins;
			app::Applications _applications;

		public:
			CFG_DECLARE_CONST_REF_GETTER_OF(GetName, _name)
			CFG_DECLARE_CONST_REF_GETTER_OF(GetDistribution, _distribution)

			CFG_DECLARE_CONST_REF_GETTER_OF(GetHost, _host)

			CFG_DECLARE_CONST_REF_GETTER_OF(GetSignedPolicy, _signed_policy)
			CFG_DECLARE_CONST_REF_GETTER_OF(GetSignedToken, _signed_token)
			CFG_DECLARE_CONST_REF_GETTER_OF(GetAdmissionWebhooks, _admission_webhooks)

			CFG_DECLARE_CONST_REF_GETTER_OF(GetOrigins, _origins)
			CFG_DECLARE_CONST_REF_GETTER_OF(GetOriginList, _origins.GetOriginList())
			CFG_DECLARE_CONST_REF_GETTER_OF(GetApplicationList, _applications.GetApplicationList())

			bool GetApplicationByName(ov::String name, cfg::vhost::app::Application &application) const
			{
				auto &app_list = GetApplicationList();
				for (auto &item : app_list)
				{
					if (item.GetName() == name)
					{
						application = item;
						return true;
					}
				}

				return false;
			}

		protected:
			void MakeList() override
			{
				Register("Name", &_name);
				Register<Optional>("Distribution", &_distribution);

				Register<Optional>("Host", &_host);

				Register<Optional>("SignedPolicy", &_signed_policy);
				Register<Optional>("SignedToken", &_signed_token);
				Register<Optional>("AdmissionWebhooks", &_admission_webhooks);

				Register<Optional>("Origins", &_origins);
				Register<Optional>("Applications", &_applications);
			}
		};
	}  // namespace vhost
}  // namespace cfg
