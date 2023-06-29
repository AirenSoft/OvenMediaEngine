//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Keukhan Kwon
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <shared_mutex>

#include "base/ovlibrary/ovlibrary.h"

namespace info
{
	typedef uint32_t managed_queue_id_t;


	class ManagedQueue
	{
	public:
		explicit ManagedQueue(size_t threshold = 0)
			: _peak(0),
			  _size(0),
			  _threshold(threshold),
			  _threshold_exceeded_time_in_us(0),
			  _imc(0),
			  _omc(0),
			  _imps(0),
			  _omps(0),
			  _waiting_time_in_us(0),
			  _dc(0){};

		void SetId(info::managed_queue_id_t id)
		{
			_id = id;
		}

		info::managed_queue_id_t GetId() const
		{
			return _id;
		}

		ov::String GetUrn() const
		{
			return _urn;
		}

		ov::String GetTypeName() const
		{
			return _type_name;
		}

		void SetThreshold(size_t threshold)
		{
			auto lock_guard = std::lock_guard(_name_mutex);

			_threshold = threshold;
		}

		size_t GetThreshold() const
		{
			return _threshold;
		}

		size_t GetPeak() const
		{
			return _peak;
		}

		size_t GetSize() const
		{
			return _size;
		}

		size_t GetIMPS() const
		{
			return _imps;
		}

		size_t GetOMPS() const
		{
			return _omps;
		}

		uint64_t GetDropCount() const
		{
			return _dc;
		}

		int64_t GetWaitingTimeInUs() const
		{
			return _waiting_time_in_us;
		}

		int64_t GetThresholdExceededTimeInUs() const
		{
			return _threshold_exceeded_time_in_us;
		}
		
		void SetUrn(const char* urn, const char* type_name)
		{
			auto lock_guard = std::lock_guard(_name_mutex);

			_type_name = type_name;

			if ((urn != nullptr) && (urn[0] != '\0'))
			{
				_urn = urn;
			}
			else
			{
				_urn.Format("Queue<%s>", type_name);
			}
		}

	protected:
		// ID of the queue
		managed_queue_id_t _id = 0;

		// Name of the queue
		std::shared_mutex _name_mutex;
		ov::String _urn;

		// Type of template
		ov::String _type_name;

		// Peak size of the queue
		size_t _peak;

		// Current size of the queue
		size_t _size;

		// Threshold of the queue
		size_t _threshold;

		// threshold_exceeded_time increases from the point the queue is exceeded
		int64_t _threshold_exceeded_time_in_us;

		// Input Message Count
		size_t _imc;

		// Output Message Count
		size_t _omc;

		// Input Message Per Second
		size_t _imps;

		// Output Message Per Second
		size_t _omps;

		// Average Waiting Time(microseconds)
		int64_t _waiting_time_in_us;

		// Drop Count
		uint64_t _dc;

	public:		
		// Create URN specification for queue
		/*
			[URN Pattern]
				- mngq:v={VhostName}#{AppName}[s=/{StreamName}]:p={PART}:r={ROLE}

			[PART]
				- pvd: provider
				- imr: mediarouter(inbound)
				- trs: transcoder
				- omr: mediarouter(outbound)
				- pub: publisher

			[ROLE]
				- filter_{video|audio}
				- encoder_{codec_name}_{trackid}
				- decoder_{codec_name}_{trackid}
				- appworker_[{protocol}]_{id}
				- stremworker_[{protocol}]_{id}

			examples
				- mngq:v=#default#app:s=stream:p=trs:r=decoer_h264_0
				- mngq:v=#default#app:s=stream:p=trs:r=filter_video
				- mngq:v=#default#app:s=stream:p=trs:r=filter_audio
				- mngq:v=#default#app:s=stream:p=trs:r=encoder_opus_0
				- mngq:v=#default#app:s=stream:p=trs:r=encoder_h264_1
				- mngq:v=#default#app:p=imr:r=indicator
				- mngq:v=#default#app:p=omr:r=appworker
				- mngq:v=#default#app:p=pub:r=appworker
		*/
		static ov::String URN(const char* vhost_app = nullptr, const char* stream = nullptr, const char* part = nullptr, const char* role = nullptr)
		{
			
			ov::String uri = "mngq";

			if(vhost_app)
			{
				uri.Append(":");
				uri.Append("v=");
				uri.Append(vhost_app);
			}
			if(stream)
			{
				uri.Append(":");
				uri.Append("s=");
				uri.Append(stream);
			}

			if(part)
			{
				uri.Append(":");
				uri.Append("p=");
				uri.Append(part);
			}	

			if(role)
			{
				uri.Append(":");
				uri.Append("r=");
				uri.Append(role);
			}

			return uri;
		}

		static ov::String ParseVHostApp(const char* urn)
		{
			ov::String tmp(urn);

			auto start_pos = tmp.IndexOf(":v=");
			if(start_pos == -1L)
			{
				return "";
			}

			auto end_pos = tmp.IndexOf(":", start_pos + 3);
			if(end_pos == -1L)
			{
				return tmp.Substring(start_pos + 3);
			}

			size_t length = end_pos - start_pos - 3;
			return tmp.Substring(start_pos + 3, length);
		}

		static ov::String ParseStream(const char* urn)
		{
			ov::String tmp(urn);

			auto start_pos = tmp.IndexOf(":s=");
			if(start_pos == -1L)
			{
				return "";
			}

			auto end_pos = tmp.IndexOf(":", start_pos + 3);
			if(end_pos == -1L)
			{
				return tmp.Substring(start_pos + 3);
			}

			size_t length = end_pos - start_pos - 3;
			return tmp.Substring(start_pos + 3, length);
		}
	};

}  // namespace info