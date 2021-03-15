//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "templates/stun_address_attribute_format.h"
class StunMappedAddressAttribute : public StunAddressAttributeFormat
{
public:
	StunMappedAddressAttribute():StunMappedAddressAttribute(0){}
	StunMappedAddressAttribute(int length):StunAddressAttributeFormat(StunAttributeType::MappedAddress, length){}
};
