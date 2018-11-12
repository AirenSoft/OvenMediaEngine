//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Gil Hoon Choi
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <pugixml-1.9/src/pugixml.hpp>

#include <base/ovlibrary/ovlibrary.h>

class ConfigLoader
{
public:
	ConfigLoader();
	explicit ConfigLoader(const ov::String &config_path);
	virtual ~ConfigLoader();

	virtual bool Parse() = 0;
	void Reset();

	const ov::String GetConfigPath() const noexcept;
	void SetConfigPath(ov::String config_path);

protected:
	bool Load();

	pugi::xml_document _document;

private:
	ov::String _config_path;
};
