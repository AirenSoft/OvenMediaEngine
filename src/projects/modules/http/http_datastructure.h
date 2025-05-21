//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/ovcrypto/ovcrypto.h>
#include <base/ovlibrary/ovlibrary.h>
#include <base/ovsocket/ovsocket.h>

#include <functional>
#include <memory>

namespace http
{
	enum class ConnectionType : uint8_t
	{
		Unknown = 0,
		Http10,
		Http11,	 // Default over Non-TLS. It can be upgraded to Http2(h2c) or WebSocket
		Http20,	 // Default over TLS (If the client supports h2). It can be upgraded to WebSocket
		WebSocket
	};

	inline ov::String StringFromConnectionType(ConnectionType type)
	{
		switch (type)
		{
			case ConnectionType::Http10:
				return "HTTP/1.0";
			case ConnectionType::Http11:
				return "HTTP/1.1";
			case ConnectionType::Http20:
				return "HTTP/2";
			case ConnectionType::WebSocket:
				return "WebSocket";
			default:
				return "Unknown";
		}
	}

	// RFC7231 - 4. Request Methods
	// +---------+-------------------------------------------------+-------+
	// | Method  | Description                                     | Sec.  |
	// +---------+-------------------------------------------------+-------+
	// | GET     | Transfer a current representation of the target | 4.3.1 |
	// |         | resource.                                       |       |
	// | HEAD    | Same as GET, but only transfer the status line  | 4.3.2 |
	// |         | and header section.                             |       |
	// | POST    | Perform resource-specific processing on the     | 4.3.3 |
	// |         | request payload.                                |       |
	// | PUT     | Replace all current representations of the      | 4.3.4 |
	// |         | target resource with the request payload.       |       |
	// | DELETE  | Remove all current representations of the       | 4.3.5 |
	// |         | target resource.                                |       |
	// | CONNECT | Establish a tunnel to the server identified by  | 4.3.6 |
	// |         | the target resource.                            |       |
	// | OPTIONS | Describe the communication options for the      | 4.3.7 |
	// |         | target resource.                                |       |
	// | TRACE   | Perform a message loop-back test along the path | 4.3.8 |
	// |         | to the target resource.                         |       |
	// +---------+-------------------------------------------------+-------+
	//
	// RFC5789 - PATCH Method for HTTP
	enum class Method : uint16_t
	{
		Unknown = 0x0000,

		Get = 0x0001,
		Head = 0x0002,
		Post = 0x0004,
		Put = 0x0008,
		Delete = 0x0010,
		Connect = 0x0020,
		Options = 0x0040,
		Trace = 0x0080,

		Patch = 0x0100,

		All = Get | Head | Post | Put | Delete | Connect | Options | Trace | Patch
	};

	constexpr inline Method operator|(Method lhs, Method rhs)
	{
		return static_cast<Method>(
			ov::ToUnderlyingType(lhs) |
			ov::ToUnderlyingType(rhs));
	}

	constexpr inline Method &operator|=(Method &lhs, Method rhs)
	{
		lhs = static_cast<Method>(
			ov::ToUnderlyingType(lhs) |
			ov::ToUnderlyingType(rhs));

		return lhs;
	}

	constexpr inline Method operator&(Method lhs, Method rhs)
	{
		return static_cast<Method>(
			ov::ToUnderlyingType(lhs) &
			ov::ToUnderlyingType(rhs));
	}

	constexpr inline Method &operator&=(Method &lhs, Method rhs)
	{
		lhs = static_cast<Method>(
			ov::ToUnderlyingType(lhs) &
			ov::ToUnderlyingType(rhs));

		return lhs;
	}

	// RFC7231 - 6. Response Status Codes
	// +------+-------------------------------+--------------------------+
	// | Code | Reason-Phrase                 | Defined in...            |
	// +------+-------------------------------+--------------------------+
	// | 100  | Continue                      | Section 6.2.1            |
	// | 101  | Switching Protocols           | Section 6.2.2            |
	// | 200  | OK                            | Section 6.3.1            |
	// | 201  | Created                       | Section 6.3.2            |
	// | 202  | Accepted                      | Section 6.3.3            |
	// | 203  | Non-Authoritative Information | Section 6.3.4            |
	// | 204  | No Content                    | Section 6.3.5            |
	// | 205  | Reset Content                 | Section 6.3.6            |
	// | 206  | Partial Content               | Section 4.1 of [RFC7233] |
	// | 300  | Multiple Choices              | Section 6.4.1            |
	// | 301  | Moved Permanently             | Section 6.4.2            |
	// | 302  | Found                         | Section 6.4.3            |
	// | 303  | See Other                     | Section 6.4.4            |
	// | 304  | Not Modified                  | Section 4.1 of [RFC7232] |
	// | 305  | Use Proxy                     | Section 6.4.5            |
	// | 307  | Temporary Redirect            | Section 6.4.7            |
	// | 400  | Bad Request                   | Section 6.5.1            |
	// | 401  | Unauthorized                  | Section 3.1 of [RFC7235] |
	// | 402  | Payment Required              | Section 6.5.2            |
	// | 403  | Forbidden                     | Section 6.5.3            |
	// | 404  | Not Found                     | Section 6.5.4            |
	// | 405  | Method Not Allowed            | Section 6.5.5            |
	// | 406  | Not Acceptable                | Section 6.5.6            |
	// | 407  | Proxy Authentication Required | Section 3.2 of [RFC7235] |
	// | 408  | Request Timeout               | Section 6.5.7            |
	// | 409  | Conflict                      | Section 6.5.8            |
	// | 410  | Gone                          | Section 6.5.9            |
	// | 411  | Length Required               | Section 6.5.10           |
	// | 412  | Precondition Failed           | Section 4.2 of [RFC7232] |
	// | 413  | Payload Too Large             | Section 6.5.11           |
	// | 414  | URI Too Long                  | Section 6.5.12           |
	// | 415  | Unsupported Media Type        | Section 6.5.13           |
	// | 416  | Range Not Satisfiable         | Section 4.4 of [RFC7233] |
	// | 417  | Expectation Failed            | Section 6.5.14           |
	// | 426  | Upgrade Required              | Section 6.5.15           |
	// | 500  | Internal Server Error         | Section 6.6.1            |
	// | 501  | Not Implemented               | Section 6.6.2            |
	// | 502  | Bad Gateway                   | Section 6.6.3            |
	// | 503  | Service Unavailable           | Section 6.6.4            |
	// | 504  | Gateway Timeout               | Section 6.6.5            |
	// | 505  | HTTP Version Not Supported    | Section 6.6.6            |
	// +------+-------------------------------+--------------------------+
	enum class StatusCode : uint16_t
	{
		Unknown = 0,

		Continue = 100,
		SwitchingProtocols = 101,
		OK = 200,
		Created = 201,
		Accepted = 202,
		NonAuthoritativeInformation = 203,
		NoContent = 204,
		ResetContent = 205,
		PartialContent = 206,
		// RFC 4918 (https://tools.ietf.org/html/rfc4918#section-11.1)
		MultiStatus = 207,
		MultipleChoices = 300,
		MovedPermanently = 301,
		Found = 302,
		SeeOther = 303,
		NotModified = 304,
		UseProxy = 305,
		TemporaryRedirect = 307,
		BadRequest = 400,
		Unauthorized = 401,
		PaymentRequired = 402,
		Forbidden = 403,
		NotFound = 404,
		MethodNotAllowed = 405,
		NotAcceptable = 406,
		ProxyAuthenticationRequired = 407,
		RequestTimeout = 408,
		Conflict = 409,
		Gone = 410,
		LengthRequired = 411,
		PreconditionFailed = 412,
		PayloadTooLarge = 413,
		URITooLong = 414,
		UnsupportedMediaType = 415,
		RangeNotSatisfiable = 416,
		ExpectationFailed = 417,
		Locked = 423,
		FailedDependency = 424,
		UpgradeRequired = 426,
		InternalServerError = 500,
		NotImplemented = 501,
		BadGateway = 502,
		ServiceUnavailable = 503,
		GatewayTimeout = 504,
		HTTPVersionNotSupported = 505
	};

#define HTTP_IF_EXPR(condition, expr) \
	do                                \
	{                                 \
		if (condition)                \
		{                             \
			expr;                     \
		}                             \
	} while (false)

	inline constexpr bool IsValidStatusCode(StatusCode status_code)
	{
		switch (status_code)
		{
			OV_CASE_RETURN(StatusCode::Unknown, false);
			OV_CASE_RETURN(StatusCode::Continue, true);
			OV_CASE_RETURN(StatusCode::SwitchingProtocols, true);
			OV_CASE_RETURN(StatusCode::OK, true);
			OV_CASE_RETURN(StatusCode::Created, true);
			OV_CASE_RETURN(StatusCode::Accepted, true);
			OV_CASE_RETURN(StatusCode::NonAuthoritativeInformation, true);
			OV_CASE_RETURN(StatusCode::NoContent, true);
			OV_CASE_RETURN(StatusCode::ResetContent, true);
			OV_CASE_RETURN(StatusCode::PartialContent, true);
			OV_CASE_RETURN(StatusCode::MultiStatus, true);
			OV_CASE_RETURN(StatusCode::MultipleChoices, true);
			OV_CASE_RETURN(StatusCode::MovedPermanently, true);
			OV_CASE_RETURN(StatusCode::Found, true);
			OV_CASE_RETURN(StatusCode::SeeOther, true);
			OV_CASE_RETURN(StatusCode::NotModified, true);
			OV_CASE_RETURN(StatusCode::UseProxy, true);
			OV_CASE_RETURN(StatusCode::TemporaryRedirect, true);
			OV_CASE_RETURN(StatusCode::BadRequest, true);
			OV_CASE_RETURN(StatusCode::Unauthorized, true);
			OV_CASE_RETURN(StatusCode::PaymentRequired, true);
			OV_CASE_RETURN(StatusCode::Forbidden, true);
			OV_CASE_RETURN(StatusCode::NotFound, true);
			OV_CASE_RETURN(StatusCode::MethodNotAllowed, true);
			OV_CASE_RETURN(StatusCode::NotAcceptable, true);
			OV_CASE_RETURN(StatusCode::ProxyAuthenticationRequired, true);
			OV_CASE_RETURN(StatusCode::RequestTimeout, true);
			OV_CASE_RETURN(StatusCode::Conflict, true);
			OV_CASE_RETURN(StatusCode::Gone, true);
			OV_CASE_RETURN(StatusCode::LengthRequired, true);
			OV_CASE_RETURN(StatusCode::PreconditionFailed, true);
			OV_CASE_RETURN(StatusCode::PayloadTooLarge, true);
			OV_CASE_RETURN(StatusCode::URITooLong, true);
			OV_CASE_RETURN(StatusCode::UnsupportedMediaType, true);
			OV_CASE_RETURN(StatusCode::RangeNotSatisfiable, true);
			OV_CASE_RETURN(StatusCode::ExpectationFailed, true);
			OV_CASE_RETURN(StatusCode::Locked, true);
			OV_CASE_RETURN(StatusCode::FailedDependency, true);
			OV_CASE_RETURN(StatusCode::UpgradeRequired, true);
			OV_CASE_RETURN(StatusCode::InternalServerError, true);
			OV_CASE_RETURN(StatusCode::NotImplemented, true);
			OV_CASE_RETURN(StatusCode::BadGateway, true);
			OV_CASE_RETURN(StatusCode::ServiceUnavailable, true);
			OV_CASE_RETURN(StatusCode::GatewayTimeout, true);
			OV_CASE_RETURN(StatusCode::HTTPVersionNotSupported, true);
		}

		return false;
	}

	inline constexpr const char *StringFromStatusCode(StatusCode status_code)
	{
		switch (status_code)
		{
			OV_CASE_RETURN(StatusCode::Unknown, "Unknown");
			OV_CASE_RETURN(StatusCode::Continue, "Continue");
			OV_CASE_RETURN(StatusCode::SwitchingProtocols, "Switching Protocols");
			OV_CASE_RETURN(StatusCode::OK, "OK");
			OV_CASE_RETURN(StatusCode::Created, "Created");
			OV_CASE_RETURN(StatusCode::Accepted, "Accepted");
			OV_CASE_RETURN(StatusCode::NonAuthoritativeInformation, "Non-Authoritative Information");
			OV_CASE_RETURN(StatusCode::NoContent, "No Content");
			OV_CASE_RETURN(StatusCode::ResetContent, "Reset Content");
			OV_CASE_RETURN(StatusCode::PartialContent, "Partial Content");
			OV_CASE_RETURN(StatusCode::MultiStatus, "Multi Status");
			OV_CASE_RETURN(StatusCode::MultipleChoices, "Multiple Choices");
			OV_CASE_RETURN(StatusCode::MovedPermanently, "Moved Permanently");
			OV_CASE_RETURN(StatusCode::Found, "Found");
			OV_CASE_RETURN(StatusCode::SeeOther, "See Other");
			OV_CASE_RETURN(StatusCode::NotModified, "Not Modified");
			OV_CASE_RETURN(StatusCode::UseProxy, "Use Proxy");
			OV_CASE_RETURN(StatusCode::TemporaryRedirect, "Temporary Redirect");
			OV_CASE_RETURN(StatusCode::BadRequest, "Bad Request");
			OV_CASE_RETURN(StatusCode::Unauthorized, "Unauthorized");
			OV_CASE_RETURN(StatusCode::PaymentRequired, "Payment Required");
			OV_CASE_RETURN(StatusCode::Forbidden, "Forbidden");
			OV_CASE_RETURN(StatusCode::NotFound, "Not Found");
			OV_CASE_RETURN(StatusCode::MethodNotAllowed, "Method Not Allowed");
			OV_CASE_RETURN(StatusCode::NotAcceptable, "Not Acceptable");
			OV_CASE_RETURN(StatusCode::ProxyAuthenticationRequired, "Proxy Authentication Required");
			OV_CASE_RETURN(StatusCode::RequestTimeout, "Request Timeout");
			OV_CASE_RETURN(StatusCode::Conflict, "Conflict");
			OV_CASE_RETURN(StatusCode::Gone, "Gone");
			OV_CASE_RETURN(StatusCode::LengthRequired, "Length Required");
			OV_CASE_RETURN(StatusCode::PreconditionFailed, "Precondition Failed");
			OV_CASE_RETURN(StatusCode::PayloadTooLarge, "Payload Too Large");
			OV_CASE_RETURN(StatusCode::URITooLong, "URI Too Long");
			OV_CASE_RETURN(StatusCode::UnsupportedMediaType, "Unsupported Media Type");
			OV_CASE_RETURN(StatusCode::RangeNotSatisfiable, "Range Not Satisfiable");
			OV_CASE_RETURN(StatusCode::ExpectationFailed, "Expectation Failed");
			OV_CASE_RETURN(StatusCode::Locked, "Locked");
			OV_CASE_RETURN(StatusCode::FailedDependency, "Failed Dependency");
			OV_CASE_RETURN(StatusCode::UpgradeRequired, "Upgrade Required");
			OV_CASE_RETURN(StatusCode::InternalServerError, "Internal Server Error");
			OV_CASE_RETURN(StatusCode::NotImplemented, "Not Implemented");
			OV_CASE_RETURN(StatusCode::BadGateway, "Bad Gateway");
			OV_CASE_RETURN(StatusCode::ServiceUnavailable, "Service Unavailable");
			OV_CASE_RETURN(StatusCode::GatewayTimeout, "Gateway Timeout");
			OV_CASE_RETURN(StatusCode::HTTPVersionNotSupported, "HTTP Version Not Supported");
		}

		return "Unknown";
	}

	// Since http::Method can contains multiple methods, the include_all_method flag is used to determine whether to return the value for the entire methods
	inline ov::String StringFromMethod(Method method, bool include_all_methods = true)
	{
		if (include_all_methods)
		{
			std::vector<ov::String> method_list;

			HTTP_IF_EXPR(OV_CHECK_FLAG(method, Method::Get), method_list.push_back("GET"));
			HTTP_IF_EXPR(OV_CHECK_FLAG(method, Method::Head), method_list.push_back("HEAD"));
			HTTP_IF_EXPR(OV_CHECK_FLAG(method, Method::Post), method_list.push_back("POST"));
			HTTP_IF_EXPR(OV_CHECK_FLAG(method, Method::Put), method_list.push_back("PUT"));
			HTTP_IF_EXPR(OV_CHECK_FLAG(method, Method::Delete), method_list.push_back("DELETE"));
			HTTP_IF_EXPR(OV_CHECK_FLAG(method, Method::Connect), method_list.push_back("CONNECT"));
			HTTP_IF_EXPR(OV_CHECK_FLAG(method, Method::Options), method_list.push_back("OPTIONS"));
			HTTP_IF_EXPR(OV_CHECK_FLAG(method, Method::Trace), method_list.push_back("TRACE"));
			HTTP_IF_EXPR(OV_CHECK_FLAG(method, Method::Patch), method_list.push_back("PATCH"));

			return ov::String::Join(method_list, " | ");
		}
		else
		{
			OV_IF_RETURN(OV_CHECK_FLAG(method, Method::Get), "GET");
			OV_IF_RETURN(OV_CHECK_FLAG(method, Method::Head), "HEAD");
			OV_IF_RETURN(OV_CHECK_FLAG(method, Method::Post), "POST");
			OV_IF_RETURN(OV_CHECK_FLAG(method, Method::Put), "PUT");
			OV_IF_RETURN(OV_CHECK_FLAG(method, Method::Delete), "DELETE");
			OV_IF_RETURN(OV_CHECK_FLAG(method, Method::Connect), "CONNECT");
			OV_IF_RETURN(OV_CHECK_FLAG(method, Method::Options), "OPTIONS");
			OV_IF_RETURN(OV_CHECK_FLAG(method, Method::Trace), "TRACE");
			OV_IF_RETURN(OV_CHECK_FLAG(method, Method::Patch), "PATCH");
		}

		return "";
	}

	// Since http::Method can contains multiple methods, the include_all_method flag is used to determine whether to return the value for the entire methods
	constexpr inline Method MethodFromString(const char *method, bool include_all_methods = true)
	{
		auto method_enum = Method::Unknown;

		if (include_all_methods)
		{
			HTTP_IF_EXPR(ov::cexpr::IndexOf(method, "GET") >= 0, method_enum |= Method::Get);
			HTTP_IF_EXPR(ov::cexpr::IndexOf(method, "HEAD") >= 0, method_enum |= Method::Head);
			HTTP_IF_EXPR(ov::cexpr::IndexOf(method, "POST") >= 0, method_enum |= Method::Post);
			HTTP_IF_EXPR(ov::cexpr::IndexOf(method, "PUT") >= 0, method_enum |= Method::Put);
			HTTP_IF_EXPR(ov::cexpr::IndexOf(method, "DELETE") >= 0, method_enum |= Method::Delete);
			HTTP_IF_EXPR(ov::cexpr::IndexOf(method, "CONNECT") >= 0, method_enum |= Method::Connect);
			HTTP_IF_EXPR(ov::cexpr::IndexOf(method, "OPTIONS") >= 0, method_enum |= Method::Options);
			HTTP_IF_EXPR(ov::cexpr::IndexOf(method, "TRACE") >= 0, method_enum |= Method::Trace);
			HTTP_IF_EXPR(ov::cexpr::IndexOf(method, "PATCH") >= 0, method_enum |= Method::Patch);
		}
		else
		{
			if (method[0] != '\0')
			{
				auto position = ov::cexpr::IndexOf(method, ' ');

				if (position < 0)
				{
					position = ov::cexpr::StrLen(method);
				}

				OV_IF_RETURN(ov::cexpr::StrNCmp(method, "GET", position) == 0, Method::Get);
				OV_IF_RETURN(ov::cexpr::StrNCmp(method, "HEAD", position) == 0, Method::Head);
				OV_IF_RETURN(ov::cexpr::StrNCmp(method, "POST", position) == 0, Method::Post);
				OV_IF_RETURN(ov::cexpr::StrNCmp(method, "PUT", position) == 0, Method::Put);
				OV_IF_RETURN(ov::cexpr::StrNCmp(method, "DELETE", position) == 0, Method::Delete);
				OV_IF_RETURN(ov::cexpr::StrNCmp(method, "CONNECT", position) == 0, Method::Connect);
				OV_IF_RETURN(ov::cexpr::StrNCmp(method, "OPTIONS", position) == 0, Method::Options);
				OV_IF_RETURN(ov::cexpr::StrNCmp(method, "TRACE", position) == 0, Method::Trace);
				OV_IF_RETURN(ov::cexpr::StrNCmp(method, "PATCH", position) == 0, Method::Patch);
			}
		}

		return method_enum;
	}

	constexpr inline http::StatusCode StatusCodeFromCommonError(const CommonErrorCode &code)
	{
		switch (code)
		{
			OV_CASE_RETURN(CommonErrorCode::DISABLED, http::StatusCode::Locked);
			OV_CASE_RETURN(CommonErrorCode::SUCCESS, http::StatusCode::OK);
			OV_CASE_RETURN(CommonErrorCode::CREATED, http::StatusCode::Created);

			OV_CASE_RETURN(CommonErrorCode::ERROR, http::StatusCode::InternalServerError);
			OV_CASE_RETURN(CommonErrorCode::NOT_FOUND, http::StatusCode::NotFound);
			OV_CASE_RETURN(CommonErrorCode::ALREADY_EXISTS, http::StatusCode::Conflict);
			OV_CASE_RETURN(CommonErrorCode::INVALID_REQUEST, http::StatusCode::BadRequest);
			OV_CASE_RETURN(CommonErrorCode::UNAUTHORIZED, http::StatusCode::Unauthorized);
			OV_CASE_RETURN(CommonErrorCode::INVALID_PARAMETER, http::StatusCode::InternalServerError);
			OV_CASE_RETURN(CommonErrorCode::INVALID_STATE, http::StatusCode::InternalServerError);
		}

		return http::StatusCode::InternalServerError;
	}

	namespace svr
	{
		enum class InterceptorResult : char
		{
			Completed,
			Error,
			Moved  // Control is transferred to another thread so DO NOT close the connection
		};
	}  // namespace svr

	class HttpError;

	namespace clnt
	{
		using ResponseHandler = std::function<void(StatusCode status_code, const std::shared_ptr<ov::Data> &data, const std::shared_ptr<const ov::Error> &error)>;
	}
}  // namespace http
