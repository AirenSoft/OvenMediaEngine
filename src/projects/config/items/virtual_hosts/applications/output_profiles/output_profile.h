//=============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "encodes/encodes.h"
#include "playlist/playlist.h"

namespace cfg
{
	namespace vhost
	{
		namespace app
		{
			namespace oprf
			{
				struct OutputProfile : public Item
				{
				protected:
					ov::String _name;
					ov::String _output_stream_name;
					Encodes _encodes;
					std::vector<Playlist> _playlists;

				public:
					CFG_DECLARE_CONST_REF_GETTER_OF(GetName, _name)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetOutputStreamName, _output_stream_name)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetEncodes, _encodes)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetPlaylists, _playlists)

				protected:
					void MakeList() override
					{
						Register("Name", &_name);
						Register("OutputStreamName", &_output_stream_name);
						Register<Optional>("Encodes", &_encodes);

						Register<Optional>({"Playlist", "playlists"}, &_playlists,
							[=]() -> std::shared_ptr<ConfigError> {
								return nullptr;
							},
							[=]() -> std::shared_ptr<ConfigError> {
								
								std::map<ov::String, bool> playlist_file_names;

								for (auto &playlist : _playlists)
								{
									// Check if there is duplicate playlist file name
									auto file_name = playlist.GetFileName();
									if (playlist_file_names.find(file_name) != playlist_file_names.end())
									{
										return CreateConfigErrorPtr("Duplicate playlist file name: %s", file_name.CStr());
									}
									playlist_file_names[file_name] = true;

									// Check if there is unavailable encodes names in playlist
									if (playlist.SetEncodes(_encodes) == false)
									{
										return CreateConfigErrorPtr("Playlist Error");
									}
								}
								return nullptr;
							}
						);
					}
				};
			}  // namespace oprf
		}	   // namespace app
	}		   // namespace vhost
}  // namespace cfg
