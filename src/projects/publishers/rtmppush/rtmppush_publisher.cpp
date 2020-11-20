#include "rtmppush_private.h"
#include "rtmppush_publisher.h"

#define UNUSED(expr) do { (void)(expr); } while (0)

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

void RtmpPushPublisher::StartSession(std::shared_ptr<RtmpPushSession> session)
{
	// Check the status of the session.
	auto session_state = session->GetState();

	switch(session_state)
	{
		// State of disconnected and ready to connect
		case pub::Session::SessionState::Ready:
			session->Start();
		break;
		case pub::Session::SessionState::Stopped:
			session->Start();
		break;
		// State of Recording
		case pub::Session::SessionState::Started:
			[[fallthrough]];
		// State of Stopping
		case pub::Session::SessionState::Stopping:
			[[fallthrough]];
		// State of Record failed
		case pub::Session::SessionState::Error:
			[[fallthrough]];
	}

	auto next_session_state = session->GetState();
	if(session_state != next_session_state)
	{
		logtd("Changed State. State(%d - %d)", session_state, next_session_state);		
	}	
}

void RtmpPushPublisher::StopSession(std::shared_ptr<RtmpPushSession> session)
{
	auto session_state = session->GetState();

	switch(session_state)
	{
		case pub::Session::SessionState::Started:
			session->Stop();
			break;
		case pub::Session::SessionState::Ready:
			[[fallthrough]];
		case pub::Session::SessionState::Stopping:
			[[fallthrough]];
		case pub::Session::SessionState::Stopped:
			[[fallthrough]];
		case pub::Session::SessionState::Error:
			[[fallthrough]];
	}

	auto next_session_state = session->GetState();
	if(session_state != next_session_state)
	{
		logtd("Changed State. State(%d - %d)", session_state, next_session_state);		
	}	
}


void RtmpPushPublisher::SessionController()
{
	std::shared_lock<std::shared_mutex> lock(_userdata_sets_mutex);

	auto userdata_sets = _userdata_sets.GetUserdataSets();
	for ( auto& [ key, userdata ] : userdata_sets )
	{
		UNUSED(key);

		// Find a session related to Userdata.
		auto vhost_app_name = info::VHostAppName(userdata->GetVhost(), userdata->GetApplication());
		auto stream = std::static_pointer_cast<RtmpPushStream>(GetStream(vhost_app_name, userdata->GetStreamName()));
		if(stream != nullptr)
		{
			// If there is no session, create a new file(record) session.
			auto session = std::static_pointer_cast<RtmpPushSession>(stream->GetSession(userdata->GetSessionId()));
			if (session == nullptr)
			{
				session = stream->CreateSession();
				if(session == nullptr)
				{
					logte("Could not create session");
					continue;
				}
				userdata->SetSessionId(session->GetId());

				session->SetPush(userdata);
			}

			if(userdata->GetEnable() == true && userdata->GetRemove() == false)
			{
				StartSession(session);
			}		

			if(userdata->GetEnable() == false || userdata->GetRemove() == true)
			{
				StopSession(session);
			}
		}
		else
		{
			userdata->SetState(info::Push::PushState::Ready);
		}

		if(userdata->GetRemove() == true)
		{
			logtd("Remove userdata of file publiser. id(%s)", userdata->GetId().CStr());

			if(stream != nullptr && userdata->GetSessionId() != 0)
				stream->DeleteSession(userdata->GetSessionId());

			_userdata_sets.DeleteByKey(userdata->GetId());
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

		usleep(1000);
	}
}

std::shared_ptr<ov::Error> RtmpPushPublisher::PushStart(const info::VHostAppName &vhost_app_name, 
													  const std::shared_ptr<info::Push> &push)
{
	std::lock_guard<std::shared_mutex> lock(_userdata_sets_mutex);	

	if(_userdata_sets.GetByKey(push->GetId()) != nullptr)
	{
		return ov::Error::CreateError(PushPublisherErrorCode::Failure, 
			"Duplicate identification Code");		
	}



	push->SetEnable(true);
	push->SetRemove(false);

	_userdata_sets.Set(push->GetId(), push);

	return ov::Error::CreateError(PushPublisherErrorCode::Success, 
		"Request completed");
}

std::shared_ptr<ov::Error> RtmpPushPublisher::PushStop(const info::VHostAppName &vhost_app_name, 
												     const std::shared_ptr<info::Push> &push)
{
	std::lock_guard<std::shared_mutex> lock(_userdata_sets_mutex);	

	auto userdata = _userdata_sets.GetByKey(push->GetId());
	if(userdata == nullptr)
	{
		return ov::Error::CreateError(PushPublisherErrorCode::Failure, 
			"identification code does not exist");	
	}

	userdata->SetEnable(false);
	userdata->SetRemove(true);

	return ov::Error::CreateError(PushPublisherErrorCode::Success, 
		"Request complted");
}

std::shared_ptr<ov::Error> RtmpPushPublisher::GetPushes(const info::VHostAppName &vhost_app_name, 
											         std::vector<std::shared_ptr<info::Push>> &push_list)
{
	std::lock_guard<std::shared_mutex> lock(_userdata_sets_mutex);	

	auto userdata_sets = _userdata_sets.GetUserdataSets();
	for (  auto& [ key, userdata ] : userdata_sets )
	{
		UNUSED(key);

		push_list.push_back(userdata);
	}

	return ov::Error::CreateError(PushPublisherErrorCode::Success, 
		"Look up the push list");
}


