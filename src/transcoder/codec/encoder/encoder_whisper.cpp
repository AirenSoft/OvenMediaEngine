//==============================================================================
//
//  Whisper encoder for speech recognition and subtitle generation.
//
//  Created by Getroot
//  Copyright (c) 2025 AirenSoft. All rights reserved.
//
//==============================================================================
#include <orchestrator/orchestrator.h>
#include <base/modules/data_format/webvtt/webvtt_frame.h>
#include <base/event/command/commands.h>
#include <base/ovlibrary/stop_watch.h>

#include "encoder_whisper.h"
#include "../../transcoder_private.h"
#include "../../transcoder_whisper_model_registry.h"

#include <algorithm>
#include <vector>

// Room left after the longest sliding window, below Whisper's 30 s input and below the
// encoder context (see Configure).
static constexpr int64_t kMaxWindowMarginMs = 1000;
// Audio covered by one Whisper encoder position (the unit of audio_ctx): two 10 ms mel frames.
static constexpr int64_t kAudioCtxPositionMs = 20;
// AudioContextMs value (and default) that sizes the encoder context from the window.
static constexpr int32_t kAudioContextAuto = -1;
// Automatic context: whole multiples of kAutoCtxAlignPositions (with flash attention the
// decoder's cross-attention reads the context padded to 256 positions in any case), and
// no smaller than kAutoCtxMinPositions, because accuracy drops faster below it (measured
// on one test stream: precision 89.8% at 768, 87.1% at 541, 90.9% at the full 1500).
// The cost of the context depends on the device: on a GPU 541 saved little over 768
// (GPU use 17% against 18%), while on the CPU the time of a call grows roughly with the
// context (measured with 5 threads, median per call: 0.60 s at 240, 1.56 s at 541,
// 2.07 s at 768).
static constexpr int64_t kAutoCtxAlignPositions = 256;
static constexpr int64_t kAutoCtxMinPositions = 768;

// A decoding loop repeats one phrase back to back ("No, no, no, ..." for hundreds of
// characters, or a whole sentence over and over). Every run of a phrase of up to
// kRepeatMaxPhraseWords words repeated back to back is collapsed to its first occurrence,
// wherever it is in the cue; the text before and after the run is kept.
//
// How this rule was chosen (measured on recorded test streams with reference subtitles, and
// on live streams where loops and real speech were told apart by reading the cues):
// - A 4-word phrase seen 3 times anywhere in a cue also matched real speech that repeats
//   with other words in between ("Thank you for the prime. Sir ... Thank you for the six
//   month prime. Neo. Thank you for the ..."), so only back-to-back repetition counts.
// - Cutting the cue from the second occurrence to its end, and only when the repetition
//   reached the end, missed loops that other words follow (a 798-character "Lucky. Lucky.
//   ..." cue was published as is) and dropped the words after a loop ("Oh, oh, oh, ...
//   five dollars!" became "Oh,"). Collapsing covers both. On the recorded test streams it lost
//   half as many reference words (4 against 8) and raised the precision of the most
//   loop-prone test configuration further (46.8% to 75.1%, against 67.8%).
// - Short phrases need more repeats, because "no, no, no" is ordinary speech. At 5 repeats,
//   sound tags and short loops ("[Music]" x5, "Alright." x6) are collapsed, and so is
//   emphatic speech ("No, no, no, no, no." becomes "No."). At 8 the emphasis would stay, and
//   so would the tags. 5 was chosen: the meaning of the emphasis survives.
// - A phrase repeated with other words in between is left alone.
static constexpr int kRepeatMaxPhraseWords = 16;
static constexpr int kRepeatLongPhraseWords = 3;
static constexpr int kRepeatMinRepeatsLong = 3;   // phrases of kRepeatLongPhraseWords words or more
static constexpr int kRepeatMinRepeatsShort = 5;  // shorter phrases

// Punctuation outside ASCII, which separates words like ASCII punctuation does. Only
// scripts written without spaces need it: in a script with spaces, a loop is already split
// at the spaces, and a repeat that carries the same punctuation each time still matches.
// So this covers the general punctuation and the punctuation of Chinese and Japanese. A
// loop with neither spaces nor punctuation between its phrases is not collapsed.
static bool IsRepeatPunctuation(uint32_t code_point)
{
	// General Punctuation ("…", "—", quotes), except the apostrophe "’"
	return ((code_point >= 0x2000) && (code_point <= 0x206F) && (code_point != 0x2019)) ||
		   // Ideographic space, "、", "。", "〃"
		   ((code_point >= 0x3000) && (code_point <= 0x3003)) ||
		   // CJK brackets and quotes ("「", "」", "【", "】", ...) and "〜"
		   ((code_point >= 0x3008) && (code_point <= 0x3011)) ||
		   ((code_point >= 0x3014) && (code_point <= 0x301F)) ||
		   // Japanese middle dot "・"
		   (code_point == 0x30FB) ||
		   // Full-width ASCII punctuation ("！", "，", "：", "？", ...) and half-width "｡", "｢", "｣", "､", "･"
		   ((code_point >= 0xFF01) && (code_point <= 0xFF0F)) ||
		   ((code_point >= 0xFF1A) && (code_point <= 0xFF20)) ||
		   ((code_point >= 0xFF3B) && (code_point <= 0xFF40)) ||
		   ((code_point >= 0xFF5B) && (code_point <= 0xFF65));
}

// Whether the character at data[i] belongs to a word, with its length in bytes in
// `char_length`. Words are ASCII letters, digits and apostrophes, and the characters of
// other scripts except their punctuation, so words of every script are compared, not only
// Latin ones. A byte that does not start a valid UTF-8 sequence counts as a one-byte word
// character.
static bool IsRepeatWordChar(const char *data, size_t length, size_t i, size_t *char_length)
{
	const auto c = static_cast<unsigned char>(data[i]);
	*char_length = 1;
	if (c < 0x80)
	{
		return ((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')) || ((c >= '0') && (c <= '9')) || (c == '\'');
	}

	size_t sequence_length = 0;
	uint32_t code_point = 0;
	if ((c & 0xE0) == 0xC0)
	{
		sequence_length = 2;
		code_point = c & 0x1F;
	}
	else if ((c & 0xF0) == 0xE0)
	{
		sequence_length = 3;
		code_point = c & 0x0F;
	}
	else if ((c & 0xF8) == 0xF0)
	{
		sequence_length = 4;
		code_point = c & 0x07;
	}
	else
	{
		return true;
	}

	if (i + sequence_length > length)
	{
		return true;
	}

	for (size_t k = 1; k < sequence_length; k++)
	{
		const auto next = static_cast<unsigned char>(data[i + k]);
		if ((next & 0xC0) != 0x80)
		{
			return true;
		}
		code_point = (code_point << 6) | (next & 0x3F);
	}

	*char_length = sequence_length;
	return IsRepeatPunctuation(code_point) == false;
}

// Returns `text` with every back-to-back repeated phrase collapsed to its first occurrence,
// and the number of collapsed runs in `runs`.
static ov::String CollapseRepeatedPhrases(const ov::String &text, int *runs)
{
	const char *data = text.CStr();
	const size_t length = text.GetLength();

	// Words (ASCII lowercased, so "No" and "no" match; punctuation is not compared) and the
	// byte offset just past each word.
	std::vector<ov::String> words;
	std::vector<size_t> word_ends;
	for (size_t i = 0; i < length;)
	{
		size_t char_length = 1;
		if (IsRepeatWordChar(data, length, i, &char_length) == false)
		{
			i += char_length;
			continue;
		}

		ov::String word;
		while ((i < length) && IsRepeatWordChar(data, length, i, &char_length))
		{
			const auto c = static_cast<unsigned char>(data[i]);
			if ((c >= 'A') && (c <= 'Z'))
			{
				word.Append(static_cast<char>(c - 'A' + 'a'));
			}
			else
			{
				word.Append(data + i, char_length);
			}
			i += char_length;
		}
		words.push_back(std::move(word));
		word_ends.push_back(i);
	}

	// From each word, take the phrase length whose back-to-back repetition covers the most
	// words. When it repeats often enough, copy the text up to the end of its first
	// occurrence and resume after the last repeat, so the separator that follows the run
	// ("...oh, five dollars!") is kept.
	const int n = static_cast<int>(words.size());
	ov::String out;
	size_t copied = 0;
	*runs = 0;
	for (int i = 0; i < n;)
	{
		int best_p = 0;
		int best_repeats = 0;
		for (int p = 1; (p <= kRepeatMaxPhraseWords) && (i + p <= n); p++)
		{
			const int min_repeats = (p >= kRepeatLongPhraseWords) ? kRepeatMinRepeatsLong : kRepeatMinRepeatsShort;
			int repeats = 1;
			while ((i + (repeats + 1) * p <= n) &&
				   std::equal(words.begin() + i, words.begin() + i + p, words.begin() + i + repeats * p))
			{
				repeats++;
			}

			if ((repeats >= min_repeats) && (repeats * p > best_repeats * best_p))
			{
				best_p = p;
				best_repeats = repeats;
			}
		}

		if (best_p == 0)
		{
			i++;
			continue;
		}

		out.Append(data + copied, word_ends[i + best_p - 1] - copied);
		copied = word_ends[i + best_repeats * best_p - 1];
		i += best_repeats * best_p;
		(*runs)++;
	}

	if (*runs == 0)
	{
		return text;
	}
	out.Append(data + copied, length - copied);
	return out;
}

EncoderWhisper::EncoderWhisper(const info::Stream &stream_info)
	: TranscodeEncoder(stream_info)
{
}

EncoderWhisper::~EncoderWhisper()
{
}

bool EncoderWhisper::SetCodecParams()
{
	return true;
}

bool EncoderWhisper::Configure(std::shared_ptr<MediaTrack> context)
{
	auto parent_stream_info = _stream_info.GetLinkedInputStream();
	if (parent_stream_info == nullptr)
	{
		logte("Whisper encoder requires an input stream.");
		return false;
	}

	_parent_stream = ocst::Orchestrator::GetInstance()->GetProviderStream(parent_stream_info);
	if (_parent_stream == nullptr)
	{
		logte("Whisper encoder requires an input stream. (%s/%s)", parent_stream_info->GetApplicationName(), parent_stream_info->GetName().CStr());
		return false;
	}

	_step_ms = context->GetStepMs();
	_length_ms = context->GetLengthMs();
	_keep_ms = context->GetKeepMs();
	_audio_context_ms = context->GetAudioContextMs();

	// Reject window settings that ThreadLoop cannot run. The configuration accepts any
	// integer, and some values would otherwise crash the whole process instead of
	// disabling this non-essential track. Messages name only configuration items.
	ov::String invalid_reason;
	if (_step_ms <= 0)
	{
		// _length_ms / _step_ms below would divide by zero.
		invalid_reason = "StepMs must be greater than 0.";
	}
	else if (_length_ms <= 0)
	{
		invalid_reason = "LengthMs must be greater than 0.";
	}
	else if (_keep_ms < 0)
	{
		// KeepMs is a length of audio, so a negative value has no meaning. (The original
		// cycle reset read past the end of the buffer with it.)
		invalid_reason = "KeepMs must not be negative.";
	}
	else if ((_audio_context_ms <= 0) && (_audio_context_ms != kAudioContextAuto))
	{
		invalid_reason = "AudioContextMs must be -1 (auto) or greater than 0.";
	}
	else
	{
		// LengthMs below StepMs is accepted: the cycle is then a single step, so every
		// window is reset (a fixed-length window).
		_n_new_lines = std::max(1, (_length_ms / _step_ms) - 1);

		// Longest window ThreadLoop builds: the audio kept over a reset plus the new
		// audio of one cycle. The previous audio taken into a window is limited to
		// KeepMs + LengthMs - new, so with LengthMs below StepMs the window is
		// KeepMs + LengthMs, or one step of new audio alone when that is longer.
		const int64_t max_window_ms = std::max(static_cast<int64_t>(_step_ms),
											   static_cast<int64_t>(_keep_ms) +
												   std::min(static_cast<int64_t>(_n_new_lines) * _step_ms, static_cast<int64_t>(_length_ms)));

		// Whisper decodes in 30 s windows (WHISPER_CHUNK_SIZE). An input that fills or
		// exceeds one makes a single call run the encoder more than once: with no
		// trailing silence left, Whisper treats the last segment as cut off and decodes
		// the remainder in another window. Audio is collected in whole input frames, so
		// every step overshoots StepMs by up to one frame (~21 ms for 48 kHz AAC, 64 ms
		// for 16 kHz AAC); kMaxWindowMarginMs absorbs that over a typical cycle and keeps
		// some padding after the audio. Longer cycles with long frames are covered by the
		// run-time limit in ThreadLoop.
		const int64_t window_limit_ms = WHISPER_CHUNK_SIZE * 1000 - kMaxWindowMarginMs;
		if (max_window_ms > window_limit_ms)
		{
			invalid_reason = ov::String::FormatString("The window grows to %" PRId64 " ms; it must not exceed %" PRId64 " ms to fit Whisper's %d ms input. Lower LengthMs or KeepMs.",
													  max_window_ms, window_limit_ms, WHISPER_CHUNK_SIZE * 1000);
		}
		else
		{
			_max_window_ms = static_cast<int32_t>(max_window_ms);
		}
	}

	if (invalid_reason.IsEmpty() == false)
	{
		logte("Invalid Whisper sliding window. stream=%s, track_id=%d, label=%s, step_ms=%d, length_ms=%d, keep_ms=%d, audio_context_ms=%d: %s STT track will be disabled.",
			  _stream_info.GetName().CStr(), context->GetId(), context->GetOutputTrackLabel().CStr(),
			  _step_ms, _length_ms, _keep_ms, _audio_context_ms, invalid_reason.CStr());
		return false;
	}

	// Encoder context (Whisper's audio_ctx): the audio, from the start of the window, that
	// the encoder runs over. The full 30 s also runs it over the zero padding after the
	// window; a shorter context saves that compute but costs accuracy (measured on one
	// test stream: precision 90.9% at 30000 ms, 90.0% at 20480, 89.8% at 15360, 87.1% at
	// 10820).
	// A window longer than the context does not fit: Whisper decodes the rest in another
	// encoder pass, and the text past the context was measured to drop words, loop and
	// hallucinate. So the context always covers the longest window plus the margin.
	const int64_t min_context_ms = static_cast<int64_t>(_max_window_ms) + kMaxWindowMarginMs;
	if (_audio_context_ms == kAudioContextAuto)
	{
		// Sized from the window: a longer window costs more compute and gives more accuracy.
		const int64_t min_positions = (min_context_ms + kAudioCtxPositionMs - 1) / kAudioCtxPositionMs;
		const int64_t positions = std::max(kAutoCtxMinPositions,
										   (min_positions + kAutoCtxAlignPositions - 1) / kAutoCtxAlignPositions * kAutoCtxAlignPositions);
		_audio_context_ms = static_cast<int32_t>(std::min(positions * kAudioCtxPositionMs, static_cast<int64_t>(WHISPER_CHUNK_SIZE) * 1000));
		logti("Whisper AudioContextMs auto: %d ms for the longest window of %d ms. stream=%s, track_id=%d, label=%s",
			  _audio_context_ms, _max_window_ms,
			  _stream_info.GetName().CStr(), context->GetId(), context->GetOutputTrackLabel().CStr());
	}
	else if (_audio_context_ms < min_context_ms)
	{
		logtw("Whisper AudioContextMs %d ms is shorter than the longest window (%d ms) plus %" PRId64 " ms; using %" PRId64 " ms. stream=%s, track_id=%d, label=%s",
			  _audio_context_ms, _max_window_ms, kMaxWindowMarginMs, min_context_ms,
			  _stream_info.GetName().CStr(), context->GetId(), context->GetOutputTrackLabel().CStr());
		_audio_context_ms = static_cast<int32_t>(min_context_ms);
	}
	else if (_audio_context_ms > WHISPER_CHUNK_SIZE * 1000)
	{
		// The encoder never sees more than Whisper's 30 s input, so a longer value means the
		// full context; the log then shows the value actually used.
		logtw("Whisper AudioContextMs %d ms is longer than Whisper's %d ms input; using %d ms. stream=%s, track_id=%d, label=%s",
			  _audio_context_ms, WHISPER_CHUNK_SIZE * 1000, WHISPER_CHUNK_SIZE * 1000,
			  _stream_info.GetName().CStr(), context->GetId(), context->GetOutputTrackLabel().CStr());
		_audio_context_ms = WHISPER_CHUNK_SIZE * 1000;
	}
	// 0 is the model's full context (30 s); otherwise round up to whole positions.
	_audio_ctx = (_audio_context_ms >= WHISPER_CHUNK_SIZE * 1000)
					 ? 0
					 : static_cast<int32_t>((_audio_context_ms + kAudioCtxPositionMs - 1) / kAudioCtxPositionMs);
	// The window must also fit at run time. Each step collects whole input frames, so it
	// overshoots StepMs by up to one frame, and that adds up over a cycle: more than the
	// margin above with many steps per cycle and long frames (128 ms for 8 kHz AAC).
	// ThreadLoop takes less previous audio when a window would grow past the context.
	_n_samples_context = static_cast<int32_t>(((_audio_ctx > 0) ? (_audio_ctx * kAudioCtxPositionMs) : (WHISPER_CHUNK_SIZE * 1000)) *
											  WHISPER_SAMPLE_RATE / 1000);

	_n_samples_step = (static_cast<double>(_step_ms) / 1000.0) * WHISPER_SAMPLE_RATE;
	_n_samples_length = (static_cast<double>(_length_ms) / 1000.0) * WHISPER_SAMPLE_RATE;
	_n_samples_keep = (static_cast<double>(_keep_ms) / 1000.0) * WHISPER_SAMPLE_RATE;
	
	// SourceLanguage is "auto" or a language Whisper knows, by code ("ko") or by name
	// ("korean"), stored as the code. Whisper does not reject an unknown value: it logs an
	// error on every call and decodes with an invalid language token, so the output
	// language is unpredictable. An unknown value (including an empty one) is replaced with "auto" and a
	// warning, so subtitles keep coming. Case and surrounding spaces are ignored.
	const ov::String requested_language = context->GetSourceLanguage().Trim().LowerCaseString();
	const int requested_language_id = (requested_language == "auto") ? -1 : whisper_lang_id(requested_language.CStr());
	ov::String source_language = "auto";
	if (requested_language_id >= 0)
	{
		source_language = whisper_lang_str(requested_language_id);
	}
	else if (requested_language != "auto")
	{
		logtw("Unsupported SourceLanguage \"%s\"; using auto. stream=%s, track_id=%d, label=%s",
			  context->GetSourceLanguage().CStr(),
			  _stream_info.GetName().CStr(), context->GetId(), context->GetOutputTrackLabel().CStr());
	}
	{
		ov::LockGuard lock(_source_language_mutex);
		_source_language = source_language;
	}
	_translate = context->ShouldTranslate();
	_output_track_label = context->GetOutputTrackLabel();

	// Initialize muted state from config (can be changed at runtime via API).
	_audio_muted.store(!context->IsSttEnabled(), std::memory_order_relaxed);

	// The base Configure starts the encoding thread (ThreadLoop) and waits for init.
	return TranscodeEncoder::Configure(context, _n_samples_step);
}

// Called by CodecThread
bool EncoderWhisper::Initialize()
{
	// check sample rate and channel
	if (_track->GetSampleRate() != WHISPER_SAMPLE_RATE)
	{
		logte("Whisper encoder only supports 16kHz sample rate. sample_rate=%d", _track->GetSampleRate());
		return false;
	}

	if (_track->GetChannel().GetCounts() != 1)
	{
		logte("Whisper encoder only supports mono channel. channels=%d", _track->GetChannel().GetCounts());
		return false;
	}

	// Acquire the shared model context from the registry. The model stays loaded for
	// the encoder's lifetime; only the per-instance whisper_state is allocated dynamically.
	_whisper_ctx = WhisperModelRegistry::GetInstance()->GetModelContext(_track->GetModel(), _device_id);
	if (_whisper_ctx == nullptr)
	{
		// Error already logged by the registry.
		return false;
	}

	// If STT is enabled in the configuration (<STT><Enable>true</Enable>), allocate the
	// per-instance whisper_state eagerly so the encoder is ready to transcribe the moment
	// the stream starts. If it starts disabled, the state is allocated later when enabled
	// via the API (see CodecThread / AllocWhisperState / FreeWhisperState).
	if (_audio_muted.load(std::memory_order_relaxed) == false)
	{
		if (AllocWhisperState() == false)
		{
			_whisper_ctx.reset();
			return false;
		}
	}

	// Window lengths fed to Whisper. The first window is one step of new audio (the
	// buffer starts empty, and again after STT is re-enabled); windows then grow up
	// to the computed maximum. audio_ctx is the encoder context actually used (in
	// positions of 20 ms), from AudioContextMs as Configure set it: sized from the window
	// (auto), or the configured value raised to cover the longest window.
	logti("Whisper sliding window configured. stream=%s, track_id=%d, label=%s, step_ms=%d, length_ms=%d, keep_ms=%d, window_ms=%d~%d, audio_context_ms=%d, audio_ctx=%d",
		  _stream_info.GetName().CStr(), _track->GetId(), _output_track_label.CStr(),
		  _step_ms, _length_ms, _keep_ms,
		  _step_ms, _max_window_ms,
		  _audio_context_ms, (_audio_ctx > 0) ? _audio_ctx : whisper_model_n_audio_ctx(_whisper_ctx.get()));

	return true;
}

// Allocates the per-instance whisper_state via the registry.
// The registry serializes allocation and pre-checks available memory so an
// oversubscribed server refuses the state instead of being OOM-killed.
bool EncoderWhisper::AllocWhisperState()
{
	if (_whisper_state != nullptr)
	{
		return true;
	}

	_whisper_state = WhisperModelRegistry::GetInstance()->NewState(_track->GetModel(), _device_id);
	if (_whisper_state == nullptr)
	{
		logte("Failed to create whisper state. stream=%s, track_id=%d, label=%s, model=%s",
			  _stream_info.GetName().CStr(), _track->GetId(), _output_track_label.CStr(), _track->GetModel().CStr());
		return false;
	}

	// Initial share for the log line below; recomputed before every window.
	_n_threads			   = WhisperModelRegistry::GetInstance()->GetThreadShare(_track->GetThreadCount());
	_state_marked_resident = false;

	logti("Whisper state created. stream=%s, track_id=%d, label=%s, model=%s, threads=%d",
		  _stream_info.GetName().CStr(), _track->GetId(), _output_track_label.CStr(), _track->GetModel().CStr(), _n_threads);

	return true;
}

// Releases the per-instance whisper_state; its share of the thread budget goes
// back to the other tracks on their next window.
void EncoderWhisper::FreeWhisperState()
{
	if (_whisper_state != nullptr)
	{
		WhisperModelRegistry::GetInstance()->DeleteState(_whisper_state);
		_whisper_state = nullptr;
		_n_threads = 0;
		_state_marked_resident = false;

		logti("Whisper state released. stream=%s, track_id=%d, label=%s",
			  _stream_info.GetName().CStr(), _track->GetId(), _output_track_label.CStr());
	}
}

void EncoderWhisper::ThreadLoop()
{
	ov::logger::ThreadHelper thread_helper;

	// Initialize the codec and notify the main thread.
	if (_codec_init_event.Submit(Initialize()) == false)
	{
		return;
	}

	int32_t n_samples_30s = (static_cast<double>(30000.0) / 1000.0) * WHISPER_SAMPLE_RATE;

	// PCM float32 buffer for 30 seconds
	std::vector<float> pcmf32_buffer;
	std::vector<float> pcmf32_buffer_old;
	std::vector<float> pcmf32_buffer_new;

	pcmf32_buffer.reserve(n_samples_30s);
	pcmf32_buffer_old.reserve(n_samples_30s);
	pcmf32_buffer_new.reserve(n_samples_30s);

	int64_t new_buffer_start_cs = 0;
	int64_t new_buffer_end_cs = 0;
	int64_t last_commit_end_cs = 0;

	int32_t n_iter = 0;
	while (!_kill_flag)
	{
		// Collect audio samples until n_samples_step is reached.
		new_buffer_start_cs = 0;
		new_buffer_end_cs = 0;
		pcmf32_buffer_new.clear();
		while (!_kill_flag)
		{
			// React to STT enable/disable transitions before waiting for audio.
			// Pause()/Resume() flip _audio_muted and call _input_buffer.InjectWakeup(),
			// which wakes the Dequeue() below so this runs promptly even when no
			// audio is flowing.
			if (_audio_muted.load(std::memory_order_relaxed))
			{
				// Disabled: release the per-instance whisper_state to free its
				// memory and discard the rolling-window so a later resume is clean.
				if (_whisper_state != nullptr)
				{
					FreeWhisperState();

					pcmf32_buffer_old.clear();
					pcmf32_buffer_new.clear();
					new_buffer_start_cs = 0;
					new_buffer_end_cs = 0;
					last_commit_end_cs = 0;
					n_iter = 0;
				}
			}
			else if (_whisper_state == nullptr)
			{
				// Enabled but no state yet: lazily (re)allocate it. Allocation can
				// fail under memory pressure, so retries are throttled.
				auto now = std::chrono::steady_clock::now();
				if (now - _last_state_alloc_fail_ts >= std::chrono::seconds(5))
				{
					if (AllocWhisperState() == false)
					{
						_last_state_alloc_fail_ts = now;
					}
				}
			}

			auto obj = _input_buffer.Dequeue();
			if (obj.has_value() == false)
			{
				// Woken by InjectWakeup() (state transition) or stopped; re-evaluate.
				continue;
			}

			// Drop the frame without inference while muted, or until the
			// whisper_state has been (re)allocated after a failed attempt.
			if (_audio_muted.load(std::memory_order_relaxed) || _whisper_state == nullptr)
			{
				continue;
			}

			auto media_frame = std::move(obj.value());
			OV_ASSERT2(media_frame != nullptr);

			int64_t start_ts = media_frame->GetPts();
			int64_t end_ts = start_ts + media_frame->GetDuration();

			start_ts = ov::Converter::Rescale(start_ts, 100, _track->GetTimeBase().GetDen());
			end_ts = ov::Converter::Rescale(end_ts, 100, _track->GetTimeBase().GetDen());

			auto av_frame = static_cast<AVFrame *>(media_frame->GetPrivData());
			if (!av_frame)
			{
				logte("Could not allocate the frame data");
				break;
			}

			auto format = media_frame->GetFormat<cmn::AudioSample::Format>();
			auto channel_count = media_frame->GetChannelCount();

			if (format != cmn::AudioSample::Format::Flt || channel_count != 1)
			{
				logtw("Unsupported audio format for Whisper encoder. Only mono Flt is supported. pts=%" PRId64 ", format=%s, channels=%d, sample_rate=%d, nb_samples=%d",
					media_frame->GetPts(),
					format == cmn::AudioSample::Format::None ? "none" : format == cmn::AudioSample::Format::S16 ? "s16" : "other",
					channel_count,
					media_frame->GetSampleRate(),
					media_frame->GetNbSamples());
				continue;
			}

			// Mono float32
			const float* pcmf32_data = reinterpret_cast<const float*>(av_frame->data[0]);
			const int nb_samples = av_frame->nb_samples;

			// Append the new samples to the buffer.
			if (pcmf32_data != nullptr && nb_samples > 0)
			{
				pcmf32_buffer_new.insert(pcmf32_buffer_new.end(), pcmf32_data, pcmf32_data + nb_samples);
			}

			if (new_buffer_start_cs == 0 || start_ts < new_buffer_start_cs)
			{
				new_buffer_start_cs = start_ts;
			}

			if (end_ts > new_buffer_end_cs)
			{
				new_buffer_end_cs = end_ts;
			}

			// If we have enough samples, break the loop.
			if (pcmf32_buffer_new.size() >= static_cast<size_t>(_n_samples_step))
			{
				logtt("Collected %zu samples for Whisper processing. pts=%" PRId64 "", pcmf32_buffer_new.size(), media_frame->GetPts());
				break;
			}
		}

		// The inner loop also exits on _kill_flag (Stop). Skip inference then;
		// with STT disabled _whisper_state is null and would be dereferenced below.
		if (_kill_flag || _audio_muted.load(std::memory_order_relaxed) || _whisper_state == nullptr)
		{
			continue;
		}

		const int n_samples_new = static_cast<int>(pcmf32_buffer_new.size());
		// Previous audio taken into the window, limited so the window fits in the encoder
		// context (see Configure). Only the oldest audio is left out; every earlier
		// window of the cycle already had it.
		const int n_samples_old_keep = std::min({static_cast<int>(pcmf32_buffer_old.size()),
												 std::max(0, _n_samples_keep + _n_samples_length - n_samples_new),
												 std::max(0, _n_samples_context - n_samples_new)});

		pcmf32_buffer.resize(n_samples_new + n_samples_old_keep);

		// Copy old samples to the beginning of the buffer.
		for (int i = 0; i < n_samples_old_keep; i++)
		{
			pcmf32_buffer[i] = pcmf32_buffer_old[pcmf32_buffer_old.size() - n_samples_old_keep + i];
		}

		logtt("Processing Whisper with %d old samples and %d new samples (total %zu samples)", n_samples_old_keep, n_samples_new, pcmf32_buffer.size());

		// Copy new samples to the buffer.
		memcpy(pcmf32_buffer.data() + n_samples_old_keep, pcmf32_buffer_new.data(), n_samples_new * sizeof(float));
		pcmf32_buffer_old = pcmf32_buffer;

		int64_t buffer_start_cs = new_buffer_start_cs - (static_cast<double>(n_samples_old_keep) / WHISPER_SAMPLE_RATE * 100);
		int64_t buffer_end_cs = new_buffer_end_cs;

		// Threads for this window: the track's <ThreadCount> request capped by
		// an equal share of the server-wide budget. Re-read every window so the
		// share follows other STT tracks as they start and stop.
		const int32_t requested_threads = WhisperModelRegistry::ResolveRequestedThreads(_track->GetThreadCount());
		_n_threads						= WhisperModelRegistry::GetInstance()->GetThreadShare(requested_threads);
		if ((_n_threads < requested_threads) && _thread_share_warn_gate.TryConsume())
		{
			logtw("Whisper thread budget is shared by too many STT tracks: using %d of %d requested threads. Raise <MaxThreads> or run fewer STT tracks. stream=%s, track_id=%d, label=%s",
				  _n_threads, requested_threads, _stream_info.GetName().CStr(), _track->GetId(), _output_track_label.CStr());
		}

		// Everything from here to the end of whisper_full_with_state() has to
		// fit in one step, language detection included, so time it as a whole.
		ov::StopWatch inference_timer;
		inference_timer.Start();

		// Auto detect language if needed.
		if (_source_language == "auto" && _translate == false)
		{
			if (whisper_pcm_to_mel_with_state(_whisper_ctx.get(), _whisper_state, pcmf32_buffer.data(), pcmf32_buffer.size(), _n_threads) != 0)
			{
				logte("Failed to process audio samples for language detection with Whisper");
				continue;
			}

			std::vector<float> probs(whisper_lang_max_id() + 1, 0.0f);
			const auto lang_id = whisper_lang_auto_detect_with_state(_whisper_ctx.get(), _whisper_state, 0, _n_threads, probs.data());
			if (lang_id < 0)
			{
				logte("Failed to detect language with Whisper");
				continue;
			}

			auto lang_str = whisper_lang_str(lang_id);
			auto lang_prob = probs[lang_id];

			if (lang_prob > 0.9f)
			{
				// _source_language = lang_str;
				logti("Set source language [label : %s] to %s with high confidence (id=%d) with probabilities:[%f]", _track->GetOutputTrackLabel().CStr(), lang_str, lang_id, lang_prob);

				SendLangDetectionEvent(_track->GetOutputTrackLabel(), lang_str);
			}
			else
			{
				logtw("Detected language [label : %s] is not confident enough. Detected %s (id=%d) with probabilities:[%f]. Keep auto-detection. Please consider setting source_language manually.", _track->GetOutputTrackLabel().CStr(), lang_str, lang_id, lang_prob);
			}
		}
		else if (_source_language == "auto" && _translate == true)
		{
			SendLangDetectionEvent(_track->GetOutputTrackLabel(), "en");
			logti("Translation enabled. Set source language [label : %s] to English for Whisper processing.", _track->GetOutputTrackLabel().CStr());
		}

		whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
		wparams.print_progress   = false;
		wparams.print_special    = false;
		wparams.print_realtime   = false;
		wparams.print_timestamps = true;
		wparams.translate        = _translate;
		wparams.single_segment   = false;
		wparams.max_tokens       = 0; 
		wparams.language         = _translate ? "en" : _source_language.CStr();
		wparams.n_threads        = _n_threads;
		wparams.beam_search.beam_size = -1; // disable beam search
		wparams.greedy.best_of = 1; // disable best_of
		// Temperature fallback with whisper's default step (0.2): a decode that whisper
		// judges degenerate or low-confidence is retried at higher temperatures, up to 1.0.
		// It changed the result of only 1-4% of windows (measured), mostly repairing badly
		// wrong ones, but a call that retries can take longer than one step (measured on a
		// GPU, worst cases: 2.9 s and 3.9 s).
		wparams.temperature_inc = 0.2f;
		// Encoder context from AudioContextMs (see Configure), the same for every call.
		// Changing audio_ctx between calls on the same whisper_state was measured to
		// corrupt the results (a call no longer matches a fresh-state run of the same
		// window): the flash-attention path reads the cross-attention cache padded to 256
		// positions, and the padded part keeps what earlier calls left there. That, not
		// the context size, is what broke the earlier per-window sizing (window + 64
		// positions). The state's buffers are sized for the full context regardless, so
		// a smaller value saves compute only, not memory.
		wparams.audio_ctx        = _audio_ctx;
		wparams.tdrz_enable      = false;
		// No prompt is passed. A prompt would carry a degenerate (repeating) result into
		// the following calls, and the window after a reset already contains the audio
		// of the prompt's last words (the KeepMs overlap).
		wparams.prompt_tokens    = nullptr;
		wparams.prompt_n_tokens  = 0;
		wparams.token_timestamps = false;
		wparams.split_on_word	 = true;
		wparams.thold_pt		 = 0.01f;
		wparams.thold_ptsum		 = 0.01f;
		wparams.max_len			 = 0;

		logtt("Starting Whisper processing with %d samples", static_cast<int>(pcmf32_buffer.size()));
		logtt("Audio buffer time range for Whisper: %" PRId64 " ~ %" PRId64 " (last_commit_end_cs=%" PRId64 ")",
			buffer_start_cs, buffer_end_cs, last_commit_end_cs);

		if (whisper_full_with_state(_whisper_ctx.get(), _whisper_state, wparams, pcmf32_buffer.data(), pcmf32_buffer.size()) != 0)
		{
			logte("Failed to process audio samples with Whisper");
			continue;
		}
		const int64_t inference_ms = inference_timer.Elapsed();
		logtt("Whisper processing completed in %" PRId64 " ms", inference_ms);

		// The first pass faulted the state's compute buffers in; from here on
		// MemAvailable reflects them, so the admission reservation can go.
		if (_state_marked_resident == false)
		{
			WhisperModelRegistry::GetInstance()->MarkStateResident(_whisper_state);
			_state_marked_resident = true;
		}

		// One inference must finish within StepMs, otherwise the rolling window
		// falls behind the live audio and subtitles drift. Warn at most once a
		// minute so a struggling stream does not flood the log.
		if ((inference_ms > _step_ms) && _slow_inference_warn_gate.TryConsume())
		{
			logtw("Whisper inference is slower than real time (%" PRId64 " ms for a %d ms step) and subtitles will fall behind. "
				  "Use a smaller model, raise <ThreadCount>, or reduce the number of concurrent STT tracks. "
				  "stream=%s, track_id=%d, label=%s, model=%s, threads=%d",
				  inference_ms, _step_ms,
				  _stream_info.GetName().CStr(), _track->GetId(), _output_track_label.CStr(), _track->GetModel().CStr(), _n_threads);
		}

		ov::String result_text;
		
		const int n_segments = whisper_full_n_segments_from_state(_whisper_state);
		for (int i = 0; i < n_segments; ++i) 
		{
			const char *text = whisper_full_get_segment_text_from_state(_whisper_state, i);

			// How to output subtitles without overlapping : but the quality is low - commented out for now.
			// int64_t t0 = whisper_full_get_segment_t0(_whisper_ctx, i);
			// int64_t t1 = whisper_full_get_segment_t1(_whisper_ctx, i);

			// t0 += buffer_start_cs;
			// t1 += buffer_start_cs;

			// // sometimes, t1 is larger than the audio buffer end time due to bug?
			// if (t1 > buffer_end_cs)
			// {
			// 	t1 = buffer_end_cs;
			// }

			// logti("[%d] t0 : %" PRId64 ", t1 : %" PRId64 ", last_commit_end_cs : %" PRId64 ", text : %s", i, t0, t1, last_commit_end_cs, text);

			// if (t1 <= last_commit_end_cs)
			// {
			// 	// This segment has already been committed.
			// 	logtw("Ignoring Segment: t0=%" PRId64 ", t1=%" PRId64 ", last_commit_end_cs=%" PRId64 "", t0, t1, last_commit_end_cs);
			// 	continue;
			// }
			// else if (t0 < last_commit_end_cs)
			// {
			// 	// token
			// 	int nt = whisper_full_n_tokens(_whisper_ctx, i);
			// 	for (int it = 0; it < nt; ++it)
			// 	{
			// 		auto td = whisper_full_get_token_data(_whisper_ctx, i, it);
			// 		int64_t tt0 = td.t0 + buffer_start_cs;
			// 		int64_t tt1 = td.t1 + buffer_start_cs;

			// 		if (tt0 < 0 || tt1 < 0 || tt1 <= tt0)
			// 		{
			// 			continue;
			// 		}
			// 		if (tt1 <= last_commit_end_cs)
			// 		{
			// 			// This token has already been committed.
			// 			//logtw("Ignoring Token: t0=%" PRId64 ", t1=%" PRId64 ", last_commit_end_cs=%" PRId64 "", tt0, tt1, last_commit_end_cs);
			// 			continue;
			// 		}
			// 		else 
			// 		{
			// 			auto token_text = whisper_token_to_str(_whisper_ctx, td.id);
			// 			if (token_text && *token_text)
			// 			{
			// 				logti("\t\tToken Added: t0=%" PRId64 ", t1=%" PRId64 ", last_commit_end_cs=%" PRId64 ", text=%s", tt0, tt1, last_commit_end_cs, token_text);
			// 				result_text.Append(token_text);
			// 			}
			// 		}
			// 	}
			// }
			// else
			{
				result_text.Append(text);	
			}
		}

		if (last_commit_end_cs < buffer_end_cs)
		{
			last_commit_end_cs = buffer_end_cs;
		}

		logtt("[%" PRId64 "] : %s", last_commit_end_cs, result_text.CStr());

		// Keep a decoding loop off the screen: publish each repeated phrase once.
		int collapsed_runs = 0;
		const ov::String collapsed = CollapseRepeatedPhrases(result_text, &collapsed_runs);
		if (collapsed_runs > 0)
		{
			logtd("Whisper repetition loop collapsed. stream=%s, label=%s, %zu -> %zu chars, runs=%d",
				  _stream_info.GetName().CStr(), _output_track_label.CStr(),
				  result_text.GetLength(), collapsed.GetLength(), collapsed_runs);
			result_text = collapsed;
		}

		SendVttToProvider(result_text);

		result_text.Clear();

		// Counted within a cycle and reset at its end, so a long-running stream never
		// overflows it.
		n_iter++;
		if (n_iter >= _n_new_lines)
		{
			n_iter = 0;

			// End of a cycle: cut the window back to its last KeepMs, so a word cut at
			// this point is heard again at the start of the next window. The buffer can
			// be shorter than KeepMs (the first cycle starts from an empty buffer, and so
			// does the first one after STT is re-enabled), so keep at most all of it;
			// taking KeepMs unconditionally would read before the start of the buffer.
			const size_t n_samples_keep = std::min(pcmf32_buffer.size(), static_cast<size_t>(_n_samples_keep));
			pcmf32_buffer_old.assign(pcmf32_buffer.end() - n_samples_keep, pcmf32_buffer.end());
		}
	}

	FreeWhisperState();
	_whisper_ctx.reset();
}

bool EncoderWhisper::SendVttToProvider(const ov::String &text)
{
	if (_parent_stream == nullptr)
	{
		logte("[%s/%s] Whisper encoder requires an input stream.", _stream_info.GetApplicationName(), _stream_info.GetName().CStr());
		return false;
	}

	int64_t current_timestamp = _parent_stream->GetCurrentTimestampMs();
	int64_t start_timestamp = current_timestamp;
	int64_t end_timestamp = current_timestamp + _step_ms;
	ov::String settings = ov::String::FormatString("line:85%% size:75%%");

	auto vtt_frame = WebVTTFrame::Create(_output_track_label, start_timestamp, end_timestamp, settings, text);
	if (vtt_frame == nullptr)
	{
		logte("[%s/%s] Could not create VTT frame.", _stream_info.GetApplicationName(), _stream_info.GetName().CStr());
		return false;
	}

	if (_parent_stream->SendSubtitleFrame(_output_track_label, start_timestamp, _step_ms, cmn::BitstreamFormat::WebVTT, vtt_frame->Serialize(), true) == false)
	{
		logte("[%s/%s] Could not send VTT frame.", _stream_info.GetApplicationName(), _stream_info.GetName().CStr());
		return false;
	}

	return true;
}

bool EncoderWhisper::SendLangDetectionEvent(const ov::String &label, const ov::String &language)
{
	if (_parent_stream == nullptr)
	{
		logte("[%s/%s] Whisper encoder requires an input stream.", _stream_info.GetApplicationName(), _stream_info.GetName().CStr());
		return false;
	}

	if (_source_language == language)
	{
		// No change
		return true;
	}

	logti("Detected subtitle language: %s -> %s", _source_language.CStr(), language.CStr());

	{
		ov::LockGuard lock(_source_language_mutex);
		_source_language = language;
	}

	auto event = MediaEvent::BuildEvent(EventCommandUpdateLanguage::Create(label, language));
	event->SetHighPriority(true);
	if (_parent_stream->SendEvent(event) == false)
	{
		logte("[%s/%s] Could not send language detection event.", _stream_info.GetApplicationName(), _stream_info.GetName().CStr());
		return false;
	}

	return true;
}

ov::String EncoderWhisper::ToTimeString(int64_t ten_ms)
{
	int64_t total_seconds = ten_ms / 100;
	int64_t hours = total_seconds / 3600;
	int64_t minutes = (total_seconds % 3600) / 60;
	int64_t seconds = total_seconds % 60;
	int64_t milliseconds = (ten_ms % 100) * 10;

	return ov::String::FormatString("%02" PRId64 ":%02" PRId64 ":%02" PRId64 ".%03" PRId64 "", hours, minutes, seconds, milliseconds);
}

