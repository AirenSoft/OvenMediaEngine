#pragma once

#include "base/common_types.h"
#include "base/info/media_track.h"

namespace info
{
	typedef uint32_t stream_id_t;
	constexpr stream_id_t InvalidStreamId = std::numeric_limits<stream_id_t>::max();
	constexpr stream_id_t MinStreamId = std::numeric_limits<stream_id_t>::min();
	constexpr stream_id_t MaxStreamId = (InvalidStreamId - static_cast<stream_id_t>(1));

	class Application;

	//TODO: It should be changed class name to Stream
	class Stream
	{
	public:
		Stream(StreamSourceType source);
		Stream(const info::Application &app_info, StreamSourceType source);
		Stream(const info::Application &app_info, info::stream_id_t stream_id, StreamSourceType source);
		Stream(const Stream &stream);
		virtual ~Stream();

		bool operator==(const Stream &stream_info) const;

		void SetId(info::stream_id_t id);
		info::stream_id_t GetId() const;

		void SetMsid(uint32_t );
		uint32_t GetMsid();

		ov::String GetUUID() const;
		ov::String GetName() const;
		void SetName(ov::String name);

		ov::String GetMediaSource() const;
		void SetMediaSource(ov::String url);

		bool IsInputStream() const;
		bool IsOutputStream() const;

		void LinkInputStream(const std::shared_ptr<Stream> &stream);
		const std::shared_ptr<Stream> GetLinkedInputStream() const;

		// Only used in OVT provider
		void SetOriginStreamUUID(const ov::String &uuid);
		ov::String GetOriginStreamUUID() const;

		const std::chrono::system_clock::time_point &GetCreatedTime() const;
		uint32_t GetUptimeSec();
		StreamSourceType GetSourceType() const;

		bool AddTrack(std::shared_ptr<MediaTrack> track);
		const std::shared_ptr<MediaTrack> GetTrack(int32_t id) const;
		const std::map<int32_t, std::shared_ptr<MediaTrack>> &GetTracks() const;

		ov::String GetInfoString();
		void ShowInfo();


		void SetApplicationInfo(const std::shared_ptr<Application> &app_info)
		{
			_app_info = app_info;
		}
		const Application &GetApplicationInfo() const
		{
			return *_app_info;
		}

		const char* GetApplicationName();

	protected:
		info::stream_id_t _id = 0;
		uint32_t _msid = 0;
		ov::String _name;
		ov::String _source_url;
		
		// Key : MediaTrack ID
		std::map<int32_t, std::shared_ptr<MediaTrack>> _tracks;

	private:
		std::chrono::system_clock::time_point _created_time;
		// Where does the stream come from?
		StreamSourceType _source_type;

		std::shared_ptr<Application>	_app_info = nullptr;

		// If the Source Type of this stream is LiveTranscoder,
		// the original stream coming from the Provider can be recognized with _origin_stream.
		std::shared_ptr<Stream> 	_origin_stream = nullptr;

		// If the source if this stream is a remote stream of the origin server, store the uuid of origin stream
		ov::String _origin_stream_uuid;
	};
}  // namespace info