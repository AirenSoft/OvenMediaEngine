#pragma once

#include "base/ovlibrary/ovlibrary.h"
#include "base/info/record.h"


class FileExport : public ov::Singleton<FileExport>
{
public:
	FileExport();
	~FileExport();

	bool ExportRecordToXml(const ov::String path, const std::shared_ptr<info::Record> &record);

	std::shared_mutex _mutex;;
};