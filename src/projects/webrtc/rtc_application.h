#pragma once

#include <base/common_types.h>
#include <base/publisher/application.h>
#include <base/application/session_info.h>
#include <base/ovcrypto/certificate.h>
#include <ice/ice_port.h>
#include <rtc_signalling/rtc_signalling.h>

#include "rtc_stream.h"

class RtcApplication : public Application
{
public:
	static std::shared_ptr<RtcApplication> Create(const info::Application &application_info,
	                                              std::shared_ptr<IcePort> ice_port,
	                                              std::shared_ptr<RtcSignallingServer> rtc_signalling);
	RtcApplication(const info::Application &application_info,
	               std::shared_ptr<IcePort> ice_port,
	               std::shared_ptr<RtcSignallingServer> rtc_signalling);
	~RtcApplication() final;

	std::shared_ptr<Certificate> GetCertificate();

private:
	bool Start() override;
	bool Stop() override;


	void SendVideoFrame(std::shared_ptr<StreamInfo> info,
	                    std::shared_ptr<MediaTrack> track,
	                    std::unique_ptr<EncodedFrame> encoded_frame,
	                    std::unique_ptr<CodecSpecificInfo> codec_info,
	                    std::unique_ptr<FragmentationHeader> fragmentation) override;

	// Application Implementation
	std::shared_ptr<Stream> CreateStream(std::shared_ptr<StreamInfo> info) override;
	bool DeleteStream(std::shared_ptr<StreamInfo> info) override;

	std::shared_ptr<IcePort> _ice_port;
	std::shared_ptr<RtcSignallingServer> _rtc_signalling;
	std::shared_ptr<Certificate> _certificate;
};