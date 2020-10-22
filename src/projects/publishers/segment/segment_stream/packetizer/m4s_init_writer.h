//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/info/media_track.h>

#include "m4s_writer.h"

class M4sInitWriter : public M4sWriter
{
public:
	M4sInitWriter(M4sMediaType media_type,
				  // duration in second
				  uint32_t duration,
				  std::shared_ptr<MediaTrack> video_track, std::shared_ptr<MediaTrack> audio_track,
				  const std::shared_ptr<const ov::Data> &avc_sps, const std::shared_ptr<const ov::Data> &avc_pps);

	~M4sInitWriter() override = default;

public:
	const std::shared_ptr<ov::Data> CreateData();

protected:
	int FtypBoxWrite(std::shared_ptr<ov::Data> &data_stream);
	int MoovBoxWrite(std::shared_ptr<ov::Data> &data_stream);

	int MvhdBoxWrite(std::shared_ptr<ov::Data> &data_stream);

	int TrakBoxWrite(std::shared_ptr<ov::Data> &data_stream);
	int TkhdBoxWrite(std::shared_ptr<ov::Data> &data_stream);
	//int EdtsBoxWrite(std::shared_ptr<ov::Data> &data_stream);
	//int ElstBoxWrite(std::shared_ptr<ov::Data> &data_stream);
	int MdiaBoxWrite(std::shared_ptr<ov::Data> &data_stream);
	int MdhdBoxWrite(std::shared_ptr<ov::Data> &data_stream);
	int HdlrBoxWrite(std::shared_ptr<ov::Data> &data_stream);
	int MinfBoxWrite(std::shared_ptr<ov::Data> &data_stream);

	int VmhdBoxWrite(std::shared_ptr<ov::Data> &data_stream);
	int SmhdBoxWrite(std::shared_ptr<ov::Data> &data_stream);

	int DinfBoxWrite(std::shared_ptr<ov::Data> &data_stream);
	int DrefBoxWrite(std::shared_ptr<ov::Data> &data_stream);
	int UrlBoxWrite(std::shared_ptr<ov::Data> &data_stream);

	int StblBoxWrite(std::shared_ptr<ov::Data> &data_stream);
	int StsdBoxWrite(std::shared_ptr<ov::Data> &data_stream);

	int Avc1BoxWrite(std::shared_ptr<ov::Data> &data_stream);
	int AvccBoxWrite(std::shared_ptr<ov::Data> &data_stream);
	int Mp4aBoxWrite(std::shared_ptr<ov::Data> &data_stream);
	int EsdsBoxWrite(std::shared_ptr<ov::Data> &data_stream);

	int SttsBoxWrite(std::shared_ptr<ov::Data> &data_stream);
	int StscBoxWrite(std::shared_ptr<ov::Data> &data_stream);
	int StszBoxWrite(std::shared_ptr<ov::Data> &data_stream);
	int StcoBoxWrite(std::shared_ptr<ov::Data> &data_stream);

	int MvexBoxWrite(std::shared_ptr<ov::Data> &data_stream);
	int MehdBoxWrite(std::shared_ptr<ov::Data> &data_stream);
	int TrexBoxWrite(std::shared_ptr<ov::Data> &data_stream);

private:
	uint32_t _duration = 0U;

	int _track_id = 0;

	std::shared_ptr<MediaTrack> _main_track;
	std::shared_ptr<MediaTrack> _video_track;
	std::shared_ptr<MediaTrack> _audio_track;

	// avc Configuration
	std::shared_ptr<const ov::Data> _avc_sps;
	std::shared_ptr<const ov::Data> _avc_pps;

	uint32_t _audio_sample_index = 0U;

	ov::String _language;
	ov::String _handler_type;
	ov::String _compressor_name;
};