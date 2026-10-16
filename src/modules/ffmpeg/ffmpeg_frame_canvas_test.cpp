//==============================================================================
//
//  OvenMediaEngine
//
//  Copyright (c) 2026 OvenMediaLabs. All rights reserved.
//
//==============================================================================
#include <gtest/gtest.h>

#include <cstring>

#include "compat.h"
#include "ffmpeg_frame_canvas.h"

namespace
{
	std::shared_ptr<MediaFrame> MakeFrame(AVPixelFormat format, int width, int height, AVColorRange range, uint8_t value)
	{
		AVFrame *frame		= ::av_frame_alloc();
		frame->format		= format;
		frame->width		= width;
		frame->height		= height;
		frame->color_range	= range;
		frame->pts			= 1234;
		if (::av_frame_get_buffer(frame, 0) < 0)
		{
			::av_frame_free(&frame);
			return nullptr;
		}

		for (int plane = 0; plane < AV_NUM_DATA_POINTERS && frame->data[plane] != nullptr; plane++)
		{
			std::memset(frame->data[plane], value, frame->linesize[plane] * height);
		}

		auto media_frame = ffmpeg::compat::ToMediaFrame(cmn::MediaType::Video, frame);
		::av_frame_free(&frame);
		return media_frame;
	}

	// Stands in for a frame of another backend (e.g. a NETINT surface)
	class OtherBackendData : public MediaFrameData
	{
	public:
		Backend GetBackend() const override { return Backend::Unknown; }
		void *GetNativeHandle() const override { return const_cast<int *>(&_dummy); }
		std::shared_ptr<MediaFrameData> Clone(bool) const override { return nullptr; }
		bool IsHardwareFrame() const override { return true; }
		std::shared_ptr<MediaFrameData> DownloadToHost() const override { return nullptr; }
		void FillZero() override {}
		void SetPts(int64_t) override {}
		void SetDuration(int64_t) override {}
		void SetNbSamples(int32_t) override {}

	private:
		int _dummy = 0;
	};

	std::shared_ptr<MediaFrame> MakeOtherBackendFrame(int width, int height)
	{
		auto frame = std::make_shared<MediaFrame>();
		frame->SetMediaType(cmn::MediaType::Video);
		frame->SetWidth(width);
		frame->SetHeight(height);
		frame->SetData(std::make_shared<OtherBackendData>());
		return frame;
	}

	const AVFrame *AV(const std::shared_ptr<MediaFrame> &frame)
	{
		return static_cast<const AVFrame *>(frame->GetPrivData());
	}
}  // namespace

TEST(FrameCanvas, PlacesYuv420pOnBlack)
{
	ffmpeg::FrameCanvas canvas;
	auto src = MakeFrame(AV_PIX_FMT_YUV420P, 4, 4, AVCOL_RANGE_MPEG, 200);
	ASSERT_NE(src, nullptr);

	auto out = canvas.Place(src, 8, 8, 2, 2);
	ASSERT_NE(out, nullptr) << canvas.GetLastErrorString().CStr();

	auto f = AV(out);
	EXPECT_EQ(f->width, 8);
	EXPECT_EQ(f->height, 8);
	EXPECT_EQ(f->pts, 1234);

	// Limited-range black around, the source inside
	EXPECT_EQ(f->data[0][0], 16);
	EXPECT_EQ(f->data[0][2 * f->linesize[0] + 2], 200);
	EXPECT_EQ(f->data[0][5 * f->linesize[0] + 5], 200);
	EXPECT_EQ(f->data[0][6 * f->linesize[0] + 6], 16);
	EXPECT_EQ(f->data[1][0], 128);
	EXPECT_EQ(f->data[1][1 * f->linesize[1] + 1], 200);
	EXPECT_EQ(f->data[2][3 * f->linesize[2] + 3], 128);
}

TEST(FrameCanvas, UsesFullRangeBlack)
{
	ffmpeg::FrameCanvas canvas;
	auto out = canvas.Place(MakeFrame(AV_PIX_FMT_YUVJ420P, 4, 4, AVCOL_RANGE_JPEG, 200), 8, 4, 4, 0);
	ASSERT_NE(out, nullptr) << canvas.GetLastErrorString().CStr();

	EXPECT_EQ(AV(out)->data[0][0], 0);
	EXPECT_EQ(AV(out)->data[0][4], 200);
}

TEST(FrameCanvas, TreatsYuvjAsFullRange)
{
	// The range is not always set on frames coming out of the filter graph
	ffmpeg::FrameCanvas canvas;
	auto out = canvas.Place(MakeFrame(AV_PIX_FMT_YUVJ420P, 4, 4, AVCOL_RANGE_UNSPECIFIED, 200), 8, 4, 4, 0);
	ASSERT_NE(out, nullptr) << canvas.GetLastErrorString().CStr();

	EXPECT_EQ(AV(out)->data[0][0], 0);
	EXPECT_EQ(AV(out)->data[1][0], 128);
}

TEST(FrameCanvas, PlacesPackedRgba)
{
	ffmpeg::FrameCanvas canvas;
	auto out = canvas.Place(MakeFrame(AV_PIX_FMT_RGBA, 2, 2, AVCOL_RANGE_JPEG, 200), 4, 2, 2, 0);
	ASSERT_NE(out, nullptr) << canvas.GetLastErrorString().CStr();

	auto row = AV(out)->data[0];
	// Opaque black, then the source
	EXPECT_EQ(row[0], 0);
	EXPECT_EQ(row[3], 255);
	EXPECT_EQ(row[2 * 4], 200);
	EXPECT_EQ(row[3 * 4 + 3], 200);
}

TEST(FrameCanvas, RejectsWhatDoesNotFit)
{
	ffmpeg::FrameCanvas canvas;
	auto src = MakeFrame(AV_PIX_FMT_YUV420P, 4, 4, AVCOL_RANGE_MPEG, 200);

	EXPECT_EQ(canvas.Place(src, 8, 8, 6, 0), nullptr);
	EXPECT_EQ(canvas.Place(src, 8, 8, -2, 0), nullptr);
	EXPECT_EQ(canvas.Place(nullptr, 8, 8, 0, 0), nullptr);
	EXPECT_FALSE(canvas.GetLastErrorString().IsEmpty());
}

TEST(FrameCanvas, FramesOutliveTheCanvas)
{
	std::shared_ptr<MediaFrame> out;
	{
		ffmpeg::FrameCanvas canvas;
		out = canvas.Place(MakeFrame(AV_PIX_FMT_YUV420P, 4, 4, AVCOL_RANGE_MPEG, 200), 8, 8, 2, 2);
		ASSERT_NE(out, nullptr);

		// A new format rebuilds the pool while the first frame is still held
		ASSERT_NE(canvas.Place(MakeFrame(AV_PIX_FMT_NV12, 4, 4, AVCOL_RANGE_MPEG, 100), 8, 8, 2, 2), nullptr);
	}

	EXPECT_EQ(AV(out)->data[0][2 * AV(out)->linesize[0] + 2], 200);
}

TEST(FrameCanvas, RejectsOtherBackends)
{
	ffmpeg::FrameCanvas canvas;
	EXPECT_EQ(canvas.Place(MakeOtherBackendFrame(4, 4), 8, 8, 2, 2), nullptr);
	EXPECT_FALSE(canvas.GetLastErrorString().IsEmpty());
}

TEST(FrameCanvas, KeepsChromaAligned)
{
	ffmpeg::FrameCanvas canvas;

	// 4:2:0 chroma covers 2x2 luma samples, so an odd offset would split one
	auto yuv = MakeFrame(AV_PIX_FMT_YUV420P, 4, 4, AVCOL_RANGE_MPEG, 200);
	EXPECT_EQ(canvas.Place(yuv, 8, 8, 1, 2), nullptr);
	EXPECT_EQ(canvas.Place(yuv, 8, 8, 2, 1), nullptr);
	EXPECT_NE(canvas.Place(yuv, 8, 8, 2, 2), nullptr);

	// RGBA has no subsampling
	EXPECT_NE(canvas.Place(MakeFrame(AV_PIX_FMT_RGBA, 2, 2, AVCOL_RANGE_JPEG, 200), 4, 4, 1, 1), nullptr) << canvas.GetLastErrorString().CStr();
}
