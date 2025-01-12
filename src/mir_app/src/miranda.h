/*

Miranda NG: the free IM client for Microsoft* Windows*

Copyright (C) 2012-19 Miranda NG team (https://miranda-ng.org),
Copyright (c) 2000-12 Miranda IM project,
all portions of this codebase are copyrighted to the people
listed in contributors.txt.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#pragma once

typedef HRESULT (STDAPICALLTYPE *pfnDrawThemeTextEx)(HTHEME, HDC, int, int, LPCWSTR, int, DWORD, LPRECT, const struct _DTTOPTS *);
typedef HRESULT (STDAPICALLTYPE *pfnSetWindowThemeAttribute)(HWND, enum WINDOWTHEMEATTRIBUTETYPE, PVOID, DWORD);
typedef HRESULT (STDAPICALLTYPE *pfnBufferedPaintInit)(void);
typedef HRESULT (STDAPICALLTYPE *pfnBufferedPaintUninit)(void);
typedef HANDLE  (STDAPICALLTYPE *pfnBeginBufferedPaint)(HDC, RECT *, BP_BUFFERFORMAT, BP_PAINTPARAMS *, HDC *);
typedef HRESULT (STDAPICALLTYPE *pfnEndBufferedPaint)(HANDLE, BOOL);
typedef HRESULT (STDAPICALLTYPE *pfnGetBufferedPaintBits)(HANDLE, RGBQUAD **, int *);

extern pfnDrawThemeTextEx drawThemeTextEx;
extern pfnSetWindowThemeAttribute setWindowThemeAttribute;
extern pfnBufferedPaintInit bufferedPaintInit;
extern pfnBufferedPaintUninit bufferedPaintUninit;
extern pfnBeginBufferedPaint beginBufferedPaint;
extern pfnEndBufferedPaint endBufferedPaint;
extern pfnGetBufferedPaintBits getBufferedPaintBits;

extern ITaskbarList3 * pTaskbarInterface;

typedef HRESULT (STDAPICALLTYPE *pfnDwmExtendFrameIntoClientArea)(HWND hwnd, const MARGINS *margins);
typedef HRESULT (STDAPICALLTYPE *pfnDwmIsCompositionEnabled)(BOOL *);

extern pfnDwmExtendFrameIntoClientArea dwmExtendFrameIntoClientArea;
extern pfnDwmIsCompositionEnabled dwmIsCompositionEnabled;

/**** database.cpp *********************************************************************/

extern MDatabaseCommon *currDb;
extern DATABASELINK *currDblink;
extern LIST<DATABASELINK> arDbPlugins;

int  InitIni(void);
void UninitIni(void);

/**** idle.cpp *************************************************************************/

int  LoadIdleModule(void);
void UnloadIdleModule(void);

/**** miranda.cpp **********************************************************************/

extern DWORD hMainThreadId;
extern HANDLE hOkToExitEvent, hModulesLoadedEvent;
extern HANDLE hAccListChanged;
extern wchar_t mirandabootini[MAX_PATH];
extern struct pluginEntry *plugin_crshdmp, *plugin_service, *plugin_ssl, *plugin_clist;
extern bool g_bModulesLoadedFired, g_bMirandaTerminated;

/**** newplugins.cpp *******************************************************************/

char* GetPluginNameByInstance(HINSTANCE hInstance);
int   LoadStdPlugins(void);
int   LaunchServicePlugin(pluginEntry *p);

/**** path.cpp *************************************************************************/

void InitPathVar(void);

/**** srmm.cpp *************************************************************************/

void KillModuleSrmmIcons(HPLUGIN);
void KillModuleToolbarIcons(HPLUGIN);

/**** utf.cpp **************************************************************************/

__forceinline char* Utf8DecodeA(const char* src)
{
	char* tmp = mir_strdup(src);
	mir_utf8decode(tmp, nullptr);
	return tmp;
}

#pragma optimize("", on)

/**** skinicons.cpp ********************************************************************/

extern int g_iIconX, g_iIconY, g_iIconSX, g_iIconSY;

HICON LoadIconEx(HINSTANCE hInstance, LPCTSTR lpIconName, BOOL bShared);
int ImageList_AddIcon_NotShared(HIMAGELIST hIml, LPCTSTR szResource);

int ImageList_ReplaceIcon_IconLibLoaded(HIMAGELIST hIml, int nIndex, HICON hIcon);

#define Safe_DestroyIcon(hIcon) if (hIcon) DestroyIcon(hIcon)

/**** clistmenus.cpp ********************************************************************/

extern int hMainMenuObject, hContactMenuObject, hStatusMenuObject;
extern HANDLE hPreBuildMainMenuEvent, hPreBuildContactMenuEvent;
extern HANDLE hShutdownEvent, hPreShutdownEvent;
extern HMENU hMainMenu, hStatusMenu;

extern OBJLIST<CListEvent> g_cliEvents;

struct MStatus
{
	int iStatus;
	int iSkinIcon;
	int Pf2flag;

	INT_PTR iHotKey;
	HGENMENU hStatusMenu;
};

extern MStatus g_statuses[MAX_STATUS_COUNT];

/**** protocols.cpp *********************************************************************/

#define OFFSET_PROTOPOS 200
#define OFFSET_VISIBLE  400
#define OFFSET_ENABLED  600
#define OFFSET_NAME     800

extern LIST<PROTOACCOUNT> accounts;

struct MBaseProto : public PROTOCOLDESCRIPTOR, public MZeroedObject
{
	MBaseProto(const char *_proto)
	{
		this->szName = mir_strdup(_proto);
	}

	~MBaseProto()
	{
		mir_free(szName);
		mir_free(szUniqueId);
	}

	pfnInitProto fnInit;
	pfnUninitProto fnUninit;

	HINSTANCE hInst;
	char *szUniqueId;  // name of the unique setting that identifies a contact
};

extern OBJLIST<MBaseProto> g_arProtos;
extern LIST<MBaseProto> g_arFilters;

INT_PTR ProtoCallService(const char *szModule, const char *szService, WPARAM wParam, LPARAM lParam);

PROTOACCOUNT* __fastcall Proto_GetAccount(MCONTACT hContact);

PROTO_INTERFACE* AddDefaultAccount(const char *szProtoName);
int  FreeDefaultAccount(PROTO_INTERFACE* ppi);

bool ActivateAccount(PROTOACCOUNT *pa, bool bIsDynamic);
void EraseAccount(const char *pszProtoName);
void OpenAccountOptions(PROTOACCOUNT *pa);

/////////////////////////////////////////////////////////////////////////////////////////

#define DAF_DYNAMIC 0x0001
#define DAF_ERASE   0x0002
#define DAF_FORK    0x0004

void DeactivateAccount(PROTOACCOUNT *pa, int flags);
void UnloadAccount(PROTOACCOUNT *pa, int flags);

/////////////////////////////////////////////////////////////////////////////////////////

void LoadDbAccounts(void);
void WriteDbAccounts(void);

void KillModuleAccounts(HINSTANCE);

INT_PTR CallProtoServiceInt(MCONTACT hContact, const char* szModule, const char* szService, WPARAM wParam, LPARAM lParam);

INT_PTR stubChainRecv(WPARAM, LPARAM);
#define MS_PROTO_HIDDENSTUB "Proto/stubChainRecv"

/**** utils.cpp ************************************************************************/

void RegisterModule(CMPluginBase*);

void HotkeyToName(wchar_t *buf, int size, BYTE shift, BYTE key);
WORD GetHotkeyValue(INT_PTR idHotkey);

HBITMAP ConvertIconToBitmap(HIMAGELIST hIml, int iconId);
MBaseProto* Proto_GetProto(const char *szProtoName);

///////////////////////////////////////////////////////////////////////////////

extern "C"
{
	MIR_CORE_DLL(int)  Langpack_MarkPluginLoaded(const MUUID &uuid);
	MIR_CORE_DLL(int)  GetSubscribersCount(struct THook *hHook);
};
