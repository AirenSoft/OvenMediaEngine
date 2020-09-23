//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "client_socket.h"

#include "server_socket.h"
#include "socket_private.h"

// If no packet is sent during this time, the connection is disconnected
#define CLIENT_SOCKET_SEND_TIMEOUT (60 * 1000)

namespace ov
{
	ClientSocket::ClientSocket(ServerSocket *server_socket, SocketWrapper socket, const SocketAddress &remote_address)
		: Socket(socket, remote_address),

		  _server_socket(server_socket)
	{
		OV_ASSERT2(_server_socket != nullptr);

		_local_address = (server_socket != nullptr) ? server_socket->GetLocalAddress() : nullptr;

		// TODO(dimiden): (ClientSocketBlocking) Currently, 1 thread is created per socket, so blocking mode is used until implementing code that handles EAGAIN
		// MakeNonBlocking();

		ov::String queue_name;

		queue_name.Format("ClientSocket #%d %s", socket.GetSocket(), remote_address.ToString().CStr());

		if (server_socket != nullptr)
		{
			queue_name.AppendFormat(" (of #%d)", server_socket->GetSocket().GetSocket());
		}

		_dispatch_queue.SetAlias(queue_name);
	}

	ClientSocket::~ClientSocket()
	{
		OV_ASSERT(_is_thread_running, "The thread must not be running");

		if (_is_thread_running)
		{
			StopDispatchThread(true);
		}
	}

	bool ClientSocket::PrepareSocketOptions()
	{
		return
			// Enable TCP keep-alive
			SetSockOpt<int>(SOL_SOCKET, SO_KEEPALIVE, 1) &&
			// Wait XX seconds before starting to determine that the connection is alive
			SetSockOpt<int>(SOL_TCP, TCP_KEEPIDLE, 30) &&
			// Period of sending probe packet to determine keep alive
			SetSockOpt<int>(SOL_TCP, TCP_KEEPINTVL, 10) &&
			// Number of times to probe
			SetSockOpt<int>(SOL_TCP, TCP_KEEPCNT, 3);
	}

	bool ClientSocket::StartDispatchThread()
	{
		if (_is_thread_running)
		{
			// Thread is already running
			OV_ASSERT2(_is_thread_running == false);
			return false;
		}

		_is_thread_running = true;
		_force_stop = false;

		// To keep the shared_ptr from being released while DispatchThread() is called
		auto that = GetSharedPtrAs<ClientSocket>();
		_send_thread = std::thread(std::bind(&ClientSocket::DispatchThread, that));
		_send_thread.detach();

		return true;
	}

	bool ClientSocket::StopDispatchThread(bool stop_immediately)
	{
		if (_is_thread_running == false)
		{
			// Thread is not running
			OV_ASSERT2(_is_thread_running);
			return false;
		}

		// Enqueue close command
		_dispatch_queue.Enqueue(DispatchCommand::Type::Close);

		if (stop_immediately)
		{
			// Signal stop command
			_force_stop = true;
			_dispatch_queue.Stop();
		}

		return true;
	}

	bool ClientSocket::SendAsync(const ClientSocket::DispatchCommand &send_item)
	{
		// An item is dequeued successfully
		auto data = send_item.data->GetDataAs<uint8_t>();
		auto remained = send_item.data->GetLength();
		size_t total_sent_bytes = 0ULL;

		while (_force_stop == false)
		{
			// Wait for transmission up to CLIENT_SOCKET_SEND_TIMEOUT
			// TODO(dimiden): (ClientSocketBlocking) Temporarily comment while processing as blocking
			// if (send_item.IsExpired(CLIENT_SOCKET_SEND_TIMEOUT))
			// {
			// 	logtw("[%p] [#%d] Expired (%zu bytes sent)", this, _socket.GetSocket(), total_sent_bytes);
			// 	return false;
			// }

			auto sent_bytes = SendInternal(data, remained);

			if (sent_bytes < 0)
			{
				// An error occurred
				logtw("[%p] [#%d] Could not send data (%zu bytes sent)", this, _socket.GetSocket(), total_sent_bytes);
				return false;
			}

			OV_ASSERT2(static_cast<ssize_t>(remained) >= sent_bytes);

			remained -= sent_bytes;
			data += sent_bytes;
			total_sent_bytes += sent_bytes;

			if (remained == 0)
			{
				// All data are sent
				break;
			}
		}

		return true;
	}

	void ClientSocket::DispatchThread()
	{
		bool stop = false;

		while (stop == false)
		{
			auto item = _dispatch_queue.Dequeue();

			if (item.has_value() == false)
			{
				if (_force_stop)
				{
					break;
				}

				// Timed out
				continue;
			}

			auto &send_item = item.value();

			switch (send_item.type)
			{
				case DispatchCommand::Type::Unknown:
					OV_ASSERT2(false);
					break;

				case DispatchCommand::Type::SendData:
					if (SendAsync(send_item) == false)
					{
						// Item is expired / Could not send data

						// Disconnect the client
						stop = true;
					}

					break;

				case DispatchCommand::Type::Close:
					logtd("[%p] [#%d] Trying to close the socket", this, _socket.GetSocket());
					stop = true;
					break;
			}
		}

		[[maybe_unused]] auto sock = _socket.GetSocket();

		Socket::CloseInternal();

		logtd("[%p] [#%d] Thread is stopped, queue: %zu", this, sock, _dispatch_queue.Size());
	}

	ssize_t ClientSocket::Send(const std::shared_ptr<const Data> &data)
	{
		logtd("[%p] [#%d] Trying to enqueue data: %zu...", this, _socket.GetSocket(), data->GetLength());
		_dispatch_queue.Enqueue(data);
		logtd("[%p] [#%d] Enqueued", this, _socket.GetSocket());
		return data->GetLength();
	}

	ssize_t ClientSocket::Send(const void *data, size_t length)
	{
		// TODO(dimiden): Consider sending a copy and storing it as a deep copy only if it fails to send
		return Send(std::make_shared<const ov::Data>(data, length));
	}

	ssize_t ClientSocket::Send(const ov::String &string, bool include_null_char)
	{
		return Send(string.ToData(include_null_char));
	}

	bool ClientSocket::Close()
	{
		if (GetState() != SocketState::Closed)
		{
			// 1) ServerSocket::DisconnectClient();
			// 2) ClientSocket::CloseInternal();
			// 3) ClientSocket::StopSendThread();
			return _server_socket->DisconnectClient(this->GetSharedPtrAs<ClientSocket>(), SocketConnectionState::Disconnect);
		}

		return true;
	}

	bool ClientSocket::CloseInternal()
	{
		// Socket::CloseInternal() will be called in SendThread() later
		return StopDispatchThread(false);
	}

	String ClientSocket::ToString() const
	{
		return Socket::ToString("ClientSocket");
	}
}  // namespace ov
