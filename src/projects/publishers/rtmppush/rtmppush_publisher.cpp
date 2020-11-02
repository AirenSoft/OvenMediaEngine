#include "rtmppush_private.h"
#include "rtmppush_publisher.h"

std::shared_ptr<RtmpPushPublisher> RtmpPushPublisher::Create(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router)
{
	auto obj = std::make_shared<RtmpPushPublisher>(server_config, router);

	if (!obj->Start())
	{
		logte("An error occurred while creating RtmpPushPublisher");
		return nullptr;
	}

	return obj;
}

RtmpPushPublisher::RtmpPushPublisher(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router)
		: Publisher(server_config, router)
{
	logtd("RtmpPushPublisher has been create");
}

RtmpPushPublisher::~RtmpPushPublisher()
{
	logtd("RtmpPushPublisher has been terminated finally");
}

bool RtmpPushPublisher::Start()
{
#if 0
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Create Dummy Userdata
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	_userdata_sets.Set("id_001", RtmpPushUserdata::Create(true, "default", "app", "stream", "rtmp://127.0.0.1:1935/app", "stream_clone"));
	// _userdata_sets.Set("id_001", RtmpPushUserdata::Create(true, "default", "app", "stream", "rtmp://222.239.124.20:1935/lscs", "input_41_146309578"));
	// _userdata_sets.Set("id_002", RtmpPushUserdata::Create(true, "default", "app", "stream", "rtmp://127.0.0.1/app", "stream2"));
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif

	_stop_thread_flag = false;
	_worker_thread = std::thread(&RtmpPushPublisher::WorkerThread, this);

	return Publisher::Start();
}

bool RtmpPushPublisher::Stop()
{
	_stop_thread_flag = true;

	if (_worker_thread.joinable())
	{
		_worker_thread.join();
	}

	return Publisher::Stop();
}

std::shared_ptr<pub::Application> RtmpPushPublisher::OnCreatePublisherApplication(const info::Application &application_info)
{
	return RtmpPushApplication::Create(RtmpPushPublisher::GetSharedPtrAs<pub::Publisher>(), application_info);
}

bool RtmpPushPublisher::OnDeletePublisherApplication(const std::shared_ptr<pub::Application> &application)
{
	auto rtmppush_application = std::static_pointer_cast<RtmpPushApplication>(application);
	if(rtmppush_application == nullptr)
	{
		logte("Could not found file application. app:%s", rtmppush_application->GetName().CStr());
		return false;
	}

	// Applications and child streams must be terminated.

	return true;
}

void RtmpPushPublisher::SessionController()
{
	std::shared_lock<std::shared_mutex> lock(_userdata_sets_mutex);

	// Session Management by Userdata
	for(uint32_t i=0 ; i<_userdata_sets.GetCount() ; i++)
	{
		auto userdata = _userdata_sets.GetAt(i);
		if(userdata == nullptr)
			continue;

		// logtd("index(%d) info(%s)", i, userdata->GetInfoString().CStr());

		auto vhost_app_name = info::VHostAppName(userdata->GetVhost(), userdata->GetApplication());
		auto stream_name = userdata->GetStream();

		if(userdata->GetRemove() == true)
		{
			continue;
		}

		// Find a session related to Userdata.
		auto stream = std::static_pointer_cast<RtmpPushStream>(GetStream(vhost_app_name, stream_name));
		if(stream == nullptr)
		{
			logtw("There is no such stream. (%s/%s)", vhost_app_name.CStr(), stream_name.CStr());
			continue;
		}

		// If there is no session, create a new session.
		auto session = std::static_pointer_cast<RtmpPushSession>(stream->GetSession(userdata->GetSessionId()));
		if (session == nullptr)
		{
			session = stream->CreateSession(userdata->GetTargetUrl(), userdata->GetTargetStreamKey());
			if(session == nullptr)
			{
				logte("Could not create session");
				continue;
			}

			session->SetUrl(userdata->GetTargetUrl());
			session->SetStreamKey(userdata->GetTargetStreamKey());

			userdata->SetSessionId(session->GetId());
		}

		// Check the status of the session.
		auto session_state = session->GetState();
		
		switch(session_state)
		{
			case pub::Session::SessionState::Ready:
				// State of disconnected and ready to connect
				if(userdata->GetEnable() == true)
					session->Start();
			break;
			case pub::Session::SessionState::Started:
				// State of connecting, connected
				if(userdata->GetEnable() == false)
					session->Stop();
			break;
			case pub::Session::SessionState::Stopping:
				// Disconnecting
			break;
			case pub::Session::SessionState::Stopped:
				// Disconnected 
				if(userdata->GetEnable() == true)
					session->Start();			
			break;
			case pub::Session::SessionState::Error:
			 	// retry to connect
				if(userdata->GetEnable() == true)
				{
					session->Start();
				}
			break;
		}

		auto new_sesion_state = session->GetState();
		if(session_state != new_sesion_state)
		{
			logtd("Changed State. SessionId(%d),  State(%d - %d)", i, session_state, new_sesion_state);		
		}
	}

	// Garbage Collection : Delete sessions that are not in Userdata.
	//  - Look up all sessions. Delete sessions that are not in the UserdataSet.
	for(uint32_t i=0; i<GetApplicationCount() ; i++)
	{
		auto application = std::static_pointer_cast<RtmpPushApplication>(GetApplicationAt(i));
		if(application == nullptr)
			continue;

		for(uint32_t j=0; j<application->GetStreamCount() ; j++)
		{
			auto stream = std::static_pointer_cast<RtmpPushStream>(application->GetStreamAt(j));
			if(stream == nullptr)
				continue;


			for(uint32_t k=0; k<stream->GetSessionCount() ; k++)	
			{
				auto session = std::static_pointer_cast<RtmpPushSession>(stream->GetSessionAt(k));
				if(session == nullptr)
					continue;

				auto userdata = _userdata_sets.GetBySessionId(session->GetId());
				if(userdata == nullptr)
				{
					// Userdata does not have this session. This session needs to be deleted.
					logtw("Userdata does not have this session. This session needs to be deleted. session_id(%d)", session->GetId());
					stream->DeleteSession(session->GetId());

					// If the session is deleted, check again from the beginning.
					k=0;
				}
			}
		}
	}
}

void RtmpPushPublisher::WorkerThread()
{
	ov::StopWatch stat_stop_watch;
	stat_stop_watch.Start();

	while (!_stop_thread_flag)
	{
		if (stat_stop_watch.IsElapsed(500) && stat_stop_watch.Update())
		{
			SessionController();
		}

		// To prevent zombies that eat up system resources.
		usleep(1000);
	}
}

std::shared_ptr<ov::Error> RtmpPushPublisher::HandlePushCreate(const info::VHostAppName &vhost_app_name, ov::String stream_name)
{
	std::lock_guard<std::shared_mutex> lock(_userdata_sets_mutex);

	// Append userdata(target information) to userdata_set

	return ov::Error::CreateError(0, "Success");
}

std::shared_ptr<ov::Error> RtmpPushPublisher::HandlePushUpdate(const info::VHostAppName &vhost_app_name, ov::String stream_name)
{
	std::lock_guard<std::shared_mutex> lock(_userdata_sets_mutex);
	
	// Update userdata(target information) on userdata_set	

	return ov::Error::CreateError(0, "Success");
}

std::shared_ptr<ov::Error> RtmpPushPublisher::HandlePushRead(const info::VHostAppName &vhost_app_name, ov::String stream_name)
{
	std::shared_lock<std::shared_mutex> lock(_userdata_sets_mutex);
	
	// Read userdata(target information) from userdata_set	

	return ov::Error::CreateError(0, "Success");
}

std::shared_ptr<ov::Error> RtmpPushPublisher::HandlePushDelete(const info::VHostAppName &vhost_app_name, ov::String stream_name)
{
	std::unique_lock<std::shared_mutex> lock(_userdata_sets_mutex);

	// Delete userdata(target information) from userdata_set	

	return ov::Error::CreateError(0, "Success");
}

