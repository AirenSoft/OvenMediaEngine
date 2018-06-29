//==============================================================================
//
//  RtmpProvider
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "rtmp_connector.h"

#include <stdio.h>
#include <stdlib.h>

#include <base/ovlibrary/ovlibrary.h>

#include "rtmp_tcp_server.h"

#define OV_LOG_TAG "RtmpConnector"

RtmpConnector::RtmpConnector(int32_t sock_fd, void* parent) 
{
	_parent = parent;

	// RTMP_LogSetLevel(RTMP_LOGALL);

	_packet = (RTMPPacket*)malloc(sizeof(RTMPPacket));
	memset((void*)_packet, 0, sizeof(RTMPPacket));
	RTMPPacket_Free(_packet);

	_rtmp = RTMP_Alloc();

	RTMP_Init(_rtmp);

	// 소켓 FD 값을 전달함
	_rtmp->m_sb.sb_socket = (int)sock_fd;

	// Simple Handshake
	if (!RTMP_Serve(_rtmp))
	{
		RTMP_Log(RTMP_LOGERROR, "Handshake failed");
		return;
	}

	_app = "";
	_stream = "";
	_has_video = false;
	_has_audio = false;
}

RtmpConnector::~RtmpConnector()
{
	logtd("destory RtmpConnector");

	// RTMPPacket_Free(_packet);
	delete _packet;

	RTMP_Close(_rtmp);
	
	RTMP_Free(_rtmp);

}

int RtmpConnector::do_cycle()
{
	// logtd("do_cycle\r");

	int ret = 0;

	ret = RTMP_IsConnected(_rtmp);
	if(!ret)
	{
		// error disconnected
		return 1;
	}

	// TODO: to make non-blocking. 
	ret = RTMP_ReadPacket(_rtmp, _packet);
	if(!ret)
	{
		// RTMPPacket_Free(_packet);
		// read packet error
		return 2;
	}

	ret = RTMPPacket_IsReady(_packet);
	// ret = RTMPPacket_IsReady(&_packet);
	if(!ret)
	{
		// RTMPPacket_Free(_packet);
		// packet is not ready
		return 0;
	}

	// 패캇 처리
	ret = serve_packet(_rtmp, _packet);

	RTMPPacket_Free(_packet);

	return 0;
}

// RTMP 패킷을 처리하는 함수
int RtmpConnector::serve_packet(RTMP *r, RTMPPacket *packet)
{
	int ret = 0;

	if(packet->m_packetType != RTMP_PACKET_TYPE_AUDIO && packet->m_packetType != RTMP_PACKET_TYPE_VIDEO)
	{
		logtd("%s, received packet type %02X, time %u size %u bytes", __PRETTY_FUNCTION__, packet->m_packetType, packet->m_nTimeStamp, packet->m_nBodySize);
		logtd("%s", ov::Dump(packet->m_body, packet->m_nBodySize).CStr());
	}
 	
 	switch (packet->m_packetType)
    {
    case RTMP_PACKET_TYPE_CHUNK_SIZE:
		handle_chunksize(r, packet);
		break;
    case RTMP_PACKET_TYPE_BYTES_READ_REPORT:
      break;
    case RTMP_PACKET_TYPE_CONTROL:
      break;
    case RTMP_PACKET_TYPE_SERVER_BW:
      break;
    case RTMP_PACKET_TYPE_CLIENT_BW:
      break;
    case RTMP_PACKET_TYPE_AUDIO:

		// 오디오 패킷 수진
		// CALLBACK =====================================
		((RtmpTcpServer*)_parent)->OnAudioPacket(this, packet->m_hasAbsTimestamp, packet->m_nTimeStamp, (int8_t*)packet->m_body, packet->m_nBodySize);
		// end of CALLBACK ==============================

      break;
    case RTMP_PACKET_TYPE_VIDEO:
    	// 비디오 패킷 수진
	    // CALLBACK =====================================
		((RtmpTcpServer*)_parent)->OnVideoPacket(this, packet->m_hasAbsTimestamp, packet->m_nTimeStamp, (int8_t*)packet->m_body, packet->m_nBodySize);
		// end of CALLBACK ==============================
      break;
    case RTMP_PACKET_TYPE_FLEX_STREAM_SEND:
      break;
    case RTMP_PACKET_TYPE_FLEX_SHARED_OBJECT:
      break;
    case RTMP_PACKET_TYPE_FLEX_MESSAGE:
    	break;
    case RTMP_PACKET_TYPE_INFO:
    	handle_info(r, packet, 0);
      break;
    case RTMP_PACKET_TYPE_SHARED_OBJECT:
      break;
    case RTMP_PACKET_TYPE_INVOKE:
    	ret = handle_invoke(r, packet, 0);
		break;
    case RTMP_PACKET_TYPE_FLASH_VIDEO:
      break;
    default:
      logtd("%s, unknown packet type received: 0x%02x", __PRETTY_FUNCTION__, packet->m_packetType);
    }
  return ret;
}


void RtmpConnector::handle_chunksize(RTMP *r, const RTMPPacket *packet)
{
	if (packet->m_nBodySize >= 4)
	{
		r->m_inChunkSize = AMF_DecodeInt32(packet->m_body);
		logtd("chunk size change to %d", r->m_inChunkSize);
	}
}


int RtmpConnector::handle_info(RTMP * r, RTMPPacket *packet, unsigned int offset)
{
	const char *body;
	unsigned int nBodySize;
	int ret = 0;

	body = packet->m_body + offset;
	nBodySize = packet->m_nBodySize - offset;

	AMFObject obj;
	AVal metastring;

	int nRes = AMF_Decode(&obj, body, nBodySize, FALSE);
	if (nRes < 0)
	{
		logtd("%s, error decoding meta data packet", __FUNCTION__);
		return FALSE;
	}

	// AMF_Dump(&obj);

	for(int i = 0 ; i<obj.o_num ; i++)
	{
		AMFProp_GetString(AMF_GetProp(&obj, NULL, i), &metastring);

		// @onMetaData
		if (AVMATCH(&metastring, &av_onMetaData))
		{
			AMFObjectProperty prop;

			// video 또는 audio로 시작하는 메타를 찾음
			if (RTMP_FindPrefixProperty(&obj, &av_video, &prop))
			{
				r->m_read.dataType |= 1;
				
			}
			if (RTMP_FindPrefixProperty(&obj, &av_audio, &prop))
			{
				r->m_read.dataType |= 4;
			
			}

			/* Show metadata */
			if (RTMP_FindFirstMatchingProperty(&obj, &av_duration, &prop))
			{
				r->m_fDuration = prop.p_vu.p_number;
				_duration = r->m_fDuration;
				logtd("Set Duration: %.2f", _duration); 
			}
			if (RTMP_FindFirstMatchingProperty(&obj, &av_fileSize, &prop))
			{
				_filesize = prop.p_vu.p_number;
				logtd("Set Filesize: %.2f", _filesize); 
			}
			if (RTMP_FindFirstMatchingProperty(&obj, &av_width, &prop))
			{
				_width = prop.p_vu.p_number;
				logtd("Set Width: %.2f", _width); 
			}
			if (RTMP_FindFirstMatchingProperty(&obj, &av_height, &prop))
			{
				_height = prop.p_vu.p_number;
				logtd("Set Height: %.2f", _height); 
			}
			if (RTMP_FindFirstMatchingProperty(&obj, &av_videocodecid, &prop))
			{
				_video_codec_id = prop.p_vu.p_aval.av_val;
				_has_video = true;
				logtd("Set Video Codec Id: %s", _video_codec_id.c_str()); 
			}
			if (RTMP_FindFirstMatchingProperty(&obj, &av_videodatarate, &prop))
			{
				_video_bitrate = prop.p_vu.p_number;
				logtd("Set Video Bitrate: %.2f", _video_bitrate); 
			}
			if (RTMP_FindFirstMatchingProperty(&obj, &av_framerate, &prop))
			{
				_framerate = prop.p_vu.p_number;
				logtd("Set Framerate: %.2f", _framerate); 
			}

			if (RTMP_FindFirstMatchingProperty(&obj, &av_audiocodecid, &prop))
			{
				_audio_codec_id = prop.p_vu.p_aval.av_val;
				_has_audio = true;
				logtd("Set Audio Codec Id: %s", _audio_codec_id.c_str()); 
			}
			if (RTMP_FindFirstMatchingProperty(&obj, &av_audiodatarate, &prop))
			{
				_audio_bitrate = prop.p_vu.p_number;
				logtd("Set Audio Bitrate: %.2f", _audio_bitrate); 
			}
			if (RTMP_FindFirstMatchingProperty(&obj, &av_audiosamplerate, &prop))
			{
				_audio_samplerate = prop.p_vu.p_number;
				logtd("Set Audio Samplerate: %.2f", _audio_samplerate); 
			}
			if (RTMP_FindFirstMatchingProperty(&obj, &av_audiosamplesize, &prop))
			{
				_audio_samplesize = prop.p_vu.p_number;
				logtd("Set Audio Samplesize: %.2f", _audio_samplesize); 
			}
			if (RTMP_FindFirstMatchingProperty(&obj, &av_audiochannels, &prop))
			{
				_audio_channels = prop.p_vu.p_number;
				logtd("Set Audio Channels: %.2f", _audio_channels); 
			}
			if (RTMP_FindFirstMatchingProperty(&obj, &av_encoder, &prop))
			{
				_encoder = prop.p_vu.p_aval.av_val;
				logtd("Encoder: %s", _encoder.c_str()); ; 
			}
			ret = TRUE;
		}
	}
	AMF_Reset(&obj);

	// CALLBACK =====================================
	((RtmpTcpServer*)_parent)->OnMetaData(this);
	// end of CALLBACK ==============================

	return ret;
}

int RtmpConnector::handle_invoke(RTMP * r, RTMPPacket *packet, unsigned int offset)
{
	const char *body;
	unsigned int nBodySize;
	int ret = 0, nRes;

	body = packet->m_body + offset;
	nBodySize = packet->m_nBodySize - offset;

	if (body[0] != 0x02)		// make sure it is a string method name we start with
	{
		logtd("Sanity failed. no string method in invoke packet");
		return 0;
	}

	AMFObject obj;
	nRes = AMF_Decode(&obj, (const char*)body, (int)nBodySize, 0);
	if (nRes < 0)
	{
		logte("error decoding invoke packet\r\n");
		return 0;
	}

	// logtd("AMF_CountProp(%d)", AMF_CountProp(&obj));
	
	// AMF_Dump(&obj);

	// RTMP 패킷의 Method를 분석함
	AVal method;
	AMFProp_GetString(AMF_GetProp(&obj, NULL, 0), &method);
	double txn = AMFProp_GetNumber(AMF_GetProp(&obj, NULL, 1));
	logtd("client invoking <%s>, txn<%f>", method.av_val, txn);


	if (AVMATCH(&method, &av_connect))
	{
		AMFObject cobj;
		
		// ENCODER 정보를 분석함
		AMFProp_GetObject(AMF_GetProp(&obj, NULL, 2), &cobj);
		
		for(int i=0; i<cobj.o_num; i++)
		{
			AVal pname, pval;

			pname = cobj.o_props[i].p_name;

			if (cobj.o_props[i].p_type == AMF_STRING)
				pval = cobj.o_props[i].p_vu.p_aval;

			if (AVMATCH(&pname, &av_app))
			{
				r->Link.app = pval;
				pval.av_val = NULL;
				if (!r->Link.app.av_val)
					r->Link.app.av_val = (char*)"";
			}

			else if (AVMATCH(&pname, &av_flashVer))
			{
				r->Link.flashVer = pval;
				pval.av_val = NULL;
			}
			else if (AVMATCH(&pname, &av_swfUrl))
			{
				r->Link.swfUrl = pval;
				pval.av_val = NULL;
			}
			else if (AVMATCH(&pname, &av_tcUrl))
			{
				r->Link.tcUrl = pval;
				pval.av_val = NULL;
			}
			else if (AVMATCH(&pname, &av_pageUrl))
			{
				r->Link.pageUrl = pval;
				pval.av_val = NULL;
			}
			else if (AVMATCH(&pname, &av_audioCodecs))
			{
				r->m_fAudioCodecs = cobj.o_props[i].p_vu.p_number;
			}
			else if (AVMATCH(&pname, &av_videoCodecs))
			{
				r->m_fVideoCodecs = cobj.o_props[i].p_vu.p_number;
			}
			else if (AVMATCH(&pname, &av_objectEncoding))
			{
				r->m_fEncoding = cobj.o_props[i].p_vu.p_number;
			}
		}
		
		// 파라미터가 더 존재한다면, EXTRA Property에 저장함
		/* Still have more parameters? Copy them */
		if (obj.o_num > 3)
		{
			int i = obj.o_num - 3;
			r->Link.extras.o_num = i;
			r->Link.extras.o_props = (AMFObjectProperty*)malloc(i*sizeof(AMFObjectProperty));
			memcpy(r->Link.extras.o_props, obj.o_props+3, i*sizeof(AMFObjectProperty));
			obj.o_num = 3;
		}

		RTMP_SendServerBW(_rtmp);
		RTMP_SendClientBW(_rtmp);
		send_chunk_size(_rtmp);
		send_connect_result(_rtmp, txn);

		_app = r->Link.app.av_val;

		// CALLBACK =====================================
		int ret_code = ((RtmpTcpServer*)_parent)->OnConnect(this, r->Link.app.av_val, r->Link.flashVer.av_val, r->Link.swfUrl.av_val, r->Link.tcUrl.av_val);
		if(ret_code < 0)
		{
			RTMP_Close(_rtmp);		
		}
		// end of CALLBACK ==============================
	}
	else if (AVMATCH(&method, &av_releaseStream))
	{
		AVal tmp;
		AMFProp_GetString(AMF_GetProp(&obj, NULL, 3), &tmp);

		std::string stream_name;
		stream_name.assign((const char*)tmp.av_val, (size_t)tmp.av_len);
		logtd("stream_name : %s, len:%d", stream_name.c_str(), tmp.av_len);

		// CALLBACK =====================================
		_stream = stream_name;
		((RtmpTcpServer*)_parent)->OnReleaseStream(this, stream_name.c_str());
		// end of CALLBACK ==============================

		// 응답 전송
		send_release_stream_result(_rtmp, txn);
	}
	else if (AVMATCH(&method, &av_FCPublish))
	{
		// [CALLBACK : publish]
		AVal tmp;
		AMFProp_GetString(AMF_GetProp(&obj, NULL, 3), &tmp);

		std::string stream_name;
		stream_name.assign((const char*)tmp.av_val, (size_t)tmp.av_len);
		logtd("stream_name : %s, len:%d", stream_name.c_str(), tmp.av_len);

		// CALLBACK =====================================
		_stream = stream_name;
		((RtmpTcpServer*)_parent)->OnFCPublish(this, stream_name.c_str());
		// end of CALLBACK ==============================

		send_fcpublich_result(_rtmp, txn);
	}
	else if (AVMATCH(&method, &av_createStream))
	{
		// CALLBACK =====================================
		((RtmpTcpServer*)_parent)->OnCreateStream(this);
		// end of CALLBACK ==============================

		// RTMP_SendCtrl(_rtmp, 3, _rtmp->m_stream_id, 1000);
		
		send_create_stream_result(r, txn);

	}
	else if (AVMATCH(&method, &av_publish))
	{
		AVal tmp;
		AMFProp_GetString(AMF_GetProp(&obj, NULL, 3), &tmp);

		std::string stream_name;
		stream_name.assign((const char*)tmp.av_val, (size_t)tmp.av_len);
		logtd("stream_name : %s, len:%d", stream_name.c_str(), tmp.av_len);

		AMFProp_GetString(AMF_GetProp(&obj, NULL, 4), &tmp);

		std::string stream_type;
		stream_type.assign((const char*)tmp.av_val, (size_t)tmp.av_len);
		logtd("type : %s", stream_type.c_str());


		// CALLBACK =====================================
		_stream = stream_name;
		((RtmpTcpServer*)_parent)->OnPublish(this, stream_name.c_str(), stream_type.c_str());
		// end of CALLBACK ==============================

		send_onfcpublish(r, txn);
		send_onstatus_play_start(r, txn);
	}
	else if (AVMATCH(&method, &av_FCUnpublish))
	{
		AVal tmp;
		AMFProp_GetString(AMF_GetProp(&obj, NULL, 3), &tmp);

		std::string stream_name;
		stream_name.assign((const char*)tmp.av_val, (size_t)tmp.av_len);
		logtd("stream_name : %s, len:%d", stream_name.c_str(), tmp.av_len);

		// CALLBACK =====================================
		_stream = stream_name;
		((RtmpTcpServer*)_parent)->OnFCUnpublish(this, stream_name.c_str());
		// end of CALLBACK ==============================
	}
	else if (AVMATCH(&method, &av_deleteStream))
	{
		// CALLBACK =====================================
		((RtmpTcpServer*)_parent)->OnDeleteStream(this);
		// end of CALLBACK ==============================
	}
	else 
	{
		logtd("unknown invoke message");
	}

	return 0;
}

int RtmpConnector::send_chunk_size(RTMP *r)
{
	RTMPPacket packet;
	char pbuf[384], *pend = pbuf+sizeof(pbuf);
	AMFObject obj;
	AMFObjectProperty p, op;
	AVal av;

	packet.m_nChannel = 0x02;     // control channel (invoke)
	packet.m_headerType = 0; /* RTMP_PACKET_SIZE_MEDIUM; */
	packet.m_packetType = RTMP_PACKET_TYPE_CHUNK_SIZE;
	packet.m_nTimeStamp = 0;
	packet.m_nInfoField2 = 0;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

	char *enc = packet.m_body;
	enc = AMF_EncodeInt32(enc, pend, 256);

	packet.m_nBodySize = enc - packet.m_body;
	return RTMP_SendPacket(r, &packet, FALSE);	
}

int	RtmpConnector::send_connect_result(RTMP *r, double txn)
{
	RTMPPacket packet;
	char pbuf[384], *pend = pbuf+sizeof(pbuf);
	AMFObject obj;
	AMFObjectProperty p, op;
	AVal av;

	packet.m_nChannel = 0x03;     // control channel (invoke)
	packet.m_headerType = 0; /* RTMP_PACKET_SIZE_MEDIUM; */
	packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
	packet.m_nTimeStamp = 0;
	packet.m_nInfoField2 = 0;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;


	char *enc = packet.m_body;
	enc = AMF_EncodeString(enc, pend, &av__result);
	enc = AMF_EncodeNumber(enc, pend, txn);


	*enc++ = AMF_OBJECT;
	STR2AVAL(av, "FMS/3,5,3,888");
	enc = AMF_EncodeNamedString(enc, pend, &av_fmsVer, &av);
	enc = AMF_EncodeNamedNumber(enc, pend, &av_capabilities, 127.0);
	enc = AMF_EncodeNamedNumber(enc, pend, &av_mode, 1.0);
	*enc++ = 0;
	*enc++ = 0;
	*enc++ = AMF_OBJECT_END;

	*enc++ = AMF_OBJECT;
	STR2AVAL(av, "status");
	enc = AMF_EncodeNamedString(enc, pend, &av_level, &av);
	STR2AVAL(av, "NetConnection.Connect.Success");
	enc = AMF_EncodeNamedString(enc, pend, &av_code, &av);
	STR2AVAL(av, "Connection succeeded");
	enc = AMF_EncodeNamedString(enc, pend, &av_description, &av);
	enc = AMF_EncodeNamedNumber(enc, pend, &av_objectEncoding, r->m_fEncoding);
	*enc++ = 0;
	*enc++ = 0;
	*enc++ = AMF_OBJECT_END;

	packet.m_nBodySize = enc - packet.m_body;
	return RTMP_SendPacket(r, &packet, FALSE);
}


int	RtmpConnector::send_release_stream_result(RTMP *r, double txn)
{
	RTMPPacket packet;
	char pbuf[384], *pend = pbuf+sizeof(pbuf);
	AMFObject obj;
	AMFObjectProperty p, op;
	AVal av;

	packet.m_nChannel = 0x03;     // control channel (invoke)
	packet.m_headerType = 0; /* RTMP_PACKET_SIZE_MEDIUM; */
	packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
	packet.m_nTimeStamp = 0;
	packet.m_nInfoField2 = 0;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

	char *enc = packet.m_body;
	enc = AMF_EncodeString(enc, pend, &av__result);
	enc = AMF_EncodeNumber(enc, pend, txn);
	*enc++ = AMF_NULL;
	*enc++ = AMF_UNDEFINED;

	packet.m_nBodySize = enc - packet.m_body;
	return RTMP_SendPacket(r, &packet, FALSE);
}


int RtmpConnector::send_fcpublich_result(RTMP *r, double txn)
{
	RTMPPacket packet;
	char pbuf[384], *pend = pbuf+sizeof(pbuf);
	AMFObject obj;
	AMFObjectProperty p, op;
	AVal av;

	packet.m_nChannel = 0x03;     // control channel (invoke)
	packet.m_headerType = 0; /* RTMP_PACKET_SIZE_MEDIUM; */
	packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
	packet.m_nTimeStamp = 0;
	packet.m_nInfoField2 = 0;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

	char *enc = packet.m_body;
	enc = AMF_EncodeString(enc, pend, &av__result);
	enc = AMF_EncodeNumber(enc, pend, txn);
	*enc++ = AMF_NULL;
	*enc++ = AMF_UNDEFINED;

	packet.m_nBodySize = enc - packet.m_body;
	return RTMP_SendPacket(r, &packet, FALSE);		
}

int RtmpConnector::send_create_stream_result(RTMP *r, double txn)
{
	RTMPPacket packet;
	char pbuf[256], *pend = pbuf+sizeof(pbuf);

	packet.m_nChannel = 0x03;     // control channel (invoke)
	packet.m_headerType = 0; /* RTMP_PACKET_SIZE_MEDIUM; */
	packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
	packet.m_nTimeStamp = 0;
	packet.m_nInfoField2 = 0;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

	char *enc = packet.m_body;
	enc = AMF_EncodeString(enc, pend, &av__result);
	enc = AMF_EncodeNumber(enc, pend, txn);
	*enc++ = AMF_NULL;
	enc = AMF_EncodeNumber(enc, pend, 1);

	packet.m_nBodySize = enc - packet.m_body;

	return RTMP_SendPacket(r, &packet, FALSE);
}


int RtmpConnector::send_onfcpublish(RTMP *r, double txn)
{
	RTMPPacket packet;
	char pbuf[384], *pend = pbuf+sizeof(pbuf);
	AMFObject obj;
	AMFObjectProperty p, op;
	AVal av;

	packet.m_nChannel = 0x05;     // control channel (invoke)
	packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM; /* RTMP_PACKET_SIZE_MEDIUM; */
	packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
	packet.m_nTimeStamp = 0;
	packet.m_nInfoField2 = 1;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

	char *enc = packet.m_body;
	enc = AMF_EncodeString(enc, pend, &av_onFCPublish);
	enc = AMF_EncodeNumber(enc, pend, txn);

	*enc++ = AMF_NULL;

	*enc++ = AMF_OBJECT;
	enc = AMF_EncodeNamedString(enc, pend, &av_code, &av_NetStream_Publish_Start);
	enc = AMF_EncodeNamedString(enc, pend, &av_description, &av_Started_publishing);
	*enc++ = 0;
	*enc++ = 0;
	*enc++ = AMF_OBJECT_END;

	packet.m_nBodySize = enc - packet.m_body;
	return RTMP_SendPacket(r, &packet, FALSE);		
}


int RtmpConnector::send_onstatus_play_start(RTMP *r, double txn)
{
	RTMPPacket packet;
	char pbuf[512], *pend = pbuf+sizeof(pbuf);

	packet.m_nChannel = 0x05;     // control channel (invoke)
	packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM; 
	packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
	packet.m_nTimeStamp = 0;
	packet.m_nInfoField2 = 0;
	packet.m_hasAbsTimestamp = 0;
	packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

	char *enc = packet.m_body;
	enc = AMF_EncodeString(enc, pend, &av_onStatus);
	enc = AMF_EncodeNumber(enc, pend, txn);
	*enc++ = AMF_NULL;

	*enc++ = AMF_OBJECT;
	enc = AMF_EncodeNamedString(enc, pend, &av_level, &av_status);
	enc = AMF_EncodeNamedString(enc, pend, &av_code, &av_NetStream_Play_Start);
	enc = AMF_EncodeNamedString(enc, pend, &av_description, &av_Started_playing);
	enc = AMF_EncodeNamedString(enc, pend, &av_clientid, &av_clientid);
	*enc++ = 0;
	*enc++ = 0;
	*enc++ = AMF_OBJECT_END;

	packet.m_nBodySize = enc - packet.m_body;
	return RTMP_SendPacket(r, &packet, FALSE);
}

// 어플리케이션명 반환
std::string RtmpConnector::GetApplication()
{
	return _app;
}

// 스트림명 반환
std::string RtmpConnector::GetStream()
{
	return _stream;
}