#include "rtsp.h"

#include <string>

bool ExtractApplicationAndStreamNames(const std::string_view &rtsp_uri, std::string_view &address, std::string_view &application_name, std::string_view &stream_name)
{
    std::string_view path;
    if (ExtractPath(rtsp_uri, address, path) == false)
    {
        return false;
    }
    const auto application_name_end = path.find('/');
    if (application_name_end != std::string_view::npos)
    {
        application_name = path.substr(0, application_name_end);
        stream_name = path.substr(application_name_end + 1);
        return true;
    }
    return false;
}

bool ExtractPath(const std::string_view &rtsp_uri, std::string_view &address, std::string_view &rtsp_path)
{
    // With a properly formatted uri we should skip 3 / to get to the application name, e.g rtsp://<address>[:<port>]/<rtsp_path>,
    // everything else should be treated as a malformed path
    size_t skips = 3;
    size_t address_start_position = std::string_view::npos, delimiter_position = 0;
    while (skips > 0)
    {
        delimiter_position = rtsp_uri.find('/', delimiter_position + 1);
        if (delimiter_position == std::string_view::npos)
        {
            break;
        }
        skips--;
        if (skips == 1)
        {
            address_start_position = delimiter_position;
        }
    }
    if (delimiter_position == std::string_view::npos)
    {
        return false;
    }
    address = rtsp_uri.substr(address_start_position + 1, delimiter_position - (address_start_position + 1));
    // Strip port from address if present
    auto port_start_position = address.find(':');
    if (port_start_position != std::string_view::npos)
    {
        address = address.substr(0, port_start_position);
    }
    rtsp_path = rtsp_uri.substr(delimiter_position + 1);
    return true;
}

RtspMethod GetRtspMethod(const std::string_view &method)
{
    if (method == "OPTIONS")
    {
        return RtspMethod::Options;
    }
    else if (method == "ANNOUNCE")
    {
        return RtspMethod::Announce;
    }
    else if (method == "SETUP")
    {
        return RtspMethod::Setup;
    }
    else if (method == "RECORD")
    {
        return RtspMethod::Record;
    }
    else if (method == "TEARDOWN")
    {
        return RtspMethod::Teardown;
    }
    return RtspMethod::Unknown;
}