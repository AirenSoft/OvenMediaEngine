//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "encoder_opus.h"

#include <opus/opus.h>
#include <unistd.h>

#include "../../transcoder_private.h"

#if 0
size_t AudioEncoderOpusImpl::SufficientOutputBufferSize() const {
  // Calculate the number of bytes we expect the encoder to produce,
  // then multiply by two to give a wide margin for error.
  const size_t bytes_per_millisecond =
	  static_cast<size_t>(GetBitrateBps(config_) / (1000 * 8) + 1);
  const size_t approx_encoded_bytes =
	  Num10msFramesPerPacket() * 10 * bytes_per_millisecond;
  return 2 * approx_encoded_bytes;
}
#endif

EncoderOPUS::~EncoderOPUS()
{
	Stop();

	if (_encoder)
	{
		::opus_encoder_destroy(_encoder);
	}
}

bool EncoderOPUS::Configure(std::shared_ptr<TranscodeContext> context)
{
	if (TranscodeEncoder::Configure(context) == false)
	{
		return false;
	}

	// Coding Mode
	// int application = OPUS_APPLICATION_VOIP;
	// int application = OPUS_APPLICATION_AUDIO;
	int application = OPUS_APPLICATION_RESTRICTED_LOWDELAY;
	int error;

	_encoder = ::opus_encoder_create(context->GetAudioSampleRate(), context->GetAudioChannel().GetCounts(), application, &error);
	if ((_encoder == nullptr) || (error != OPUS_OK))
	{
		logte("Could not create OPUS encoder: %d", error);

		::opus_encoder_destroy(_encoder);
		return false;
	}

	// Enable FEC
	::opus_encoder_ctl(_encoder, OPUS_SET_INBAND_FEC(1));
	::opus_encoder_ctl(_encoder, OPUS_SET_PACKET_LOSS_PERC(10));

	// Duration per frame
	// _expert_frame_duration = OPUS_FRAMESIZE_2_5_MS;
	// _expert_frame_duration = OPUS_FRAMESIZE_5_MS;
	// _expert_frame_duration = OPUS_FRAMESIZE_10_MS;
	_expert_frame_duration = OPUS_FRAMESIZE_20_MS;
	// _expert_frame_duration = OPUS_FRAMESIZE_40_MS;
	// _expert_frame_duration = OPUS_FRAMESIZE_60_MS;
	::opus_encoder_ctl(_encoder, OPUS_SET_EXPERT_FRAME_DURATION(_expert_frame_duration));

	// Bitrate
	::opus_encoder_ctl(_encoder, OPUS_SET_BITRATE(context->GetBitrate()));

	// (48000Hz / 100ms) * 6 = 2880 samples( == 60ms)
	const int max_opus_frame_count = (context->GetAudioSampleRate() / 100) * 6;
	// OPUS supports up to 256, but only 16 are used here.
	const int estimated_channel_count = 16;
	// OPUS supports int16 or float
	const int estimated_frame_size = std::max(sizeof(opus_int16), sizeof(float));

	// Setting the maximum size of PCM data to be encoded 
	_buffer = std::make_shared<ov::Data>(max_opus_frame_count * estimated_channel_count * estimated_frame_size);
	_format = cmn::AudioSample::Format::None;
	_current_pts = -1;

	// Generates a thread that reads and encodes frames in the input_buffer queue and places them in the output queue.
	try
	{
		_kill_flag = false;

		_thread_work = std::thread(&EncoderOPUS::ThreadEncode, this);
		pthread_setname_np(_thread_work.native_handle(),  ov::String::FormatString("Enc%s", avcodec_get_name(GetCodecID())).CStr());
	}
	catch (const std::system_error &e)
	{
		_kill_flag = true;

		logte("Failed to start transcode stream thread.");
	}

	return true;
}

void EncoderOPUS::Stop()
{
	_kill_flag = true;

	_input_buffer.Stop();
	_output_buffer.Stop();

	if (_thread_work.joinable())
	{
		_thread_work.join();
		logtd("OPUS encoder thread has ended.");
	}
}

void EncoderOPUS::ThreadEncode()
{
	// Refrence : https://opus-codec.org/docs/opus_api-1.1.3/group__opus__encoder.html#gad2d6bf6a9ffb6674879d7605ed073e25
	// Number of samples per channel in the input signal. This must be an Opus frame size for the encoder's sampling rate.
	// For example, at 48 kHz the permitted values are 120, 240, 480, 960, 1920, and 2880. Passing in a duration of less than 10 ms (480 samples at 48 kHz)

	//  frame_size is the duration of the frame in samples (per channel).
	const uint32_t default_frame_size = _output_context->GetAudioSampleRate() / 100;  // default 10ms
	unsigned int frame_size = 0;

	switch (_expert_frame_duration)
	{
		case OPUS_FRAMESIZE_2_5_MS:
			frame_size = default_frame_size / 4;
			break;
		case OPUS_FRAMESIZE_5_MS:
			frame_size = default_frame_size / 2;
			break;
		case OPUS_FRAMESIZE_10_MS:
			frame_size = default_frame_size;
			break;
		case OPUS_FRAMESIZE_20_MS:
			frame_size = default_frame_size * 2;
			break;
		case OPUS_FRAMESIZE_40_MS:
			frame_size = default_frame_size * 4;
			break;
		case OPUS_FRAMESIZE_60_MS:
			frame_size = default_frame_size * 6;
			break;
	}
	OV_ASSERT2(frame_size > 0);

	const unsigned int bytes_to_encode = frame_size * _output_context->GetAudioChannel().GetCounts() * _output_context->GetAudioSample().GetSampleSize();

	while (!_kill_flag)
	{
		// If there is no data to encode, the data is fetched from the queue.
		if (_buffer->GetLength() < bytes_to_encode)
		{
			auto obj = _input_buffer.Dequeue();
			if (obj.has_value() == false)
				continue;

			auto frame_buffer = std::move(obj.value());

			const MediaFrame *frame = frame_buffer.get();

			OV_ASSERT2(frame != nullptr);

			// Store frame informations
			_format = frame->GetFormat<cmn::AudioSample::Format>();

			if (_current_pts == -1)
			{
				_current_pts = frame->GetPts();
			}

			// Append frame data into the buffer
			if (frame->GetChannels() == 1)
			{
				// Just copy data into buffer
				_buffer->Append(frame->GetBuffer(0), frame->GetBufferSize(0));
			}
			else if (frame->GetChannels() >= 2)
			{
				// Currently, OME's OPUS encoder supports up to 2 channels
				switch (_format)
				{
					case cmn::AudioSample::Format::S16P:
					case cmn::AudioSample::Format::FltP: {
						// Need to interleave if sample type is planar
						off_t current_offset = _buffer->GetLength();

						// Reserve extra spaces
						size_t total_bytes = frame->GetBufferSize(0) + frame->GetBufferSize(1);
						_buffer->SetLength(current_offset + total_bytes);

						if (_format == cmn::AudioSample::Format::S16P)
						{
							// S16P
							ov::Interleave<int16_t>(_buffer->GetWritableDataAs<uint8_t>() + current_offset, frame->GetBuffer(0), frame->GetBuffer(1), frame->GetNbSamples());
							_format = cmn::AudioSample::Format::S16;
						}
						else
						{
							// FltP
							ov::Interleave<float>(_buffer->GetWritableDataAs<uint8_t>() + current_offset, frame->GetBuffer(0), frame->GetBuffer(1), frame->GetNbSamples());
							_format = cmn::AudioSample::Format::Flt;
						}

						break;
					}

					case cmn::AudioSample::Format::S16:
					case cmn::AudioSample::Format::Flt:
						// Do not need to interleave if sample type is non-planar
						_buffer->Append(frame->GetBuffer(0), frame->GetBufferSize(0));
						break;

					default:
						logte("Not supported format: %d", _format);
						break;
				}
			}
		}

		if (_buffer->GetLength() < bytes_to_encode)
		{
			// There is no data to encode
			// logte("There is no data to encode");
			continue;
		}
		OV_ASSERT2(_current_pts >= 0);
		OV_ASSERT2(_buffer->GetLength() >= bytes_to_encode);

		// "1275 * 3 + 7" formula is used in opusenc.c:813
		// or, use the formula in "AudioEncoderOpusImpl::SufficientOutputBufferSize()" of the native code.
		std::shared_ptr<ov::Data> encoded = std::make_shared<ov::Data>(1275 * 3 + 7);
		encoded->SetLength(encoded->GetCapacity());

		// result of opus_encode[_float] function
		//  The length of the encoded packet (in bytes) on success or a negative error code (see Error codes) on failure.
		int encoded_bytes = -1;
		// Encode
		switch (_format)
		{
			case cmn::AudioSample::Format::S16:
				encoded_bytes = ::opus_encode(_encoder, _buffer->GetDataAs<const opus_int16>(), frame_size, encoded->GetWritableDataAs<unsigned char>(), static_cast<opus_int32>(encoded->GetCapacity()));
				break;

			case cmn::AudioSample::Format::Flt:
				encoded_bytes = ::opus_encode_float(_encoder, _buffer->GetDataAs<float>(), frame_size, encoded->GetWritableDataAs<unsigned char>(), static_cast<opus_int32>(encoded->GetCapacity()));
				break;

			default:
				continue;
		}

		if (encoded_bytes < 0)
		{
			logte("An error occurred while encode data %zu bytes. error:%d", _buffer->GetLength(), encoded_bytes);
			continue;
		}

		encoded->SetLength(static_cast<size_t>(encoded_bytes));

		// Data is encoded successfully
		// dequeue <bytes_to_encoded> bytes
		auto buffer = _buffer->GetWritableDataAs<uint8_t>();
		::memmove(buffer, buffer + bytes_to_encode, _buffer->GetLength() - bytes_to_encode);
		_buffer->SetLength(_buffer->GetLength() - bytes_to_encode);

		int64_t duration = frame_size;

		auto packet_buffer = std::make_shared<MediaPacket>(cmn::MediaType::Audio, 0, encoded, _current_pts, _current_pts, duration, MediaPacketFlag::Key);
		packet_buffer->SetBitstreamFormat(cmn::BitstreamFormat::OPUS);
		packet_buffer->SetPacketType(cmn::PacketType::RAW);

		_current_pts += duration;

		SendOutputBuffer(std::move(packet_buffer));
	}
}

std::shared_ptr<MediaPacket> EncoderOPUS::RecvBuffer(TranscodeResult *result)
{
	if (!_output_buffer.IsEmpty())
	{
		*result = TranscodeResult::DataReady;

		auto obj = _output_buffer.Dequeue();
		if (obj.has_value())
		{
			return std::move(obj.value());
		}
	}

	*result = TranscodeResult::NoData;

	return nullptr;
}
