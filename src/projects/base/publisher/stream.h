#pragma once

#include "session.h"
#include "base/common_types.h"
#include "base/application/stream_info.h"


class Stream : public StreamInfo
{
public:
	// Session을 추가한다.
	bool AddSession(std::shared_ptr<Session> session);
	bool RemoveSession(session_id_t id);
	std::shared_ptr<Session> GetSession(session_id_t id);

	virtual void SendVideoFrame(std::shared_ptr<MediaTrack> track,
	                            std::unique_ptr<EncodedFrame> encoded_frame,
	                            std::unique_ptr<CodecSpecificInfo> codec_info,
	                            std::unique_ptr<FragmentationHeader> fragmentation) = 0;

	virtual void SendAudioFrame(std::shared_ptr<MediaTrack> track,
	                            std::unique_ptr<EncodedFrame> encoded_frame,
	                            std::unique_ptr<CodecSpecificInfo> codec_info,
	                            std::unique_ptr<FragmentationHeader> fragmentation) = 0;

	const std::map<session_id_t, std::shared_ptr<Session>> &GetSessionMap();

	virtual bool Start();
	virtual bool Stop();
protected:
	Stream(const std::shared_ptr<Application> application,
	       const StreamInfo &info);
	virtual ~Stream();
	std::shared_ptr<Application> GetApplication();

private:
	std::map<session_id_t, std::shared_ptr<Session>> _sessions;
	std::mutex _session_map_guard;
	std::shared_ptr<Application> _application;
};