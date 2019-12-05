#include "sdp.h"

#include <base/ovlibrary/stl.h>
#include <base/ovcrypto/base_64.h>

bool ParseSdp(const std::vector<uint8_t> &sdp, RtspMediaInfo &rtsp_media_info)
{
    std::vector<std::string_view> lines = Split(sdp, {static_cast<uint8_t>('\r'), static_cast<uint8_t>('\n')});
    int16_t last_payload_type = -1;
    for (const auto &line : lines)
    {
        // https://tools.ietf.org/html/rfc4566 RFC-4566 states what whitespace MUST not be used on any side of = so just access the delimiter directly by index
        if (line.size() >= 2 && line[1] == '=')
        {
            switch (line[0])
            {
            case 'a':
                {
                    constexpr char rtmpmap[] = { 'r', 't', 'p', 'm', 'a', 'p', ':' };
                    constexpr char control[] = { 'c', 'o', 'n', 't', 'r', 'o', 'l', ':' };
                    constexpr char fmtp[] = {'f', 'm', 't', 'p', ':' };
                    if (HasSubstring(line, 2, control))
                    {
                        const auto &control_uri = line.substr(2 + sizeof(control));
                        rtsp_media_info.tracks_.emplace(std::string(control_uri.data(), control_uri.size()), last_payload_type);
                    }
                    else if (HasSubstring(line, 2, rtmpmap))
                    {
                        auto codec_paramerters = Split(line.substr(2 + sizeof(rtmpmap)), ' ');
                        // https://tools.ietf.org/html/rfc4566#section-6
                        // a=rtpmap:<payload type> <encoding name>/<clock rate> [/<encoding parameters>]
                        if (codec_paramerters.size() >= 2)
                        {
                            uint8_t payload_type = 0;
                            if (Stoi(std::string(codec_paramerters[0].data(), codec_paramerters[0].size()), payload_type) == false)
                            {
                                continue;
                            }
                            auto codec = Split(codec_paramerters[1], '/');
                            if (codec.empty() == false)
                            {
                                auto &payload = rtsp_media_info.payloads_[payload_type];
                                if (codec[0] == "H264")
                                {
                                    payload.SetCodecId(common::MediaCodecId::H264);
                                }
                                else if (codec[0] == "opus")
                                {
                                    payload.SetCodecId(common::MediaCodecId::Opus);
                                    // For Opus stereo should be detected from the sprop-stereo field
                                    auto &audio_channel = payload.GetChannel();
                                    audio_channel.SetLayout(common::AudioChannel::Layout::LayoutMono);
                                }
                                if (codec.size() > 1)
                                {
                                    int32_t clock_rate = 0;
                                    if (Stoi(std::string(codec[1].data(), codec[1].size()), clock_rate))
                                    {
                                        payload.SetTimeBase(1, clock_rate);
                                        if (payload.GetMediaType() == common::MediaType::Audio)
                                        {
                                            payload.SetSampleRate(clock_rate);
                                        }
                                    }
                                }
                            }
                        }
                    }
                    else if (HasSubstring(line, 2, fmtp))
                    {
                        auto format_components = Split(line.substr(2 + sizeof(fmtp)), ' ');
                        if (format_components.size() > 1)
                        {
                            uint8_t payload_type = 0;
                            if (Stoi(std::string(format_components[0].data(), format_components[0].size()), payload_type) == false)
                            {
                                continue;
                            }
                            for (size_t component_index = 1; component_index < format_components.size(); ++component_index)
                            {
                                auto format_component_part_delimiter = format_components[component_index].find('=');
                                if (format_component_part_delimiter != std::string_view::npos)
                                {
                                    auto format_component_name = std::string_view(format_components[component_index].data(), format_component_part_delimiter);
                                    if (format_component_name == "sprop-parameter-sets")
                                    {
                                        auto format_component_value = std::string_view(format_components[component_index].data() + format_component_part_delimiter + 1, format_components[component_index].size() - format_component_part_delimiter - 1);
                                        auto parameter_sets = Split(format_component_value, ',');
                                        if (parameter_sets.size() == 2)
                                        {
                                            auto sps = ov::Base64::Decode(ov::String(parameter_sets[0].data(), parameter_sets[0].size()));
                                            auto pps = ov::Base64::Decode(ov::String(parameter_sets[1].data(), parameter_sets[1].size()));
                                            H264Extradata h264_extradata;
                                            h264_extradata.AddSps(std::vector<uint8_t>(sps->GetDataAs<uint8_t>(), sps->GetDataAs<uint8_t>() +sps->GetLength()));
                                            h264_extradata.AddPps(std::vector<uint8_t>(pps->GetDataAs<uint8_t>(), pps->GetDataAs<uint8_t>() + pps->GetLength()));
                                            rtsp_media_info.payloads_[payload_type].SetCodecExtradata(h264_extradata.Serialize());
                                        }
                                    }
                                    else if (format_component_name == "sprop-stereo")
                                    {
                                        auto format_component_value = std::string_view(format_components[component_index].data() + format_component_part_delimiter + 1, format_components[component_index].size() - format_component_part_delimiter - 1);
                                        if (*format_component_value.data() == '1')
                                        {
                                            auto &audio_channel = rtsp_media_info.payloads_[payload_type].GetChannel();
                                            audio_channel.SetLayout(common::AudioChannel::Layout::LayoutStereo);
                                        }
                                    }
                                }
                            }

                        }
                    }
                }
                break;
            case 'c':
                break;
            case 'm':
                {
                    auto components = Split(line.substr(2), static_cast<uint8_t>(' '));
                    if (components.size() == 4)
                    {
                        uint8_t payload_type = 0;
                        if (Stoi(std::string(components[3].data(), components[3].size()), payload_type) == false)
                        {
                            continue;
                        }
                        last_payload_type = payload_type;
                        if (components[0] == "video")
                        {
                            rtsp_media_info.payloads_[payload_type].SetMediaType(common::MediaType::Video);
                        }
                        else if (components[0] == "audio")
                        {
                            rtsp_media_info.payloads_[payload_type].SetMediaType(common::MediaType::Audio);
                        }
                    }
                }
                break;
            case 's':
                break;
            }
        }
    }
    return true;
}