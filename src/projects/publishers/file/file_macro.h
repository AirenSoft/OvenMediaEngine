#pragma once

#include <base/info/media_track.h>
#include <base/publisher/session.h>
#include <modules/ffmpeg/ffmpeg_writer.h>

#include "base/info/record.h"

namespace pub
{
	class FileMacro
	{
	public:
		static ov::String ConvertMacro(const std::shared_ptr<info::Application> &app, const std::shared_ptr<info::Stream> &stream, const std::shared_ptr<info::Record> record, const ov::String &src);
	};
}  // namespace pub