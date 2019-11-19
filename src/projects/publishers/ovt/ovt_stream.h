#pragma once

#include <base/common_types.h>
#include <base/publisher/stream.h>

class OvtStream : public Stream
{
public:
	static std::shared_ptr<OvtStream> Create(const std::shared_ptr<Application> application,
											 const StreamInfo &info,
											 uint32_t worker_count);
	explicit OvtStream(const std::shared_ptr<Application> application,
					   const StreamInfo &info);
	~OvtStream() final;

	void SendVideoFrame(std::shared_ptr<MediaTrack> track,
						std::unique_ptr<EncodedFrame> encoded_frame,
						std::unique_ptr<CodecSpecificInfo> codec_info,
						std::unique_ptr<FragmentationHeader> fragmentation) override;

	void SendAudioFrame(std::shared_ptr<MediaTrack> track,
						std::unique_ptr<EncodedFrame> encoded_frame,
						std::unique_ptr<CodecSpecificInfo> codec_info,
						std::unique_ptr<FragmentationHeader> fragmentation) override;

private:
	bool Start(uint32_t worker_count) override;
	bool Stop() override;
};
