//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/info/media_track.h>
#include <base/mediarouter/media_buffer.h>

#define BMFF_BOX_HEADER_SIZE (8)		// size(4) + type(4)
#define BMFF_FULL_BOX_HEADER_SIZE (12)	// size(4) + type(4) + version(1) + flag(3)

namespace bmff
{
	class Packager
	{
	public:
		class Samples
		{
		public:
			bool AppendSample(const std::shared_ptr<const MediaPacket> &media_packet)
			{
				if (media_packet == nullptr)
				{
					return false;
				}

				if (_list.size() == 0)
				{
					_start_timestamp = media_packet->GetDts();
					if (media_packet->GetFlag() == MediaPacketFlag::Key)
					{
						_independent = true;
					}
				}

				_end_timestamp = media_packet->GetDts() + media_packet->GetDuration();

				_list.push_back(media_packet);

				_total_duration += media_packet->GetDuration();
				_total_size += media_packet->GetData()->GetLength();
				_total_count += 1;

				return true;
			}

			// Get Data List
			const std::vector<std::shared_ptr<const MediaPacket>> &GetList() const
			{
				return _list;
			}

			// Get Data At
			const std::shared_ptr<const MediaPacket> &GetAt(size_t index) const
			{
				return _list.at(index);
			}

			void PopFront()
			{
				_list.erase(_list.begin());
			}

			// Get Start Timestamp
			int64_t GetStartTimestamp() const
			{
				return _start_timestamp;
			}

			// Get End Timestamp
			int64_t GetEndTimestamp() const
			{
				return _end_timestamp;
			}

			// Get Total Duration
			double GetTotalDuration() const
			{
				return _total_duration;
			}

			// Get Total Size
			uint32_t GetTotalSize() const
			{
				return _total_size;
			}

			// Get Total Count
			uint32_t GetTotalCount() const
			{
				return _total_count;
			}

			// Is Empty
			bool IsEmpty() const
			{
				return _list.empty();
			}

			// Is Independent
			bool IsIndependent() const
			{
				return _independent;
			}

		private:
			std::vector<std::shared_ptr<const MediaPacket>> _list;
			int64_t _start_timestamp = 0;
			int64_t _end_timestamp = 0;
			double _total_duration = 0.0;
			uint64_t _total_size = 0;
			uint32_t _total_count = 0;
			bool _independent = false;
		};

		Packager(const std::shared_ptr<const MediaTrack> &media_track, const std::shared_ptr<const MediaTrack> &data_track);

	protected:
		// Get track 
		const std::shared_ptr<const MediaTrack> &GetMediaTrack() const;
		const std::shared_ptr<const MediaTrack> &GetDataTrack() const;

		// Fytp Box
		virtual bool WriteFtypBox(ov::ByteStream &container_stream);
		// Moov Box Group
		virtual bool WriteMoovBox(ov::ByteStream &container_stream);
		virtual bool WriteMvhdBox(ov::ByteStream &container_stream);
		virtual bool WriteTrakBox(ov::ByteStream &container_stream);
		virtual bool WriteTkhdBox(ov::ByteStream &container_stream);
		virtual bool WriteMdiaBox(ov::ByteStream &container_stream);
		virtual bool WriteMdhdBox(ov::ByteStream &container_stream);
		virtual bool WriteHdlrBox(ov::ByteStream &container_stream);
		virtual bool WriteMinfBox(ov::ByteStream &container_stream);
		virtual bool WriteVmhdBox(ov::ByteStream &container_stream);
		virtual bool WriteSmhdBox(ov::ByteStream &container_stream);
		virtual bool WriteDinfBox(ov::ByteStream &container_stream);
		virtual bool WriteDrefBox(ov::ByteStream &container_stream);
		virtual bool  WriteUrlBox(ov::ByteStream &container_stream);
		virtual bool WriteStblBox(ov::ByteStream &container_stream);
		virtual bool WriteStsdBox(ov::ByteStream &container_stream);
		virtual bool WriteAvc1Box(ov::ByteStream &container_stream);
		virtual bool WriteAvccBox(ov::ByteStream &container_stream);
		virtual bool WriteMp4aBox(ov::ByteStream &container_stream);
		virtual bool WriteEsdsBox(ov::ByteStream &container_stream);
		
		virtual bool WriteESDesciptor(ov::ByteStream &container_stream);
		virtual bool WriteDecoderConfigDescriptor(ov::ByteStream &container_stream);
		virtual bool WriteAudioSpecificInfo(ov::ByteStream &container_stream);

		virtual bool WriteSLConfigDescriptor(ov::ByteStream &container_stream);

		virtual bool WriteSttsBox(ov::ByteStream &container_stream);
		virtual bool WriteStscBox(ov::ByteStream &container_stream);
		virtual bool WriteStszBox(ov::ByteStream &container_stream);
		virtual bool WriteStcoBox(ov::ByteStream &container_stream);

		virtual bool WriteMvexBox(ov::ByteStream &container_stream);
		virtual bool WriteTrexBox(ov::ByteStream &container_stream);

		// Emsg Box
		virtual bool WriteEmsgBox(ov::ByteStream &container_stream, const std::shared_ptr<const Samples> &samples);

		// Moof Box Gourp
		virtual bool WriteMoofBox(ov::ByteStream &container_stream, const std::shared_ptr<const Samples> &samples);
		virtual bool WriteMfhdBox(ov::ByteStream &container_stream, const std::shared_ptr<const Samples> &samples);
		virtual bool WriteTrafBox(ov::ByteStream &container_stream, const std::shared_ptr<const Samples> &samples);
		virtual bool WriteTfhdBox(ov::ByteStream &container_stream, const std::shared_ptr<const Samples> &samples);
		virtual bool WriteTfdtBox(ov::ByteStream &container_stream, const std::shared_ptr<const Samples> &samples);
		virtual bool WriteTrunBox(ov::ByteStream &container_stream, const std::shared_ptr<const Samples> &samples);
		virtual bool GetSampleFlags(const std::shared_ptr<const MediaPacket> &sample, uint32_t &flags);

		virtual bool WriteMdatBox(ov::ByteStream &container_stream, const std::shared_ptr<const Samples> &samples);

		// Write BaseDescriptor
		bool WriteBaseDescriptor(ov::ByteStream &stream, uint8_t tag, const ov::Data &data);
		// Write Box
		bool WriteBox(ov::ByteStream &stream, const ov::String &box_name, const ov::Data &box_data);
		// Write Full Box
		bool WriteFullBox(ov::ByteStream &stream, const ov::String &box_name, const ov::Data &box_data, uint8_t version, uint32_t flags);
		
	private:
		std::shared_ptr<const MediaTrack> _media_track = nullptr;
		std::shared_ptr<const MediaTrack> _data_track = nullptr;

		uint32_t _sequence_number = 1; // For Mfhd Box

		// Trun box size
		uint32_t _last_trun_box_size = 0;
	};
}