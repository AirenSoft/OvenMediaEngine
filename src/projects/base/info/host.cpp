//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "host.h"
#include "host_private.h"

namespace info
{
    Host::Host(const cfg::Host &host_info)
            : cfg::Host(host_info)
    {
        _host_id = ov::Random::GenerateUInt32();

        auto error = PrepareCertificates();

        if(error != nullptr)
        {
            logtw("Could not prepare certificate for application %s: %s",
                  host_info.GetName().CStr(),
                  error->ToString().CStr()
            );
        }
    }

    std::shared_ptr<ov::Error> Host::PrepareCertificates()
    {
        const cfg::Tls &tls = GetDomain().GetTls();

        if(tls.IsParsed() == false)
        {
            // TLS is disabled
            return nullptr;
        }

        ov::String cert_path = tls.GetCertPath();
        ov::String key_path = tls.GetKeyPath();
        if(cert_path.IsEmpty() || key_path.IsEmpty())
        {
            return ov::Error::CreateError(
                    OV_LOG_TAG,
                    "Invalid TLS configuration\n"
                    "\tCert path: %s\n"
                    "\tPrivate key path: %s",
                    cert_path.CStr(),
                    key_path.CStr()
            );
        }

        _certificate = std::make_shared<Certificate>();

        logti("Trying to create a certificate using\n"
              "\tCert file path: %s\n"
              "\tPrivate key file path: %s",
              cert_path.CStr(),
              key_path.CStr()
        );

        auto error = _certificate->GenerateFromPem(cert_path, key_path);

        if(error != nullptr)
        {
            logte("Could not create a certificate from files: %s", error->ToString().CStr());
            _certificate = nullptr;
            return error;
        }

        ov::String chain_cert_path = tls.GetChainCertPath();
        if(chain_cert_path.IsEmpty())
        {
            // Chain certificate is not available
            return nullptr;
        }

        _chain_certificate = std::make_shared<Certificate>();

        logti("Trying to create a chain certificate using\n"
              "\tChain cert file path: %s",
              chain_cert_path.CStr()
        );

        error = _chain_certificate->GenerateFromPem(chain_cert_path, true);

        if(error != nullptr)
        {
            logte("Could not create a chain certificate from file: %s", error->ToString().CStr());
            _chain_certificate = nullptr;
            return error;
        }

        return nullptr;
    }
}
