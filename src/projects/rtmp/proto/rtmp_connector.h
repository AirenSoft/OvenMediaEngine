//==============================================================================
//
//  RtmpProvider
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include <vector>
#include <string>

#include "librtmp-2.4/rtmp_sys.h"
#include "librtmp-2.4/log.h"

#define AVC2(str)	{(char*)str,sizeof(str)-1}
#define SAVC(x) 	AVal av_##x = AVC2(#x)
#define STR2AVAL(av,str)	av.av_val = (char*)str; av.av_len = strlen(av.av_val)

class RtmpConnector
{
public:
	SAVC(connect);

	SAVC(app);
	SAVC(flashVer);
	SAVC(swfUrl);
	SAVC(pageUrl);
	SAVC(tcUrl);
	SAVC(audioCodecs);
	SAVC(videoCodecs);
	SAVC(objectEncoding);

	SAVC(_result);
	SAVC(fmsVer);
	SAVC(capabilities);
	SAVC(mode);
	SAVC(level);
	SAVC(code);
	SAVC(description);

	SAVC(releaseStream);
	SAVC(FCPublish);
	SAVC(createStream);
	
	SAVC(publish);
	SAVC(onFCPublish);

	SAVC(status);
	SAVC(clientid);
	SAVC(onStatus);

	SAVC(FCUnpublish);
	SAVC(deleteStream);
	
	SAVC(onMetaData);
	SAVC(video);
	SAVC(audio);
	SAVC(duration);
	SAVC(fileSize);
	SAVC(width);
	SAVC(height);
	SAVC(videocodecid);
	SAVC(videodatarate);
	SAVC(framerate);
	SAVC(audiocodecid);
	SAVC(audiodatarate);
	SAVC(audiosamplerate);
	SAVC(audiosamplesize);
	SAVC(audiochannels);
	SAVC(encoder);
	
	const AVal av_NetStream_Play_Start = AVC2("NetStream.Play.Start");
	const AVal av_Started_playing = AVC2("Started playing");
	const AVal av_NetStream_Play_Stop = AVC2("NetStream.Play.Stop");
	const AVal av_Stopped_playing = AVC2("Stopped playing");
	const AVal av_NetStream_Authenticate_UsherToken = AVC2("NetStream.Authenticate.UsherToken");
	const AVal av_NetStream_Publish_Start = AVC2("NetStream.Publish.Start");
	const AVal av_Started_publishing = AVC2("Started publishing stream.");

public:
    RtmpConnector(int32_t sock_fd, void* parent);
    ~RtmpConnector();

	int  serve_packet(RTMP *r, RTMPPacket *packet);

	int  do_cycle();

	void handle_chunksize(RTMP *r, const RTMPPacket *packet);
	int  handle_info(RTMP * r, RTMPPacket *packet, unsigned int offset);
	int  handle_invoke(RTMP * r, RTMPPacket *packet, unsigned int offset);

	int  send_chunk_size(RTMP *r);
	int	 send_connect_result(RTMP *r, double txn);
	int	 send_release_stream_result(RTMP *r, double txn);
	int  send_fcpublich_result(RTMP *r, double txn);
	int  send_create_stream_result(RTMP *r, double txn);
	int  send_onfcpublish(RTMP *r, double txn);
	int  send_onstatus_play_start(RTMP *r, double txn);

	std::string 	GetApplication();
	
	std::string 	GetStream();

private:
	RTMP*			_rtmp;
	RTMPPacket*		_packet;

public:
	void*			_parent;

	std::string 	_app;
	std::string 	_stream;

	bool			_has_video;
	bool			_has_audio;
	
	double 			_duration;
	double			_filesize;
	double			_width;
	double			_height;
	std::string 	_video_codec_id;
	double 			_video_bitrate;
	double 			_framerate;
	std::string 	_audio_codec_id;
	double 			_audio_bitrate;
	double 			_audio_samplerate;
	double 			_audio_samplesize;
	double 			_audio_channels;
	std::string 	_encoder;

public:
	// @base/ApplicatinInfo
	uint32_t		_app_id;
	// @base/StreamInfo
	uint32_t 		_stream_id;
};


