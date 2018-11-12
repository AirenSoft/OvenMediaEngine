//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Gil Hoon Choi
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "config_loader.h"
#include "config_private.h"

#define CONFIG_LOADER_PRINT_XML     0

struct xml_string_writer : pugi::xml_writer
{
	ov::String result;

	virtual void write(const void *data, size_t size)
	{
		result.Append(static_cast<const char *>(data), size);
	}
};

ConfigLoader::ConfigLoader()
{
}

ConfigLoader::ConfigLoader(const ov::String &config_path)
	: _config_path(config_path)
{
}

ConfigLoader::~ConfigLoader()
{
}

void ConfigLoader::Reset()
{
	_document.reset();
	_config_path.Clear();
}

bool ConfigLoader::Load()
{
	if((_config_path == nullptr) || _config_path.IsEmpty())
	{
		logte("Config file path is empty");
		return false;
	}

	pugi::xml_parse_result result = _document.load_file(_config_path);

	if(!result)
	{
		// 정상적인 XML이 아닌 경우
		// TODO: XML 파싱에 실패한 경우 어떻게 처리할지 생각해 봐야함
		logte("Could not load config... reason: %s [%s]", result.description(), _config_path.CStr());
		return false;
	}

#if CONFIG_LOADER_PRINT_XML
		xml_string_writer writer;
		_document.print(writer);

		logtd("Config loaded... %s\n%s", _config_path.CStr(), writer.result.CStr());
#else
	logtd("Config is loaded successfully: %s", _config_path.CStr());
#endif

	return true;
}

const ov::String ConfigLoader::GetConfigPath() const noexcept
{
	return _config_path;
}

void ConfigLoader::SetConfigPath(ov::String config_path)
{
	_config_path = config_path;
}