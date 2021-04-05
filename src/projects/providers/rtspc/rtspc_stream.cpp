//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================

#include "base/info/application.h"
#include "rtspc_stream.h"
#include "rtspc_provider.h"

#define OV_LOG_TAG "RtspcStream"

namespace pvd
{
	std::shared_ptr<RtspcStream> RtspcStream::Create(const std::shared_ptr<pvd::PullApplication> &application, 
		const uint32_t stream_id, const ov::String &stream_name,
		const std::vector<ov::String> &url_list)
	{
		info::Stream stream_info(*std::static_pointer_cast<info::Application>(application), StreamSourceType::RtspPull);

		stream_info.SetId(stream_id);
		stream_info.SetName(stream_name);

		auto stream = std::make_shared<RtspcStream>(application, stream_info, url_list);
		if (!stream->Start())
		{
			// Explicit deletion
			stream.reset();
			return nullptr;
		}

		return stream;
	}

	RtspcStream::RtspcStream(const std::shared_ptr<pvd::PullApplication> &application, const info::Stream &stream_info, const std::vector<ov::String> &url_list)
	: pvd::PullStream(application, stream_info)
	{
		_state = State::IDLE;

		for(auto &url : url_list)
		{
			auto parsed_url = ov::Url::Parse(url);
			if(parsed_url)
			{
				_url_list.push_back(parsed_url);
			}
		}

		if(!_url_list.empty())
		{
			_curr_url = _url_list[0];
			SetMediaSource(_curr_url->ToUrlString(true));
		}
	}

	RtspcStream::~RtspcStream()
	{
		Stop();
		Release();
	}

	std::shared_ptr<pvd::RtspcProvider> RtspcStream::GetRtspcProvider()
	{
		return std::static_pointer_cast<RtspcProvider>(_application->GetParentProvider());
	}

	void RtspcStream::Release()
	{
	}

	bool RtspcStream::Start()
	{
		if(_state != State::IDLE)
		{
			return false;
		}

		ov::StopWatch stop_watch;

		stop_watch.Start();
		if(ConnectTo() == false)
		{
			return false;
		}
		_origin_request_time_msec = stop_watch.Elapsed();

		stop_watch.Update();

		if(RequestDescribe() == false)
		{
			return false;
		}

		if(RequestSetup() == false)
		{
			return false;
		}

		_origin_response_time_msec = stop_watch.Elapsed();

		return pvd::PullStream::Start();
	}

	bool RtspcStream::Play()
	{
		if (!RequestPlay())
		{
			return false;
		}

		// Stream was created completly 
		_stream_metrics = StreamMetrics(*std::static_pointer_cast<info::Stream>(GetSharedPtr()));
		if(_stream_metrics != nullptr)
		{
			_stream_metrics->SetOriginRequestTimeMSec(_origin_request_time_msec);
			_stream_metrics->SetOriginResponseTimeMSec(_origin_response_time_msec);
		}

		return pvd::PullStream::Play();
	}

	bool RtspcStream::Stop()
	{
		// Already stopping
		if(_state != State::PLAYING)
		{
			return true;
		}
		
		if(!RequestStop())
		{
			// Force terminate 
			_state = State::ERROR;
		}

		_state = State::STOPPED;
	
		return pvd::PullStream::Stop();
	}

	bool RtspcStream::ConnectTo()
	{
		if(_state != State::IDLE && _state != State::ERROR)
		{
			return false;
		}

		logti("Requested url[%d] : %s", strlen(_curr_url->Source().CStr()), _curr_url->Source().CStr() );

		auto scheme = _curr_url->Scheme();
		if(scheme.UpperCaseString() != "RTSP")
		{
			_state = State::ERROR;
			logte("The scheme is not rtsp : %s", scheme.CStr());
			return false;
		}

		auto signalling_socket_pool = GetRtspcProvider()->GetSignallingSocketPool();
		if (signalling_socket_pool == nullptr)
		{
			// Provider is not initialized
			logte("Could not get socket from socket pool");
			return false;
		}

		// Connect
		_signalling_socket = signalling_socket_pool->AllocSocket();
		if ((_signalling_socket == nullptr) || (_signalling_socket->AttachToWorker() == false))
		{
			_state = State::ERROR;
			logte("To create client socket is failed.");
			
			_signalling_socket = nullptr;
			return false;
		}

		_signalling_socket->MakeBlocking();

		struct timeval tv = {3, 0}; // 1.5 sec
		// Only work for blocking mode
		_signalling_socket->SetRecvTimeout(tv);

		ov::SocketAddress socket_address(_curr_url->Host(), _curr_url->Port());

		auto error = _signalling_socket->Connect(socket_address, 1500);
		if (error != nullptr)
		{
			_state = State::ERROR;
			logte("Cannot connect to server (%s) : %s:%d", error->GetMessage().CStr(), _curr_url->Host().CStr(), _curr_url->Port());
			return false;
		}

		_state = State::CONNECTED;

		return true;
	}

	bool RtspcStream::RequestDescribe()
	{
		if(_state != State::CONNECTED)
		{
			return false;
		}

		auto describe = std::make_shared<RtspMessage>(RtspMethod::DESCRIBE, GetNextCSeq(), _curr_url->ToUrlString(true));
		describe->AddHeaderField(std::make_shared<RtspHeaderField>(RtspHeaderFieldType::Accept,"application/sdp"));
		describe->AddHeaderField(std::make_shared<RtspHeaderField>(RtspHeaderFieldType::UserAgent, RTSP_USER_AGENT_NAME));
		
		if(SendRequestMessage(describe) == false)
		{
			_state = State::ERROR;
			logte("Could not request DESCIBE to RTSP server (%s)", _curr_url->ToUrlString().CStr());
			return false;
		}

		auto reply = WaitForResponse(std::make_shared<RequestResponse>(describe), 3000);
		if(reply == nullptr || reply->GetStatusCode() != 200)
		{
			_state = State::ERROR;
			logte("Rtsp server(%s) rejected the describe request : %d(%s)", _curr_url->ToUrlString(), reply->GetStatusCode(), reply->GetReasonPhrase().CStr());
			return false;
		}

		// Parse SDP to add track information
		SessionDescription	sdp;
		if(sdp.FromString(reply->GetBody()->ToString()) == false)
		{
			_state = State::ERROR;
			logte("Parsing of SDP received from rtsp url (%s)failed. ", _curr_url->ToUrlString());
			return false;
		}

		auto media_desc_list = sdp.GetMediaList();
		for(const auto &media_desc : media_desc_list)
		{
			auto first_payload = media_desc->GetFirstPayload();
			if(first_payload == nullptr)
			{
				logte("Failed to get the first Payload type of peer sdp");
				return false;
			}

			if(media_desc->GetMediaType() == MediaDescription::MediaType::Audio)
			{
				_audio_payload_type = first_payload->GetId();

				auto audio_track = std::make_shared<MediaTrack>();

				auto codec = first_payload->GetCodec();
				auto samplerate = first_payload->GetCodecRate();
				auto channels = std::atoi(first_payload->GetCodecParams());
				
				
			}
			else if(media_desc->GetMediaType() == MediaDescription::MediaType::Video)
			{
				_video_payload_type = first_payload->GetId();
			}
		}

		_state = State::DESCRIBED;

		return true;
	}

	bool RtspcStream::RequestSetup()
	{
		return false;
	}

	bool RtspcStream::RequestPlay()
	{
		if(_state != State::DESCRIBED)
		{
			return false;
		}


		_state = State::PLAYING;

		return true;
	}

	bool RtspcStream::RequestStop()
	{
		if (_state != State::PLAYING)
		{
			return false;
		}

		_state = State::STOPPING;

		return true;
	}

	int32_t RtspcStream::GetNextCSeq()
	{
		return _cseq++;
	}

	bool RtspcStream::SendRequestMessage(const std::shared_ptr<RtspMessage> &message)
	{
		return true;
	}

	std::shared_ptr<RtspMessage> RtspcStream::WaitForResponse(const std::shared_ptr<RequestResponse>& request, uint64_t timeout_ms)
	{
		// state == Playing
			// return RequestMessage.WaitForResponse(), erase
		
		// else
			// return Receivemessage, erase


		// timeout
			// Remove

		return nullptr;
	}

	std::shared_ptr<RtspMessage> RtspcStream::ReceiveMessage(int64_t timeout_msec)
	{
		ov::StopWatch stop_watch;

		stop_watch.Start();
		while(true)
		{
			if(ReceivePacket(false) == false)
			{
				return nullptr;
			}

			if(_rtsp_demuxer.IsAvailableMessage())
			{
				return _rtsp_demuxer.PopMessage();
			}

			if(stop_watch.IsElapsed(timeout_msec))
			{
				return nullptr;
			}
		}

		return nullptr;
	}

	bool RtspcStream::ReceivePacket(bool non_block, int64_t timeout_msec)
	{
		return true;
	}

	int RtspcStream::GetFileDescriptorForDetectingEvent()
	{
		return 0;
	}

	PullStream::ProcessMediaResult RtspcStream::ProcessMediaPacket()
	{
		// Receive Packet

		// Append packet to demuxer

		// In an interleaved session, the server sends both messages and data in the same session.
		// Check if there are available messages and interleaved data

		return ProcessMediaResult::PROCESS_MEDIA_SUCCESS;
	}
}



