//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2025 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <arpa/inet.h>
#include <base/ovlibrary/ovlibrary.h>
#include <modules/containers/flv_v2/flv_parser.h>

#include <map>
#include <memory>
#include <vector>

namespace modules
{
	namespace rtmp
	{
		static constexpr const auto HANDSHAKE_VERSION = 0x03;

		static constexpr const auto TIME_SCALE = 1000;
		static constexpr const auto EXTENDED_TIMESTAMP_VALUE = 0x00FFFFFF;
		static constexpr const auto EXTENDED_TIMESTAMP_SIZE = 4;

		// Basic header is 3 bytes when `chunk_stream_id == 0b000001`
		static constexpr const auto MAX_BASIC_HEADER_SIZE = 3;

		// Message header is 11 bytes when `format_type == MessageHeaderType::T0`
		static constexpr const auto MAX_MESSAGE_HEADER_SIZE = (11 + EXTENDED_TIMESTAMP_SIZE);

		static constexpr const auto MAX_TYPE0_PACKET_HEADER_SIZE = (MAX_BASIC_HEADER_SIZE + 11 + EXTENDED_TIMESTAMP_SIZE);
		static constexpr const auto MAX_TYPE1_PACKET_HEADER_SIZE = (MAX_BASIC_HEADER_SIZE + 7 + EXTENDED_TIMESTAMP_SIZE);
		static constexpr const auto MAX_TYPE2_PACKET_HEADER_SIZE = (MAX_BASIC_HEADER_SIZE + 3 + EXTENDED_TIMESTAMP_SIZE);
		static constexpr const auto MAX_TYPE3_PACKET_HEADER_LENGTH = (MAX_BASIC_HEADER_SIZE + 0 + EXTENDED_TIMESTAMP_SIZE);

		static constexpr const auto MAX_PACKET_HEADER_SIZE = ov::cexpr::Max(
			MAX_TYPE0_PACKET_HEADER_SIZE,
			MAX_TYPE1_PACKET_HEADER_SIZE,
			MAX_TYPE2_PACKET_HEADER_SIZE,
			MAX_TYPE3_PACKET_HEADER_LENGTH);

		static constexpr const auto HANDSHAKE_PACKET_LENGTH = 1536;

		enum class ChunkStreamId : uint32_t
		{
			Urgent = 0b00000010,   // 2
			Control = 0b00000011,  // 3
			Media = 0b00001000,	   // 8

			Mask = 0b00111111,
		};

		// Overload operator to allow usage like `10 & ChunkStreamId::Mask`
		inline ov::UnderylingType<ChunkStreamId> operator&(ov::UnderylingType<ChunkStreamId> number, ChunkStreamId id)
		{
			return number & ov::ToUnderlyingType(id);
		}

		enum class MessageTypeID : uint8_t
		{
			Unknown = 0,

			SetChunkSize = 1,
			Abort = 2,
			Acknowledgement = 3,
			UserControl = 4,
			WindowAcknowledgementSize = 5,
			SetPeerBandwidth = 6,
			Audio = 8,
			Video = 9,
			Amf3Data = 15,
			Amf3SharedObject = 16,
			Amf3Command = 17,
			Amf0Data = 18,
			Amf0SharedObject = 19,
			Amf0Command = 20,
			Aggregate = 22,
		};
		constexpr const char *EnumToString(MessageTypeID type)
		{
			switch (type)
			{
				OV_CASE_RETURN_ENUM_STRING(MessageTypeID, Unknown);
				OV_CASE_RETURN_ENUM_STRING(MessageTypeID, SetChunkSize);
				OV_CASE_RETURN_ENUM_STRING(MessageTypeID, Abort);
				OV_CASE_RETURN_ENUM_STRING(MessageTypeID, Acknowledgement);
				OV_CASE_RETURN_ENUM_STRING(MessageTypeID, UserControl);
				OV_CASE_RETURN_ENUM_STRING(MessageTypeID, WindowAcknowledgementSize);
				OV_CASE_RETURN_ENUM_STRING(MessageTypeID, SetPeerBandwidth);
				OV_CASE_RETURN_ENUM_STRING(MessageTypeID, Audio);
				OV_CASE_RETURN_ENUM_STRING(MessageTypeID, Video);
				OV_CASE_RETURN_ENUM_STRING(MessageTypeID, Amf3Data);
				OV_CASE_RETURN_ENUM_STRING(MessageTypeID, Amf3SharedObject);
				OV_CASE_RETURN_ENUM_STRING(MessageTypeID, Amf3Command);
				OV_CASE_RETURN_ENUM_STRING(MessageTypeID, Amf0Data);
				OV_CASE_RETURN_ENUM_STRING(MessageTypeID, Amf0SharedObject);
				OV_CASE_RETURN_ENUM_STRING(MessageTypeID, Amf0Command);
				OV_CASE_RETURN_ENUM_STRING(MessageTypeID, Aggregate);
			}

			return "Invalid";
		}

		enum class UserControlEventType : uint16_t
		{
			StreamBegin = 0,
			StreamEof = 1,
			StreamDry = 2,
			SetBufferLength = 3,
			StreamIsRecorded = 4,
			PingRequest = 6,
			PingResponse = 7,
			BufferEmpty = 31,
			BufferReady = 32,
		};
		static_assert(sizeof(UserControlEventType) == 2, "UserControlEventType size is not 2");
		constexpr const char *EnumToString(UserControlEventType type)
		{
			switch (type)
			{
				OV_CASE_RETURN_ENUM_STRING(UserControlEventType, StreamBegin);
				OV_CASE_RETURN_ENUM_STRING(UserControlEventType, StreamEof);
				OV_CASE_RETURN_ENUM_STRING(UserControlEventType, StreamDry);
				OV_CASE_RETURN_ENUM_STRING(UserControlEventType, SetBufferLength);
				OV_CASE_RETURN_ENUM_STRING(UserControlEventType, StreamIsRecorded);
				OV_CASE_RETURN_ENUM_STRING(UserControlEventType, PingRequest);
				OV_CASE_RETURN_ENUM_STRING(UserControlEventType, PingResponse);
				OV_CASE_RETURN_ENUM_STRING(UserControlEventType, BufferEmpty);
				OV_CASE_RETURN_ENUM_STRING(UserControlEventType, BufferReady);
			}

			return "Unknown";
		}

		enum class PeerBandwidthLimitType : uint8_t
		{
			Hard = 0,
			Soft = 1,
			Dynamic = 2,
		};

		enum class Command : uint16_t
		{
			Unknown,

			// NetConnection Commands
			Connect,	   // "connect"
			Call,		   // "call"
			Close,		   // "close"
			CreateStream,  // "createStream"

			// NetStream Commands
			Play,		   // "play"
			Play2,		   // "play2"
			DeleteStream,  // "deleteStream"
			CloseStream,   // "closeStream"
			ReceiveAudio,  // "receiveAudio"
			ReceiveVideo,  // "receiveVideo"
			Publish,	   // "publish"
			Seek,		   // "seek"
			Pause,		   // "pause"

			ReleaseStream,	// "releaseStream"
			FCPublish,		// "FCPublish"
			FCUnpublish,	// "FCUnpublish"
			SetChallenge,	// "setChallenge"
			SetDataFrame,	// "@setDataFrame"
			Ping,			// "ping"

			OnStatus,		// "onStatus"
			OnFCPublish,	// "onFCPublish"
			OnFCUnpublish,	// "onFCUnpublish"
			OnFI,			// "onFI"
			OnClientLogin,	// "onClientLogin"
			OnMetaData,		// "onMetaData"
			OnBWDone,		// "onBWDone"
			OnTextData,		// "onTextData"
			OnCuePoint,		// "onCuePoint"

			// Response strings
			AckResult,	// "_result"
			AckError,	// "_error"
		};
		constexpr Command ToCommand(const char *name)
		{
			OV_IF_RETURN(ov::cexpr::StrCmp(name, "connect"), Command::Connect);
			OV_IF_RETURN(ov::cexpr::StrCmp(name, "call"), Command::Call);
			OV_IF_RETURN(ov::cexpr::StrCmp(name, "close"), Command::Close);
			OV_IF_RETURN(ov::cexpr::StrCmp(name, "createStream"), Command::CreateStream);
			OV_IF_RETURN(ov::cexpr::StrCmp(name, "play"), Command::Play);
			OV_IF_RETURN(ov::cexpr::StrCmp(name, "play2"), Command::Play2);
			OV_IF_RETURN(ov::cexpr::StrCmp(name, "deleteStream"), Command::DeleteStream);
			OV_IF_RETURN(ov::cexpr::StrCmp(name, "closeStream"), Command::CloseStream);
			OV_IF_RETURN(ov::cexpr::StrCmp(name, "receiveAudio"), Command::ReceiveAudio);
			OV_IF_RETURN(ov::cexpr::StrCmp(name, "receiveVideo"), Command::ReceiveVideo);
			OV_IF_RETURN(ov::cexpr::StrCmp(name, "publish"), Command::Publish);
			OV_IF_RETURN(ov::cexpr::StrCmp(name, "seek"), Command::Seek);
			OV_IF_RETURN(ov::cexpr::StrCmp(name, "pause"), Command::Pause);
			OV_IF_RETURN(ov::cexpr::StrCmp(name, "releaseStream"), Command::ReleaseStream);
			OV_IF_RETURN(ov::cexpr::StrCmp(name, "FCPublish"), Command::FCPublish);
			OV_IF_RETURN(ov::cexpr::StrCmp(name, "FCUnpublish"), Command::FCUnpublish);
			OV_IF_RETURN(ov::cexpr::StrCmp(name, "setChallenge"), Command::SetChallenge);
			OV_IF_RETURN(ov::cexpr::StrCmp(name, "@setDataFrame"), Command::SetDataFrame);
			OV_IF_RETURN(ov::cexpr::StrCmp(name, "ping"), Command::Ping);
			OV_IF_RETURN(ov::cexpr::StrCmp(name, "onStatus"), Command::OnStatus);
			OV_IF_RETURN(ov::cexpr::StrCmp(name, "onFCPublish"), Command::OnFCPublish);
			OV_IF_RETURN(ov::cexpr::StrCmp(name, "onFCUnpublish"), Command::OnFCUnpublish);
			OV_IF_RETURN(ov::cexpr::StrCmp(name, "onFI"), Command::OnFI);
			OV_IF_RETURN(ov::cexpr::StrCmp(name, "onClientLogin"), Command::OnClientLogin);
			OV_IF_RETURN(ov::cexpr::StrCmp(name, "onMetaData"), Command::OnMetaData);
			OV_IF_RETURN(ov::cexpr::StrCmp(name, "onBWDone"), Command::OnBWDone);
			OV_IF_RETURN(ov::cexpr::StrCmp(name, "onTextData"), Command::OnTextData);
			OV_IF_RETURN(ov::cexpr::StrCmp(name, "onCuePoint"), Command::OnCuePoint);
			OV_IF_RETURN(ov::cexpr::StrCmp(name, "_result"), Command::AckResult);
			OV_IF_RETURN(ov::cexpr::StrCmp(name, "_error"), Command::AckError);

			return Command::Unknown;
		}
		constexpr const char *EnumToString(const Command command)
		{
			switch (command)
			{
				OV_CASE_RETURN(Command::Unknown, nullptr);
				OV_CASE_RETURN(Command::Connect, "connect");
				OV_CASE_RETURN(Command::Call, "call");
				OV_CASE_RETURN(Command::Close, "close");
				OV_CASE_RETURN(Command::CreateStream, "createStream");
				OV_CASE_RETURN(Command::Play, "play");
				OV_CASE_RETURN(Command::Play2, "play2");
				OV_CASE_RETURN(Command::DeleteStream, "deleteStream");
				OV_CASE_RETURN(Command::CloseStream, "closeStream");
				OV_CASE_RETURN(Command::ReceiveAudio, "receiveAudio");
				OV_CASE_RETURN(Command::ReceiveVideo, "receiveVideo");
				OV_CASE_RETURN(Command::Publish, "publish");
				OV_CASE_RETURN(Command::Seek, "seek");
				OV_CASE_RETURN(Command::Pause, "pause");
				OV_CASE_RETURN(Command::ReleaseStream, "releaseStream");
				OV_CASE_RETURN(Command::FCPublish, "FCPublish");
				OV_CASE_RETURN(Command::FCUnpublish, "FCUnpublish");
				OV_CASE_RETURN(Command::SetChallenge, "setChallenge");
				OV_CASE_RETURN(Command::SetDataFrame, "@setDataFrame");
				OV_CASE_RETURN(Command::Ping, "ping");
				OV_CASE_RETURN(Command::OnStatus, "onStatus");
				OV_CASE_RETURN(Command::OnFCPublish, "onFCPublish");
				OV_CASE_RETURN(Command::OnFCUnpublish, "onFCUnpublish");
				OV_CASE_RETURN(Command::OnFI, "onFI");
				OV_CASE_RETURN(Command::OnClientLogin, "onClientLogin");
				OV_CASE_RETURN(Command::OnMetaData, "onMetaData");
				OV_CASE_RETURN(Command::OnBWDone, "onBWDone");
				OV_CASE_RETURN(Command::OnTextData, "onTextData");
				OV_CASE_RETURN(Command::OnCuePoint, "onCuePoint");
				OV_CASE_RETURN(Command::AckResult, "_result");
				OV_CASE_RETURN(Command::AckError, "_error");
			}

			return nullptr;
		}

		enum class HandshakeState
		{
			WaitingForC0,
			WaitingForC1,
			WaitingForC2,
			Completed,
		};
	}  // namespace rtmp
}  // namespace modules
