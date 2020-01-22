#pragma once

#include "base/common_types.h"
#include "base/provider/stream.h"

using namespace pvd;

class RtspStream : public Stream
{
public:
	static std::shared_ptr<RtspStream> Create();

public:
	explicit RtspStream();
	~RtspStream() final;

private:
};