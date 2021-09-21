//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/ovlibrary/ovlibrary.h>
#include <ctype.h>

namespace cfg
{
	class Item;
	class ListInterface;
	class Child;

	enum class DataType
	{
		Xml,
		Json
	};

	// omit_rule: Indicates whether to omit the name when the child item is array
	//
	// For example:
	// 1) OmitRule::Omit
	// 2) OmitRule::DontOmit (default)
	enum class OmitRule
	{
		// Omit the name
		// {
		//     "iceCandidates": [
		//         "1.1.1.1/udp",
		//         "1.2.2.2/udp"
		//     ]
		// }
		Omit,

		// Don't omit the name
		// {
		//     "iceCandidates": [
		//         { "iceCandidate": "1.1.1.1/udp" },
		//         { "iceCandidate": "1.2.2.2/udp" }
		//     ]
		// }
		DontOmit,

		Default = DontOmit
	};

	struct ItemName
	{
		friend class Item;
		friend class ListInterface;
		friend class Child;

	public:
		ItemName(const char *xml_name);
		ItemName(const char *xml_name, const char *json_name);
		ItemName(const char *xml_name, OmitRule omit_rule);
		ItemName(const char *xml_name, const char *json_name, OmitRule omit_rule);

		ov::String ToString() const;

		const ov::String &GetName(DataType type) const
		{
			switch (type)
			{
				case DataType::Xml:
					[[fallthrough]];
				default:
					return xml_name;

				case DataType::Json:
					return json_name;
			}
		}

		bool operator==(const ItemName &name) const
		{
			return (xml_name == name.xml_name) && (json_name == name.json_name);
		}

		int index = -1;

		ov::String xml_name;
		ov::String json_name;
		OmitRule omit_rule = OmitRule::Default;

	protected:
		ItemName() = default;

		bool _created_from_xml_name = false;
	};
}  // namespace cfg
