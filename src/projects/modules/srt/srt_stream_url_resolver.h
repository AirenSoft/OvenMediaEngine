//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2025 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/ovsocket/ovsocket.h>
#include <config/config.h>

namespace modules::srt
{
	class StreamUrlResolver
	{
	public:
		void Initialize(const std::vector<cfg::cmn::SrtStream> &stream_list);
		std::shared_ptr<ov::Url> Resolve(const std::shared_ptr<ov::Socket> &remote);

	private:
		std::optional<ov::String> GetStreamPath(int port);

	private:
		std::shared_mutex _stream_map_mutex;
		// Key: port, Value: stream_path
		std::map<int, ov::String> _stream_map;
	};

}  // namespace modules::srt
