//==============================================================================
//
//  OvenMediaEngine
//
//  Copyright (c) 2026 OvenMediaLabs. All rights reserved.
//
//==============================================================================

#include "ffmpeg_frame_canvas.h"

#include "compat.h"

extern "C"
{
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
}

namespace ffmpeg
{
	namespace
	{
		constexpr int kAlign = 32;
	}

	FrameCanvas::~FrameCanvas()
	{
		Release();
	}

	void FrameCanvas::Release()
	{
		::av_buffer_pool_uninit(&_pool);
		_pix_fmt = AV_PIX_FMT_NONE;
		_width	 = 0;
		_height	 = 0;
	}

	bool FrameCanvas::PreparePool(AVPixelFormat pix_fmt, int32_t width, int32_t height)
	{
		if (_pool != nullptr && _pix_fmt == pix_fmt && _width == width && _height == height)
		{
			return true;
		}

		Release();

		int size = ::av_image_get_buffer_size(pix_fmt, width, height, kAlign);
		_pool	 = (size > 0) ? ::av_buffer_pool_init(size, nullptr) : nullptr;
		if (_pool == nullptr)
		{
			_last_error = ov::String::FormatString("Could not create a frame pool (%s, %dx%d)", ::av_get_pix_fmt_name(pix_fmt), width, height);
			return false;
		}

		_pix_fmt = pix_fmt;
		_width	 = width;
		_height	 = height;

		return true;
	}

	std::shared_ptr<MediaFrame> FrameCanvas::Place(const std::shared_ptr<MediaFrame> &frame, int32_t width, int32_t height, int32_t x, int32_t y)
	{
		auto data = (frame != nullptr) ? frame->GetData() : nullptr;
		if (data == nullptr || data->GetBackend() != MediaFrameData::Backend::FFmpeg)
		{
			_last_error = "Not an FFmpeg frame";
			return nullptr;
		}

		auto src	 = static_cast<AVFrame *>(data->GetNativeHandle());
		auto pix_fmt = static_cast<AVPixelFormat>(src != nullptr ? src->format : AV_PIX_FMT_NONE);
		auto desc	 = ::av_pix_fmt_desc_get(pix_fmt);
		if (desc == nullptr || (desc->flags & (AV_PIX_FMT_FLAG_HWACCEL | AV_PIX_FMT_FLAG_PAL)) != 0)
		{
			_last_error = ov::String::FormatString("Unsupported frame format %d", static_cast<int>(pix_fmt));
			return nullptr;
		}

		if (x < 0 || y < 0 || x + src->width > width || y + src->height > height)
		{
			_last_error = ov::String::FormatString("A %dx%d frame at (%d, %d) does not fit in %dx%d", src->width, src->height, x, y, width, height);
			return nullptr;
		}

		// A subsampled chroma plane can only start on a whole chroma sample
		const int x_step = 1 << desc->log2_chroma_w;
		const int y_step = 1 << desc->log2_chroma_h;
		if ((x % x_step) != 0 || (y % y_step) != 0)
		{
			_last_error = ov::String::FormatString("(%d, %d) is not on the %dx%d chroma grid of %s", x, y, x_step, y_step, desc->name);
			return nullptr;
		}

		if (PreparePool(pix_fmt, width, height) == false)
		{
			return nullptr;
		}

		AVFrame *dst = ::av_frame_alloc();
		if (dst == nullptr)
		{
			_last_error = "Could not allocate a frame";
			return nullptr;
		}

		dst->format = pix_fmt;
		dst->width	= width;
		dst->height = height;
		dst->buf[0] = ::av_buffer_pool_get(_pool);
		if (dst->buf[0] == nullptr ||
			::av_image_fill_arrays(dst->data, dst->linesize, dst->buf[0]->data, pix_fmt, width, height, kAlign) < 0 ||
			::av_frame_copy_props(dst, src) < 0)
		{
			_last_error = ov::String::FormatString("Could not prepare a %s %dx%d frame", desc->name, width, height);
			::av_frame_free(&dst);
			return nullptr;
		}

		// The YUVJ formats are full range whatever the frame says
		auto range = src->color_range;
		switch (pix_fmt)
		{
			case AV_PIX_FMT_YUVJ420P:
			case AV_PIX_FMT_YUVJ422P:
			case AV_PIX_FMT_YUVJ444P:
			case AV_PIX_FMT_YUVJ440P:
			case AV_PIX_FMT_YUVJ411P:
				range = AVCOL_RANGE_JPEG;
				break;
			default:
				break;
		}

		ptrdiff_t linesizes[4] = {dst->linesize[0], dst->linesize[1], dst->linesize[2], dst->linesize[3]};
		if (::av_image_fill_black(dst->data, linesizes, pix_fmt, range, width, height) < 0)
		{
			_last_error = ov::String::FormatString("Could not fill a %s frame with black", desc->name);
			::av_frame_free(&dst);
			return nullptr;
		}

		for (int plane = 0; plane < ::av_pix_fmt_count_planes(pix_fmt); plane++)
		{
			// Only the chroma planes of a YUV format are subsampled
			const bool is_chroma = (plane == 1 || plane == 2) && (desc->flags & AV_PIX_FMT_FLAG_RGB) == 0;
			const int v_shift	 = is_chroma ? desc->log2_chroma_h : 0;

			const int x_bytes	  = ::av_image_get_linesize(pix_fmt, x, plane);
			const int width_bytes = ::av_image_get_linesize(pix_fmt, src->width, plane);
			uint8_t *dst_origin	  = dst->data[plane] + (y >> v_shift) * dst->linesize[plane] + x_bytes;

			::av_image_copy_plane(dst_origin, dst->linesize[plane], src->data[plane], src->linesize[plane], width_bytes, AV_CEIL_RSHIFT(src->height, v_shift));
		}

		auto output_frame = compat::ToMediaFrame(cmn::MediaType::Video, dst);
		::av_frame_free(&dst);

		return output_frame;
	}
}  // namespace ffmpeg
