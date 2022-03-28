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
		Http11, // Default over Non-TLS. It can be upgraded to Http2(h2c) or WebSocket
		Http20, // Default over TLS (If the client supports h2). It can be upgraded to WebSocket
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

	inline Method operator|(Method lhs, Method rhs)
	{
		return static_cast<Method>(
			ov::ToUnderlyingType(lhs) |
			ov::ToUnderlyingType(rhs));
	}

	inline Method &operator|=(Method &lhs, Method rhs)
	{
		lhs = static_cast<Method>(
			ov::ToUnderlyingType(lhs) |
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
		UpgradeRequired = 426,
		InternalServerError = 500,
		NotImplemented = 501,
		BadGateway = 502,
		ServiceUnavailable = 503,
		GatewayTimeout = 504,
		HTTPVersionNotSupported = 505
	};

#define HTTP_CASE_RETURN(condition, value) \
	case condition:                        \
		return value

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
			HTTP_CASE_RETURN(StatusCode::Unknown, false);
			HTTP_CASE_RETURN(StatusCode::Continue, true);
			HTTP_CASE_RETURN(StatusCode::SwitchingProtocols, true);
			HTTP_CASE_RETURN(StatusCode::OK, true);
			HTTP_CASE_RETURN(StatusCode::Created, true);
			HTTP_CASE_RETURN(StatusCode::Accepted, true);
			HTTP_CASE_RETURN(StatusCode::NonAuthoritativeInformation, true);
			HTTP_CASE_RETURN(StatusCode::NoContent, true);
			HTTP_CASE_RETURN(StatusCode::ResetContent, true);
			HTTP_CASE_RETURN(StatusCode::PartialContent, true);
			HTTP_CASE_RETURN(StatusCode::MultiStatus, true);
			HTTP_CASE_RETURN(StatusCode::MultipleChoices, true);
			HTTP_CASE_RETURN(StatusCode::MovedPermanently, true);
			HTTP_CASE_RETURN(StatusCode::Found, true);
			HTTP_CASE_RETURN(StatusCode::SeeOther, true);
			HTTP_CASE_RETURN(StatusCode::NotModified, true);
			HTTP_CASE_RETURN(StatusCode::UseProxy, true);
			HTTP_CASE_RETURN(StatusCode::TemporaryRedirect, true);
			HTTP_CASE_RETURN(StatusCode::BadRequest, true);
			HTTP_CASE_RETURN(StatusCode::Unauthorized, true);
			HTTP_CASE_RETURN(StatusCode::PaymentRequired, true);
			HTTP_CASE_RETURN(StatusCode::Forbidden, true);
			HTTP_CASE_RETURN(StatusCode::NotFound, true);
			HTTP_CASE_RETURN(StatusCode::MethodNotAllowed, true);
			HTTP_CASE_RETURN(StatusCode::NotAcceptable, true);
			HTTP_CASE_RETURN(StatusCode::ProxyAuthenticationRequired, true);
			HTTP_CASE_RETURN(StatusCode::RequestTimeout, true);
			HTTP_CASE_RETURN(StatusCode::Conflict, true);
			HTTP_CASE_RETURN(StatusCode::Gone, true);
			HTTP_CASE_RETURN(StatusCode::LengthRequired, true);
			HTTP_CASE_RETURN(StatusCode::PreconditionFailed, true);
			HTTP_CASE_RETURN(StatusCode::PayloadTooLarge, true);
			HTTP_CASE_RETURN(StatusCode::URITooLong, true);
			HTTP_CASE_RETURN(StatusCode::UnsupportedMediaType, true);
			HTTP_CASE_RETURN(StatusCode::RangeNotSatisfiable, true);
			HTTP_CASE_RETURN(StatusCode::ExpectationFailed, true);
			HTTP_CASE_RETURN(StatusCode::UpgradeRequired, true);
			HTTP_CASE_RETURN(StatusCode::InternalServerError, true);
			HTTP_CASE_RETURN(StatusCode::NotImplemented, true);
			HTTP_CASE_RETURN(StatusCode::BadGateway, true);
			HTTP_CASE_RETURN(StatusCode::ServiceUnavailable, true);
			HTTP_CASE_RETURN(StatusCode::GatewayTimeout, true);
			HTTP_CASE_RETURN(StatusCode::HTTPVersionNotSupported, true);
		}

		return false;
	}

	inline constexpr const char *StringFromStatusCode(StatusCode status_code)
	{
		switch (status_code)
		{
			HTTP_CASE_RETURN(StatusCode::Unknown, "Unknown");
			HTTP_CASE_RETURN(StatusCode::Continue, "Continue");
			HTTP_CASE_RETURN(StatusCode::SwitchingProtocols, "Switching Protocols");
			HTTP_CASE_RETURN(StatusCode::OK, "OK");
			HTTP_CASE_RETURN(StatusCode::Created, "Created");
			HTTP_CASE_RETURN(StatusCode::Accepted, "Accepted");
			HTTP_CASE_RETURN(StatusCode::NonAuthoritativeInformation, "Non-Authoritative Information");
			HTTP_CASE_RETURN(StatusCode::NoContent, "No Content");
			HTTP_CASE_RETURN(StatusCode::ResetContent, "Reset Content");
			HTTP_CASE_RETURN(StatusCode::PartialContent, "Partial Content");
			HTTP_CASE_RETURN(StatusCode::MultiStatus, "Multi Status");
			HTTP_CASE_RETURN(StatusCode::MultipleChoices, "Multiple Choices");
			HTTP_CASE_RETURN(StatusCode::MovedPermanently, "Moved Permanently");
			HTTP_CASE_RETURN(StatusCode::Found, "Found");
			HTTP_CASE_RETURN(StatusCode::SeeOther, "See Other");
			HTTP_CASE_RETURN(StatusCode::NotModified, "Not Modified");
			HTTP_CASE_RETURN(StatusCode::UseProxy, "Use Proxy");
			HTTP_CASE_RETURN(StatusCode::TemporaryRedirect, "Temporary Redirect");
			HTTP_CASE_RETURN(StatusCode::BadRequest, "Bad Request");
			HTTP_CASE_RETURN(StatusCode::Unauthorized, "Unauthorized");
			HTTP_CASE_RETURN(StatusCode::PaymentRequired, "Payment Required");
			HTTP_CASE_RETURN(StatusCode::Forbidden, "Forbidden");
			HTTP_CASE_RETURN(StatusCode::NotFound, "Not Found");
			HTTP_CASE_RETURN(StatusCode::MethodNotAllowed, "Method Not Allowed");
			HTTP_CASE_RETURN(StatusCode::NotAcceptable, "Not Acceptable");
			HTTP_CASE_RETURN(StatusCode::ProxyAuthenticationRequired, "Proxy Authentication Required");
			HTTP_CASE_RETURN(StatusCode::RequestTimeout, "Request Timeout");
			HTTP_CASE_RETURN(StatusCode::Conflict, "Conflict");
			HTTP_CASE_RETURN(StatusCode::Gone, "Gone");
			HTTP_CASE_RETURN(StatusCode::LengthRequired, "Length Required");
			HTTP_CASE_RETURN(StatusCode::PreconditionFailed, "Precondition Failed");
			HTTP_CASE_RETURN(StatusCode::PayloadTooLarge, "Payload Too Large");
			HTTP_CASE_RETURN(StatusCode::URITooLong, "URI Too Long");
			HTTP_CASE_RETURN(StatusCode::UnsupportedMediaType, "Unsupported Media Type");
			HTTP_CASE_RETURN(StatusCode::RangeNotSatisfiable, "Range Not Satisfiable");
			HTTP_CASE_RETURN(StatusCode::ExpectationFailed, "Expectation Failed");
			HTTP_CASE_RETURN(StatusCode::UpgradeRequired, "Upgrade Required");
			HTTP_CASE_RETURN(StatusCode::InternalServerError, "Internal Server Error");
			HTTP_CASE_RETURN(StatusCode::NotImplemented, "Not Implemented");
			HTTP_CASE_RETURN(StatusCode::BadGateway, "Bad Gateway");
			HTTP_CASE_RETURN(StatusCode::ServiceUnavailable, "Service Unavailable");
			HTTP_CASE_RETURN(StatusCode::GatewayTimeout, "Gateway Timeout");
			HTTP_CASE_RETURN(StatusCode::HTTPVersionNotSupported, "HTTP Version Not Supported");
		}

		return "Unknown";
	}

	// Since http::Method can contains multiple methods, the include_all_method flag is used to determine whether to return the value for the entire methods
	inline ov::String StringFromMethod(Method method, bool include_all_methods = true)
	{
		auto method_value = ov::ToUnderlyingType(method);

		if (include_all_methods)
		{
			std::vector<ov::String> method_list;

			HTTP_IF_EXPR(OV_CHECK_FLAG(method_value, ov::ToUnderlyingType(Method::Get)), method_list.push_back("GET"));
			HTTP_IF_EXPR(OV_CHECK_FLAG(method_value, ov::ToUnderlyingType(Method::Head)), method_list.push_back("HEAD"));
			HTTP_IF_EXPR(OV_CHECK_FLAG(method_value, ov::ToUnderlyingType(Method::Post)), method_list.push_back("POST"));
			HTTP_IF_EXPR(OV_CHECK_FLAG(method_value, ov::ToUnderlyingType(Method::Put)), method_list.push_back("PUT"));
			HTTP_IF_EXPR(OV_CHECK_FLAG(method_value, ov::ToUnderlyingType(Method::Delete)), method_list.push_back("DELETE"));
			HTTP_IF_EXPR(OV_CHECK_FLAG(method_value, ov::ToUnderlyingType(Method::Connect)), method_list.push_back("CONNECT"));
			HTTP_IF_EXPR(OV_CHECK_FLAG(method_value, ov::ToUnderlyingType(Method::Options)), method_list.push_back("OPTIONS"));
			HTTP_IF_EXPR(OV_CHECK_FLAG(method_value, ov::ToUnderlyingType(Method::Trace)), method_list.push_back("TRACE"));
			HTTP_IF_EXPR(OV_CHECK_FLAG(method_value, ov::ToUnderlyingType(Method::Patch)), method_list.push_back("PATCH"));

			return ov::String::Join(method_list, " | ");
		}
		else
		{
			HTTP_IF_EXPR(OV_CHECK_FLAG(method_value, ov::ToUnderlyingType(Method::Get)), return "GET");
			HTTP_IF_EXPR(OV_CHECK_FLAG(method_value, ov::ToUnderlyingType(Method::Head)), return "HEAD");
			HTTP_IF_EXPR(OV_CHECK_FLAG(method_value, ov::ToUnderlyingType(Method::Post)), return "POST");
			HTTP_IF_EXPR(OV_CHECK_FLAG(method_value, ov::ToUnderlyingType(Method::Put)), return "PUT");
			HTTP_IF_EXPR(OV_CHECK_FLAG(method_value, ov::ToUnderlyingType(Method::Delete)), return "DELETE");
			HTTP_IF_EXPR(OV_CHECK_FLAG(method_value, ov::ToUnderlyingType(Method::Connect)), return "CONNECT");
			HTTP_IF_EXPR(OV_CHECK_FLAG(method_value, ov::ToUnderlyingType(Method::Options)), return "OPTIONS");
			HTTP_IF_EXPR(OV_CHECK_FLAG(method_value, ov::ToUnderlyingType(Method::Trace)), return "TRACE");
			HTTP_IF_EXPR(OV_CHECK_FLAG(method_value, ov::ToUnderlyingType(Method::Patch)), return "PATCH");
		}

		return "";
	}

	// Since http::Method can contains multiple methods, the include_all_method flag is used to determine whether to return the value for the entire methods
	inline Method MethodFromString(ov::String method, bool include_all_methods = true)
	{
		Method method_enum = Method::Unknown;

		if (include_all_methods)
		{
			HTTP_IF_EXPR(method.IndexOf("GET") >= 0, method_enum |= Method::Get);
			HTTP_IF_EXPR(method.IndexOf("HEAD") >= 0, method_enum |= Method::Head);
			HTTP_IF_EXPR(method.IndexOf("POST") >= 0, method_enum |= Method::Post);
			HTTP_IF_EXPR(method.IndexOf("PUT") >= 0, method_enum |= Method::Put);
			HTTP_IF_EXPR(method.IndexOf("DELETE") >= 0, method_enum |= Method::Delete);
			HTTP_IF_EXPR(method.IndexOf("CONNECT") >= 0, method_enum |= Method::Connect);
			HTTP_IF_EXPR(method.IndexOf("OPTIONS") >= 0, method_enum |= Method::Options);
			HTTP_IF_EXPR(method.IndexOf("TRACE") >= 0, method_enum |= Method::Trace);
			HTTP_IF_EXPR(method.IndexOf("PATCH") >= 0, method_enum |= Method::Patch);
		}
		else
		{
			auto methods = method.Split(" ");

			if (methods.size() >= 1)
			{
				auto first_method = methods.front();

				HTTP_IF_EXPR(first_method == "GET", return Method::Get);
				HTTP_IF_EXPR(first_method == "HEAD", return Method::Head);
				HTTP_IF_EXPR(first_method == "POST", return Method::Post);
				HTTP_IF_EXPR(first_method == "PUT", return Method::Put);
				HTTP_IF_EXPR(first_method == "DELETE", return Method::Delete);
				HTTP_IF_EXPR(first_method == "CONNECT", return Method::Connect);
				HTTP_IF_EXPR(first_method == "OPTIONS", return Method::Options);
				HTTP_IF_EXPR(first_method == "TRACE", return Method::Trace);
				HTTP_IF_EXPR(first_method == "PATCH", return Method::Patch);
			}
			else
			{
				// There is no method in the string
			}
		}

		return method_enum;
	}

	namespace svr
	{
		enum class InterceptorResult : char
		{
			Completed,
			Error,
			Moved		// Control is transferred to another thread so DO NOT close the connection
		};

		enum class NextHandler : char
		{
			// Call the next handler
			Call,
			// Do not call the next handler
			DoNotCall
		};

		class HttpServer;
		class HttpExchange;

		using RequestHandler = std::function<NextHandler(const std::shared_ptr<HttpExchange> &exchange)>;
		using RequestErrorHandler = std::function<void(const std::shared_ptr<HttpExchange> &exchange)>;

		using ResponseWriteHandler = std::function<bool(const std::shared_ptr<ov::Data> &data)>;
	}  // namespace svr

	class HttpError;

	namespace clnt
	{
		using ResponseHandler = std::function<void(StatusCode status_code, const std::shared_ptr<ov::Data> &data, const std::shared_ptr<const ov::Error> &error)>;
	}
}  // namespace http
