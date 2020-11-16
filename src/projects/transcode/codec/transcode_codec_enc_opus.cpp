//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "transcode_codec_enc_opus.h"

#include <unistd.h>

#include <opus/opus.h>

#define OV_LOG_TAG "TranscodeCodec"

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

OvenCodecImplAvcodecEncOpus::~OvenCodecImplAvcodecEncOpus()
{
	Stop();

	if (_encoder)
	{
		::opus_encoder_destroy(_encoder);
	}
}

bool OvenCodecImplAvcodecEncOpus::Configure(std::shared_ptr<TranscodeContext> context)
{
	if (TranscodeEncoder::Configure(context) == false)
	{
		return false;
	}

	// Initialize OPUS encoder
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

	// (48000Hz / 100ms) * 6 = 2880 samples / 600ms
	const int max_opus_frame_count = (48000 / 100) * 6;
	// OPUS supports up to 256, but only 16 are used here.
	const int estimated_channel_count = 16;
	// OPUS supports int16 or float
	const int estimated_frame_size = std::max(sizeof(opus_int16), sizeof(float));

	_buffer = std::make_shared<ov::Data>(max_opus_frame_count * estimated_channel_count * estimated_frame_size);
	_format = cmn::AudioSample::Format::None;
	_current_pts = -1;

	// Generates a thread that reads and encodes frames in the input_buffer queue and places them in the output queue.
	try
	{
		_kill_flag = false;

		_thread_work = std::thread(&OvenCodecImplAvcodecEncOpus::ThreadEncode, this);
	}
	catch (const std::system_error &e)
	{
		_kill_flag = true;

		logte("Failed to start transcode stream thread.");
	}


	return true;
}

void OvenCodecImplAvcodecEncOpus::Stop()
{
	_kill_flag = true;

	_queue_event.Notify();

	if (_thread_work.joinable())
	{
		_thread_work.join();
		logtd("OPUS encoder thread has ended.");
	}
}

void OvenCodecImplAvcodecEncOpus::ThreadEncode()
{
	const unsigned int frame_count_to_encode = 480 * 2;
	const unsigned int bytes_to_encode = frame_count_to_encode * _output_context->GetAudioChannel().GetCounts() * _output_context->GetAudioSample().GetSampleSize();

	int64_t duration = 0LL;

	while(!_kill_flag)
	{
		if (_buffer->GetLength() < bytes_to_encode)
		{
			// dequeue
			_queue_event.Wait();

			std::unique_lock<std::mutex> mlock(_mutex);

			if (_input_buffer.empty())
			{
				continue;
			}

			auto frame_buffer = std::move(_input_buffer.front());
			_input_buffer.pop_front();

			mlock.unlock();

			const MediaFrame *frame = frame_buffer.get();

			OV_ASSERT2(frame != nullptr);

			// Store frame informations
			_format = frame->GetFormat<cmn::AudioSample::Format>();

			if (_current_pts == -1)
			{
				_current_pts = frame->GetPts();
			}

			duration += frame->GetDuration();

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
				case cmn::AudioSample::Format::FltP:
				{
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

		int encoded_bytes = -1;

		// "1275 * 3 + 7" formula is used in opusenc.c:813
		// or, use the formula in "AudioEncoderOpusImpl::SufficientOutputBufferSize()" of the native code.
		std::shared_ptr<ov::Data> encoded = std::make_shared<ov::Data>(1275 * 3 + 7);
		encoded->SetLength(encoded->GetCapacity());

		// Encode
		switch (_format)
		{
		case cmn::AudioSample::Format::S16:
			encoded_bytes = ::opus_encode(_encoder, _buffer->GetDataAs<const opus_int16>(), frame_count_to_encode, encoded->GetWritableDataAs<unsigned char>(), static_cast<opus_int32>(encoded->GetCapacity()));
			break;

		case cmn::AudioSample::Format::Flt:
			encoded_bytes = ::opus_encode_float(_encoder, _buffer->GetDataAs<float>(), frame_count_to_encode, encoded->GetWritableDataAs<unsigned char>(), static_cast<opus_int32>(encoded->GetCapacity()));
			break;

		default:
			// *result = TranscodeResult::DataError;
			continue;
		}

		if (encoded_bytes < 0)
		{
			logte("An error occurred while encode data %zu bytes: %d (Buffer: %zu bytes)", _buffer->GetLength(), encoded_bytes, encoded->GetCapacity());
			// *result = TranscodeResult::DataError;
			continue;
		}

		encoded->SetLength(static_cast<size_t>(encoded_bytes));

		// Data is encoded successfully
		// dequeue <bytes_to_encoded> bytes
		auto buffer = _buffer->GetWritableDataAs<uint8_t>();
		::memmove(buffer, buffer + bytes_to_encode, _buffer->GetLength() - bytes_to_encode);
		_buffer->SetLength(_buffer->GetLength() - bytes_to_encode);

		auto packet_buffer = std::make_shared<MediaPacket>(cmn::MediaType::Audio, 1, encoded, _current_pts, _current_pts, duration, MediaPacketFlag::Key);
		packet_buffer->SetBitstreamFormat(cmn::BitstreamFormat::OPUS);
		packet_buffer->SetPacketType(cmn::PacketType::RAW);
		
		_current_pts += frame_count_to_encode;
		// logte("opus pts : %lld, queue:%d, buffer:%d", _current_pts, _input_buffer.size(), _buffer->GetLength());
		// *result = TranscodeResult::DataReady;
		
		SendOutputBuffer(std::move(packet_buffer));
	
		duration = 0L;		
	}

}

std::shared_ptr<MediaPacket> OvenCodecImplAvcodecEncOpus::RecvBuffer(TranscodeResult *result)
{
	std::unique_lock<std::mutex> mlock(_mutex);

	if (!_output_buffer.empty())
	{
		*result = TranscodeResult::DataReady;

		auto packet = std::move(_output_buffer.front());
		_output_buffer.pop_front();

		return std::move(packet);
	}

	*result = TranscodeResult::NoData;

	return nullptr;
}
