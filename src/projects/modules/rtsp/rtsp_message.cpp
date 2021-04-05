#include "rtsp_message.h"

ov::String RtspMessage::RtspMethodToString(RtspMethod method)
{
	switch(method)
	{
		case RtspMethod::DESCRIBE:
			return "DESCRIBE";
		case RtspMethod::ANNOUNCE:
			return "ANNOUNCE";
		case RtspMethod::GET_PARAMETER:
			return "GET_PARAMETER";
		case RtspMethod::OPTIONS:
			return "OPTIONS";
		case RtspMethod::PAUSE:
			return "PAUSE";
		case RtspMethod::PLAY:
			return "PLAY";
		case RtspMethod::RECORD:
			return "RECORD";
		case RtspMethod::REDIRECT:
			return "REDIRECT";
		case RtspMethod::SETUP:
			return "SETUP";
		case RtspMethod::SET_PARAMETER:
			return "SET_PARAMETER";
		case RtspMethod::TEARDOWN:
			return "TEARDOWN";
		case RtspMethod::UNKNOWN:
		default:
			return "UNKNOWN";
	}
}

std::tuple<std::shared_ptr<RtspMessage>, int> RtspMessage::Parse(const std::shared_ptr<const ov::Data> &data)
{

	return {nullptr, -1};
}

int RtspMessage::ParseInternal(const std::shared_ptr<const ov::Data> &data)
{

	_is_header_data_uptodate = false;
	return 0;
}

RtspMessage::RtspMessage(RtspMethod method, uint32_t cseq, ov::String request_uri)
{
	RtspMessage(RtspMethodToString(method), cseq, request_uri);
}

RtspMessage::RtspMessage(ov::String method, uint32_t cseq, ov::String request_uri)
{
	_rtsp_version = RTSP_VERSION;
	_type = RtspMessageType::REQUEST;
	_method_string = method;
	_request_uri = request_uri;
	_cseq = cseq;

	// CSeq
	// https://tools.ietf.org/html/rfc2326#section-12.17
	// This field MUST be present in all requests and responses.
	AddHeaderField(std::make_shared<RtspHeaderField>(RtspHeaderFieldType::CSeq, _cseq));
}

RtspMessage::RtspMessage(uint32_t status_code, uint32_t cseq, ov::String reason_phrase)
{
	_rtsp_version = RTSP_VERSION;
	_type = RtspMessageType::RESPONSE;
	_status_code = status_code;
	_reason_phrase = reason_phrase;
	_cseq = cseq;
	
	// CSeq
	// https://tools.ietf.org/html/rfc2326#section-12.17
	// This field MUST be present in all requests and responses.
	AddHeaderField(std::make_shared<RtspHeaderField>(RtspHeaderFieldType::CSeq, _cseq));
}

bool RtspMessage::AddHeaderField(const std::shared_ptr<RtspHeaderField> &field)
{
	_header_fields.emplace(field->GetName(), field);

	_is_header_data_uptodate = false;

	return true;
}

bool RtspMessage::DelHeaderField(ov::String field_name)
{
	_header_fields.erase(field_name);

	_is_header_data_uptodate = false;

	return true;
}

bool RtspMessage::SetBody(const std::shared_ptr<ov::Data> &body_data)
{
	_body = body_data;

	// Update "Content-length" field
	
	// https://tools.ietf.org/html/rfc2326#section-12.14
	// Unlike HTTP, it MUST be included in all messages that carry content beyond the header
	// portion of the message. If it is missing, a default value of zero is
	// assumed. It is interpreted according to [H14.14].

	if(_body != nullptr && _body->GetLength() > 0)
	{
		DelHeaderField(RtspHeaderField::FieldTypeToString(RtspHeaderFieldType::ContentLength));
		
		auto field = std::make_shared<RtspHeaderField>();
		field->SetContent(RtspHeaderField::FieldTypeToString(RtspHeaderFieldType::ContentLength), body_data->GetLength());
		AddHeaderField(field);

		_is_header_data_uptodate = false;
	}

	return true;
}

bool RtspMessage::SerializeHeader()
{
	ov::String header;

	if(_type == RtspMessageType::REQUEST)
	{
		//<Method> <URI> RTSP/1.0\r\n
		header.Format("%s %s RTSP/%s\r\n", _method_string.CStr(), _request_uri.CStr(), _rtsp_version.CStr());
	}
	else if(_type == RtspMessageType::RESPONSE)
	{
		// RTSP/1.0 <Status code> <Reason phrase>\r\n
		header.Format("RTSP/%s %d %s", _rtsp_version.CStr(), _status_code, _reason_phrase.CStr());
	}
	else
	{
		return false;
	}

	// Header fields
	for(const auto &it : _header_fields)
	{
		auto field = it.second;

		header.Append(field->Serialize());
		header.Append("\r\n");
	}

	header.Append("\r\n");

	_header = header.ToData(false);

	_is_header_data_uptodate = true;

	return true;
}

std::shared_ptr<ov::Data> RtspMessage::GetHeader()
{
	if(_is_header_data_uptodate == false)
	{
		// remake header data
		if(SerializeHeader() == false)
		{
			return nullptr;
		}
	}

	return _header;
}

std::shared_ptr<ov::Data> RtspMessage::GetBody()
{
	return _body;
}

std::shared_ptr<ov::Data> RtspMessage::GetMessage()
{
	auto message = std::make_shared<ov::Data>();

	auto header = GetHeader();
	if(header == nullptr)
	{
		return nullptr;
	}

	message->Append(header);
	
	auto body = GetBody();
	if(body != nullptr)
	{
		message->Append(body);
	}

	return message;
}

uint32_t RtspMessage::GetStatusCode()
{
	return _status_code;
}

ov::String RtspMessage::GetReasonPhrase()
{
	return _reason_phrase;
}