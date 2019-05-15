//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Gil Hoon Choi
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "config_logger_loader.h"
#include "config_private.h"

std::shared_ptr<LoggerTagInfo> ParseTag(pugi::xml_node tag_node);

ConfigLoggerLoader::ConfigLoggerLoader()
{
}

ConfigLoggerLoader::ConfigLoggerLoader(const ov::String config_path)
        : ConfigLoader(config_path)
{
}

ConfigLoggerLoader::~ConfigLoggerLoader()
{
}

bool ConfigLoggerLoader::Parse()
{
    // validates
    bool result = ConfigLoader::Load();
    if(!result)
    {
        return false;
    }

    pugi::xml_node logger_node = _document.child("Logger");
    if(logger_node.empty())
    {
        logte("Could not found <Logger> config...");
        return false;
    }

    pugi::xml_node tag_node = logger_node.child("Tag");
    if(tag_node.empty())
    {
        logte("Could not found <Tag> config...");
        return false;
    }

    // Parse Tag
    std::shared_ptr<LoggerTagInfo> tag_info;
    for(; tag_node; tag_node = tag_node.next_sibling())
    {
        tag_info = ParseTag(tag_node);
        if(tag_info != nullptr)
        {
            _tags.push_back(tag_info);
        }
    }

    _log_path = logger_node.child_value("Path");

    if (strlen(logger_node.attribute("version").value()) != 0)
    {
        _version = logger_node.attribute("version").value();
    }

    return true;
}

void ConfigLoggerLoader::Reset()
{
    _tags.clear();

    ConfigLoader::Reset();
}

std::vector<std::shared_ptr<LoggerTagInfo>> ConfigLoggerLoader::GetTags() const noexcept
{
    return _tags;
}

std::string ConfigLoggerLoader::GetLogPath() const noexcept
{
    return _log_path;
}

std::string ConfigLoggerLoader::GetVersion() const noexcept
{
    return _version;
}

std::shared_ptr<LoggerTagInfo> ParseTag(pugi::xml_node tag_node)
{
    std::shared_ptr<LoggerTagInfo> tag_info = std::make_shared<LoggerTagInfo>();

    ov::String name = ConfigUtility::StringFromAttribute(tag_node.attribute("name"));
    ov::String level = ConfigUtility::StringFromAttribute(tag_node.attribute("level"));

    if(!LoggerTagInfo::ValidateLogLevel(level))
    {
        logtw("Invalid log level... Tag name: [%s]", name.CStr());

        return nullptr;
    }

    tag_info->SetName(name);
    tag_info->SetLevel(LoggerTagInfo::OVLogLevelFromString(level));

    return tag_info;
}