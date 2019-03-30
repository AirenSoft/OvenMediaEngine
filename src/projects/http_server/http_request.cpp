//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "http_request.h"
#include "http_private.h"
#include "http_response.h"

#include <algorithm>

HttpRequest::HttpRequest(const std::shared_ptr<HttpRequestInterceptor> &interceptor, std::shared_ptr<ov::ClientSocket> remote)
	: _interceptor(interceptor),
	  _remote(std::move(remote))
{
}

ssize_t HttpRequest::ProcessData(const std::shared_ptr<const ov::Data> &data)
{
	if(_is_header_found)
	{
		// 헤더를 찾은 상태에서는, 여기가 호출되면 안됨 (parse status가 이미 OK 이므로)
		OV_ASSERT2(false);
		return 0L;
	}

	const char NewLines[] = "\r\n\r\n";
	// null문자 제외
	const int NewLinesLength = OV_COUNTOF(NewLines) - 1;

	ssize_t used_length = data->GetLength();

	// 헤더가 아직 파싱되지 않았으므로, 매 번 데이터가 들어올 때마다 헤더 파싱 시도
	ssize_t previous_length = _request_string.GetLength();

	_request_string.Append(data->GetDataAs<char>(), data->GetLength());
	ssize_t newline_position = _request_string.IndexOf(&(NewLines[0]));

	if(newline_position >= 0)
	{
		// \r\n\r\n를 찾았다면, 파싱 시작
		_request_string.SetLength(newline_position);

		// data 에서 사용한 데이터 수 = [\r\n\r\n 찾은 후 문자열의 길이] - [찾기 전 문자열의 길이] + [\r\n\r\n 길이]
		used_length = _request_string.GetLength() - previous_length + NewLinesLength;

		_is_header_found = true;
		_parse_status = ParseMessage();

		if(_parse_status == HttpStatusCode::OK)
		{
			// Content length와 같은 정보 계산
			PostProcess();
		}
		else
		{
			// 파싱 도중 오류 발생
			used_length = -1L;
		}
	}
	else
	{
		// 아직 데이터가 덜 들어와서 헤더를 파싱 할 수 없음
	}

	// 사용한 데이터 길이 반환
	return used_length;
}

HttpStatusCode HttpRequest::ParseMessage()
{
	// RFC7230 - 3. Message Format
	// HTTP-message   = start-line
	//                  *( header-field CRLF )
	//                  CRLF
	//                  [ message-body ]

	// RFC7230 - 3.1. Start Line
	// start-line     = request-line / status-line

	// 줄바꿈 문자를 기준으로 tokenize
	std::vector<ov::String> tokens = _request_string.Split("\r\n");

	HttpStatusCode status_code;

	status_code = ParseRequestLine(tokens[0]);
	tokens.erase(tokens.begin());

	if(status_code == HttpStatusCode::OK)
	{
		for(const ov::String &token : tokens)
		{
			status_code = ParseHeader(token);

			if(status_code != HttpStatusCode::OK)
			{
				break;
			}
		}
	}

	logtd("Headers found: %ld:", _request_header.size());

	std::for_each(_request_header.begin(), _request_header.end(), [](const auto &pair) -> void
	{
		logtd("\t>> %s: %s", pair.first.CStr(), pair.second.CStr());
	});

	return status_code;
}

#define HTTP_COMPARE_METHOD(text, value_if_matches) \
    if(method == text) \
    { \
        _method = value_if_matches; \
    }

HttpStatusCode HttpRequest::ParseRequestLine(const ov::String &line)
{
	// RFC7230 - 3.1.1. Request Line
	// request-line   = method SP request-target SP HTTP-version CRLF
	ssize_t first_space_index = line.IndexOf(' ');
	ssize_t last_space_index = line.IndexOfRev(' ');

	if((first_space_index < 0) || (last_space_index < 0) || (first_space_index == last_space_index))
	{
		logtw("Invalid space index: first: %d, last: %d, line: %s", first_space_index, last_space_index, line.CStr());
		return HttpStatusCode::BadRequest;
	}

	_method = HttpMethod::Unknown;

	// RFC7231 - 4. Request Methods
	ov::String method = line.Left(static_cast<size_t>(first_space_index));

	HTTP_COMPARE_METHOD("GET", HttpMethod::Get);
	HTTP_COMPARE_METHOD("HEAD", HttpMethod::Head);
	HTTP_COMPARE_METHOD("POST", HttpMethod::Post);
	HTTP_COMPARE_METHOD("PUT", HttpMethod::Put);
	HTTP_COMPARE_METHOD("DELETE", HttpMethod::Delete);
	HTTP_COMPARE_METHOD("CONNECT", HttpMethod::Connect);
	HTTP_COMPARE_METHOD("OPTIONS", HttpMethod::Options);
	HTTP_COMPARE_METHOD("TRACE", HttpMethod::Trace);

	if(_method == HttpMethod::Unknown)
	{
		logtw("Unknown method: %s", method.CStr());
		return HttpStatusCode::MethodNotAllowed;
	}

	// RFC7230 - 5.3. Request Target
	// request-target = origin-form
	//            / absolute-form
	//            / authority-form
	//            / asterisk-form
	_request_target = line.Substring(first_space_index + 1, static_cast<size_t>(last_space_index - first_space_index - 1));

	// RFC7230 - 2.6. Protocol Versioning
	// HTTP-version  = HTTP-name "/" DIGIT "." DIGIT
	// HTTP-name     = %x48.54.54.50 ; "HTTP", case-sensitive
	_http_version = line.Substring(last_space_index + 1);

	logtd("Method: [%s], uri: [%s], version: [%s]", method.CStr(), _request_target.CStr(), _http_version.CStr());
	return HttpStatusCode::OK;
}

HttpStatusCode HttpRequest::ParseHeader(const ov::String &line)
{
	// RFC7230 - 3.2.  Header Fields
	// header-field   = field-name ":" OWS field-value OWS
	//
	// field-name     = token
	// field-value    = *( field-content / obs-fold )
	// field-content  = field-vchar [ 1*( SP / HTAB ) field-vchar ]
	// field-vchar    = VCHAR / obs-text
	//
	// obs-fold       = CRLF 1*( SP / HTAB )
	//                ; obsolete line folding
	//                ; see Section 3.2.4

	// 다음과 같은 사유로, obs-fold에 대해 처리하지 않음
	// RFC7230 - 3.2.4.  Field Parsing
	// Historically, HTTP header field values could be extended over
	// multiple lines by preceding each extra line with at least one space
	// or horizontal tab (obs-fold).  This specification deprecates such
	// line folding except within the message/http media type
	// (Section 8.3.1).  A sender MUST NOT generate a message that includes
	// line folding (i.e., that has any field-value that contains a match to
	// the obs-fold rule) unless the message is intended for packaging
	// within the message/http media type.

	ssize_t colon_index = line.IndexOf(':');

	if(colon_index == -1)
	{
		// 잘못된 헤더
		logtw("Invalid header (could not find colon): %s", line.CStr());
		return HttpStatusCode::BadRequest;
	}

	// 모든 헤더 이름은 대문자로 처리
	ov::String field_name = line.Left(static_cast<size_t>(colon_index)).UpperCaseString();
	// 처리를 용이하게 하기 위해 OWS(optional white space) 없앰
	ov::String field_value = line.Substring(colon_index + 1).Trim();

	_request_header[field_name] = field_value;

	return HttpStatusCode::OK;
}

ov::String HttpRequest::GetHeader(const ov::String &key) const noexcept
{
	return GetHeader(key, "");
}

ov::String HttpRequest::GetHeader(const ov::String &key, ov::String default_value) const noexcept
{
	auto item = _request_header.find(key.UpperCaseString());

	if(item == _request_header.cend())
	{
		return std::move(default_value);
	}

	return item->second;
}

const bool HttpRequest::IsHeaderExists(const ov::String &key) const noexcept
{
	return _request_header.find(key) != _request_header.cend();
}

void HttpRequest::PostProcess()
{
	switch(_method)
	{
		case HttpMethod::Get:
			// http body가 없음
			_content_length = 0L;
			break;

		case HttpMethod::Post:
			// http body가 있을 수도 있으므로 파싱 시도
			_content_length = ov::Converter::ToInt64(GetHeader("CONTENT-LENGTH", "0"));
			break;

		default:
			// 나머지 헤더
			_content_length = ov::Converter::ToInt64(GetHeader("CONTENT-LENGTH", "0"));
			break;
	}
}

ov::String HttpRequest::ToString() const
{
	return ov::String::FormatString("<HttpRequest: %p, Method: %d, uri: %s>", this, static_cast<int>(_method), GetUri().CStr());
}