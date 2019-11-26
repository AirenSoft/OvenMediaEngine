
#include "ovt_private.h"
#include "ovt_stream.h"

std::shared_ptr<OvtStream> OvtStream::Create(const std::shared_ptr<Application> application,
											 const StreamInfo &info,
											 uint32_t worker_count)
{
	auto stream = std::make_shared<OvtStream>(application, info);
	if(!stream->Start(worker_count))
	{
		return nullptr;
	}
	return stream;
}

OvtStream::OvtStream(const std::shared_ptr<Application> application,
					 const StreamInfo &info)
		: Stream(application, info)
{
}

OvtStream::~OvtStream()
{
	logtd("OvtStream(%s/%d) has been terminated finally", GetName().CStr(), GetId());
	Stop();
}

bool OvtStream::Start(uint32_t worker_count)
{


	return Stream::Start(worker_count);
}

bool OvtStream::Stop()
{
	return Stream::Stop();
}

void OvtStream::SendVideoFrame(const std::shared_ptr<MediaPacket> &media_packet)
{

}

void OvtStream::SendAudioFrame(const std::shared_ptr<MediaPacket> &media_packet)
{

}

