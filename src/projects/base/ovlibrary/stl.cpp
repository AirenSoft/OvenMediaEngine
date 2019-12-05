#include "stl.h"

std::vector<std::string_view> Split(const std::string_view &value, char delimiter)
{
    std::vector<std::string_view> components;
    size_t component_start = 0, component_end = std::string::npos;
    bool is_last_component = false;
    while (is_last_component == false)
    {
        component_end = value.find(delimiter, component_start);
        if (component_end == std::string::npos)
        {
            is_last_component = true;
            component_end = value.size();
        }
        components.emplace_back(value.substr(component_start, component_end - component_start));
        component_start = component_end + 1;
    }
    return components;
}

std::string_view Trim(const std::string_view &value)
{
    auto front = value.begin();
    while ( (front != value.end() && *front == ' '))
    {
        ++front;
    }
    if (front == value.end())
    {
        return std::string_view();
    }
    auto back = value.rbegin();
    while (back.base() != front && *back == ' ')
    {
        ++back;
    }
    const size_t length = back.base() == front ? 1 : back.base() - front + 1;
    return std::string_view(front, length);
}