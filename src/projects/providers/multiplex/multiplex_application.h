//==============================================================================
//
//  MultiplexApplication
//
//  Created by Getroot
//  Copyright (c) 2023 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/common_types.h>
#include <base/provider/application.h>
#include <base/provider/stream.h>

#include "multiplex_profile.h"

namespace pvd
{
    class MultiplexApplication : public Application
    {
    public:
        static std::shared_ptr<MultiplexApplication> Create(const std::shared_ptr<Provider> &provider, const info::Application &application_info);

        explicit MultiplexApplication(const std::shared_ptr<Provider> &provider, const info::Application &info);
        ~MultiplexApplication() override = default;

        bool Start() override;
        bool Stop() override;

        void OnMultiplexWatcherTick();

    private:
        struct MultiplexFileInfo
        {
            ov::String _file_path;
            struct stat _file_stat;
            std::shared_ptr<MultiplexProfile> _multiplex_profile;

            uint32_t _deleted_checked_count = 0;
        };

        std::vector<MultiplexFileInfo> SearchMultiplexFileInfoFromDir(const ov::String &multiplex_file_root) const;

        bool GetMultiplexFileInfoFromDB(const size_t &hash, MultiplexFileInfo &multiplex_file_info);
        bool AddMultiplex(MultiplexFileInfo &multiplex_file_info);
        bool UpdateMultiplex(MultiplexFileInfo &multiplex_file_info, MultiplexFileInfo &new_multiplex_file_info);
        bool RemoveMultiplex(MultiplexFileInfo &multiplex_file_info);

        ov::String _multiplex_files_path;
        ov::Regex _multiplex_file_name_regex;

        // File name hash -> MultiplexFileInfo
        std::map<size_t, MultiplexFileInfo> _multiplex_file_info_db;

        std::map<ov::String, std::shared_ptr<pvd::Stream>> _multiplex_streams;
    };
}