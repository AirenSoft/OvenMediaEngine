//==============================================================================
//
//  OvenMediaEngine
//
//  Copyright (c) 2026 OvenMediaLabs. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/mediarouter/media_type.h>

// Every scale mode is the same operation: scale `src` (a region of the input) into
// `dst` (a region of the output) and fill the rest of the output with black.
//   stretch : src = whole input,                dst = whole output
//   fit     : src = whole input,                dst = centered, aspect-preserving
//   crop    : src = centered, aspect-matched,   dst = whole output
// Partial rectangles have even offsets and sizes so the 4:2:0 chroma planes line up.
struct ScaleRect
{
	int32_t x	   = 0;
	int32_t y	   = 0;
	int32_t width  = 0;
	int32_t height = 0;

	bool operator==(const ScaleRect &other) const
	{
		return x == other.x && y == other.y && width == other.width && height == other.height;
	}
	bool operator!=(const ScaleRect &other) const
	{
		return !(*this == other);
	}

	ov::String ToString() const
	{
		return ov::String::FormatString("%dx%d+%d+%d", width, height, x, y);
	}
};

struct ScaleGeometry
{
	// The sizes Compute() was given
	cmn::Resolution input;
	cmn::Resolution output;

	ScaleRect src;
	ScaleRect dst;

	// Lengths and offsets are rounded to even values, which can leave an input that
	// already matches the output aspect ratio a pixel or two off. A difference of this
	// many pixels or less is treated as the same aspect ratio, so such an input is not
	// padded or cropped.
	static constexpr int32_t kAspectTolerance = 2;

	static ScaleGeometry Compute(const cmn::Resolution &input, const cmn::Resolution &output, cmn::VideoScaleMode mode);

	bool HasCrop() const
	{
		return src != ScaleRect{0, 0, input.width, input.height};
	}

	bool HasPadding() const
	{
		return dst != ScaleRect{0, 0, output.width, output.height};
	}

	ov::String ToString() const
	{
		return ov::String::FormatString("src(%s) dst(%s)", src.ToString().CStr(), dst.ToString().CStr());
	}
};
