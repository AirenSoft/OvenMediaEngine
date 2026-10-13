//==============================================================================
//
//  OvenMediaEngine
//
//  Copyright (c) 2026 OvenMediaLabs. All rights reserved.
//
//==============================================================================
#include <gtest/gtest.h>

#include <cstdlib>

#include "scale_geometry.h"

namespace
{
	using Mode = cmn::VideoScaleMode;

	constexpr cmn::Resolution k720p{1280, 720};

	ScaleRect Full(const cmn::Resolution &resolution)
	{
		return {0, 0, resolution.width, resolution.height};
	}

	bool IsEven(int32_t value)
	{
		return (value % 2) == 0;
	}

	// Invariants every mode must hold, whatever the input
	void ExpectValid(const cmn::Resolution &in, const cmn::Resolution &out, Mode mode)
	{
		SCOPED_TRACE(ov::String::FormatString("%dx%d -> %dx%d (%s)", in.width, in.height, out.width, out.height, cmn::GetVideoScaleModeString(mode)).CStr());

		auto g = ScaleGeometry::Compute(in, out, mode);

		EXPECT_GE(g.src.x, 0);
		EXPECT_GE(g.src.y, 0);
		EXPECT_LE(g.src.x + g.src.width, in.width);
		EXPECT_LE(g.src.y + g.src.height, in.height);
		EXPECT_GT(g.src.width, 0);
		EXPECT_GT(g.src.height, 0);

		EXPECT_GE(g.dst.x, 0);
		EXPECT_GE(g.dst.y, 0);
		EXPECT_LE(g.dst.x + g.dst.width, out.width);
		EXPECT_LE(g.dst.y + g.dst.height, out.height);
		EXPECT_GT(g.dst.width, 0);
		EXPECT_GT(g.dst.height, 0);

		// Only one side is ever adjusted
		EXPECT_FALSE(g.HasCrop() && g.HasPadding());

		// A shortened side has an even offset and length; a full side keeps the frame size
		EXPECT_TRUE(g.src.width == in.width || (IsEven(g.src.x) && IsEven(g.src.width))) << g.ToString().CStr();
		EXPECT_TRUE(g.src.height == in.height || (IsEven(g.src.y) && IsEven(g.src.height))) << g.ToString().CStr();
		EXPECT_TRUE(g.dst.width == out.width || (IsEven(g.dst.x) && IsEven(g.dst.width))) << g.ToString().CStr();
		EXPECT_TRUE(g.dst.height == out.height || (IsEven(g.dst.y) && IsEven(g.dst.height))) << g.ToString().CStr();

		if (g.HasPadding())
		{

			// Centered: the two margins differ by at most one chroma sample
			EXPECT_LE(std::abs((out.width - g.dst.width - g.dst.x) - g.dst.x), 2);
			EXPECT_LE(std::abs((out.height - g.dst.height - g.dst.y) - g.dst.y), 2);
		}
	}
}  // namespace

TEST(ScaleGeometry, KeepsTheSizesItWasComputedFor)
{
	auto g = ScaleGeometry::Compute({1080, 1920}, k720p, Mode::Fit);

	EXPECT_EQ(g.input, (cmn::Resolution{1080, 1920}));
	EXPECT_EQ(g.output, k720p);
	EXPECT_FALSE(g.HasCrop());
	EXPECT_TRUE(g.HasPadding());
}

TEST(ScaleGeometry, StretchUsesWholeFrames)
{
	auto g = ScaleGeometry::Compute({1080, 1920}, k720p, Mode::Stretch);

	EXPECT_EQ(g.src, Full({1080, 1920}));
	EXPECT_EQ(g.dst, Full(k720p));
}

TEST(ScaleGeometry, FitPortraitIntoLandscapePillarboxes)
{
	auto g = ScaleGeometry::Compute({1080, 1920}, k720p, Mode::Fit);

	EXPECT_EQ(g.src, Full({1080, 1920}));
	// 720 * 1080 / 1920 = 405 -> 406
	EXPECT_EQ(g.dst, (ScaleRect{436, 0, 406, 720}));
}

TEST(ScaleGeometry, FitWideIntoLandscapeLetterboxes)
{
	auto g = ScaleGeometry::Compute({1920, 800}, k720p, Mode::Fit);

	EXPECT_EQ(g.src, Full({1920, 800}));
	// 1280 * 800 / 1920 = 533.3 -> 534
	EXPECT_EQ(g.dst, (ScaleRect{0, 92, 1280, 534}));
}

TEST(ScaleGeometry, CropPortraitIntoLandscapeCutsTopAndBottom)
{
	auto g = ScaleGeometry::Compute({1080, 1920}, k720p, Mode::Crop);

	// 1080 * 720 / 1280 = 607.5 -> 608
	EXPECT_EQ(g.src, (ScaleRect{0, 656, 1080, 608}));
	EXPECT_EQ(g.dst, Full(k720p));
}

TEST(ScaleGeometry, CropWideIntoLandscapeCutsSides)
{
	auto g = ScaleGeometry::Compute({1920, 800}, k720p, Mode::Crop);

	// 800 * 1280 / 720 = 1422.2 -> 1422
	EXPECT_EQ(g.src, (ScaleRect{248, 0, 1422, 800}));
	EXPECT_EQ(g.dst, Full(k720p));
}

TEST(ScaleGeometry, SameAspectNeedsNoPadOrCrop)
{
	for (auto mode : {Mode::Fit, Mode::Crop})
	{
		auto g = ScaleGeometry::Compute({1920, 1080}, k720p, mode);
		EXPECT_FALSE(g.HasCrop());
		EXPECT_FALSE(g.HasPadding());

		g = ScaleGeometry::Compute(k720p, k720p, mode);
		EXPECT_FALSE(g.HasCrop());
		EXPECT_FALSE(g.HasPadding());
	}
}

TEST(ScaleGeometry, NearlySameAspectIsTreatedAsSame)
{
	// 1279x719 is within rounding of 16:9
	for (auto mode : {Mode::Fit, Mode::Crop})
	{
		auto g = ScaleGeometry::Compute({1279, 719}, k720p, mode);
		EXPECT_FALSE(g.HasCrop());
		EXPECT_FALSE(g.HasPadding());
	}

	// 854x480 -> 1280x720 is 1px off 16:9
	auto g = ScaleGeometry::Compute({854, 480}, k720p, Mode::Fit);
	EXPECT_FALSE(g.HasPadding());
}

TEST(ScaleGeometry, InvalidSizesFallBackToStretch)
{
	for (auto mode : {Mode::Fit, Mode::Crop})
	{
		auto g = ScaleGeometry::Compute({0, 0}, k720p, mode);
		EXPECT_EQ(g.dst, Full(k720p));

		g = ScaleGeometry::Compute({1920, 1080}, {0, 0}, mode);
		EXPECT_EQ(g.src, Full({1920, 1080}));
	}
}

TEST(ScaleGeometry, InvariantsHoldAcrossResolutions)
{
	const cmn::Resolution inputs[] = {
		{1920, 1080}, {1080, 1920}, {1280, 960}, {960, 1280}, {1279, 719}, {1281, 721},
		{3840, 200}, {200, 3840}, {640, 360}, {720, 576}, {1440, 1080}, {16, 16}, {3, 5}};
	const cmn::Resolution outputs[] = {
		{1280, 720}, {720, 1280}, {854, 480}, {640, 640}, {1920, 1080}, {320, 180}, {16, 16}};

	for (const auto &in : inputs)
	{
		for (const auto &out : outputs)
		{
			for (auto mode : {Mode::Fit, Mode::Crop, Mode::Stretch})
			{
				ExpectValid(in, out, mode);
			}
		}
	}
}

TEST(ScaleGeometry, ExtremeAspectKeepsAMinimumSize)
{
	auto g = ScaleGeometry::Compute({3840, 2}, k720p, Mode::Fit);
	EXPECT_GE(g.dst.height, 2);

	g = ScaleGeometry::Compute({2, 3840}, k720p, Mode::Crop);
	EXPECT_GE(g.src.height, 2);
}
