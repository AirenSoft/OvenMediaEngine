#pragma once

#include "base/ovlibrary/ovlibrary.h"
#include "base/info/record.h"


class ThumbnailExport : public ov::Singleton<ThumbnailExport>
{
public:
	ThumbnailExport();
	~ThumbnailExport();

	bool ExportRecordToXml(const ov::String path, const std::shared_ptr<info::Record> &record);

	std::shared_mutex _mutex;;
};