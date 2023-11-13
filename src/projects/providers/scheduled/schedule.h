//==============================================================================
//
//  Schedule
//
//  Created by Getroot
//  Copyright (c) 2023 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/ovlibrary/ovlibrary.h>
#include <pugixml-1.9/src/pugixml.hpp>

/*
<?xml version="1.0" encoding="UTF-8"?>
<Schedule>
    <Stream>
        <StreamName>tv1</StreamName> <!-- required -->
        <BypassTranscoder>false</BypassTranscoder> <!-- optional, default : false -->
        <VideoTrack>true</VideoTrack> <!-- optional, default : true -->
        <AudioTrack>true</AudioTrack> <!-- optional, default : true -->
        <FallbackContent> <!-- optional -->
            <Item url="file://sample.mp4" start="0" duration="60000"/>
            <Item url="stream://default/app/stream1" duration="60000"/>
        </FallbackContent>
    </Stream>

    <Program scheduled="2023-09-27T00:00:00.000Z" repeat="true"> <!-- optional -->
        <Item url="file://sample.mp4" start="0" duration="60000"/> <!-- Milliseconds -->
        <Item url="stream://default/app/stream1" duration="60000"/>
    </Program>
    <Program scheduled="2023-09-27 03:11:22.000Z" repeat="true">
        <Item url="file://sample.mp4" start="0" duration="60000"/>
        <Item url="stream://default/app/stream1" duration="60000"/>
        <Item url="file://sample.mp4" start="60000" duration="120000"/>
    </Program>
</Schedule>
*/
 
namespace pvd
{
    class Schedule
    {
    public:
        struct Item
        {
            // == operator
            bool operator==(const Item &rhs) const
            {
                if (url != rhs.url ||
                    start_time_ms != rhs.start_time_ms ||
                    duration_ms != rhs.duration_ms)
                {
                    return false;
                }

                return true;
            }

            // != operator
            bool operator!=(const Item &rhs) const
            {
                return !(*this == rhs);
            }

            ov::String url;
            int64_t start_time_ms;
            int64_t duration_ms;

            bool file = true;
        };

        struct Stream
        {
            ov::String name;
            bool bypass_transcoder;
            bool video_track;
            bool audio_track;
            std::vector<std::shared_ptr<Item>> fallback_contents;
        };
            
        struct Program
        {
            std::shared_ptr<Item> GetNextItem()
            {
                if (items.empty())
                {
                    return nullptr;
                }

                if (size_t(current_item_index) >= items.size())
                {
                    if (repeat)
                    {
                        current_item_index = 0;
                    }
                    else
                    {
                        off_air = true;
                        return nullptr;
                    }
                }

                auto item = items[current_item_index];
                current_item_index++;

                return item;
            }

            bool IsOffAir() const
            {
                return off_air;
            }

            // == operator
            bool operator==(const Program &rhs) const
            {
                if (scheduled_time != rhs.scheduled_time ||
                    //duration_ms != rhs.duration_ms || // duration_ms is changed by next program
                    repeat != rhs.repeat || 
                    items.size() != rhs.items.size())
                {
                    return false;
                }

                for (size_t i = 0; i < items.size(); i++)
                {
                    if (*items[i] != *rhs.items[i])
                    {
                        return false;
                    }
                }

                return true;
            }

            // != operator
            bool operator!=(const Program &rhs) const
            {
                return !(*this == rhs);
            }

            ov::String scheduled;
            std::chrono::system_clock::time_point scheduled_time;
            int64_t duration_ms;
            std::chrono::system_clock::time_point end_time;
            bool repeat;
            std::vector<std::shared_ptr<Item>> items;

        private:
            int current_item_index = 0;
            bool off_air = false;
        };

        static std::shared_ptr<Schedule> Create(const ov::String &file_path, const ov::String &media_root_dir);

        Schedule() = default;
        ~Schedule() = default;

        bool LoadFromXMLFile(const ov::String &file_path, const ov::String &media_file_root_dir);
        std::chrono::system_clock::time_point GetCreatedTime() const;

        const Stream &GetStream() const;
        const std::vector<std::shared_ptr<Program>> &GetPrograms() const;

        const std::shared_ptr<Program> GetCurrentProgram() const;
        const std::shared_ptr<Program> GetNextProgram() const;

    private:
        bool ReadStreamNode(const pugi::xml_node &schedule_node);
        bool ReadProgramNodes(const pugi::xml_node &schedule_node);
        bool ReadItemNodes(const pugi::xml_node &item_parent_node, std::vector<std::shared_ptr<Item>> &items);

        Stream _stream;
        std::vector<std::shared_ptr<Program>> _programs;

        ov::String _file_path;
        ov::String _media_root_dir;

        // Created time
        std::chrono::system_clock::time_point _created_time;
    };
}