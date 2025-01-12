#pragma once

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <commctrl.h>

#include <newpluginapi.h>
#include <m_database.h>
#include <m_clist.h>
#include <m_options.h>
#include <m_skin.h>
#include <m_langpack.h>
#include <m_protosvc.h>

#include "resource.h"
#include "version.h"

struct CMPlugin : public PLUGIN<CMPlugin>
{
	CMPlugin();

	int Load() override;
};

struct TOPTIONS
{
	BOOL MessageRead;
	BOOL MessageSend;
	BOOL FileRead;
	BOOL FileSend;

	int ChangeTo; // ID_STATUS_XXX

	BOOL IfOffline;
	BOOL IfOnline;
	BOOL IfAway;
	BOOL IfNA;
	BOOL IfOccupied;
	BOOL IfDND;
	BOOL IfInvisible;
};
