//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "applications/applications.h"
#include "domain/domain.h"
#include "origins/origins.h"
#include "signed_url/signed_url.h"

namespace cfg
{
	struct VirtualHost : public Item
	{
		CFG_DECLARE_REF_GETTER_OF(GetName, _name)

		CFG_DECLARE_REF_GETTER_OF(GetDomain, _domain)


		CFG_DECLARE_REF_GETTER_OF(GetSignedUrl, _signed_url)

		CFG_DECLARE_REF_GETTER_OF(GetOrigins, _origins)
		CFG_DECLARE_REF_GETTER_OF(GetOriginList, _origins.GetOriginList())
		CFG_DECLARE_REF_GETTER_OF(GetApplicationList, _applications.GetApplicationList())

	protected:
		void MakeParseList() override
		{
			RegisterValue("Name", &_name);

			RegisterValue<Optional>("Domain", &_domain);

			RegisterValue<Optional>("SignedURL", &_signed_url);

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

		Domain _domain;
		SignedUrl _signed_url;
		Origins _origins;
		Applications _applications;
	};
}  // namespace cfg
