//==============================================================================
//
//  Provider Base Class
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/common_types.h>
#include <base/media_route/media_route_interface.h>
#include <orchestrator/data_structure.h>

#include <shared_mutex>

namespace pvd
{
	class PullingItem
	{
	public:
		enum class PullingItemState : uint8_t
		{
			PULLING,
			PULLED,
			ERROR
		};

		PullingItem(const ov::String &app_name, const ov::String &stream_name, const std::vector<ov::String> &url_list, off_t offset)
		{
			_app_name = app_name;
			_stream_name = stream_name;
			_url_list = url_list;
			_offset = offset;
		}

		void SetState(PullingItemState state)
		{
			_state = state;
		}

		PullingItemState State()
		{
			return _state;
		}

		void Wait()
		{
			std::shared_lock lock(_mutex);
		}

		void Lock()
		{
			_mutex.lock();
		}

		void Unlock()
		{
			_mutex.unlock();
		}

	private:
		ov::String				_app_name;
		ov::String				_stream_name;
		std::vector<ov::String> _url_list;
		off_t 					_offset;
		PullingItemState		_state = PullingItemState::PULLING;
		std::shared_mutex 		_mutex;
	};

	class Application;
	class Stream;
	// RTMP Server와 같은 모든 Provider는 다음 Interface를 구현하여 MediaRouterInterface에 자신을 등록한다.
	class Provider : public OrchestratorProviderModuleInterface
	{
	public:
		virtual ProviderType GetProviderType() const = 0;
		virtual ProviderStreamDirection GetProviderStreamDirection() const = 0;
		virtual const char* GetProviderName() const= 0;

		virtual bool Start();
		virtual bool Stop();

		// app_name으로 Application을 찾아서 반환한다.
		std::shared_ptr<Application> GetApplicationByName(ov::String app_name);
		std::shared_ptr<Stream> GetStreamByName(ov::String app_name, ov::String stream_name);

		std::shared_ptr<Application> GetApplicationById(info::application_id_t app_id);
		std::shared_ptr<Stream> GetStreamById(info::application_id_t app_id, uint32_t stream_id);

	protected:
		Provider(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router);
		virtual ~Provider();

		const cfg::Server &GetServerConfig() const;

		// For child class
		virtual std::shared_ptr<Application> OnCreateProviderApplication(const info::Application &app_info) = 0;
		virtual bool OnDeleteProviderApplication(const std::shared_ptr<pvd::Application> &application) = 0;

		//--------------------------------------------------------------------
		// Implementation of OrchestratorModuleInterface
		//--------------------------------------------------------------------
		bool OnCreateApplication(const info::Application &app_info) override;
		bool OnDeleteApplication(const info::Application &app_info) override;

		bool LockPullStreamIfNeeded(const info::Application &app_info, const ov::String &stream_name, const std::vector<ov::String> &url_list, off_t offset);
		bool UnlockPullStreamIfNeeded(const info::Application &app_info, const ov::String &stream_name, PullingItem::PullingItemState state);

		std::shared_ptr<pvd::Stream> PullStream(const info::Application &app_info, const ov::String &stream_name, const std::vector<ov::String> &url_list, off_t offset) override;
		bool StopStream(const info::Application &app_info, const std::shared_ptr<pvd::Stream> &stream) override;
		
	private:	
		ov::String		GeneratePullingKey(const ov::String &app_name, const ov::String &stream_name);

		const cfg::Server _server_config;
		
		std::map<info::application_id_t, std::shared_ptr<Application>> _applications;
		std::shared_mutex  _application_map_mutex;

		std::shared_ptr<MediaRouteInterface> _router;
		std::map<ov::String, std::shared_ptr<PullingItem>>	_pulling_table;
		std::mutex 											_pulling_table_mutex;
	};

}  // namespace pvd