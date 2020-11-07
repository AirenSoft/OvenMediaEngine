#include <base/ovlibrary/url.h>
#include "file_private.h"
#include "file_publisher.h"

std::shared_ptr<FilePublisher> FilePublisher::Create(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router)
{
	auto file = std::make_shared<FilePublisher>(server_config, router);

	if (!file->Start())
	{
		logte("An error occurred while creating FilePublisher");
		return nullptr;
	}

	return file;
}

FilePublisher::FilePublisher(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router)
		: Publisher(server_config, router)
{
	logtd("FilePublisher has been create");
}

FilePublisher::~FilePublisher()
{
	logtd("FilePublisher has been terminated finally");
}

bool FilePublisher::Start()
{
	_stop_thread_flag = false;
	_worker_thread = std::thread(&FilePublisher::WorkerThread, this);

	return Publisher::Start();
}

bool FilePublisher::Stop()
{
	_stop_thread_flag = true;

	if (_worker_thread.joinable())
	{
		_worker_thread.join();
	}

	return Publisher::Stop();
}

std::shared_ptr<pub::Application> FilePublisher::OnCreatePublisherApplication(const info::Application &application_info)
{
	return FileApplication::Create(FilePublisher::GetSharedPtrAs<pub::Publisher>(), application_info);
}

bool FilePublisher::OnDeletePublisherApplication(const std::shared_ptr<pub::Application> &application)
{
	auto file_application = std::static_pointer_cast<FileApplication>(application);
	if(file_application == nullptr)
	{
		logte("Could not found file application. app:%s", file_application->GetName().CStr());
		return false;
	}

	return true;
}

void FilePublisher::SessionController()
{
	std::shared_lock<std::shared_mutex> lock(_userdata_sets_mutex);

	// Session management by Userdata
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
		auto stream = std::static_pointer_cast<FileStream>(GetStream(vhost_app_name, stream_name));
		if(stream == nullptr)
		{
			logtw("There is no such stream. (%s/%s)", vhost_app_name.CStr(), stream_name.CStr());
			continue;
		}

		// If there is no session, create a new session.
		auto session = std::static_pointer_cast<FileSession>(stream->GetSession(userdata->GetSessionId()));
		if (session == nullptr)
		{
			session = stream->CreateSession();
			if(session == nullptr)
			{
				logte("Could not create session");
				continue;
			}

			session->SetSelectTracks(userdata->GetSelectTrack());

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
				// Recording
				if(userdata->GetEnable() == false)
					session->Stop();
			break;
			case pub::Session::SessionState::Stopping:
				// stopping
			break;
			case pub::Session::SessionState::Stopped:
				// completed
			break;
			case pub::Session::SessionState::Error:
				// failure
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
		auto application = std::static_pointer_cast<FileApplication>(GetApplicationAt(i));
		if(application == nullptr)
			continue;

		for(uint32_t j=0; j<application->GetStreamCount() ; j++)
		{
			auto stream = std::static_pointer_cast<FileStream>(application->GetStreamAt(j));
			if(stream == nullptr)
				continue;


			for(uint32_t k=0; k<stream->GetSessionCount() ; k++)	
			{
				auto session = std::static_pointer_cast<FileSession>(stream->GetSessionAt(k));
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


void FilePublisher::WorkerThread()
{
	ov::StopWatch stat_stop_watch;
	stat_stop_watch.Start();

	while (!_stop_thread_flag)
	{
		if (stat_stop_watch.IsElapsed(500) && stat_stop_watch.Update())
		{
			SessionController();
		}

		// To prevent zombies that eat up system resources. :)
		usleep(1000);
	}
}

std::shared_ptr<ov::Error> FilePublisher::RecordStart(const info::VHostAppName &vhost_app_name, const std::shared_ptr<info::Record> &record)
{
	std::lock_guard<std::shared_mutex> lock(_userdata_sets_mutex);	

	std::vector<int32_t> dummy_tracks;
	_userdata_sets.Set(record->GetId(), FileUserdata::Create(
		true, 
		vhost_app_name.GetVHostName(), 
		vhost_app_name.GetAppName(), 
		record->GetStream().GetName(), 
		dummy_tracks)
	);

	return ov::Error::CreateError(0, "Success");
}

std::shared_ptr<ov::Error> FilePublisher::RecordStop(const info::VHostAppName &vhost_app_name, const std::shared_ptr<info::Record> &record)
{
	std::lock_guard<std::shared_mutex> lock(_userdata_sets_mutex);	

	auto user_data = _userdata_sets.GetByKey(record->GetId());
	if(user_data == nullptr)
	{
		return ov::Error::CreateError(1, "Could not find record");	
	}

	user_data->SetRemove(true);

	return ov::Error::CreateError(0, "Success");
}

std::shared_ptr<ov::Error> FilePublisher::GetRecords(const info::VHostAppName &vhost_app_name, std::vector<std::shared_ptr<info::Record>> &record_list)
{
	std::lock_guard<std::shared_mutex> lock(_userdata_sets_mutex);	


	return ov::Error::CreateError(0, "Success");
}

