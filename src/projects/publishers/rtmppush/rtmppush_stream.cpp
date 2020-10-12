#include "rtmppush_private.h"
#include "rtmppush_stream.h"
#include "base/publisher/application.h"
#include "base/publisher/stream.h"
#include <regex>

std::shared_ptr<RtmpPushStream> RtmpPushStream::Create(const std::shared_ptr<pub::Application> application,
											 const info::Stream &info)
{
	auto stream = std::make_shared<RtmpPushStream>(application, info);
	if(!stream->Start())
	{
		return nullptr;
	}
	return stream;
}

RtmpPushStream::RtmpPushStream(const std::shared_ptr<pub::Application> application,
					 const info::Stream &info)
		: Stream(application, info),
		_writer(nullptr)
{
}

RtmpPushStream::~RtmpPushStream()
{
	logtd("RtmpPushStream(%s/%s) has been terminated finally", 
		GetApplicationName() , GetName().CStr());
}

bool RtmpPushStream::Start()
{
	logtd("RtmpPushStream(%ld) has been started", GetId());

	//-----------------
	// for test
	// std::vector<int32_t> selected_tracks;
	// RecordStart(selected_tracks);
	//-----------------

	return Stream::Start();
}

bool RtmpPushStream::Stop()
{
	logtd("RtmpPushStream(%u) has been stopped", GetId());

	//-----------------
	// for test
	// RecordStop();
	//-----------------

	return Stream::Stop();
}


void RtmpPushStream::PushStart()
{
#if 0	
	_writer = RtmpWriter::Create();

	ov::String tmp_output_fullpath = GetOutputTempFilePath();
	ov::String tmp_output_directory = ov::PathManager::ExtractPath(tmp_output_fullpath).CStr();

	logtd("Temp output path : %s", tmp_output_fullpath.CStr());
	logtd("Temp output directory : %s", tmp_output_directory.CStr());

	// Create temporary directory
	if(MakeDirectoryRecursive(tmp_output_directory.CStr()) == false)
	{
		logte("Could not create directory. path(%s)", tmp_output_directory.CStr());
		
		return;
	}

	// Record it on a temporary path.
	if(_writer->SetPath(tmp_output_fullpath, "mpegts") == false)
	{
		_writer = nullptr;

		return;
	}

	for(auto &track_item : _tracks)
	{
		auto &track = track_item.second;

		// If the selected track list exists. if the current trackid does not exist on the list, ignore it.
		// If no track list is selected, save all tracks.
		if( (selected_tracks.empty() != true) && 
			(std::find(selected_tracks.begin(), selected_tracks.end(), track->GetId()) == selected_tracks.end()) )
		{
			continue;
		}

		auto track_info = RtmpTrackInfo::Create();

		track_info->SetCodecId( track->GetCodecId() );
		track_info->SetBitrate( track->GetBitrate() );
		track_info->SetTimeBase( track->GetTimeBase() );
		track_info->SetWidth( track->GetWidth() );
		track_info->SetHeight( track->GetHeight() );
		track_info->SetSample( track->GetSample() );
		track_info->SetChannel( track->GetChannel() );
		track_info->SetExtradata( track->GetCodecExtradata() );

		bool ret = _writer->AddTrack(track->GetMediaType(), track->GetId(), track_info);
		if(ret == false)
		{
			logte("Failed to add new track");
		}
	}

	if(_writer->Start() == false)
	{
		_writer = nullptr;
	}
#endif	
}

void RtmpPushStream::PushStop()
{
#if 0	
	if(_writer == nullptr)
		return;

	// End recording.
	_writer->Stop();
#endif	
}

void RtmpPushStream::PushStat()
{
	
}

void RtmpPushStream::SendVideoFrame(const std::shared_ptr<MediaPacket> &media_packet)
{
	if(_writer == nullptr)
		return;

	bool ret = _writer->PutData(
		media_packet->GetTrackId(), 
		media_packet->GetPts(),
		media_packet->GetDts(), 
		media_packet->GetFlag(), 
		media_packet->GetData());

	if(ret == false)
	{
		logte("Failed to add packet");
	}	
}

void RtmpPushStream::SendAudioFrame(const std::shared_ptr<MediaPacket> &media_packet)
{
	if(_writer == nullptr)
		return;

	bool ret = _writer->PutData(
		media_packet->GetTrackId(), 
		media_packet->GetPts(),
		media_packet->GetDts(), 
		media_packet->GetFlag(), 
		media_packet->GetData());

	if(ret == false)
	{
		logte("Failed to add packet");
	}	
}
