/*
Weather Protocol plugin for Miranda NG
Copyright (C) 2012-19 Miranda NG team
Copyright (c) 2005-2011 Boris Krasnovskiy All Rights Reserved
Copyright (c) 2002-2005 Calvin Che

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2
of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/* This file contains the includes, weather constants/declarations,
   the structs, and the primitives for some of the functions.
*/

#pragma once

//============  THE INCLUDES  ===========

#include <share.h>
#include <time.h>
#include <windows.h>
#include <commctrl.h>
#include <richedit.h>
#include <malloc.h>

#include <newpluginapi.h>
#include <m_icolib.h>
#include <m_options.h>
#include <m_langpack.h>
#include <m_skin.h>
#include <m_database.h>
#include <m_history.h>
#include <m_protosvc.h>
#include <m_userinfo.h>
#include <m_netlib.h>
#include <m_ignore.h>
#include <m_findadd.h>
#include <m_button.h>
#include <m_avatars.h>
#include <m_clc.h>
#include <m_fontservice.h>
#include <m_skin_eng.h>
#include <m_cluiframes.h>
#include <m_popup.h>
#include <win2k.h>
#include <m_acc.h>

#include <m_weather.h>
#include <m_toptoolbar.h>

#include "resource.h"
#include "version.h"

//============  CONSTANTS  ============

// name
#define MODULENAME         "Weather"
#define WEATHERPROTOTEXT   "Weather"
#define DEFCURRENTWEATHER  "WeatherCondition"
#define WEATHERCONDITION   "Current"

// weather conditions
#define SUNNY     ID_STATUS_ONLINE
#define NA        ID_STATUS_OFFLINE
#define PCLOUDY   ID_STATUS_AWAY
#define CLOUDY    ID_STATUS_NA
#define RAIN      ID_STATUS_OCCUPIED
#define FOG       ID_STATUS_DND
#define SNOW      ID_STATUS_INVISIBLE
#define UNAVAIL   40081

// status
#define NOSTATUSDATA	1

// limits
#define MAX_TEXT_SIZE   4096
#define MAX_DATA_LEN    1024

// db info mangement mode
#define WDBM_REMOVE			1
#define WDBM_DETAILDISPLAY	2

// more info list column width
#define LIST_COLUMN		 150

// others
#define NODATA			TranslateT("N/A")
#define UM_SETCONTACT	40000

// weather update error codes
#define INVALID_ID_FORMAT	10
#define INVALID_SVC			11
#define	INVALID_ID			12
#define SVC_NOT_FOUND		20
#define	NETLIB_ERROR		30
#define	DATA_EMPTY			40
#define	DOC_NOT_FOUND		42
#define	DOC_TOO_SHORT		43
#define	UNKNOWN_ERROR		99

// weather update error text
#define E10		TranslateT("Invalid ID format, missing \"/\" (10)")
#define E11		TranslateT("Invalid service (11)")
#define E12		TranslateT("Invalid station (12)")
#define E20		TranslateT("Weather service ini for this station is not found (20)")
#define E30		TranslateT("Netlib error - check your internet connection (30)")
#define E40		TranslateT("Empty data is retrieved (40)")
#define E42		TranslateT("Document not found (42)")
#define E43		TranslateT("Document too short to contain any weather data (43)")
#define E99		TranslateT("Unknown error (99)")

// HTTP error... not all translated
// 100 Continue
// 101 Switching Protocols
// 200 OK
// 201 Created
// 202 Accepted
// 203 Non-Authoritative Information
#define E204	TranslateT("HTTP Error: No content (204)")
// 205 Reset Content
// 206 Partial Content
// 300 Multiple Choices
#define E301	TranslateT("HTTP Error: Data moved (301)")
// 302 Found
// 303 See Other
// 304 Not Modified
#define E305	TranslateT("HTTP Error: Use proxy (305)")
// 306 (Unused)
#define E307	TranslateT("HTTP Error: Temporary redirect (307)")
#define E400	TranslateT("HTTP Error: Bad request (400)")
#define E401	TranslateT("HTTP Error: Unauthorized (401)")
#define E402	TranslateT("HTTP Error: Payment required (402)")
#define E403	TranslateT("HTTP Error: Forbidden (403)")
#define E404	TranslateT("HTTP Error: Not found (404)")
#define E405	TranslateT("HTTP Error: Method not allowed (405)")
// 406 Not Acceptable
#define E407	TranslateT("HTTP Error: Proxy authentication required (407)")
// 408 Request Timeout
// 409 Conflict
#define E410	TranslateT("HTTP Error: Gone (410)")
// 411 Length Required
// 412 Precondition Failed
// 413 Request Entity Too Large
// 414 Request-URI Too Long
// 415 Unsupported Media Type
// 416 Requested Range Not Satisfiable
// 417 Expectation Failed
#define E500	TranslateT("HTTP Error: Internal server error (500)")
// 501 Not Implemented
#define E502	TranslateT("HTTP Error: Bad gateway (502)")
#define E503	TranslateT("HTTP Error: Service unavailable (503)")
#define E504	TranslateT("HTTP Error: Gateway timeout (504)")
// 505 HTTP Version Not Supported

// defaults constants
#define C_DEFAULT L"%n  [%t, %c]"
#define N_DEFAULT TranslateT("%c\\nTemperature: %t\\nFeel-Like: %f\\nPressure: %p\\nWind: %i  %w\\nHumidity: %m\\nDew Point: %e\\nVisibility: %v\\n\\nSun Rise: %r\\nSun Set: %y\\n\\n5 Days Forecast:\\n%[Forecast Day 1]\\n%[Forecast Day 2]\\n%[Forecast Day 3]\\n%[Forecast Day 4]\\n%[Forecast Day 5]")
#define B_DEFAULT TranslateT("Feel-Like: %f\\nPressure: %p\\nWind: %i  %w\\nHumidity: %m\\nDew Point: %e\\nVisibility: %v\\n\\nSun Rise: %r\\nSun Set: %y\\n\\n5 Days Forecast:\\n%[Forecast Day 1]\\n%[Forecast Day 2]\\n%[Forecast Day 3]\\n%[Forecast Day 4]\\n%[Forecast Day 5]")
#define b_DEFAULT TranslateT("Weather Condition for %n as of %u")
#define X_DEFAULT N_DEFAULT
#define H_DEFAULT TranslateT("%c, %t (feel-like %f)	Wind: %i %w	Humidity: %m")
#define E_DEFAULT TranslateT("%n at %u:	%c, %t (feel-like %f)	Wind: %i %w	Humidity: %m")
#define P_DEFAULT TranslateT("%n   (%u)")
#define p_DEFAULT TranslateT("%c, %t\\nToday:  High %h, Low %l")
#define s_DEFAULT TranslateT("Temperature: %[Temperature]")
#define VAR_LIST_POPUP TranslateT("%c\tcurrent condition\n%d\tcurrent date\n%e\tdewpoint\n%f\tfeel-like temperature\n%h\ttoday's high\n%i\twind direction\n%l\ttoday's low\n%m\thumidity\n%n\tstation name\n%p\tpressure\n%r\tsunrise time\n%s\tstation ID\n%t\ttemperature\n%u\tupdate time\n%v\tvisibility\n%w\twind speed\n%y\tsun set")
#define VAR_LIST_OPT   TranslateT("%c\tcurrent condition\n%d\tcurrent date\n%e\tdewpoint\n%f\tfeel-like temp\n%h\ttoday's high\n%i\twind direction\n%l\ttoday's low\n%m\thumidity\n%n\tstation name\n%p\tpressure\n%r\tsunrise time\n%s\tstation ID\n%t\ttemperature\n%u\tupdate time\n%v\tvisibility\n%w\twind speed\n%y\tsun set\n----------\n\\n\tnew line")
#define WEATHER_NO_INFO TranslateT("No information available.\r\nPlease update weather condition first.")
#define CUSTOM_VARS TranslateT("%[..]\tcustom variables")
#define VARS_LIST TranslateT("Here is a list of custom variables that are currently available")
#define NO_FORECAST_URL TranslateT("The URL for complete forecast has not been set. You can set it from the Edit Settings dialog.")
#define NO_MAP_URL TranslateT("The URL for weather map has not been set. You can set it from the Edit Settings dialog.")

//============  OPTION STRUCT  ============

// option struct
struct MYOPTIONS
{
	// main options
	BYTE AutoUpdate;
	BYTE CAutoUpdate;
	BYTE StartupUpdate;
	BYTE NoProtoCondition;
	BYTE UpdateOnlyConditionChanged;
	BYTE RemoveOldData;
	BYTE MakeItalic;

	WORD UpdateTime;
	WORD AvatarSize;

	// units
	WORD tUnit;
	WORD wUnit;
	WORD vUnit;
	WORD pUnit;
	WORD dUnit;
	WORD eUnit;
	wchar_t DegreeSign[4];
	BYTE DoNotAppendUnit;
	BYTE NoFrac;

	// texts
	wchar_t *cText;
	wchar_t *bTitle;
	wchar_t *bText;
	wchar_t *nText;
	wchar_t *eText;
	wchar_t *hText;
	wchar_t *xText;
	wchar_t *sText;

	// advanced
	BYTE DisCondIcon;

	// popup options
	BYTE UsePopup;
	BYTE UpdatePopup;
	BYTE AlertPopup;
	BYTE PopupOnChange;
	BYTE ShowWarnings;

	// popup colors
	BYTE UseWinColors;
	COLORREF BGColour;
	COLORREF TextColour;

	// popup actions
	DWORD LeftClickAction;
	DWORD RightClickAction;

	// popup delay
	DWORD pDelay;

	// popup texts
	wchar_t *pTitle;
	wchar_t *pText;

	// other misc stuff
	wchar_t Default[64];
	MCONTACT DefStn;
};

void DestroyOptions(void);

//============  STRUCT USED TO MAKE AN UPDATE LIST  ============
struct WCONTACTLIST {
	MCONTACT hContact;
	struct WCONTACTLIST *next;
};

typedef struct WCONTACTLIST UPDATELIST;

extern UPDATELIST *UpdateListHead, *UpdateListTail;

void DestroyUpdateList(void);

//============  DATA FORMAT STRUCT  ============

#define WID_NORMAL	0
#define WID_SET		1
#define WID_BREAK	2

typedef struct {
	wchar_t *Name;
	wchar_t *Start;
	wchar_t *End;
	wchar_t *Unit;
	char  *Url;
	wchar_t *Break;
	int Type;
} WIDATAITEM;

struct WITEMLIST {
	WIDATAITEM Item;
	struct WITEMLIST *Next;
};

typedef struct WITEMLIST WIDATAITEMLIST;

typedef struct {
	BOOL Available;
	char *SearchURL;
	wchar_t *NotFoundStr;
	WIDATAITEM Name;
} WIIDSEARCH;

typedef struct {
	BOOL Available;
	wchar_t *First;
	WIDATAITEM Name;
	WIDATAITEM ID;
} WINAMESEARCHTYPE;

typedef struct {
	char *SearchURL;
	wchar_t *NotFoundStr;
	wchar_t *SingleStr;
	WINAMESEARCHTYPE Single;
	WINAMESEARCHTYPE Multiple;
} WINAMESEARCH;

struct STRLIST {
	wchar_t *Item;
	struct STRLIST *Next;
};

typedef struct STRLIST WICONDITEM;

typedef struct {
	WICONDITEM *Head;
	WICONDITEM *Tail;
} WICONDLIST;

typedef struct {
	wchar_t *FileName;
	wchar_t *ShortFileName;
	BOOL Enabled;

	// header
	wchar_t *DisplayName;
	wchar_t *InternalName;
	wchar_t *Description;
	wchar_t *Author;
	wchar_t *Version;
	int InternalVer;
	size_t MemUsed;

	// default
	char  *DefaultURL;
	wchar_t *DefaultMap;
	char  *UpdateURL;
	char  *UpdateURL2;
	char  *UpdateURL3;
	char  *UpdateURL4;
	char  *Cookie;
	char  *UserAgent;
// items
	int UpdateDataCount;
	WIDATAITEMLIST *UpdateData;
	WIDATAITEMLIST *UpdateDataTail;
	WIIDSEARCH IDSearch;
	WINAMESEARCH NameSearch;
	WICONDLIST CondList[10];
} WIDATA;

//============  DATA LIST (LINKED LIST)  ============

struct DATALIST {
	WIDATA Data;
	struct DATALIST *next;
};

typedef struct DATALIST WIDATALIST;

//============  GLOBAL VARIABLES  ============

extern WIDATALIST *WIHead, *WITail;

extern HWND hPopupWindow, hWndSetup;

extern MYOPTIONS opt;

extern unsigned status, old_status;

extern MWindowList hDataWindowList, hWindowList;

extern HNETLIBUSER hNetlibUser;
extern HANDLE hHookWeatherUpdated, hHookWeatherError, hTBButton, hUpdateMutex;
extern UINT_PTR timerId;

extern HGENMENU hMwinMenu;

// check if weather is currently updating
extern BOOL ThreadRunning;

//============  FUNCTION PRIMITIVES  ============

// functions in weather_addstn.c
INT_PTR WeatherAddToList(WPARAM wParam,LPARAM lParam);
BOOL CheckSearch();

int IDSearch(wchar_t *id, const int searchId);
int NameSearch(wchar_t *name, const int searchId);

INT_PTR WeatherBasicSearch(WPARAM wParam,LPARAM lParam);
INT_PTR WeatherCreateAdvancedSearchUI(WPARAM wParam, LPARAM lParam);
INT_PTR WeatherAdvancedSearch(WPARAM wParam, LPARAM lParam);

int WeatherAdd(WPARAM wParam, LPARAM lParam);

// functions used in weather_contacts.c
INT_PTR ViewLog(WPARAM wParam,LPARAM lParam);
INT_PTR LoadForecast(WPARAM wParam,LPARAM lParam);
INT_PTR WeatherMap(WPARAM wParam,LPARAM lParam);
INT_PTR EditSettings(WPARAM wParam,LPARAM lParam);

int ContactDeleted(WPARAM wParam,LPARAM lParam);

BOOL IsMyContact(MCONTACT hContact);

// functions in weather_conv.c
void GetTemp(wchar_t *tempchar, wchar_t *unit, wchar_t *str);
void GetSpeed(wchar_t *tempchar, wchar_t *unit, wchar_t *str);
void GetPressure(wchar_t *tempchar, wchar_t *unit, wchar_t *str);
void GetDist(wchar_t *tempchar, wchar_t *unit, wchar_t *str);
void GetElev(wchar_t *tempchar, wchar_t *unit, wchar_t *str);

WORD GetIcon(const wchar_t* cond, WIDATA *Data);
void CaseConv(wchar_t *str);
void TrimString(char *str);
void TrimString(WCHAR *str);
void ConvertBackslashes(char *str);
char *GetSearchStr(char *dis);

wchar_t *GetDisplay(WEATHERINFO *w, const wchar_t *dis, wchar_t* str);
INT_PTR GetDisplaySvcFunc(WPARAM wParam, LPARAM lParam);

void GetSvc(wchar_t *pszID);
void GetID(wchar_t *pszID);

wchar_t *GetError(int code);

// functions in weather_data.c
void GetStationID(MCONTACT hContact, wchar_t* id, int idlen);
WEATHERINFO LoadWeatherInfo(MCONTACT Change);
int DBGetData(MCONTACT hContact, char *setting, DBVARIANT *dbv);

void EraseAllInfo(void);

void GetDataValue(WIDATAITEM *UpdateData, wchar_t *Data, wchar_t** szInfo);
void ConvertDataValue(WIDATAITEM *UpdateData, wchar_t *Data);
void wSetData(char **Data, const char *Value);
void wSetData(WCHAR **Data, const char *Value);
void wSetData(WCHAR **Data, const WCHAR *Value);
void wfree(char **Data);
void wfree(WCHAR **Data);

void DBDataManage(MCONTACT hContact, WORD Mode, WPARAM wParam, LPARAM lParam);

// functions in weather_http.c
int InternetDownloadFile (char *szUrl, char *cookie, char *userAgent, wchar_t** szData);
void NetlibInit();

// functions in weather_ini.c
WIDATA* GetWIData(wchar_t *pszServ);

bool IsContainedInCondList(const wchar_t *pszStr, WICONDLIST *List);

void DestroyWIList();
bool LoadWIData(bool dial);

INT_PTR CALLBACK DlgPopupOpts(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

// functions in weather_info.c
void GetINIInfo(wchar_t *pszSvc);

void MoreVarList();

// functions in weather_opt.c
void SetTextDefault(const char* in);
void LoadOptions();
void SaveOptions();

int OptInit(WPARAM wParam,LPARAM lParam);


// functions in weather_popup.c
int WeatherPopup(WPARAM wParam, LPARAM lParam);
int WeatherError(WPARAM wParam, LPARAM lParam);
int WPShowMessage(wchar_t* lpzText, WORD kind);

LRESULT CALLBACK PopupWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// functions in weather_svcs.c
void InitServices(void);

INT_PTR WeatherSetStatus(WPARAM new_status, LPARAM lParam);
INT_PTR WeatherGetCaps(WPARAM wParam, LPARAM lParam);
INT_PTR WeatherGetName(WPARAM wParam, LPARAM lParam);
INT_PTR WeatherGetStatus(WPARAM wParam, LPARAM lParam);
INT_PTR WeatherLoadIcon(WPARAM wParam, LPARAM lParam);

void UpdateMenu(BOOL State);
void UpdatePopupMenu(BOOL State);
void AddMenuItems();
void AvatarDownloaded(MCONTACT hContact);

// functions in weather_update.c
int UpdateWeather(MCONTACT hContact);

void UpdateAll(BOOL AutoUpdate, BOOL RemoveOld);
INT_PTR UpdateSingleStation(WPARAM wParam,LPARAM lParam);
INT_PTR UpdateAllInfo(WPARAM wParam,LPARAM lParam);
INT_PTR UpdateSingleRemove(WPARAM wParam,LPARAM lParam);
INT_PTR UpdateAllRemove(WPARAM wParam,LPARAM lParam);

int GetWeatherData(MCONTACT hContact);

void CALLBACK timerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);
void CALLBACK timerProc2(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);

// function from multiwin module
void InitMwin(void);
void DestroyMwin(void);
INT_PTR Mwin_MenuClicked(WPARAM wParam, LPARAM lParam);
int BuildContactMenu(WPARAM wparam, LPARAM lparam);
void UpdateMwinData(MCONTACT hContact);
void removeWindow(MCONTACT hContact);

// functions in weather_userinfo.c
int UserInfoInit(WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK DlgProcINIPage(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

#define WM_UPDATEDATA WM_USER + 2687

int BriefInfo(WPARAM wParam, LPARAM lParam);
INT_PTR BriefInfoSvc(WPARAM wParam, LPARAM lParam);

void InitIcons(void);

//============  Plugin Class ============

struct CMPlugin : public PLUGIN<CMPlugin>
{
	CMPlugin();

	int Load() override;
	int Unload() override;
};

