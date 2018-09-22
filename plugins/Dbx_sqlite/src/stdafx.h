#pragma once

#include <windows.h>
#include <sqlite3.h>

#include <memory>

#include <newpluginapi.h>
#include <win2k.h>

#include <m_core.h>
#include <m_system.h>
#include <m_database.h>
#include <m_db_int.h>

#include "dbintf.h"
#include "resource.h"
#include "version.h"

constexpr auto SQLITE_HEADER_STR = "SQLite format 3";

struct CMPlugin : public PLUGIN<CMPlugin>
{
	CMPlugin();

	int Load() override;
};