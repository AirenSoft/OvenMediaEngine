#pragma once

#include <base/mediarouter/media_buffer.h>
#include <base/mediarouter/media_type.h>

#include <cstdint>

#include "filter/filter_base.h"
#include "transcoder_context.h"

enum class TranscodeFilterType : int8_t
{
	None = -1,
	AudioResampler,
	VideoRescaler,

	Count  ///< Number of sample formats. DO NOT USE if linking dynamically
};

class TranscodeFilter
{
public:
	typedef std::function<void(int32_t, std::shared_ptr<MediaFrame>)> CompleteHandler;

public:
	TranscodeFilter();

	~TranscodeFilter();

	bool Configure(int32_t filter_id, std::shared_ptr<MediaTrack> input_track, std::shared_ptr<MediaTrack> output_track, CompleteHandler complete_handler);

	bool SendBuffer(std::shared_ptr<MediaFrame> buffer);

	void Stop(); 
	cmn::Timebase GetInputTimebase() const;
	cmn::Timebase GetOutputTimebase() const;

	int64_t _last_pts = -1LL;
	int64_t _threshold_ts_increment = 0LL;

	std::shared_ptr<MediaTrack> _input_track;
	std::shared_ptr<MediaTrack> _output_track;

	void SetAlias(ov::String alias);

	void SetCompleteHandler(CompleteHandler func)
	{
		_complete_handler = move(func);
	}

	void OnComplete(std::shared_ptr<MediaFrame> frame);

private:
	bool CreateFilter();
	bool IsNeedUpdate(std::shared_ptr<MediaFrame> buffer);

	int32_t _filter_id;

	FilterBase *_impl;

	ov::String _alias;

	CompleteHandler _complete_handler;
};
