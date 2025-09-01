//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2024 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/ovlibrary/ovlibrary.h>
#include <modules/rtmp/amf0/amf_document.h>

/*
	{
		"eventFormat": "amf",
		"events": [
			{
				"amfType": "onTextData",
				"data": {
					"key1": "value",    // String Type
					"key3": 354.1,       // Number Type [Double]
					"key4": true        // Boolean Type [true | false]
				}
			}
		]
	}
*/

class AmfTextDataEvent
{
public:
	static bool IsMatch(const ov::String &amf_type);
	static std::shared_ptr<AmfTextDataEvent> Create();
	static std::shared_ptr<AmfTextDataEvent> Parse(const std::shared_ptr<ov::Data> &data);

	static bool IsValid(const Json::Value &json);
	static std::shared_ptr<AmfTextDataEvent> Parse(const Json::Value &json);

	AmfTextDataEvent();
	~AmfTextDataEvent() = default;

	std::shared_ptr<ov::Data> Serialize() const;

	bool Append(const char *name, const bool value);
	bool Append(const char *name, const double value);
	bool Append(const char *name, const char *value);

private:
	std::shared_ptr<AmfEcmaArray> _arrays = nullptr;
};