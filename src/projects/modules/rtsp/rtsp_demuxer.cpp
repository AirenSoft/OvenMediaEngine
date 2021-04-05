#include "rtsp_demuxer.h"

RtspDemuxer::RtspDemuxer()
{
	_buffer = std::make_shared<ov::Data>();
}

bool RtspDemuxer::AppendPacket(const std::shared_ptr<ov::Data> &packet)
{
	_buffer->Append(packet);

	while(_buffer->GetLength() > 0)
	{
		// Interleaved Binary data
		if(_buffer->At(0) == '$')
		{
			auto [rtsp_data, result] = RtspData::Parse(_buffer);
			// Success
			if(result > 0)
			{
				_datas.Enqueue(rtsp_data);
				_buffer->Erase(0, result);
				continue;
			}
			// Not enough buffer
			else if(result == 0)
			{
				break;
			}
			// Error
			else
			{
				return false;
			}
		}
		// Message
		else
		{
			auto [rtsp_message, result] = RtspMessage::Parse(_buffer);
			// Success
			if(result > 0)
			{
				_messages.Enqueue(rtsp_message);
				_buffer->Erase(0, result);
				continue;
			}
			// Not enough buffer
			else if(result == 0)
			{
				break;
			}
			// Error
			else
			{
				return false;
			}
		}
		
	}

	return true;
}

bool RtspDemuxer::IsAvailableMessage()
{

	return false;
}

bool RtspDemuxer::IsAvaliableData()
{

	return false;
}

std::shared_ptr<RtspMessage> RtspDemuxer::PopMessage()
{

	return nullptr;
}

std::shared_ptr<ov::Data> RtspDemuxer::PopData()
{

	return nullptr;
}