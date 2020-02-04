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

#define CLIENT_SOCKET_SEND_TIMEOUT (60 * 1000)
#define CLIENT_SOCKET_SEND_QUEUE_TIMEOUT (100)

namespace ov
{
	ClientSocket::ClientSocket(ServerSocket *server_socket)
		: Socket(),

		  _server_socket(server_socket)
	{
		OV_ASSERT2(_server_socket != nullptr);

		StartSendThread();
	}

	ClientSocket::ClientSocket(ServerSocket *server_socket, SocketWrapper socket, const SocketAddress &remote_address)
		: Socket(socket, remote_address),

		  _server_socket(server_socket)
	{
		OV_ASSERT2(_server_socket != nullptr);

		MakeNonBlocking();

		StartSendThread();
	}

	ClientSocket::~ClientSocket()
	{
		OV_ASSERT(_stop_send_thread, "The thread must not be running");

		if (_stop_send_thread == false)
		{
			StopSendThread(false);
		}

		if (_send_thread.joinable())
		{
			_send_thread.join();
		}
	}

	bool ClientSocket::StartSendThread()
	{
		if (_stop_send_thread == false)
		{
			// Thread is already running
			OV_ASSERT2(_stop_send_thread);
			return false;
		}

		_stop_send_thread = false;
		_send_thread = std::thread(std::bind(&ClientSocket::SendThread, this));

		return true;
	}

	bool ClientSocket::StopSendThread(bool send_remained_data)
	{
		if (_stop_send_thread)
		{
			// Thread is not running
			OV_ASSERT2(_stop_send_thread == false);
			return false;
		}

		_send_remained_data = send_remained_data;

		if (send_remained_data == false)
		{
			_send_queue.Clear();
		}

		// Indicates that the thread should be stopped
		_stop_send_thread = true;

		return true;
	}

	void ClientSocket::SendThread()
	{
		SendItem send_item;

		while (true)
		{
			logtd("[%p] [#%d] Trying to dequeue (%d)...", this, _socket.GetSocket(), _stop_send_thread);

			if (_send_queue.Dequeue(&send_item, CLIENT_SOCKET_SEND_QUEUE_TIMEOUT) == false)
			{
				// There is no item to dequeue

				if (_stop_send_thread)
				{
					// StopSendThread() is called and there is no data to send
					break;
				}
				else
				{
					// Timed out - StopSendThread() is not called
					continue;
				}
			}
			else
			{
				if (_stop_send_thread)
				{
					// StopSendThread() is called, but there are data to send
					OV_ASSERT2(_send_remained_data);
				}
				else
				{
					// StopSendThread() is not called
				}
			}

			// An item is dequeued successfully
			bool succeeded = false;
			bool is_expired = false;
			auto data = send_item.data->GetDataAs<uint8_t>();
			auto remained = send_item.data->GetLength();
			size_t total_sent_bytes = 0ULL;

			while ((_stop_send_thread == false) || _send_remained_data)
			{
				// Wait for transmission up to CLIENT_SOCKET_SEND_TIMEOUT
				is_expired = send_item.IsExpired(CLIENT_SOCKET_SEND_TIMEOUT);

				if (is_expired)
				{
					logtw("[%p] [#%d] Expired (%zu bytes sent)", this, _socket.GetSocket(), total_sent_bytes);
					break;
				}

				auto sent_bytes = SendInternal(data, remained);

				if (sent_bytes < 0)
				{
					// An error occurred
					break;
				}

				OV_ASSERT2(static_cast<ssize_t>(remained) >= sent_bytes);

				remained -= sent_bytes;
				data += sent_bytes;
				total_sent_bytes += sent_bytes;

				if (remained == 0)
				{
					// All data are sent
					succeeded = true;
					break;
				}
			}

			if (is_expired)
			{
				logtd("[%p] [#%d] Could not send data (timed out)", this, _socket.GetSocket());
				break;
			}

			if (succeeded == false)
			{
				// If there was a error, stop the thread
				logtd("[%p] [#%d] Some error is occurred", this, _socket.GetSocket());
				break;
			}
		}

		_stop_send_thread = true;

		Socket::CloseInternal();
	}

	ssize_t ClientSocket::Send(const std::shared_ptr<const Data> &data)
	{
		if (_stop_send_thread == false)
		{
			logtd("[%p] [#%d] Trying to enqueue data: %zu...", this, _socket.GetSocket(), data->GetLength());
			_send_queue.Enqueue(data);
			logtd("[%p] [#%d] Enqueued", this, _socket.GetSocket());
			return data->GetLength();
		}

		// Send thread is stopped
		return -1LL;
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

	bool ClientSocket::Close(bool wait_for_send)
	{
		if (GetState() != SocketState::Closed)
		{
			// 1) ServerSocket::DisconnectClient()
			// 2) ClientSocket::CloseInternal()
			// 3) ClientSocket::StopSendThread();
			return _server_socket->DisconnectClient(this->GetSharedPtrAs<ClientSocket>(), SocketConnectionState::Disconnected);
		}

		return true;
	}

	bool ClientSocket::CloseInternal()
	{
		// Socket::CloseInternal() will be called in SendThread() later
		return StopSendThread(true);
	}

	String ClientSocket::ToString() const
	{
		return Socket::ToString("ClientSocket");
	}
}  // namespace ov
