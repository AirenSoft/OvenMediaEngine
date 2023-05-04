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
			: _peak(0), _size(0), _threshold(threshold), _imc(0), _omc(0), _imps(0), _omps(0), _dc(0) { };

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

		size_t GetWaitingTimeInUs() const
		{
			return _waiting_time_in_us;
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

		// Input Message Count
		size_t _imc;

		// Output Message Count
		size_t _omc;

		// Input Message Per Second
		size_t _imps;

		// Output Message Per Second
		size_t _omps;

		// average waiting time(microseconds)
		size_t _waiting_time_in_us;

		// Drop Count
		uint64_t _dc;

	public:		
		// Create URN specification for queue
		/*
			[URN Pattern]
				- mngq:{VhostName}#{AppName}[/{StreamName}]:{PART}:{ROLE}

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
				- mngq:#default#app/stream:trs:decoer_h264_0
				- mngq:#default#app/stream:trs:filter_video
				- mngq:#default#app/stream:trs:filter_audio
				- mngq:#default#app/stream:trs:encoder_opus_0
				- mngq:#default#app/stream:trs:encoder_h264_1
				- mngq:#default#app:imr:indicator
				- mngq:#default#app:omr:appworker
				- mngq:#default#app:pub:appworker
		*/
		static ov::String URN(const char* vhost_app = nullptr, const char* stream = nullptr, const char* part = nullptr, const char* role = nullptr)
		{
			
			ov::String uri = "mngq:";

			if(vhost_app)
			{
				uri.Append(vhost_app);
			}
			if(stream)
			{
				uri.Append("/");
				uri.Append(stream);
			}

			if(part)
			{
				uri.Append(":");
				uri.Append(part);
			}	

			if(role)
			{
				uri.Append(":");
				uri.Append(role);
			}

			return uri;
		}	
	};

}  // namespace info