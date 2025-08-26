//=============================================================================
//
//  OvenMediaEngine
//
//  Created by Keukhan
//  Copyright (c) 2025 AirenSoft. All rights reserved.
//
//==============================================================================
#include "overlay.h"

#include "common.h"

namespace serdes
{
	std::shared_ptr<info::Overlay> OverlayFromJson(const Json::Value &json_body)
	{
		// {
		//     "url": "https://domain/path/to/filename.[png|jpg]",
		//     "left": "10",
		//     "top": "10",
		//     "width": "180",
		//     "height": "64",
		//     "opacity": 50
		// }

		if (json_body.isNull() || json_body.isObject() == false)
		{
			return nullptr;
		}

		auto overlay = std::make_shared<info::Overlay>();
		if (json_body.isMember("url") && json_body["url"].isString())
		{
			overlay->_url = json_body["url"].asString().c_str();
		}
		if (json_body.isMember("left") && json_body["left"].isString())
		{
			overlay->_left = json_body["left"].asString().c_str();
		}
		if (json_body.isMember("top") && json_body["top"].isString())
		{
			overlay->_top = json_body["top"].asString().c_str();
		}
		if (json_body.isMember("width") && json_body["width"].isString())
		{
			overlay->_width = json_body["width"].asString().c_str();
		}
		if (json_body.isMember("height") && json_body["height"].isString())
		{
			overlay->_height = json_body["height"].asString().c_str();
		}
		if (json_body.isMember("opacity") && json_body["opacity"].isInt())
		{
			overlay->_opacity = json_body["opacity"].asInt();
		}

		return overlay;
	}

	Json::Value JsonFromOverlay(const std::shared_ptr<info::Overlay> &overlay)
	{
		if (overlay == nullptr)
		{
			return Json::Value();
		}

		Json::Value json_overlay(Json::objectValue);
		json_overlay["url"] = overlay->GetUrl().CStr();
		json_overlay["left"] = overlay->GetLeft().CStr();
		json_overlay["top"] = overlay->GetTop().CStr();
		json_overlay["width"] = overlay->GetWidth().CStr();
		json_overlay["height"] = overlay->GetHeight().CStr();
		json_overlay["opacity"] = overlay->GetOpacity();

		return json_overlay;
	}

	std::shared_ptr<info::OverlayInfo> OverlayInfoFromJson(const Json::Value &json_body)
	{
		// {
		//     "outputStreamName": "stream",
		//     "variantName": "video_1080",
		//     "overlays": [
		//         {
		//             "url": "http://ovenmediaengine.com/overlay/image001.png",
		//             "left": "10",
		//             "top": "10",
		//             "width": "180",
		//             "height": "64",
		//             "opacity": 50
		//         }
		//     ]
		// }

		if (json_body.isNull() || json_body.isObject() == false)
		{
			return nullptr;
		}

		auto overlay_info = std::make_shared<info::OverlayInfo>();

		if (json_body.isMember("outputStreamName") && json_body["outputStreamName"].isString())
		{
			overlay_info->SetOutputStreamName(json_body["outputStreamName"].asString().c_str());
		}

		if (json_body.isMember("variantName") && json_body["variantName"].isString())
		{
			overlay_info->SetVariantName(json_body["variantName"].asString().c_str());
		}

		if (json_body.isMember("overlays") && json_body["overlays"].isArray())
		{
			std::vector<std::shared_ptr<info::Overlay>> overlays;
			for (const auto &overlay_json : json_body["overlays"])
			{
				auto overlay = OverlayFromJson(overlay_json);
				if (overlay != nullptr)
				{
					overlays.push_back(overlay);
				}
			}
			overlay_info->SetOverlays(overlays);
		}

		return overlay_info;
	}

	Json::Value JsonFromOverlayInfo(const std::shared_ptr<info::OverlayInfo> &overlay_info)
	{
		if (overlay_info == nullptr)
		{
			return Json::Value();
		}

		Json::Value json_overlay_info(Json::objectValue);
		json_overlay_info["outputStreamName"] = overlay_info->GetOutputStreamName().CStr();
		json_overlay_info["variantName"] = overlay_info->GetVariantName().CStr();

		Json::Value json_overlays(Json::arrayValue);
		for (const auto &overlay : overlay_info->GetOverlays())
		{
			json_overlays.append(JsonFromOverlay(overlay));
		}
		json_overlay_info["overlays"] = json_overlays;

		return json_overlay_info;
	}

}  // namespace serdes