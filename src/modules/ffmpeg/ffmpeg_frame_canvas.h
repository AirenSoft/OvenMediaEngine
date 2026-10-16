//==============================================================================
//
//  OvenMediaEngine
//
//  Copyright (c) 2026 OvenMediaLabs. All rights reserved.
//
//==============================================================================

#pragma once

#include <base/ovlibrary/ovlibrary.h>
#include <transcoder/media_frame.h>

extern "C"
{
#include <libavutil/buffer.h>
#include <libavutil/pixfmt.h>
}

namespace ffmpeg
{
	// Places a host-memory frame on a black frame of a larger size. The output buffers
	// come from a pool, which is rebuilt when the pixel format or the size changes.
	class FrameCanvas
	{
	public:
		FrameCanvas() = default;
		~FrameCanvas();

		FrameCanvas(const FrameCanvas &)			= delete;
		FrameCanvas &operator=(const FrameCanvas &) = delete;
		FrameCanvas(FrameCanvas &&)					= delete;
		FrameCanvas &operator=(FrameCanvas &&)		= delete;

		// Copies the whole `frame` to (x, y) of a black width x height frame of the same
		// pixel format. nullptr on failure; GetLastErrorString() tells why.
		std::shared_ptr<MediaFrame> Place(const std::shared_ptr<MediaFrame> &frame, int32_t width, int32_t height, int32_t x, int32_t y);

		// Frames already handed out keep their buffers; the pool is freed after them
		void Release();

		const ov::String &GetLastErrorString() const
		{
			return _last_error;
		}

	private:
		bool PreparePool(AVPixelFormat pix_fmt, int32_t width, int32_t height);

		AVBufferPool *_pool	  = nullptr;
		AVPixelFormat _pix_fmt = AV_PIX_FMT_NONE;
		int32_t _width		  = 0;
		int32_t _height		  = 0;

		ov::String _last_error;
	};
}  // namespace ffmpeg
