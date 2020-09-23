//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "socket.h"

namespace ov
{
	// 일반적으로 사용되는 소켓 (server에서 생성한 client socket)
	class ClientSocket : public Socket
	{
	public:
		friend class ServerSocket;

		ClientSocket(ServerSocket *server_socket, SocketWrapper socket, const SocketAddress &remote_address);

		~ClientSocket() override;

		bool PrepareSocketOptions();

		// 데이터 송신
		ssize_t Send(const std::shared_ptr<const Data> &data) override;
		ssize_t Send(const void *data, size_t length) override;

		ssize_t Send(const ov::String &string, bool include_null_char = false);

		template <typename T>
		bool Send(const T *data)
		{
			ssize_t sent = Send(data, sizeof(T));
			bool result = (sent == sizeof(T));

			OV_ASSERT(sent == sizeof(T), "Could not send data: sent: %zu, expected: %zu", sent, sizeof(T));

			return result;
		}

		using Socket::Connect;
		using Socket::Recv;
		using Socket::Send;

		bool Close() override;

		using Socket::GetState;

		String ToString() const override;

	protected:
		struct DispatchCommand
		{
			enum class Type
			{
				Unknown,
				SendData,
				Close
			};

			DispatchCommand(const std::shared_ptr<const ov::Data> &data)
				: type(Type::SendData),
				  data(data),
				  enqueued_time(std::chrono::system_clock::now())
			{
			}

			DispatchCommand(Type type)
				: type(type),
				  enqueued_time(std::chrono::system_clock::now())
			{
			}

			bool IsExpired(int millisecond_time) const
			{
				auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - enqueued_time);

				return (delta.count() >= millisecond_time);
			}

			Type type = Type::Unknown;
			std::shared_ptr<const ov::Data> data;
			std::chrono::time_point<std::chrono::system_clock> enqueued_time;
		};

		bool StartDispatchThread();
		bool StopDispatchThread(bool stop_immediately);

		bool SendAsync(const ClientSocket::DispatchCommand &send_item);
		void DispatchThread();

		bool CloseInternal() override;

		ServerSocket *_server_socket = nullptr;

		// TODO(dimiden): If the receiver does not read the data, EAGAIN may occur continuously.
		//                At this time, it can be blocked, so it creates a thread and processes it.
		//                I know it's inefficient to use one thread per Socket, and I'll create a send pool later.
		std::thread _send_thread;
		ov::Queue<DispatchCommand> _dispatch_queue{nullptr, 100};
		bool _is_thread_running = false;

		std::shared_ptr<ClientSocket> _instance;
	};
}  // namespace ov
