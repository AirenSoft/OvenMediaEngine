//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "socket_address.h"
#include "socket_private.h"

#include <netdb.h>
#include <algorithm>

#include "../ovlibrary/assert.h"
#include "../ovlibrary/byte_ordering.h"
#include "../ovlibrary/ovlibrary.h"

namespace ov
{
	SocketAddress::SocketAddress()
		: _address_ipv4((sockaddr_in *)(&_address_storage)),
		  _address_ipv6((sockaddr_in6 *)(&_address_storage))
	{
		::memset(&_address_storage, 0, sizeof(_address_storage));
		_address_storage.ss_family = AF_UNSPEC;
	}

	SocketAddress::SocketAddress(uint16_t port)
		: SocketAddress(nullptr, port)
	{
		OV_ASSERT2((void *)_address_ipv4 == (void *)(&_address_storage));
		OV_ASSERT2((void *)_address_ipv6 == (void *)(&_address_storage));
	}

	SocketAddress::SocketAddress(const ov::String &host_port)
		: SocketAddress()
	{
		OV_ASSERT2((void *)_address_ipv4 == (void *)(&_address_storage));
		OV_ASSERT2((void *)_address_ipv6 == (void *)(&_address_storage));

		// <host>[:<port>]
		auto tokens = host_port.Split(":");
		bool result = false;

		if(tokens.empty() == false)
		{
			// host
			result = SetHostname(tokens[0]);

			if(tokens.size() >= 2)
			{
				// port
				result = SetPort(Converter::ToUInt16(tokens[1]));
			}
		}

		if(result == false)
		{
			logte("An error occured: %s", host_port.CStr());
			// OV_ASSERT2(result);
		}
	}

	SocketAddress::SocketAddress(const ov::String &hostname, uint16_t port)
		: SocketAddress(hostname.CStr(), port)
	{
		OV_ASSERT2((void *)_address_ipv4 == (void *)(&_address_storage));
		OV_ASSERT2((void *)_address_ipv6 == (void *)(&_address_storage));
	}

	SocketAddress::SocketAddress(const char *hostname, uint16_t port)
		: SocketAddress()
	{
		OV_ASSERT2((void *)_address_ipv4 == (void *)(&_address_storage));
		OV_ASSERT2((void *)_address_ipv6 == (void *)(&_address_storage));

		bool result = SetHostname(hostname);
		result = result && SetPort(port);

		if(result == false)
		{
			logte("An error occured: %s:%d", hostname, port);
			// OV_ASSERT2(result);
		}
	}

	SocketAddress::SocketAddress(const sockaddr_in &addr_in)
		: SocketAddress()
	{
		OV_ASSERT2((void *)_address_ipv4 == (void *)(&_address_storage));
		OV_ASSERT2((void *)_address_ipv6 == (void *)(&_address_storage));

		*_address_ipv4 = addr_in;

		UpdateIPAddress();
	}

	SocketAddress::SocketAddress(const sockaddr_in6 &addr_in)
		: SocketAddress()
	{
		OV_ASSERT2((void *)_address_ipv4 == (void *)(&_address_storage));
		OV_ASSERT2((void *)_address_ipv6 == (void *)(&_address_storage));

		*_address_ipv6 = addr_in;

		UpdateIPAddress();
	}

	SocketAddress::SocketAddress(const sockaddr_storage &address)
		: SocketAddress()
	{
		OV_ASSERT2((void *)_address_ipv4 == (void *)(&_address_storage));
		OV_ASSERT2((void *)_address_ipv6 == (void *)(&_address_storage));

		OV_ASSERT((address.ss_family == AF_INET) || (address.ss_family == AF_INET6), "Unknown address family: %d", address.ss_family);

		_address_storage = address;

		UpdateIPAddress();
	}

	SocketAddress::SocketAddress(const SocketAddress &address)
		: SocketAddress()
	{
		OV_ASSERT2((void *)_address_ipv4 == (void *)(&_address_storage));
		OV_ASSERT2((void *)_address_ipv6 == (void *)(&_address_storage));

		::memcpy(&_address_storage, &(address._address_storage), sizeof(_address_storage));

		_hostname = address._hostname;
		_ip_address = address._ip_address;
	}

	SocketAddress::SocketAddress(SocketAddress &&address) noexcept
		: SocketAddress()
	{
		OV_ASSERT2((void *)_address_ipv4 == (void *)(&_address_storage));
		OV_ASSERT2((void *)_address_ipv6 == (void *)(&_address_storage));

		_address_storage = address._address_storage;

		std::swap(_hostname, address._hostname);
		std::swap(_ip_address, address._ip_address);
	}

	SocketAddress::~SocketAddress()
	{
	}

	SocketAddress &SocketAddress::operator =(const SocketAddress &address) noexcept
	{
		_address_storage = address._address_storage;

		_hostname = address._hostname;
		_ip_address = address._ip_address;

		return *this;
	}

	bool SocketAddress::operator ==(const SocketAddress &socket) const
	{
		return ::memcmp(&_address_storage, &(socket._address_storage), sizeof(_address_storage)) == 0;
	}

	bool SocketAddress::operator !=(const SocketAddress &socket) const
	{
		return !(operator ==(socket));
	}

	bool SocketAddress::operator <(const SocketAddress &socket) const
	{
		// logtd("%s", Dump(_address_storage, "_address_storage", 0, 16).CStr());
		// logtd("%s", Dump(socket._address_storage, "socket._address_storage", 0, 16).CStr());

		if((_address_storage.ss_family == AF_INET) && (socket._address_storage.ss_family == AF_INET6))
		{
			// IPv4 < IPv6
			return true;
		}

		if((_address_storage.ss_family == AF_INET6) && (socket._address_storage.ss_family == AF_INET))
		{
			// IPv4 < IPv6
			return false;
		}

		if(_address_storage.ss_family == socket._address_storage.ss_family)
		{
			int compare_result = 0;

			switch(_address_storage.ss_family)
			{
				case AF_INET:
					compare_result = ::memcmp(&(_address_ipv4->sin_addr), &(socket._address_ipv4->sin_addr), sizeof(_address_ipv4->sin_addr));

					if(compare_result == 0)
					{
						return NetworkToHost16(_address_ipv4->sin_port) < NetworkToHost16(socket._address_ipv4->sin_port);
					}

					return (compare_result < 0);

				case AF_INET6:
					compare_result = ::memcmp(&(_address_ipv6->sin6_addr), &(socket._address_ipv6->sin6_addr), sizeof(_address_ipv6->sin6_addr));

					if(compare_result == 0)
					{
						return NetworkToHost16(_address_ipv6->sin6_port) < NetworkToHost16(socket._address_ipv6->sin6_port);
					}

					return (compare_result < 0);

				default:
					// unknown family
					break;
			}
		}

		return false;
	}

	bool SocketAddress::operator >(const SocketAddress &socket) const
	{
		return (operator !=(socket)) && (operator <(socket) == false);
	}

	bool SocketAddress::SetHostname(const char *hostname)
	{
		// 문자열로 부터 IP를 계산함
		if((hostname == nullptr) || (hostname[0] == '\0') || ((hostname[0] == '*') && (hostname[1] == '\0')))
		{
			// host가 지정되어 있지 않으면 INADDR_ANY 로 설정
			_address_storage.ss_family = AF_INET;

			// 의미는 없지만, endian 맞춤
			_address_ipv4->sin_addr.s_addr = ov::HostToNetwork32(INADDR_ANY);

			_ip_address = "0.0.0.0";

			return true;
		}

		// IPv4로 변환 시도
		if(::inet_pton(AF_INET, hostname, &(_address_ipv4->sin_addr)) != 0)
		{
			// 변환 성공
			_address_storage.ss_family = AF_INET;
		}
		else
		{
			// IPv4로 변환 실패

			// IPv6으로 시도
			if(::inet_pton(AF_INET6, hostname, &(_address_ipv6->sin6_addr)) != 0)
			{
				// 변환 성공
				_address_storage.ss_family = AF_INET6;
			}
			else
			{
				// IPv6으로 변환 실패

				// DNS resolve 시도
				addrinfo hints {};

				::memset(&hints, 0, sizeof(hints));
				hints.ai_family = PF_UNSPEC;
				hints.ai_socktype = SOCK_STREAM;
				hints.ai_flags |= AI_CANONNAME;

				addrinfo *result = nullptr;

				if(::getaddrinfo(hostname, nullptr, nullptr, &result) != 0)
				{
					logte("An error occurred while resolve DNS for host [%s]", hostname);
					return false;
				}

				bool stop = false;

				for(addrinfo *item = result; item != nullptr; item = item->ai_next)
				{
					switch(item->ai_family)
					{
						case AF_INET:
							// IPv4 항목 찾음
							*_address_ipv4 = *((sockaddr_in *)(item->ai_addr));
							_address_storage.ss_family = AF_INET;

							// IPv4 항목은 우선순위가 높아, 찾으면 바로 중단
							stop = true;

							break;

						case AF_INET6:
							// IPv6 항목 찾음

							if(_address_storage.ss_family == AF_INET6)
							{
								// 이미 IPv6 항목을 최소 1개 찾은 상태
								// 아무런 조치도 취하지 않음
							}
							else
							{
								// IPv4를 찾으면 바로 중단하기 때문에, Unknown일 때에만 여기로 진입함
								*_address_ipv6 = *((sockaddr_in6 *)(item->ai_addr));
								_address_storage.ss_family = AF_INET6;
							}
							break;

						default:
							break;
					}

					if(stop)
					{
						break;
					}
				}

				OV_SAFE_FUNC(result, nullptr, ::freeaddrinfo,);

				return _address_storage.ss_family != AF_UNSPEC;
			}
		}

		_hostname = hostname;

		UpdateIPAddress();

		return true;
	}

	void SocketAddress::UpdateIPAddress()
	{
		if(_address_storage.ss_family != AF_UNSPEC)
		{
			String ip_address;

			ip_address.SetLength(AddressLength());

			const void *address = (_address_storage.ss_family == AF_INET) ? (void *)(&(_address_ipv4->sin_addr)) : (void *)(&(_address_ipv6->sin6_addr));

			if(inet_ntop(_address_storage.ss_family, address, ip_address.GetBuffer(), static_cast<socklen_t>(ip_address.GetLength())) != nullptr)
			{
				_ip_address = ip_address;
			}
		}
	}

	ov::String SocketAddress::GetHostname() const noexcept
	{
		return _hostname;
	}

	ov::String SocketAddress::GetIpAddress() const noexcept
	{
		return _ip_address;
	}

	bool SocketAddress::SetPort(uint16_t port)
	{
		switch(_address_storage.ss_family)
		{
			case AF_INET:
				_address_ipv4->sin_port = HostToNetwork16(port);
				return true;

			case AF_INET6:
				_address_ipv6->sin6_port = HostToNetwork16(port);
				return true;

			default:
				return false;
		}
	}

	uint16_t SocketAddress::Port() const noexcept
	{
		switch(_address_storage.ss_family)
		{
			case AF_INET:
				return NetworkToHost16(_address_ipv4->sin_port);

			case AF_INET6:
				return NetworkToHost16(_address_ipv6->sin6_port);

			default:
				return 0;
		}
	}

	const sockaddr *SocketAddress::Address() const noexcept
	{
		if(_address_storage.ss_family != AF_UNSPEC)
		{
			return (const sockaddr *)(&_address_storage);
		}

		return nullptr;
	}

	const sockaddr_in *SocketAddress::AddressForIPv4() const noexcept
	{
		if(_address_storage.ss_family != AF_INET)
		{
			// IPv4 => IPv6로 변환할 수 없음
			return nullptr;
		}

		return _address_ipv4;
	}

	const sockaddr_in6 *SocketAddress::AddressForIPv6() const noexcept
	{
		if(_address_storage.ss_family != AF_INET6)
		{
			// IPv6 => IPv4로 변환할 수 없음
			return nullptr;
		}

		return _address_ipv6;
	}

	const in_addr *SocketAddress::AddrInForIPv4() const noexcept
	{
		if(_address_storage.ss_family != AF_INET)
		{
			// IPv4 => IPv6로 변환할 수 없음
			return nullptr;
		}

		return &(_address_ipv4->sin_addr);
	}

	const in6_addr *SocketAddress::AddrInForIPv6() const noexcept
	{
		if(_address_storage.ss_family != AF_INET6)
		{
			// IPv6 => IPv4로 변환할 수 없음
			return nullptr;
		}

		return &(_address_ipv6->sin6_addr);
	}

	socklen_t SocketAddress::AddressLength() const noexcept
	{
		switch(_address_storage.ss_family)
		{
			case AF_INET:
				return INET_ADDRSTRLEN;

			case AF_INET6:
				return INET6_ADDRSTRLEN;

			default:
				return 0;
		}
	}

	ov::String SocketAddress::ToString() const noexcept
	{
		return ov::String::FormatString("[%s] %s:%d", (_address_storage.ss_family == AF_INET) ? "v4" : (_address_storage.ss_family == AF_INET6 ? "v6" : "?"), GetIpAddress().CStr(), Port());
	}
}
