/*
	NewXstatusNotify YM - Plugin for Miranda IM
	Copyright (c) 2001-2004 Luca Santarelli
	Copyright (c) 2005-2007 Vasilich
	Copyright (c) 2007-2011 yaho

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
	*/

#ifndef COMMON_H
#define COMMON_H

#pragma once

#define STRICT

#include <windows.h>
#include <mmsystem.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <time.h>
#include <malloc.h>

#include <newpluginapi.h>
#include <win2k.h>
#include <m_button.h>
#include <m_clc.h>
#include <m_database.h>
#include <m_genmenu.h>
#include <m_history.h>
#include <m_icolib.h>
#include <m_ignore.h>
#include <m_langpack.h>
#include <m_message.h>
#include <m_options.h>
#include <m_popup.h>
#include <m_protosvc.h>
#include <m_skin.h>
#include <m_userinfo.h>
#include <m_xstatus.h>
#include <m_extraicons.h>
#include <m_clistint.h>

#include <m_metacontacts.h>
#include <m_toptoolbar.h>

#include "resource.h"
#include "indsnd.h"
#include "options.h"
#include "popup.h"
#include "utils.h"
#include "version.h"
#include "xstatus.h"

#define MODULENAME "NewStatusNotify"

struct CMPlugin : public PLUGIN<CMPlugin>
{
	CMPlugin();

	int Load() override;
	int Unload() override;
};

#define MAX_STATUSTEXT         36
#define MAX_STANDARDTEXT       36
#define MAX_SKINSOUNDNAME      36
#define MAX_SKINSOUNDDESC      36
									    
#define GENDER_UNSPECIFIED     0
#define GENDER_MALE            1
#define GENDER_FEMALE          2
									    
#define ID_STATUS_FROMOFFLINE  0
#define ID_STATUS_SMSGREMOVED  1
#define ID_STATUS_SMSGCHANGED  2
#define ID_STATUS_XREMOVED     3
#define ID_STATUS_XCHANGED     4
#define ID_STATUS_XMSGCHANGED  5
#define ID_STATUS_XMSGREMOVED  6
#define ID_STATUSEX_MAX	       6
									    
#define STATUSEX_COUNT         ID_STATUSEX_MAX + 1
#define ID_STATUS_EXTRASTATUS  40081
#define ID_STATUS_STATUSMSG    40082
#define ID_STATUS_MAX2         ID_STATUS_STATUSMSG
#define STATUS_COUNT           ID_STATUS_MAX2 - ID_STATUS_MIN + 1
#define Index(ID_STATUS)		 ID_STATUS - ID_STATUS_OFFLINE

#define COLOR_BG_AVAILDEFAULT  RGB(173,206,247)
#define COLOR_BG_NAVAILDEFAULT RGB(255,189,189)
#define COLOR_TX_DEFAULT       RGB(0,0,0)

enum
{
	ICO_RESET,
	ICO_SOUND,
	ICO_NOTIFICATION_OFF,
	ICO_NOTIFICATION_ON,
	ICO_XSTATUS,
	ICO_DISABLEALL,
	ICO_ENABLEALL,
	ICO_VARIABLES,
	ICO_STATUS_MESSAGE,
	ICO_LOGGING_XSTATUS,
	ICO_LOGGING_SMSG,
	ICO_MAXID
};

#define JS_PARSE_XMPP_URI		"/ParseXmppURI"

#define COMPARE_SAME 0
#define COMPARE_DIFF 1
#define COMPARE_DEL  2

typedef struct tagSTATUS
{
	wchar_t lpzMStatusText[MAX_STATUSTEXT];
	wchar_t lpzFStatusText[MAX_STATUSTEXT];
	wchar_t lpzUStatusText[MAX_STATUSTEXT];
	wchar_t lpzStandardText[MAX_STANDARDTEXT];
	char lpzSkinSoundName[MAX_SKINSOUNDNAME];
	wchar_t lpzSkinSoundDesc[MAX_SKINSOUNDDESC];
	wchar_t lpzSkinSoundFile[MAX_PATH];
	COLORREF colorBack;
	COLORREF colorText;
} STATUS;

typedef struct {
	MCONTACT hContact;
	wchar_t *oldstatusmsg;
	wchar_t *newstatusmsg;
	char *proto;
	int compare;
} STATUSMSGINFO;

/*
MCONTACT hContact = wParam;
WORD oldStatus = LOWORD(lParam);
WORD newStatus = HIWORD(lParam);
oldStatus is the status the contact was before the change.
newStatus is the status the contact is now.
Cast them to (int) if you need them that way.
*/
#define ME_STATUSCHANGE_CONTACTSTATUSCHANGED "Miranda/StatusChange/ContactStatusChanged"

#define MS_STATUSCHANGE_MENUCOMMAND "NewStatusNotify/EnableDisableMenuCommand"

extern OPTIONS opt;
extern LIST<DBEVENT> eventListXStatus;
extern LIST<DBEVENT> eventListStatus;
extern LIST<DBEVENT> eventListSMsg;
extern TEMPLATES templates;
extern HGENMENU hEnableDisableMenu;
extern STATUS StatusList[STATUS_COUNT];
extern STATUS StatusListEx[STATUSEX_COUNT];
extern IconItem iconList[ICO_MAXID];

wchar_t* GetStr(STATUSMSGINFO *n, const wchar_t *tmplt);
void LogSMsgToDB(STATUSMSGINFO *smi, const wchar_t *tmplt);
void BlinkIcon(MCONTACT hContact, HICON hIcon, wchar_t *stzText);
void PlayChangeSound(MCONTACT hContact, const char *name);
#endif //COMMON_H
