//==============================================================================
//
//  Schedule
//
//  Created by Getroot
//  Copyright (c) 2023 AirenSoft. All rights reserved.
//
//==============================================================================

#include "schedule.h"
#include "schedule_private.h"


namespace pvd
{
	std::shared_ptr<Schedule> Schedule::Create(const ov::String &file_path, const ov::String &media_root_dir)
	{
		auto schedule = std::make_shared<Schedule>();
		if (schedule->LoadFromXMLFile(file_path, media_root_dir) == false)
		{
			return nullptr;
		}

		return schedule;
	}

	bool Schedule::LoadFromXMLFile(const ov::String &file_path, const ov::String &media_root_dir)
	{
		pugi::xml_document xml_doc;
		auto load_result = xml_doc.load_file(file_path.CStr());
		if (!load_result)
		{
			logte("Failed to load schedule file: %s", load_result.description());
			return false;
		}

		_file_path = file_path;
		_media_root_dir = media_root_dir;

		auto schedule_node = xml_doc.child("Schedule");
		if (!schedule_node)
		{
			logte("Failed to find Schedule node");
			return false;
		}

		if (ReadStreamNode(schedule_node) == false)
		{
			return false;
		}

		if (ReadProgramNodes(schedule_node) == false)
		{
			return false;
		}

		_created_time = std::chrono::system_clock::now();

		return true;
	}

	std::chrono::system_clock::time_point Schedule::GetCreatedTime() const
	{
		return _created_time;
	}

	bool Schedule::ReadStreamNode(const pugi::xml_node &schedule_node)
	{
		auto stream_node = schedule_node.child("Stream");
		if (!stream_node)
		{
			logte("Failed to find Stream node");
			return false;
		}

		auto stream_name_node = stream_node.child("StreamName");
		if (!stream_name_node)
		{
			logte("Failed to find StreamName node, StreamName is required");
			return false;
		}

		_stream.name = stream_name_node.text().as_string();

		auto bypass_transcoder_node = stream_node.child("BypassTranscoder");
		if (!bypass_transcoder_node)
		{
			_stream.bypass_transcoder = false;
		}
		else
		{
			_stream.bypass_transcoder = bypass_transcoder_node.text().as_bool();
		}

		auto video_track_node = stream_node.child("VideoTrack");
		if (!video_track_node)
		{
			_stream.video_track = true;
		}
		else
		{
			_stream.video_track = video_track_node.text().as_bool();
		}

		auto audio_track_node = stream_node.child("AudioTrack");
		if (!audio_track_node)
		{
			_stream.audio_track = true;
		}
		else
		{
			_stream.audio_track = audio_track_node.text().as_bool();
		}

		return true;
	}

	bool Schedule::ReadDefaultProgramNode(const pugi::xml_node &schedule_node)
	{
		auto default_program_node = schedule_node.child("DefaultProgram");
		if (!default_program_node)
		{
			return false;
		}

		auto default_program = std::make_shared<Schedule::Program>();
		default_program->name = "default";
		default_program->scheduled = "1970-01-01T00:00:00Z";
		default_program->scheduled_time = std::chrono::system_clock::time_point::min();
		default_program->duration_ms = -1;
		default_program->end_time = std::chrono::system_clock::time_point::max();
		default_program->repeat = true;
		
		if (ReadItemNodes(default_program_node, default_program->items) == false)
		{
			return false;
		}

		_default_program = default_program;

		return true;
	}

	bool Schedule::ReadProgramNodes(const pugi::xml_node &schedule_node)
	{		
		for (auto program_node = schedule_node.child("Program"); program_node; program_node = program_node.next_sibling("Program"))
		{
			auto program = std::make_shared<Schedule::Program>();
			auto next_program = program_node.next_sibling("Program");

			auto name_attribute = program_node.attribute("name");
			if (!name_attribute)
			{
				program->name = ov::Random::GenerateString(8);
			}
			else
			{
				program->name = name_attribute.as_string();
			}

			auto scheduled_attribute = program_node.attribute("scheduled");
			if (!scheduled_attribute)
			{
				logte("Failed to find scheduled attribute, scheduled is required");
				return false;
			}

			program->scheduled = scheduled_attribute.as_string();

			try 
			{
				program->scheduled_time = ov::Converter::FromISO8601(program->scheduled);
			}
			catch (std::exception &e)
			{
				logte("Failed to parse scheduled attribute: %s", e.what());
				return false;
			}

			if (next_program)
			{
				auto next_scheduled_attribute = next_program.attribute("scheduled");

				std::chrono::system_clock::time_point next_scheduled_time;
				if (next_scheduled_attribute)
				{
					try 
					{
						next_scheduled_time = ov::Converter::FromISO8601(next_scheduled_attribute.as_string());
					}
					catch (std::exception &e)
					{
						logte("Failed to parse scheduled attribute: %s", e.what());
						return false;
					}
				}
				else
				{
					std::ostringstream oss;
					next_program.print(oss);
					logte("Failed to find scheduled attribute, scheduled is required : %s", oss.str().c_str());
					return false;
				}
				
				program->duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(next_scheduled_time - program->scheduled_time).count();
				program->end_time = next_scheduled_time;
			}
			else // last program
			{
				program->duration_ms = -1; // -1 means unknown duration
				program->end_time = std::chrono::system_clock::time_point::max();
			}

			// If the program has already passed, it is ignored.
			if (program->end_time < std::chrono::system_clock::now())
			{
				continue;
			}

			auto repeat_attribute = program_node.attribute("repeat");
			if (!repeat_attribute)
			{
				program->repeat = false;
			}
			else
			{
				program->repeat = repeat_attribute.as_bool();
			}

			if (ReadItemNodes(program_node, program->items) == false)
			{
				return false;
			}

			_programs.push_back(program);
		}

		return true;
	}

	bool Schedule::ReadItemNodes(const pugi::xml_node &item_parent_node, std::vector<std::shared_ptr<Schedule::Item>> &items)
	{
		for (auto item_node = item_parent_node.child("Item"); item_node; item_node = item_node.next_sibling("Item"))
		{
			auto item = std::make_shared<Schedule::Item>();
			auto url_attribute = item_node.attribute("url");
			if (!url_attribute)
			{
				logte("Failed to find url attribute, url is required");
				return false;
			}

			ov::String url = url_attribute.as_string();
			// file://
			// stream://

			if (url.LowerCaseString().HasPrefix("file://"))
			{
				url = url.Substring(7);
				url = _media_root_dir + url;
				item->file = true;
			}
			else if (url.LowerCaseString().HasPrefix("stream://"))
			{
				item->file = false;
			}
			else
			{
				logte("Failed to parse url attribute, url must be file:// or stream://");
				return false;
			}

			item->url = url;

			auto start_attribute = item_node.attribute("start");
			if (!start_attribute)
			{
				item->start_time_ms = 0;
			}
			else
			{
				item->start_time_ms = start_attribute.as_llong();
			}

			auto duration_attribute = item_node.attribute("duration");
			if (!duration_attribute)
			{
				logte("Failed to find duration attribute, duration is required");
				return false;
			}

			item->duration_ms = duration_attribute.as_llong();

			if (item->duration_ms >= 0 && item->duration_ms <= 1000)
			{
				logtw("Item duration is too short, duration must be greater than 1000ms. url: %s, duration: %lld, it will be changed to 1000ms", item->url.CStr(), item->duration_ms);
				item->duration_ms = 1000;
			}

			items.push_back(item);
		}

		return true;
	}

	const Schedule::Stream &Schedule::GetStream() const
	{
		return _stream;
	}

	const std::vector<std::shared_ptr<Schedule::Program>> &Schedule::GetPrograms() const
	{
		return _programs;
	}

	const std::shared_ptr<Schedule::Program> Schedule::GetCurrentProgram() const
	{
		// Get current time
		auto now = std::chrono::system_clock::now();

		// Find program which between scheduled time and finished time
		for (auto program : _programs)
		{
			if (program->scheduled_time <= now && now < program->end_time)
			{
				if (program->IsOffAir() == true)
				{
					return nullptr;
				}

				return program;
			}

			// scheduled time is future, there is no current program yet
			if (program->scheduled_time > now)
			{
				break;
			}
		}

		return nullptr;
	}

	const std::shared_ptr<Schedule::Program> Schedule::GetNextProgram() const
	{
		// Get current time
		auto now = std::chrono::system_clock::now();

		// Find program which between scheduled time and finished time
		for (auto program : _programs)
		{
			if (program->scheduled_time > now)
			{
				return program;
			}
		}

		return nullptr;
	}
}