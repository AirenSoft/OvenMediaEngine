//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "./socket_datastructure.h"
#include "./socket_utilities.h"

#include "../ovlibrary/string.h"

namespace ov
{
	/// IPv4/IPv6 주소 및 포트를 나타내는 클래스
	class SocketAddress
	{
	public:
		// 유효하지 않은 socket address
		SocketAddress();

		// any ip (binding 할 때 사용)
		explicit SocketAddress(uint16_t port);

		// <host>:<port> 형태의 문자열
		// 예) 1.1.1.1:1234, 192.168.0.1:3030
		explicit SocketAddress(const ov::String &host_port);

		explicit SocketAddress(const ov::String &hostname, uint16_t port);
		explicit SocketAddress(const char *hostname, uint16_t port);
		// IPv4 주소
		explicit SocketAddress(const sockaddr_in &address);
		// IPv6 주소
		explicit SocketAddress(const sockaddr_in6 &address);
		explicit SocketAddress(const sockaddr_storage &address);

		// 복사 생성자
		SocketAddress(const SocketAddress &address);
		// 이동 생성자
		SocketAddress(SocketAddress &&address) noexcept;

		~SocketAddress();

		SocketAddress &operator =(const SocketAddress &address) noexcept;

		bool operator ==(const SocketAddress &socket) const;
		bool operator !=(const SocketAddress &socket) const;
		bool operator <(const SocketAddress &socket) const;
		bool operator >(const SocketAddress &socket) const;

		void SetFamily(SocketFamily family)
		{
			_address_storage.ss_family = static_cast<sa_family_t>(family);
		}

		SocketFamily GetFamily() const
		{
			return static_cast<SocketFamily>(_address_storage.ss_family);
		}

		bool SetHostname(const char *hostname);
		ov::String GetHostname() const noexcept;
		ov::String GetIpAddress() const noexcept;

		bool SetPort(uint16_t port);
		uint16_t Port() const noexcept;

		const sockaddr *Address() const noexcept;
		const sockaddr_in *AddressForIPv4() const noexcept;
		const sockaddr_in6 *AddressForIPv6() const noexcept;

		const in_addr *AddrInForIPv4() const noexcept;
		const in6_addr *AddrInForIPv6() const noexcept;

		inline operator in_addr *() noexcept // NOLINT
		{
			return &(_address_ipv4->sin_addr);
		}

		inline operator in6_addr *() noexcept // NOLINT
		{
			return &(_address_ipv6->sin6_addr);
		}

		void *ToAddrIn()
		{
			switch(_address_storage.ss_family)
			{
				case AF_INET:
					return &(_address_ipv4->sin_addr);

				case AF_INET6:
					return &(_address_ipv6->sin6_addr);

				default:
					return nullptr;
			}
		}

		socklen_t AddressLength() const noexcept;

		void UpdateIPAddress();

		ov::String ToString() const noexcept;

	protected:
		sockaddr_storage _address_storage;
		// _address_storage내 데이터를 가리키는 포인터 (실제로 메모리가 할당되어 있거나 하지는 않음)
		sockaddr_in *_address_ipv4;
		sockaddr_in6 *_address_ipv6;

		ov::String _hostname;
		ov::String _ip_address;
	};
}