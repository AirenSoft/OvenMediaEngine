//==============================================================================
//
//  OvenMediaEngine
//
//  Copyright (c) 2026 OvenMediaLabs. All rights reserved.
//
//==============================================================================
#include "scale_geometry.h"

#include <algorithm>
#include <cmath>

namespace
{
	// Nearest even value in [2, limit]
	int32_t ToEvenLength(double length, int32_t limit)
	{
		auto even = static_cast<int32_t>(std::lround(length / 2.0)) * 2;
		return std::clamp(even, 2, std::max(limit & ~1, 2));
	}

	int32_t ToCenteredOffset(int32_t whole, int32_t part)
	{
		return std::max((whole - part) / 2, 0) & ~1;
	}
}  // namespace

ScaleGeometry ScaleGeometry::Compute(const cmn::Resolution &input, const cmn::Resolution &output, cmn::VideoScaleMode mode)
{
	ScaleGeometry geometry;
	geometry.input	= input;
	geometry.output = output;
	geometry.src	= {0, 0, input.width, input.height};
	geometry.dst = {0, 0, output.width, output.height};

	if (mode == cmn::VideoScaleMode::Stretch ||
		input.width <= 0 || input.height <= 0 || output.width <= 0 || output.height <= 0)
	{
		return geometry;
	}

	// Compare the aspect ratios without rounding: in.w / in.h vs out.w / out.h
	const int64_t input_cross  = static_cast<int64_t>(input.width) * output.height;
	const int64_t output_cross = static_cast<int64_t>(output.width) * input.height;
	const bool input_is_wider  = input_cross > output_cross;

	if (mode == cmn::VideoScaleMode::Fit)
	{
		auto &dst = geometry.dst;
		if (input_is_wider)
		{
			dst.height = ToEvenLength(static_cast<double>(output.width) * input.height / input.width, output.height);
			dst.y	   = ToCenteredOffset(output.height, dst.height);
		}
		else
		{
			dst.width = ToEvenLength(static_cast<double>(output.height) * input.width / input.height, output.width);
			dst.x	  = ToCenteredOffset(output.width, dst.width);
		}

		if ((output.width - dst.width) <= kAspectTolerance && (output.height - dst.height) <= kAspectTolerance)
		{
			dst = {0, 0, output.width, output.height};
		}
	}
	else if (mode == cmn::VideoScaleMode::Crop)
	{
		auto &src = geometry.src;
		if (input_is_wider)
		{
			src.width = ToEvenLength(static_cast<double>(input.height) * output.width / output.height, input.width);
			src.x	  = ToCenteredOffset(input.width, src.width);
		}
		else
		{
			src.height = ToEvenLength(static_cast<double>(input.width) * output.height / output.width, input.height);
			src.y	   = ToCenteredOffset(input.height, src.height);
		}

		if ((input.width - src.width) <= kAspectTolerance && (input.height - src.height) <= kAspectTolerance)
		{
			src = {0, 0, input.width, input.height};
		}
	}

	return geometry;
}
