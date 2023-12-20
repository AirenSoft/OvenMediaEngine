//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2023 AirenSoft. All rights reserved.
//
//==============================================================================
#include "pallycon.h"

#include <modules/http/client/http_client.h>
#include <pugixml-1.9/src/pugixml.hpp>
#include <base/ovlibrary/hex.h>

#include "cpix_private.h"

namespace cpix
{
	bool Pallycon::GetKeyInfo(const ov::String &url, const ov::String &token,
							  const ov::String &content_id, const bmff::DRMSystem &drm_system, const bmff::CencProtectScheme &protect_scheme,
							  bmff::CencProperty &cenc_property)
	{
		bool result = false;
		ov::String request_body;
		if (!MakeRequestBody(content_id, drm_system, protect_scheme, request_body))
		{
			logte("Failed to make request body");
			return false;
		}

		auto client = std::make_shared<http::clnt::HttpClient>();
		client->SetMethod(http::Method::Post);
		client->SetBlockingMode(ov::BlockingMode::Blocking);
		client->SetConnectionTimeout(3000);
		client->SetRequestHeader("Content-Type", "application/xml");
		client->SetRequestHeader("Accept", "application/xml");
		client->SetRequestBody(request_body);

        ov::String final_url = url;
        
        if (final_url.HasSuffix("/") == false)
        {
            final_url += "/";
        }

        final_url += token;

		client->Request(final_url, [&](http::StatusCode status_code, const std::shared_ptr<ov::Data> &data, const std::shared_ptr<const ov::Error> &error) {
			// A response was received from the server.
			if (error == nullptr)
			{
				if (status_code == http::StatusCode::OK)
				{
					// Parsing response
					result = ParseResponse(data, cenc_property);
					return;
				}
				else
				{
					logte("CPIX server responded with an error. (status code(%d) error message(%s)", status_code, data == nullptr ? "No Data" : data->ToString().CStr());
					result = false;
					return;
				}
			}
			else
			{
				// A connection error or an error that does not conform to the HTTP spec has occurred.
				logte("The HTTP client's request failed. error code(%d) error message(%s)", error->GetCode(), error->GetMessage().CStr());
				result = false;
				return;
			}
		});

		return result;
	}

	bool Pallycon::MakeRequestBody(const ov::String &content_id, const bmff::DRMSystem &drm_system, const bmff::CencProtectScheme &protect_scheme, ov::String &request_body)
	{
		/*
        <?xml version="1.0" encoding="UTF-8"?>
        <cpix:CPIX id="your-content-id2" xmlns:cpix="urn:dashif:org:cpix" xmlns:pskc="urn:ietf:params:xml:ns:keyprov:pskc" xmlns:speke="urn:aws:amazon:com:speke">
        <cpix:ContentKeyList>
            <cpix:ContentKey kid="681e5b39-49f2-4dfa-b744-86573c22e6fc" commonEncryptionScheme="cbcs"></cpix:ContentKey>
        </cpix:ContentKeyList>
        <cpix:DRMSystemList>
            <!-- Common encryption (Widevine) -->
            <cpix:DRMSystem kid="681e5b39-49f2-4dfa-b744-86573c22e6fc" systemId="edef8ba9-79d6-4ace-a3c8-27dcd51d21ed" />

            <!-- Common encryption (Fairplay) -->
            <cpix:DRMSystem kid="681e5b39-49f2-4dfa-b744-86573c22e6fc" systemId="94ce86fb-07ff-4f43-adb8-93d2fa968ca2">
                <cpix:ContentProtectionData />
                <cpix:PSSH />
                <cpix:URIExtXKey />
            </cpix:DRMSystem>
        </cpix:DRMSystemList>
        </cpix:CPIX>
        */

		// Create a new XML document
		pugi::xml_document doc;

		// Add declaration (<?xml version="1.0" encoding="UTF-8"?>)
		pugi::xml_node decl = doc.append_child(pugi::node_declaration);
		decl.append_attribute("version") = "1.0";
		decl.append_attribute("encoding") = "UTF-8";

		// Add root node <cpix:CPIX>
		pugi::xml_node cpix = doc.append_child("cpix:CPIX");
		cpix.append_attribute("id") = content_id.CStr();
		cpix.append_attribute("xmlns:cpix") = "urn:dashif:org:cpix";
		cpix.append_attribute("xmlns:pskc") = "urn:ietf:params:xml:ns:keyprov:pskc";
		cpix.append_attribute("xmlns:speke") = "urn:aws:amazon:com:speke";

		// Add <cpix:ContentKeyList> and its child <cpix:ContentKey>
		pugi::xml_node contentKeyList = cpix.append_child("cpix:ContentKeyList");
		pugi::xml_node contentKey = contentKeyList.append_child("cpix:ContentKey");
		contentKey.append_attribute("kid") = "681e5b39-49f2-4dfa-b744-86573c22e6fc";  // Not used. Random value
		contentKey.append_attribute("commonEncryptionScheme") = bmff::CencProtectSchemeToString(protect_scheme);

		pugi::xml_node drmSystemList = cpix.append_child("cpix:DRMSystemList");
		if (drm_system == bmff::DRMSystem::Widevine || drm_system == bmff::DRMSystem::All)
		{
			// Add <cpix:DRMSystem> (Widevine)
			pugi::xml_node drmSystemWidevine = drmSystemList.append_child("cpix:DRMSystem");
			drmSystemWidevine.append_attribute("kid") = "681e5b39-49f2-4dfa-b744-86573c22e6fc";
			drmSystemWidevine.append_attribute("systemId") = "edef8ba9-79d6-4ace-a3c8-27dcd51d21ed";
		}

		if (drm_system == bmff::DRMSystem::FairPlay || drm_system == bmff::DRMSystem::All)
		{
			// Add <cpix:DRMSystem> (Fairplay) with child nodes
			pugi::xml_node drmSystemFairplay = drmSystemList.append_child("cpix:DRMSystem");
			drmSystemFairplay.append_attribute("kid") = "681e5b39-49f2-4dfa-b744-86573c22e6fc";
			drmSystemFairplay.append_attribute("systemId") = "94ce86fb-07ff-4f43-adb8-93d2fa968ca2";

			// Add child nodes for Fairplay DRM
			drmSystemFairplay.append_child("cpix:ContentProtectionData");
			drmSystemFairplay.append_child("cpix:PSSH");
			drmSystemFairplay.append_child("cpix:URIExtXKey");
		}

		// Save the XML to a string
		std::stringstream ss;
		doc.save(ss);

		request_body = ss.str().c_str();

		return true;
	}

	bool Pallycon::ParseResponse(const std::shared_ptr<ov::Data> &data, bmff::CencProperty &cenc_property)
	{
		/*
        <?xml version="1.0" encoding="UTF-8" standalone="yes"?>
        <cpix:CPIX id="your-content-id2" xmlns:cpix="urn:dashif:org:cpix" xmlns:enc="http://www.w3.org/2001/04/xmlenc#" xmlns:speke="urn:aws:amazon:com:speke" xmlns:pskc="urn:ietf:params:xml:ns:keyprov:pskc" xmlns:ds="http://www.w3.org/2000/09/xmldsig#">
            <cpix:ContentKeyList>
                <cpix:ContentKey commonEncryptionScheme="cbcs" explicitIV="==" kid="xxxxxx-xxxx-xxxx-xxxx-xxxxxx">
                    <cpix:Data>
                        <pskc:Secret>
                            <pskc:PlainValue>==</pskc:PlainValue>
                        </pskc:Secret>
                    </cpix:Data>
                </cpix:ContentKey>
            </cpix:ContentKeyList>
            <cpix:DRMSystemList>
                <cpix:DRMSystem kid="xxxxxx-xxxx-xxxx-xxxx-xxxxxx" systemId="94ce86fb-07ff-4f43-adb8-93d2fa968ca2">
                    <cpix:HLSSignalingData>==</cpix:HLSSignalingData>
                    <cpix:HLSSignalingData playlist="master"></cpix:HLSSignalingData>
                    <cpix:HLSSignalingData playlist="media">==</cpix:HLSSignalingData>
                    <cpix:URIExtXKey></cpix:URIExtXKey>
                </cpix:DRMSystem>
                <cpix:DRMSystem kid="xxxxxx-xxxx-xxxx-xxxx-xxxxxx" systemId="edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                    <cpix:ContentProtectionData>=</cpix:ContentProtectionData>
                    <cpix:PSSH>==</cpix:PSSH>
                </cpix:DRMSystem>
            </cpix:DRMSystemList>
        </cpix:CPIX>
        */

		// Parse
        ov::String xml_string = data->ToString();
		pugi::xml_document doc;
		pugi::xml_parse_result result = doc.load_string(xml_string.CStr());
        if (result.status != pugi::status_ok)
        {
            logte("XML parsing failed: %s", result.description());
            return false;
        }

        // ContentKeyList
        pugi::xml_node content_key_list_node = doc.select_node("//cpix:ContentKeyList").node();
        if (content_key_list_node == nullptr)
        {
            logte("cpix:ContentKeyList node not found.");
            return false;
        }

        for (pugi::xml_node content_key_node = content_key_list_node.child("cpix:ContentKey"); content_key_node; content_key_node = content_key_node.next_sibling("cpix:ContentKey"))
        {
            ov::String common_encryption_scheme_value = content_key_node.attribute("commonEncryptionScheme").value();
            ov::String kid_value = content_key_node.attribute("kid").value();
            ov::String explicit_iv_value = content_key_node.attribute("explicitIV").value();

            // Scheme
            if (common_encryption_scheme_value == "cbcs")
            {
                cenc_property.scheme = bmff::CencProtectScheme::Cbcs;
            }
            // else if (common_encryption_scheme_value == "cenc")
            // {
            //     cenc_property.scheme = bmff::CencProtectScheme::Cenc;
            // }
            else
            {
                logte("Invalid or not support commonEncryptionScheme value: %s", common_encryption_scheme_value.CStr());
                return false;
            }

            // explicitIV
            if (explicit_iv_value.IsEmpty())
            {
                logte("explicitIV value not found.");
                return false;
            }
            
            cenc_property.iv = ov::Base64::Decode(explicit_iv_value);
            
            // KID
            if (kid_value.IsEmpty())
            {
                logte("kid value not found.");
                return false;
            }

            cenc_property.key_id = ov::Hex::Decode(kid_value);

            auto plain_value_node = content_key_node.select_node("cpix:Data/pskc:Secret/pskc:PlainValue").node();
            if (plain_value_node == nullptr)
            {
                logte("cpix:Data/pskc:Secret/pskc:PlainValue node not found.");
                return false;
            }

            ov::String plain_value_value = plain_value_node.child_value(); // base64
            if (plain_value_value.IsEmpty())
            {
                logte("cpix:Data/pskc:Secret/pskc:PlainValue value not found.");
                return false;
            }

            cenc_property.key = ov::Base64::Decode(plain_value_value);

            break; // Now only one ContentKey is supported
        }

        
        // DRMSystemList
        pugi::xml_node drm_system_list_node = doc.select_node("//cpix:DRMSystemList").node();
        if (drm_system_list_node == nullptr)
        {
            logte("cpix:DRMSystemList node not found.");
            return false;
        }

        for (pugi::xml_node drm_system_node = drm_system_list_node.child("cpix:DRMSystem"); drm_system_node; drm_system_node = drm_system_node.next_sibling("cpix:DRMSystem"))
        {
            ov::String kid_value = drm_system_node.attribute("kid").value();
            ov::String system_id_value = drm_system_node.attribute("systemId").value();

            // Widevine
            if (system_id_value == "edef8ba9-79d6-4ace-a3c8-27dcd51d21ed")
            {
                auto pssh_node = drm_system_node.child("cpix:PSSH");
                if (pssh_node == nullptr)
                {
                    logte("cpix:PSSH node not found.");
                    return false;
                }

                ov::String pssh_value = pssh_node.child_value(); // Base64 encoded
                if (pssh_value.IsEmpty())
                {
                    logte("cpix:PSSH value not found.");
                    return false;
                }

                cenc_property.pssh_box_list.push_back(bmff::PsshBox(ov::Base64::Decode(pssh_value)));
            }
            // Fairplay
            else if (system_id_value == "94ce86fb-07ff-4f43-adb8-93d2fa968ca2")
            {
                // URIExtXKey
                auto uri_extxkey_node = drm_system_node.child("cpix:URIExtXKey");
                if (uri_extxkey_node == nullptr)
                {
                    logte("cpix:URIExtXKey node not found.");
                    return false;
                }

                ov::String uri_extxkey_value = uri_extxkey_node.child_value();
                if (uri_extxkey_value.IsEmpty())
                {
                    logte("cpix:URIExtXKey value not found.");
                    return false;
                }

                cenc_property.fairplay_key_uri = ov::Base64::Decode(uri_extxkey_value)->ToString();

                // PSSH (Now Pallycon CPIX Server does not return PSSH for Fairplay)
            }
        }

		return true;
	}
}  // namespace cpix