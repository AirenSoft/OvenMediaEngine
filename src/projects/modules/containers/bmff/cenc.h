//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2023 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/ovlibrary/ovlibrary.h>
#include <base/info/media_track.h>
#include <base/mediarouter/media_buffer.h>
#include <base/ovcrypto/aes.h>

#include "sample.h"

namespace bmff
{
    constexpr uint8_t AES_BLOCK_SIZE = 16;

    enum class CencProtectScheme : uint8_t
    {
        None,
        Cenc, // Not supported yet
        Cbcs
    };

    enum class CencEncryptMode : uint8_t
    {
        None,
        Ctr,
        Cbc
    };

    enum class DRMSystem : uint8_t
    {
        None,
        Widevine,
        FairPlay
    };

    static const char* CencProtectSchemeToString(CencProtectScheme scheme)
    {
        switch (scheme)
        {
        case CencProtectScheme::Cenc:
            return "cenc";
        case CencProtectScheme::Cbcs:
            return "cbcs";
        case CencProtectScheme::None:
        default:
            return "none";
        }
    }

    struct PsshBox
    {
        PsshBox(const std::shared_ptr<ov::Data> &data)
            : pssh_box_data(data)
        {
            // ISO/IEC 23001-7 8.1
			// aligned(8) class ProtectionSystemSpecificHeaderBox extends FullBox('pssh', version, flags = 0)
			// {
			// 	unsigned int(8)[16] SystemID;
			// 	if (version > 0)
			// 	{
			// 		unsigned int(32) KID_count;
			// 		{
			// 			unsigned int(8)[16] KID;
			// 		}
			// 		[KID_count];
			// 	}
			// 	unsigned int(32) DataSize;
			// 	unsigned int(8)[DataSize] Data;
			// }

			// Parse pssh box
            ov::ByteStream stream(data);

            [[maybe_unused]] auto box_size = stream.ReadBE32();
            [[maybe_unused]] auto box_name = stream.GetRemainData(4);
            stream.Skip<uint8_t>(4);
            [[maybe_unused]] auto version = stream.Read8();
            [[maybe_unused]] auto flags = stream.ReadBE24();

            system_id = stream.GetRemainData(16)->Clone();

            logi("DEBUG", "System ID : %s", system_id->ToHexString().LowerCaseString().CStr());

            if (system_id->ToHexString().LowerCaseString() == "edef8ba979d64acea3c827dcd51d21ed")
            {
                drm_system = DRMSystem::Widevine;
            }
            else if (system_id->ToString().LowerCaseString() == "com.apple.streamingkeydelivery")
            {
                drm_system = DRMSystem::FairPlay;
            }
        }

        DRMSystem drm_system = DRMSystem::None;
        std::shared_ptr<ov::Data> system_id = nullptr;
        std::shared_ptr<ov::Data> pssh_box_data = nullptr;
    };

    struct CencProperty
    {
        CencProtectScheme scheme = CencProtectScheme::None;

        uint8_t crypt_bytes_block = 1; // number of encrypted blocks in pattern based encryption
        uint8_t skip_bytes_block = 9; // number of unencrypted blocks in pattern based encryption

        std::shared_ptr<ov::Data> key_id = nullptr; // 16 bytes 
        std::shared_ptr<ov::Data> key = nullptr; // 16 bytes
        std::shared_ptr<ov::Data> iv = nullptr; // 16 bytes

        ov::String fairplay_key_uri; // fairplay only
        
        std::vector<PsshBox> pssh_box_list;
    };

    class Encryptor
    {
    public:
        Encryptor(const std::shared_ptr<const MediaTrack> &media_track, const CencProperty &cenc_property);
        bool Encrypt(const Sample &clear_sample, Sample &cipher_sample);

    private:
        bool GenerateSubSamples(const std::shared_ptr<const MediaPacket> &media_packet, std::vector<Sample::SubSample> &sub_samples);
        bool GenerateSubSamplesFromH264(const std::shared_ptr<const MediaPacket> &media_packet, std::vector<Sample::SubSample> &sub_samples);
        
        // If sub_samples is empty, it means that the full sample encryption is performed.
        bool EncryptInternal(const std::shared_ptr<const ov::Data> &clear_data, std::shared_ptr<ov::Data> &encrypted_data, const std::vector<Sample::SubSample> &sub_samples);

        // dest must be allocated with the same size as source.
        bool EncryptPattern(const uint8_t *source, size_t source_size, uint8_t *dest, bool last_block, std::function<bool(const uint8_t*, size_t, uint8_t*, bool)>(encrypt_func));
        bool EncryptCbc(const uint8_t *source, size_t source_size, uint8_t *dest, bool last_block);
        bool EncryptCtr(const uint8_t *source, size_t source_size, uint8_t *dest, bool last_block);

        CencProperty _cenc_property;
        std::shared_ptr<const MediaTrack> _media_track = nullptr;

        std::function<bool(const uint8_t*, size_t, uint8_t*, bool)> _encrypt_func = nullptr;

        ov::AES _aes;
    };
}