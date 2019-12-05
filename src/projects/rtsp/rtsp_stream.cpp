#include "rtsp_stream.h"

std::shared_ptr<RtspStream> RtspStream::Create()
{
    auto stream = std::make_shared<RtspStream>();
    return stream;
}

RtspStream::RtspStream()
{	
}


RtspStream::~RtspStream()
{
}
