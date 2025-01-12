// popups2.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"

CMPlugin g_plugin;
bool bShutdown = false;

MNOTIFYLINK *notifyLink = nullptr;

// used to work around a bug in neweventnotify and others with the address passed in the GetPluginData function
bool ignore_gpd_passed_addy = false;

FontIDW font_id_firstline = {}, font_id_secondline = {}, font_id_time = {};
ColourIDW colour_id_bg = {}, colour_id_border = {}, colour_id_sidebar = {}, colour_id_titleunderline = {};

COLORREF colBg = GetSysColor(COLOR_3DSHADOW);
HFONT hFontFirstLine = nullptr, hFontSecondLine = nullptr, hFontTime = nullptr;
COLORREF colFirstLine = RGB(255, 0, 0), colSecondLine = 0, colTime = RGB(0, 0, 255), colBorder = RGB(0, 0, 0),
colSidebar = RGB(128, 128, 128), colTitleUnderline = GetSysColor(COLOR_3DSHADOW);

// toptoolbar button
HANDLE hTTButton;

// menu items
HGENMENU hMenuRoot, hMenuItem, hMenuItemHistory;

/////////////////////////////////////////////////////////////////////////////////////////

PLUGININFOEX pluginInfoEx =
{
	sizeof(PLUGININFOEX),
	__PLUGIN_NAME,
	MIRANDA_VERSION_DWORD,
	__DESCRIPTION,
	__AUTHOR,
	__COPYRIGHT,
	__AUTHORWEB,
	UNICODE_AWARE,
	// {EFD15F16-7AE4-40D7-A8E3-A411ED747BD5}
	{0xefd15f16, 0x7ae4, 0x40d7, {0xa8, 0xe3, 0xa4, 0x11, 0xed, 0x74, 0x7b, 0xd5}}
};

CMPlugin::CMPlugin() :
	PLUGIN<CMPlugin>(MODULENAME, pluginInfoEx)
{
}

/////////////////////////////////////////////////////////////////////////////////////////

extern "C" __declspec(dllexport) const MUUID MirandaInterfaces[] = { MIID_POPUP, MIID_LAST };

/////////////////////////////////////////////////////////////////////////////////////////

static int ReloadFont(WPARAM, LPARAM)
{
	LOGFONT log_font;
	if (hFontFirstLine) DeleteObject(hFontFirstLine);
	colFirstLine = Font_GetW(font_id_firstline, &log_font);
	hFontFirstLine = CreateFontIndirect(&log_font);
	if (hFontSecondLine) DeleteObject(hFontSecondLine);
	colSecondLine = Font_GetW(font_id_secondline, &log_font);
	hFontSecondLine = CreateFontIndirect(&log_font);
	if (hFontTime) DeleteObject(hFontTime);
	colTime = Font_GetW(font_id_time, &log_font);
	hFontTime = CreateFontIndirect(&log_font);

	colBg = Colour_GetW(colour_id_bg);
	colBorder = Colour_GetW(colour_id_border);
	colSidebar = Colour_GetW(colour_id_sidebar);
	colTitleUnderline = Colour_GetW(colour_id_titleunderline);
	return 0;
}

static int TTBLoaded(WPARAM, LPARAM)
{
	TTBButton ttb = {};
	ttb.pszService = "Popup/EnableDisableMenuCommand";
	ttb.lParamUp = 1;
	ttb.dwFlags = TTBBF_VISIBLE | TTBBF_SHOWTOOLTIP | TTBBF_ASPUSHBUTTON;
	if (Popup_Enabled())
		ttb.dwFlags |= TTBBF_PUSHED;
	ttb.name = LPGEN("Toggle popups");
	ttb.hIconHandleUp = IcoLib_GetIconHandle(ICO_TB_POPUP_OFF);
	ttb.hIconHandleDn = IcoLib_GetIconHandle(ICO_TB_POPUP_ON);
	ttb.pszTooltipUp = LPGEN("Enable popups");
	ttb.pszTooltipDn = LPGEN("Disable popups");
	hTTButton = g_plugin.addTTB(&ttb);
	return 0;
}

static void InitFonts()
{
	font_id_firstline.flags = FIDF_ALLOWEFFECTS;
	mir_wstrcpy(font_id_firstline.group, LPGENW("Popups"));
	mir_wstrcpy(font_id_firstline.name, LPGENW("First line"));
	mir_strcpy(font_id_firstline.dbSettingsGroup, MODULENAME);
	mir_strcpy(font_id_firstline.setting, "FontFirst");
	mir_wstrcpy(font_id_firstline.backgroundGroup, L"Popups");
	mir_wstrcpy(font_id_firstline.backgroundName, L"Background");
	font_id_firstline.order = 0;
	g_plugin.addFont(&font_id_firstline);

	font_id_secondline.flags = FIDF_ALLOWEFFECTS;
	mir_wstrcpy(font_id_secondline.group, LPGENW("Popups"));
	mir_wstrcpy(font_id_secondline.name, LPGENW("Second line"));
	mir_strcpy(font_id_secondline.dbSettingsGroup, MODULENAME);
	mir_strcpy(font_id_secondline.setting, "FontSecond");
	mir_wstrcpy(font_id_secondline.backgroundGroup, L"Popups");
	mir_wstrcpy(font_id_secondline.backgroundName, L"Background");
	font_id_secondline.order = 1;
	g_plugin.addFont(&font_id_secondline);

	font_id_time.flags = FIDF_ALLOWEFFECTS;
	mir_wstrcpy(font_id_time.group, LPGENW("Popups"));
	mir_wstrcpy(font_id_time.name, LPGENW("Time"));
	mir_strcpy(font_id_time.dbSettingsGroup, MODULENAME);
	mir_strcpy(font_id_time.setting, "FontTime");
	mir_wstrcpy(font_id_time.backgroundGroup, L"Popups");
	mir_wstrcpy(font_id_time.backgroundName, L"Background");
	font_id_time.order = 2;
	g_plugin.addFont(&font_id_time);

	mir_wstrcpy(colour_id_bg.group, LPGENW("Popups"));
	mir_wstrcpy(colour_id_bg.name, LPGENW("Background"));
	mir_strcpy(colour_id_bg.dbSettingsGroup, MODULENAME);
	mir_strcpy(colour_id_bg.setting, "ColourBg");
	colour_id_bg.defcolour = GetSysColor(COLOR_3DSHADOW);
	colour_id_bg.order = 0;
	g_plugin.addColor(&colour_id_bg);

	mir_wstrcpy(colour_id_border.group, LPGENW("Popups"));
	mir_wstrcpy(colour_id_border.name, LPGENW("Border"));
	mir_strcpy(colour_id_border.dbSettingsGroup, MODULENAME);
	mir_strcpy(colour_id_border.setting, "ColourBorder");
	colour_id_border.defcolour = RGB(0, 0, 0);
	colour_id_border.order = 1;
	g_plugin.addColor(&colour_id_border);

	mir_wstrcpy(colour_id_sidebar.group, LPGENW("Popups"));
	mir_wstrcpy(colour_id_sidebar.name, LPGENW("Sidebar"));
	mir_strcpy(colour_id_sidebar.dbSettingsGroup, MODULENAME);
	mir_strcpy(colour_id_sidebar.setting, "ColourSidebar");
	colour_id_sidebar.defcolour = RGB(128, 128, 128);
	colour_id_sidebar.order = 2;
	g_plugin.addColor(&colour_id_sidebar);

	mir_wstrcpy(colour_id_titleunderline.group, LPGENW("Popups"));
	mir_wstrcpy(colour_id_titleunderline.name, LPGENW("Title underline"));
	mir_strcpy(colour_id_titleunderline.dbSettingsGroup, MODULENAME);
	mir_strcpy(colour_id_titleunderline.setting, "ColourTitleUnderline");
	colour_id_titleunderline.defcolour = GetSysColor(COLOR_3DSHADOW);
	colour_id_titleunderline.order = 3;
	g_plugin.addColor(&colour_id_titleunderline);

	ReloadFont(0, 0);
}

static void InitMenuItems(void)
{
	bool isEnabled = Popup_Enabled() == 1;

	hMenuRoot = g_plugin.addRootMenu(MO_MAIN, LPGENW("Popups"), 500010000, IcoLib_GetIcon(isEnabled ? ICO_POPUP_ON : ICO_POPUP_OFF, 0));
	Menu_ConfigureItem(hMenuRoot, MCI_OPT_UID, "043A641A-2767-4C57-AA57-9233D6F9DC54");

	CMenuItem mi(&g_plugin);
	mi.flags = CMIF_UNICODE;
	mi.root = hMenuRoot;

	SET_UID(mi, 0x92c386ae, 0x6e81, 0x452d, 0xb5, 0x71, 0x87, 0x46, 0xe9, 0x2, 0x66, 0xe9);
	mi.hIcolibItem = IcoLib_GetIcon(ICO_HISTORY, 0);
	mi.pszService = MS_POPUP_SHOWHISTORY;
	mi.name.w = LPGENW("Popup history");
	hMenuItemHistory = Menu_AddMainMenuItem(&mi);

	SET_UID(mi, 0x4353d44e, 0x177, 0x4843, 0x88, 0x30, 0x25, 0x5d, 0x91, 0xad, 0xdf, 0x3f);
	mi.hIcolibItem = IcoLib_GetIcon(isEnabled ? ICO_POPUP_ON : ICO_POPUP_OFF, 0);
	mi.pszService = "Popup/EnableDisableMenuCommand";
	mi.name.w = (isEnabled ? LPGENW("Disable popups") : LPGENW("Enable popups"));
	hMenuItem = Menu_AddMainMenuItem(&mi);
}

static int ModulesLoaded(WPARAM, LPARAM)
{
	MNotifyGetLink();

	if (ServiceExists(MS_HPP_EG_WINDOW))
		lstPopupHistory.SetRenderer(RENDER_HISTORYPP);

	HookEvent(ME_FONT_RELOAD, ReloadFont);
	HookEvent(ME_TTB_MODULELOADED, TTBLoaded);

	LoadModuleDependentOptions();

	if (GetModuleHandle(L"neweventnotify"))
		ignore_gpd_passed_addy = true;

	return 0;
}

static int PreShutdown(WPARAM, LPARAM)
{
	bShutdown = true;
	DeinitMessagePump();
	return 0;
}

int CMPlugin::Load()
{
	InitMessagePump();
	InitOptions();
	InitNotify();
	InitFonts();
	InitIcons();
	InitMenuItems();

	HookEvent(ME_SYSTEM_PRESHUTDOWN, PreShutdown);
	HookEvent(ME_SYSTEM_MODULESLOADED, ModulesLoaded);
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////

int CMPlugin::Unload()
{
	DeleteObject(hFontFirstLine);
	DeleteObject(hFontSecondLine);
	DeleteObject(hFontTime);
	return 0;
}
