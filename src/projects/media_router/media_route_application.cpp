//==============================================================================
//
//  MediaRouteApplication
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================


#include "media_route_application.h"

#include "base/application/application_info.h"
#include "base/application/stream_info.h"

#define OV_LOG_TAG "MediaRouteApplication"

std::shared_ptr<MediaRouteApplication> MediaRouteApplication::Create(std::shared_ptr<ApplicationInfo> appinfo)
{
	auto media_route_application = std::make_shared<MediaRouteApplication>(appinfo);
	media_route_application->Start();
	return media_route_application;
}


MediaRouteApplication::MediaRouteApplication(std::shared_ptr<ApplicationInfo> appinfo) 
{
	// 어플리케이션 정보 저장
	_application_info = appinfo;

	logtd("Created media route application. application(%s)", _application_info->GetName().CStr());
}


MediaRouteApplication::~MediaRouteApplication()
{
	logtd("Destroyed media router application. application(%s)", _application_info->GetName().CStr());
}

bool MediaRouteApplication::Start()
{
	try 
	{
        _kill_flag = false;
        _thread = std::thread(&MediaRouteApplication::MainTask, this);
    } 
    catch (const std::system_error& e) 
    {
        _kill_flag = true;
		logte("Failed to start media route application thread.");
        return false;
    }

    logtd("started media route application thread. application(%s)", _application_info->GetName().CStr());
    return true;
}

bool MediaRouteApplication::Stop()
{
	_kill_flag = true;
	_thread.join();	
}


// 어플리케이션의 스트림이 생성됨
bool MediaRouteApplication::RegisterConnectorApp(
	std::shared_ptr<MediaRouteApplicationConnector> app_conn)
{
	std::unique_lock<std::mutex> lock(_mutex);

	if(app_conn == NULL)
	{
		return false;
	}

	logtd("Register application connector. application(%s/%p) connector_type(%d)"
		, _application_info->GetName().CStr(), app_conn.get(), app_conn->GetConnectorType());
	
	app_conn->SetMediaRouterApplication(GetSharedPtr());

	_connectors.push_back(app_conn);

	return true;
}


// 어플리케이션의 스트림이 생성됨
bool MediaRouteApplication::UnregisterConnectorApp(
	std::shared_ptr<MediaRouteApplicationConnector> app_conn)
{
	if(app_conn == NULL)
	{
		return false;
	}

	logtd("Unregister application connector. application(%s/%p) connector_type(%d)"
		, _application_info->GetName().CStr(), app_conn.get(), app_conn->GetConnectorType());

	// 삭제
	auto position = std::find(_connectors.begin(), _connectors.end(), app_conn);
	if(position == _connectors.end())
	{
		return true;
	}

	std::unique_lock<std::mutex> lock(_mutex);
	_connectors.erase(position);
	lock.unlock();

	return true;
}


bool MediaRouteApplication::RegisterObserverApp(
	std::shared_ptr<MediaRouteApplicationObserver> app_obsrv)
{ 

	if(app_obsrv == NULL)
	{
		return false;
	}
	
	logtd("Register application observer. application(%s/%p) observer_type(%d)"
		, _application_info->GetName().CStr(), app_obsrv.get(), app_obsrv->GetObserverType());

	std::unique_lock<std::mutex> lock(_mutex);
	_observers.push_back(app_obsrv);
	lock.unlock();

	return true; 
}


bool MediaRouteApplication::UnregisterObserverApp(
	std::shared_ptr<MediaRouteApplicationObserver> app_obsrv)
{ 
	if(app_obsrv == NULL)
	{
		return false;
	}
	
	logtd("Unregister application observer. application(%s/%p) observer_type(%d)"
		, _application_info->GetName().CStr(), app_obsrv.get(), app_obsrv->GetObserverType());

	auto position = std::find(_observers.begin(), _observers.end(), app_obsrv);
	if(position == _observers.end())
	{
		return true;
	}

	_observers.erase(position);

	return true; 
}


bool MediaRouteApplication::OnCreateStream(
	std::shared_ptr<MediaRouteApplicationConnector> app_conn, 
	std::shared_ptr<StreamInfo> stream_info)
{
	if(app_conn == NULL || stream_info == NULL)
	{
		return false;
	}

	// 동일한 스트림명 Reconnect 되는 경우 처리함.
	// 기존에 사용하던 Stream의 ID를 재사용한다
	if( app_conn->GetConnectorType() == MediaRouteApplicationConnector::CONNECTOR_TYPE_PROVIDER)
	{
		for (auto it=_streams.begin(); it!=_streams.end(); ++it)
		{
			auto istream = it->second;

			if(stream_info->GetName() == istream->GetStreamInfo()->GetName())
			{
				// 기존에 사용하던 ID를 재사용
				stream_info->SetId( istream->GetStreamInfo()->GetId() );
				logtw("Reconnected same stream from provider(%s, %d)", stream_info->GetName().CStr(), stream_info->GetId());

				return true;
			}
		}
	}


	logtd("Created stream from connector. connector_type(%d), application(%s) stream(%s/%u)"
		, app_conn->GetConnectorType()
		, _application_info->GetName().CStr()
		, stream_info->GetName().CStr()
		, stream_info->GetId());

	std::unique_lock<std::mutex> lock(_mutex);
	
	auto new_stream_info = std::make_shared<StreamInfo>(*stream_info.get());

	auto new_stream = std::make_shared<MediaRouteStream>(new_stream_info);
	
	new_stream->SetConnectorType((int32_t)app_conn->GetConnectorType());

	_streams.insert ( 
		std::make_pair(new_stream_info->GetId(), new_stream)
	);
	
	lock.unlock();

	// 옵저버에 스트림 생성을 알림
	for (auto it=_observers.begin(); it!=_observers.end(); ++it)
	{
    	auto observer = *it;

    	// 프로바이더에서 들어오는 스트림은 트샌스코더에만 전달함
    	if( (app_conn->GetConnectorType() == MediaRouteApplicationConnector::CONNECTOR_TYPE_PROVIDER) && 
    		(observer->GetObserverType() == MediaRouteApplicationObserver::OBSERVER_TYPE_TRANSCODER))
    	{
    		observer->OnCreateStream(new_stream->GetStreamInfo());
    	}
    	else if( (app_conn->GetConnectorType() == MediaRouteApplicationConnector::CONNECTOR_TYPE_TRANSCODER) && 
    		(observer->GetObserverType() == MediaRouteApplicationObserver::OBSERVER_TYPE_PUBLISHER))
    	{
    		observer->OnCreateStream(new_stream->GetStreamInfo());
    	}
    }

	return true;
}


bool MediaRouteApplication::OnDeleteStream(
	std::shared_ptr<MediaRouteApplicationConnector> app_conn, 
	std::shared_ptr<StreamInfo> stream_info)
{
	if(app_conn == NULL || stream_info == NULL)
	{
		return false;
	}
	
	logtd("Deleted stream from connector. connector_type(%d), application(%s) stream(%s/%u)"
		, app_conn->GetConnectorType()
		, _application_info->GetName().CStr()
		, stream_info->GetName().CStr()
		, stream_info->GetId());


	auto new_stream_info = std::make_shared<StreamInfo>(*stream_info.get());

	// 옵저버에 스트림 삭제를 알림
	for (auto it=_observers.begin(); it!=_observers.end(); ++it)
	{
    	auto observer = *it;

		if( (app_conn->GetConnectorType() == MediaRouteApplicationConnector::CONNECTOR_TYPE_PROVIDER) && 
    		(observer->GetObserverType() == MediaRouteApplicationObserver::OBSERVER_TYPE_TRANSCODER))
    	{
    		observer->OnDeleteStream(new_stream_info);
    	}
    	else if( (app_conn->GetConnectorType() == MediaRouteApplicationConnector::CONNECTOR_TYPE_TRANSCODER) && 
    		(observer->GetObserverType() == MediaRouteApplicationObserver::OBSERVER_TYPE_PUBLISHER))
    	{
    		observer->OnDeleteStream(new_stream_info);
    	}
    }

    std::unique_lock<std::mutex> lock(_mutex);
	_streams.erase(new_stream_info->GetId());
	lock.unlock();

	return true;
}


// 프로바이더에서 인코딩된 데이터가 들어옴
// @from RtmpProvider
// @from TranscoderProvider
bool MediaRouteApplication::OnReceiveBuffer(
	std::shared_ptr<MediaRouteApplicationConnector> app_conn, 
	std::shared_ptr<StreamInfo> stream_info,
	std::unique_ptr<MediaBuffer> buffer)
{
	if(app_conn == NULL || stream_info == NULL)
	{
		return false;
	}
	
	// 스트림 ID에 해당하는 스트림을 탐색
	auto stream_bucket = _streams.find(stream_info->GetId());
	if(stream_bucket == _streams.end())
	{
		logte("cannot find stream from router. appication(%s), stream(%s)", _application_info->GetName().CStr(), stream_info->GetName().CStr() );
		
		return false;
	}

	auto stream = stream_bucket->second;	
	if(stream == nullptr)
	{
		logte("invalid stream bucket");

		return false;
	}

	bool ret = stream->Push(std::move(buffer));

	// TODO(SOULK) : Connector(Provider, Transcoder)에서 수신된 데이터에 대한 정보를 바로 처리하기 위해 버퍼의 Indicator 정보를
	// MainTask에 전달한다. 패킷이 수신되어 처리(재분배)되는 속도가 0.001초 이하의 초저지연으로 동작하나, 효율적인 구조는 
	// 아닌것으로 판단되므로, 향후에 개선이 필요하다
	_indicator.push(std::make_unique<BufferIndicator>(
		stream_info->GetId()
	));

	return ret;
}


void MediaRouteApplication::OnGarbageCollector()
{
	_indicator.push(std::make_unique<BufferIndicator>(
		BUFFFER_INDICATOR_UNIQUEID_GC // 가비지 컬렉터를 실행
	));
}

// 스트림 객제중에 데이터가 수신되지 않은 스트림은 N초후에 자동 삭제를 진행함.
void MediaRouteApplication::GarbageCollector()
{
	time_t curr_time;
	time(&curr_time);

	for (auto it=_streams.begin(); it!=_streams.end(); ++it)
	{
		auto stream = (*it).second;

		int32_t connector_type = stream->GetConnectorType();
		if(connector_type == MediaRouteApplicationConnector::CONNECTOR_TYPE_PROVIDER) 
		{
			double diff_time;

			diff_time = difftime(curr_time, stream->getLastReceivedTime());

			// TODO(soulk) 삭제 Treshold 값은 설정서 읽어오도록 수정해야함.
			// Observer 에게 삭제 메세지를 전달하는 구조도 비효율적임. 레이스컨디션 발생 가능성 있음.
			if(diff_time > 30)
			{
				logti("%s(%u) will delete the stream. diff time(%.0f)", 
					stream->GetStreamInfo()->GetName().CStr(), 
					stream->GetStreamInfo()->GetId(), 
					diff_time );


				for (auto it=_observers.begin(); it!=_observers.end(); ++it)
				{
			    	auto observer = *it;

			    	if(observer->GetObserverType() == MediaRouteApplicationObserver::OBSERVER_TYPE_TRANSCODER)
			    	{
			    		observer->OnDeleteStream(stream->GetStreamInfo());
			    	}
			    }
			
				std::unique_lock<std::mutex> lock(_mutex);
				_streams.erase(stream->GetStreamInfo()->GetId());
				lock.unlock();

				// 비효율적임
				it=_streams.begin();
			}
			
		}
	}
}

// Stream 객체 에에 있는 패킷을 Application Observer에 전달한다
// TODO: 이 구조에서 Segment Fault 문제가 발생함
// TODO: 패킷을 지연없이 전달하기위해 Application당 스레드를 생성하였음.
void MediaRouteApplication::MainTask()
{
	while(!_kill_flag) 
	{
		auto indicator = _indicator.pop_unique();
		if(indicator == nullptr)
		{
			logte("invalid indicator");
			continue;
		}

		// GC 수행
		if(indicator->_stream_id == BUFFFER_INDICATOR_UNIQUEID_GC)
		{
			GarbageCollector();
			continue;
		}

		// logtd("indicator : %u", indicator->_stream_id);

		auto stream = _streams[indicator->_stream_id];

		auto cur_buf = stream->Pop();
		if(cur_buf)
    	{
			int32_t connector_type = stream->GetConnectorType();

			auto stream_info = stream->GetStreamInfo();

			// Find Media Track
			auto media_track = stream_info->GetTrack(cur_buf->GetTrackId());

			// Observer(Publisher or Transcoder)에게 MediaBuffer를 전달함.
			for (auto it_observe=_observers.begin(); it_observe!=_observers.end(); ++it_observe)
			{
		    	auto observer = *it_observe;

		    	// 프로바이터에서 전달받은 스트림은 트랜스코더로 넘김
		    	if((connector_type == MediaRouteApplicationConnector::CONNECTOR_TYPE_PROVIDER) 
		    		&& (observer->GetObserverType() == MediaRouteApplicationObserver::OBSERVER_TYPE_TRANSCODER) )
		    	{
			    	// TODO(soulk): Application Observer의 타입에 따라서 호출하는 함수를 다르게 한다
			    	// TODO(soulk) : MediaBuffer clone 기능을 만든다.
			    	auto media_buffer_clone = std::make_unique<MediaBuffer>(
			    		cur_buf->GetMediaType(), 
			    		cur_buf->GetTrackId(), 
			    		cur_buf->GetBuffer(), 
			    		cur_buf->GetDataSize(),
			    		cur_buf->GetPts());

			    	observer->OnSendFrame(
			    		stream_info, 
			    		std::move(media_buffer_clone)
			    	);
		    	}

		    	// 트랜스코더에서 전달받은 스트림은 퍼블리셔로 넘김
				else if((connector_type == MediaRouteApplicationConnector::CONNECTOR_TYPE_TRANSCODER) 
					&& (observer->GetObserverType() == MediaRouteApplicationObserver::OBSERVER_TYPE_PUBLISHER) )
		    	{

// TODO(SOULK): OnSendVideoFrame이라는 특정 함수가 아닌 모든 미디어 타입에 대한 전송이 가능한 OnSendFrame 함수로 변경을 해야한다
#if 1
		    		if(cur_buf->GetMediaType() == MediaType::MEDIA_TYPE_VIDEO)
		    		{
		    			// TODO: 공동 구조체로 변경해야함.
		    			uint8_t* buffer = new uint8_t[cur_buf->GetBufferSize()];
		    			memcpy(buffer, cur_buf->GetBuffer(), cur_buf->GetBufferSize());

			    		auto encoded_frame = std::make_unique<EncodedFrame>(buffer, cur_buf->GetBufferSize(), 0);
			    		encoded_frame->_encodedWidth = cur_buf->_width;
			    		encoded_frame->_encodedHeight = cur_buf->_height;
			    		encoded_frame->_frameType = (FrameType)(cur_buf->_flags==1)?kVideoFrameKey:kVideoFrameDelta;

			    		// TODO : Publisher에서 Timestamp를 90000Hz로 변경하는 코드를 넣어야함. 직지금은 임시로 넣음.
			    		encoded_frame->_timeStamp = (uint32_t)((double)cur_buf->GetPts() / (double)1000000 * (double)90000);
						// encoded_frame->_timeStamp = (uint32_t)cur_buf->GetPts();
	
			    		// SDP의 Timebase가 90000이라서 읨의로 수정해줌. 
			    		// encoded_frame->_timeStamp = (uint32_t)((double)cur_buf->GetPts() * (double)0.09);
			    		
			    		auto codec_info = std::make_unique<CodecSpecificInfo>();
			    		
			    		codec_info->codecType = kVideoCodecVP8;
			    		codec_info->codecSpecific.generic.simulcast_idx = -1;
			    		codec_info->codecSpecific.VP8.pictureId = -1;
			    		codec_info->codecSpecific.VP8.nonReference = false;
			    		codec_info->codecSpecific.VP8.simulcastIdx = 0;
			    		codec_info->codecSpecific.VP8.temporalIdx = 0;
			    		codec_info->codecSpecific.VP8.layerSync = false;
			    		codec_info->codecSpecific.VP8.tl0PicIdx = 0;
			    		codec_info->codecSpecific.VP8.keyIdx = 0;

			    		auto fragmentation = std::make_unique<FragmentationHeader>();
			    		
			    		// logtd("send to publisher (1000k cr):%u, (90k cr):%u", cur_buf->GetPts(), encoded_frame->_timeStamp);

			    		
			    		observer->OnSendVideoFrame(
			    			stream_info, 
			    			media_track,
			    			std::move(encoded_frame),
			    			std::move(codec_info),
			    			std::move(fragmentation));
						
			    	}
#endif
		    	}
		    }
		}
	}
}