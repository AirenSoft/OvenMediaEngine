
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
	logtd("OvtStream(%d) has been terminated finally", GetId());
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

void OvtStream::SendVideoFrame(std::shared_ptr<MediaTrack> track,
							   std::unique_ptr<EncodedFrame> encoded_frame,
							   std::unique_ptr<CodecSpecificInfo> codec_info,
							   std::unique_ptr<FragmentationHeader> fragmentation)
{

}

void OvtStream::SendAudioFrame(std::shared_ptr<MediaTrack> track,
							   std::unique_ptr<EncodedFrame> encoded_frame,
							   std::unique_ptr<CodecSpecificInfo> codec_info,
							   std::unique_ptr<FragmentationHeader> fragmentation)
{

}

