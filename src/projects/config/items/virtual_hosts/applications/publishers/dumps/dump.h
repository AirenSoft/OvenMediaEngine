//=============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "playlists.h"

namespace cfg
{
	namespace vhost
	{
		namespace app
		{
			namespace pub
			{
				struct Dump : public Item
				{
				protected:
					ov::String _id;
					bool _enabled = false;
					ov::String _target_stream_name;
					mutable ov::Regex _target_stream_name_regex;
					ov::String _output_path;
					Playlists _playlists;
					
				public:
					CFG_DECLARE_CONST_REF_GETTER_OF(GetId, _id);
					CFG_DECLARE_CONST_REF_GETTER_OF(IsEnabled, _enabled);
					CFG_DECLARE_CONST_REF_GETTER_OF(GetTargetStreamName, _target_stream_name);
					CFG_DECLARE_CONST_REF_GETTER_OF(GetPlaylists, _playlists.GetPlaylists());
					CFG_DECLARE_CONST_REF_GETTER_OF(GetOutputPath, _output_path);

					ov::Regex GetTargetStreamNameRegex() const
					{
						if (_target_stream_name_regex.IsCompiled() == false)
						{
							_target_stream_name_regex = ov::Regex::CompiledRegex(ov::Regex::WildCardRegex(_target_stream_name));
						}

						return _target_stream_name_regex;
					}

				protected:
					void MakeList() override
					{
						Register<Optional>("Id", &_id, nullptr, 
							[=]() -> std::shared_ptr<ConfigError> {
								if (_id.IsEmpty())
								{
									_id = ov::Random::GenerateString(8);
								}
								return nullptr;
						});

						Register("Enable", &_enabled);
						Register("TargetStreamName", &_target_stream_name);
						Register("OutputPath", &_output_path);
						Register<Optional>("Playlists", &_playlists);
					}
				};
			}  // namespace pub
		} // namespace app
	} // namespace vhost
}  // namespace cfg
