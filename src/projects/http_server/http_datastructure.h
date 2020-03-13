//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <functional>
#include <memory>

#include <base/ovcrypto/ovcrypto.h>
#include <base/ovlibrary/ovlibrary.h>
#include <base/ovsocket/ovsocket.h>

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
enum class HttpMethod : uint16_t
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

	All = Get | Head | Post | Put | Delete | Connect | Options | Trace
};

enum class HttpConnection : char
{
	Closed,
	KeepAlive
};

enum class HttpInterceptorResult : char
{
	Keep,
	Disconnect,
};

enum class HttpNextHandler : char
{
	// Call the next handler
	Call,
	// Do not call the next handler
	DoNotCall
};

inline HttpMethod operator|(HttpMethod lhs, HttpMethod rhs)
{
	return static_cast<HttpMethod>(static_cast<uint16_t>(lhs) | static_cast<uint16_t>(rhs));
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
enum class HttpStatusCode : uint16_t
{
	Continue = 100,
	SwitchingProtocols = 101,
	OK = 200,
	Created = 201,
	Accepted = 202,
	NonAuthoritativeInformation = 203,
	NoContent = 204,
	ResetContent = 205,
	PartialContent = 206,
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

#define HTTP_GET_STATUS_TEXT(condition, text) \
	case condition:                           \
		return text

inline constexpr const char *StringFromHttpStatusCode(HttpStatusCode status_code)
{
	switch (status_code)
	{
		HTTP_GET_STATUS_TEXT(HttpStatusCode::Continue, "Continue");
		HTTP_GET_STATUS_TEXT(HttpStatusCode::SwitchingProtocols, "Switching Protocols");
		HTTP_GET_STATUS_TEXT(HttpStatusCode::OK, "OK");
		HTTP_GET_STATUS_TEXT(HttpStatusCode::Created, "Created");
		HTTP_GET_STATUS_TEXT(HttpStatusCode::Accepted, "Accepted");
		HTTP_GET_STATUS_TEXT(HttpStatusCode::NonAuthoritativeInformation, "Non-Authoritative Information");
		HTTP_GET_STATUS_TEXT(HttpStatusCode::NoContent, "No Content");
		HTTP_GET_STATUS_TEXT(HttpStatusCode::ResetContent, "Reset Content");
		HTTP_GET_STATUS_TEXT(HttpStatusCode::PartialContent, "Partial Content");
		HTTP_GET_STATUS_TEXT(HttpStatusCode::MultipleChoices, "Multiple Choices");
		HTTP_GET_STATUS_TEXT(HttpStatusCode::MovedPermanently, "Moved Permanently");
		HTTP_GET_STATUS_TEXT(HttpStatusCode::Found, "Found");
		HTTP_GET_STATUS_TEXT(HttpStatusCode::SeeOther, "See Other");
		HTTP_GET_STATUS_TEXT(HttpStatusCode::NotModified, "Not Modified");
		HTTP_GET_STATUS_TEXT(HttpStatusCode::UseProxy, "Use Proxy");
		HTTP_GET_STATUS_TEXT(HttpStatusCode::TemporaryRedirect, "Temporary Redirect");
		HTTP_GET_STATUS_TEXT(HttpStatusCode::BadRequest, "Bad Request");
		HTTP_GET_STATUS_TEXT(HttpStatusCode::Unauthorized, "Unauthorized");
		HTTP_GET_STATUS_TEXT(HttpStatusCode::PaymentRequired, "Payment Required");
		HTTP_GET_STATUS_TEXT(HttpStatusCode::Forbidden, "Forbidden");
		HTTP_GET_STATUS_TEXT(HttpStatusCode::NotFound, "Not Found");
		HTTP_GET_STATUS_TEXT(HttpStatusCode::MethodNotAllowed, "Method Not Allowed");
		HTTP_GET_STATUS_TEXT(HttpStatusCode::NotAcceptable, "Not Acceptable");
		HTTP_GET_STATUS_TEXT(HttpStatusCode::ProxyAuthenticationRequired, "Proxy Authentication Required");
		HTTP_GET_STATUS_TEXT(HttpStatusCode::RequestTimeout, "Request Timeout");
		HTTP_GET_STATUS_TEXT(HttpStatusCode::Conflict, "Conflict");
		HTTP_GET_STATUS_TEXT(HttpStatusCode::Gone, "Gone");
		HTTP_GET_STATUS_TEXT(HttpStatusCode::LengthRequired, "Length Required");
		HTTP_GET_STATUS_TEXT(HttpStatusCode::PreconditionFailed, "Precondition Failed");
		HTTP_GET_STATUS_TEXT(HttpStatusCode::PayloadTooLarge, "Payload Too Large");
		HTTP_GET_STATUS_TEXT(HttpStatusCode::URITooLong, "URI Too Long");
		HTTP_GET_STATUS_TEXT(HttpStatusCode::UnsupportedMediaType, "Unsupported Media Type");
		HTTP_GET_STATUS_TEXT(HttpStatusCode::RangeNotSatisfiable, "Range Not Satisfiable");
		HTTP_GET_STATUS_TEXT(HttpStatusCode::ExpectationFailed, "Expectation Failed");
		HTTP_GET_STATUS_TEXT(HttpStatusCode::UpgradeRequired, "Upgrade Required");
		HTTP_GET_STATUS_TEXT(HttpStatusCode::InternalServerError, "Internal Server Error");
		HTTP_GET_STATUS_TEXT(HttpStatusCode::NotImplemented, "Not Implemented");
		HTTP_GET_STATUS_TEXT(HttpStatusCode::BadGateway, "Bad Gateway");
		HTTP_GET_STATUS_TEXT(HttpStatusCode::ServiceUnavailable, "Service Unavailable");
		HTTP_GET_STATUS_TEXT(HttpStatusCode::GatewayTimeout, "Gateway Timeout");
		HTTP_GET_STATUS_TEXT(HttpStatusCode::HTTPVersionNotSupported, "HTTP Version Not Supported");
	}

	return "Unknown";
}

class HttpServer;
class HttpClient;

using HttpRequestHandler = std::function<HttpNextHandler(const std::shared_ptr<HttpClient> &client)>;
using HttpRequestErrorHandler = std::function<void(const std::shared_ptr<HttpClient> &client)>;

using HttpResponseWriteHandler = std::function<bool(const std::shared_ptr<ov::Data> &data)>;