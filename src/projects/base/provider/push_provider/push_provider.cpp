
#include "push_provider.h"
#include "push_provider_private.h"

namespace pvd
{
	PushProvider::PushProvider(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router)
		: Provider(server_config, router)
	{
	}

	PushProvider::~PushProvider()
	{

	}

    bool PushProvider::Start()
    {
        return Provider::Start();
    }

	bool PushProvider::Stop()
    {
        return Provider::Stop();
    }

	bool PushProvider::AssignAnonymousStream(std::shared_ptr<PushProviderStream> &stream, ov::String app_name)
	{
		auto application = std::dynamic_pointer_cast<PushProviderApplication>(GetApplicationByName(app_name));
		if(application == nullptr)
		{
			logte("Cannot find %s application to assign the anonymous stream", app_name.CStr());
			return false;
		}

		// Remove stream from provider's stream motor


		// Add to application 
		application->AttachStream(stream);

		return true;
	}

	// Case of unknown app/stream at initial connection
	bool PushProvider::CreateServer(PushProvider::SingallingProtocol protocol, const ov::SocketAddress &address)
	{
		CreateServer(protocol, address, "", "");
		return true;
	}

	// Case where server address and app/stream are mapped
	// It can be called multiple times 
	bool PushProvider::CreateServer(PushProvider::SingallingProtocol protocol, const ov::SocketAddress &address, ov::String app_name, ov::String stream_name)
	{
		auto server_sock = std::make_shared<PushProviderServerSocket>(app_name, stream_name);
		
		// TCP, SRT
			// Initialize socket
			// Add to epoll
			// If accept is not working, turn the AcceptThread on

		// UDP
			// Initialize socket
			// If a name is specified
				// Application::CreateStream
			// else 
				// Make anonymous stream and add to an anonymous stream motor

		return true;
	}

	bool PushProvider::StopAllServers()
	{

		return true;
	}


	void PushProvider::AcceptThread()
	{
		// thread for server to accept clients
		// - epoll 
		// - client = accept()
		// If a name is specified
			// Application::CreateStream
		// else 
			// Make anonymous stream and add to an anonymous stream motor
	}
}