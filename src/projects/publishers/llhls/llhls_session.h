//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/publisher/session.h>

class LLHlsSession : public pub::Session
{
public:
	static std::shared_ptr<LLHlsSession> Create(session_id_t session_id,
												const std::shared_ptr<pub::Application> &application,
												const std::shared_ptr<pub::Stream> &stream);

	LLHlsSession(const info::Session &session_info, 
				const std::shared_ptr<pub::Application> &application, 
				const std::shared_ptr<pub::Stream> &stream);

	bool Start() override;
	bool Stop() override;

	// pub::Session Interface
	void SendOutgoingData(const std::any &packet) override;
	void OnMessageReceived(const std::any &message) override;

private:

	enum class FileType : uint8_t
	{
		Playlist,
		Chunklist,
		InitializationSegment,
		Segment,
		PartialSegment,
	};

	bool ParseFileName(const ov::String &file_name, FileType &type, int32_t &track_id, int64_t &segment_number, int64_t &partial_number);

	void ResponsePlaylist();
	void ResponseChunklist();
	void ResponseInitializationSegment();
	void ResponseSegment();
	void ResponsePartialSegment();
};