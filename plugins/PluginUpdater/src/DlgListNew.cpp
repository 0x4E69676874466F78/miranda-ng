/*
Copyright (C) 2010 Mataes

This is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with this file; see the file license.txt. If
not, write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.
*/

#include "stdafx.h"

static HWND hwndDialog;
static HANDLE hListThread;

static void SelectAll(HWND hDlg, bool bEnable)
{
	OBJLIST<FILEINFO> &todo = *(OBJLIST<FILEINFO> *)GetWindowLongPtr(hDlg, GWLP_USERDATA);
	HWND hwndList = GetDlgItem(hDlg, IDC_LIST_UPDATES);

	for (int i = 0; i < todo.getCount(); i++)
		ListView_SetCheckState(hwndList, i, todo[i].bEnabled = bEnable);
}

static void ApplyDownloads(void *param)
{
	Thread_SetName("PluginUpdater: ApplyDownloads");

	HWND hDlg = (HWND)param;

	////////////////////////////////////////////////////////////////////////////////////////
	// if we need to escalate priviledges, launch a atub

	if (!PrepareEscalation()) {
		PostMessage(hDlg, WM_CLOSE, 0, 0);
		return;
	}

	////////////////////////////////////////////////////////////////////////////////////////
	// ok, let's unpack all zips

	AutoHandle pipe(hPipe);
	HWND hwndList = GetDlgItem(hDlg, IDC_LIST_UPDATES);
	OBJLIST<FILEINFO> &todo = *(OBJLIST<FILEINFO> *)GetWindowLongPtr(hDlg, GWLP_USERDATA);
	//create needed folders after escalating priviledges. Folders creates when we actually install updates
	wchar_t tszFileTemp[MAX_PATH], tszFileBack[MAX_PATH];

	mir_snwprintf(tszFileBack, L"%s\\Backups", g_tszRoot);
	SafeCreateDirectory(tszFileBack);

	mir_snwprintf(tszFileTemp, L"%s\\Temp", g_tszRoot);
	SafeCreateDirectory(tszFileTemp);

	VARSW tszMirandaPath(L"%miranda_path%");

	HNETLIBCONN nlc = nullptr;
	for (int i = 0; i < todo.getCount(); ++i) {
		auto &p = todo[i];
		ListView_EnsureVisible(hwndList, i, FALSE);
		if (p.bEnabled) {
			// download update
			ListView_SetItemText(hwndList, i, 1, TranslateT("Downloading..."));

			if (DownloadFile(&p.File, nlc)) {
				ListView_SetItemText(hwndList, i, 1, TranslateT("Succeeded."));
				if (unzip(p.File.tszDiskPath, tszMirandaPath, tszFileBack, false))
					SafeDeleteFile(p.File.tszDiskPath);  // remove .zip after successful update
				db_unset(0, DB_MODULE_NEW_FILES, _T2A(p.tszOldName));
			}
			else ListView_SetItemText(hwndList, i, 1, TranslateT("Failed!"));
		}
		else ListView_SetItemText(hwndList, i, 1, TranslateT("Skipped."));
	}
	Netlib_CloseHandle(nlc);

	ShowPopup(TranslateT("Plugin Updater"), TranslateT("Download complete"), POPUP_TYPE_INFO);

	int rc = MessageBox(hDlg, TranslateT("Download complete. Do you want to go to plugins option page?"), TranslateT("Plugin Updater"), MB_YESNO | MB_ICONQUESTION);
	if (rc == IDYES)
		CallFunctionAsync(OpenPluginOptions, nullptr);

	PostMessage(hDlg, WM_CLOSE, 0, 0);
}

/////////////////////////////////////////////////////////////////////////////////////////

static WNDPROC oldWndProc = nullptr;

static LRESULT CALLBACK PluginListWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg == WM_LBUTTONDOWN) {
		LVHITTESTINFO hi;
		hi.pt.x = LOWORD(lParam); hi.pt.y = HIWORD(lParam);
		ListView_SubItemHitTest(hwnd, &hi);
		if ((hi.iSubItem == 0) && (hi.flags & LVHT_ONITEMICON)) {
			LVITEM lvi = { 0 };
			lvi.mask = LVIF_IMAGE | LVIF_PARAM | LVIF_GROUPID;
			lvi.stateMask = -1;
			lvi.iItem = hi.iItem;
			if (ListView_GetItem(hwnd, &lvi) && lvi.iGroupId == 1) {
				FILEINFO *info = (FILEINFO *)lvi.lParam;

				wchar_t tszFileName[MAX_PATH];
				wcscpy(tszFileName, wcsrchr(info->tszNewName, L'\\') + 1);
				wchar_t *p = wcschr(tszFileName, L'.'); *p = 0;

				wchar_t link[MAX_PATH];
				mir_snwprintf(link, PLUGIN_INFO_URL, tszFileName);
				Utils_OpenUrlW(link);
			}
		}
	}

	return CallWindowProc(oldWndProc, hwnd, msg, wParam, lParam);
}

static int ListDlg_Resize(HWND, LPARAM, UTILRESIZECONTROL *urc)
{
	switch (urc->wId) {
	case IDC_SELNONE:
	case IDOK:
		return RD_ANCHORX_RIGHT | RD_ANCHORY_BOTTOM;

	case IDC_UPDATETEXT:
		return RD_ANCHORX_CENTRE;
	}
	return RD_ANCHORX_LEFT | RD_ANCHORY_TOP | RD_ANCHORX_WIDTH | RD_ANCHORY_HEIGHT;
}

int ImageList_AddIconFromIconLib(HIMAGELIST hIml, int i)
{
	HICON icon = IcoLib_GetIconByHandle(iconList[i].hIcolib);
	int res = ImageList_AddIcon(hIml, icon);
	IcoLib_ReleaseIcon(icon);
	return res;
}

INT_PTR CALLBACK DlgList(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	HWND hwndList = GetDlgItem(hDlg, IDC_LIST_UPDATES);

	switch (message) {
	case WM_INITDIALOG:
		TranslateDialogDefault(hDlg);
		oldWndProc = (WNDPROC)SetWindowLongPtr(hwndList, GWLP_WNDPROC, (LONG_PTR)PluginListWndProc);

		Window_SetIcon_IcoLib(hDlg, iconList[2].hIcolib);
		{
			HIMAGELIST hIml = ImageList_Create(16, 16, ILC_MASK | ILC_COLOR32, 4, 0);
			ImageList_AddIconFromIconLib(hIml, 1);
			ListView_SetImageList(hwndList, hIml, LVSIL_SMALL);

			if (IsWinVer7Plus()) {
				wchar_t szPath[MAX_PATH];
				GetModuleFileNameW(nullptr, szPath, _countof(szPath));
				wchar_t *ext = wcsrchr(szPath, '.');
				if (ext != nullptr)
					*ext = '\0';
				wcscat(szPath, L".test");
				HANDLE hFile = CreateFileW(szPath, GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
				if (hFile == INVALID_HANDLE_VALUE)
					Button_SetElevationRequiredState(GetDlgItem(hDlg, IDOK), !IsProcessElevated());
				else {
					CloseHandle(hFile);
					DeleteFile(szPath);
				}
			}

			//////////////////////////////////////////////////////////////////////////////////////
			LVCOLUMN lvc = { 0 };
			lvc.mask = LVCF_WIDTH | LVCF_TEXT;

			lvc.pszText = TranslateT("Component Name");
			lvc.cx = 220; // width of column in pixels
			ListView_InsertColumn(hwndList, 0, &lvc);

			lvc.pszText = TranslateT("State");
			lvc.cx = 100; // width of column in pixels
			ListView_InsertColumn(hwndList, 1, &lvc);

			//////////////////////////////////////////////////////////////////////////////////////
			LVGROUP lvg;
			lvg.cbSize = sizeof(LVGROUP);
			lvg.mask = LVGF_HEADER | LVGF_GROUPID;

			lvg.pszHeader = TranslateT("Plugins");
			lvg.iGroupId = 1;
			ListView_InsertGroup(hwndList, 0, &lvg);

			lvg.pszHeader = TranslateT("Icons");
			lvg.iGroupId = 2;
			ListView_InsertGroup(hwndList, 0, &lvg);

			lvg.pszHeader = TranslateT("Languages");
			lvg.iGroupId = 3;
			ListView_InsertGroup(hwndList, 0, &lvg);

			lvg.pszHeader = TranslateT("Other");
			lvg.iGroupId = 4;
			ListView_InsertGroup(hwndList, 0, &lvg);

			ListView_EnableGroupView(hwndList, TRUE);

			//////////////////////////////////////////////////////////////////////////////////////
			SendMessage(hwndList, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT | LVS_EX_SUBITEMIMAGES | LVS_EX_CHECKBOXES | LVS_EX_LABELTIP);
			ListView_DeleteAllItems(hwndList);

			//////////////////////////////////////////////////////////////////////////////////////
			bool enableOk = false;
			OBJLIST<FILEINFO> &todo = *(OBJLIST<FILEINFO> *)lParam;
			for (auto &p : todo) {
				LVITEM lvi = { 0 };
				lvi.mask = LVIF_PARAM | LVIF_GROUPID | LVIF_TEXT | LVIF_IMAGE;

				int groupId = 4;
				if (wcschr(p->tszOldName, L'\\') != nullptr)
					groupId = (wcsstr(p->tszOldName, L"Plugins") != nullptr) ? 1 : ((wcsstr(p->tszOldName, L"Languages") != nullptr) ? 3 : 2);

				lvi.iItem = todo.indexOf(&p);
				lvi.lParam = (LPARAM)p;
				lvi.iGroupId = groupId;
				lvi.iImage = ((groupId == 1) ? 0 : -1);
				lvi.pszText = p->tszOldName;
				ListView_InsertItem(hwndList, &lvi);

				if (p->bEnabled) {
					enableOk = true;
					ListView_SetCheckState(hwndList, lvi.iItem, 1);
				}
			}
			if (enableOk)
				EnableWindow(GetDlgItem(hDlg, IDOK), TRUE);
		}

		// do this after filling list - enables 'ITEMCHANGED' below
		SetWindowLongPtr(hDlg, GWLP_USERDATA, lParam);
		Utils_RestoreWindowPosition(hDlg, 0, MODULENAME, "ListWindow");
		return TRUE;

	case WM_NOTIFY:
		if (((LPNMHDR)lParam)->hwndFrom == hwndList) {
			switch (((LPNMHDR)lParam)->code) {
			case LVN_ITEMCHANGED:
				if (GetWindowLongPtr(hDlg, GWLP_USERDATA)) {
					NMLISTVIEW *nmlv = (NMLISTVIEW *)lParam;
					if ((nmlv->uNewState ^ nmlv->uOldState) & LVIS_STATEIMAGEMASK) {
						LVITEM lvI = { 0 };
						lvI.iItem = nmlv->iItem;
						lvI.iSubItem = 0;
						lvI.mask = LVIF_PARAM;
						ListView_GetItem(hwndList, &lvI);

						OBJLIST<FILEINFO> &todo = *(OBJLIST<FILEINFO> *)GetWindowLongPtr(hDlg, GWLP_USERDATA);
						FILEINFO *p = (FILEINFO*)lvI.lParam;
						p->bEnabled = ListView_GetCheckState(hwndList, nmlv->iItem);

						bool enableOk = false;
						for (auto &it : todo) {
							if (it->bEnabled) {
								enableOk = true;
								break;
							}
						}
						EnableWindow(GetDlgItem(hDlg, IDOK), enableOk ? TRUE : FALSE);
					}
				}
				break;
			}
		}
		break;

	case WM_COMMAND:
		if (HIWORD(wParam) == BN_CLICKED) {
			switch (LOWORD(wParam)) {
			case IDOK:
				EnableWindow(GetDlgItem(hDlg, IDOK), FALSE);
				EnableWindow(GetDlgItem(hDlg, IDC_SELNONE), FALSE);

				mir_forkthread(ApplyDownloads, hDlg);
				return TRUE;

			case IDC_SELNONE:
				SelectAll(hDlg, false);
				break;

			case IDCANCEL:
				DestroyWindow(hDlg);
				return TRUE;
			}
		}
		break;

	case WM_SIZE: // make the dlg resizeable
		if (!IsIconic(hDlg))
			Utils_ResizeDialog(hDlg, g_plugin.getInst(), MAKEINTRESOURCEA(IDD_LIST), ListDlg_Resize);
		break;

	case WM_GETMINMAXINFO:
		{
			LPMINMAXINFO mmi = (LPMINMAXINFO)lParam;

			// The minimum width in points
			mmi->ptMinTrackSize.x = 370;
			// The minimum height in points
			mmi->ptMinTrackSize.y = 300;
		}
		break;

	case WM_CLOSE:
		DestroyWindow(hDlg);
		break;

	case WM_DESTROY:
		Utils_SaveWindowPosition(hDlg, NULL, MODULENAME, "ListWindow");
		Window_FreeIcon_IcoLib(hDlg);
		hwndDialog = nullptr;
		delete (OBJLIST<FILEINFO> *)GetWindowLongPtr(hDlg, GWLP_USERDATA);
		SetWindowLongPtr(hDlg, GWLP_USERDATA, 0);
		break;
	}

	return FALSE;
}

static void __stdcall LaunchListDialog(void *param)
{
	hwndDialog = CreateDialogParam(g_plugin.getInst(), MAKEINTRESOURCE(IDD_LIST), GetDesktopWindow(), DlgList, (LPARAM)param);
}

static FILEINFO* ServerEntryToFileInfo(const ServListEntry &hash, const wchar_t* tszBaseUrl, const wchar_t* tszPath)
{
	FILEINFO *FileInfo = new FILEINFO;
	FileInfo->bDeleteOnly = FALSE;
	// copy the relative old name
	wcsncpy_s(FileInfo->tszOldName, hash.m_name, _TRUNCATE);
	wcsncpy_s(FileInfo->tszNewName, hash.m_name, _TRUNCATE);

	wchar_t tszFileName[MAX_PATH];
	wcsncpy_s(tszFileName, wcsrchr(tszPath, L'\\') + 1, _TRUNCATE);
	if (auto *tp = wcschr(tszFileName, L'.'))
		*tp = 0;

	wchar_t tszRelFileName[MAX_PATH];
	wcsncpy_s(tszRelFileName, hash.m_name, _TRUNCATE);
	if (auto *tp = wcsrchr(tszRelFileName, L'.'))
		*tp = 0;
	if (auto *tp = wcschr(tszRelFileName, L'\\'))
		wcslwr((tp) ? tp+1 : tszRelFileName);

	mir_snwprintf(FileInfo->File.tszDiskPath, L"%s\\Temp\\%s.zip", g_tszRoot, tszFileName);
	mir_snwprintf(FileInfo->File.tszDownloadURL, L"%s/%s.zip", tszBaseUrl, tszRelFileName);
	for (auto *tp = wcschr(FileInfo->File.tszDownloadURL, '\\'); tp != nullptr; tp = wcschr(tp, '\\'))
		*tp++ = '/';
	FileInfo->File.CRCsum = hash.m_crc;
	
	// Load list of checked Plugins from database
	Netlib_LogfW(hNetlibUser, L"File %s found", FileInfo->tszOldName);
	FileInfo->bEnabled = db_get_b(0, DB_MODULE_NEW_FILES, _T2A(FileInfo->tszOldName)) != 0;
	return FileInfo;
}

/////////////////////////////////////////////////////////////////////////////////////////
// building file list in the separate thread

static void GetList(void *)
{
	Thread_SetName("PluginUpdater: GetList");

	wchar_t tszTempPath[MAX_PATH];
	DWORD dwLen = GetTempPath(_countof(tszTempPath), tszTempPath);
	if (tszTempPath[dwLen - 1] == '\\')
		tszTempPath[dwLen - 1] = 0;

	ptrW updateUrl(GetDefaultUrl()), baseUrl;
	SERVLIST hashes(50, CompareHashes);
	if (!ParseHashes(updateUrl, baseUrl, hashes)) {
		hListThread = nullptr;
		return;
	}

	FILELIST *UpdateFiles = new FILELIST(20);
	VARSW dirname(L"%miranda_path%");

	for (auto &it : hashes) {
		wchar_t tszPath[MAX_PATH];
		mir_snwprintf(tszPath, L"%s\\%s", dirname.get(), it->m_name);

		if (GetFileAttributes(tszPath) == INVALID_FILE_ATTRIBUTES) {
			FILEINFO *FileInfo = ServerEntryToFileInfo(*it, baseUrl, tszPath);
			UpdateFiles->insert(FileInfo);
		}
	}

	// Show dialog
	if (UpdateFiles->getCount() == 0) {
		ShowPopup(TranslateT("Plugin Updater"), TranslateT("List is empty."), POPUP_TYPE_INFO);
		delete UpdateFiles;
	}
	else CallFunctionAsync(LaunchListDialog, UpdateFiles);

	hListThread = nullptr;
}

static void DoGetList()
{
	if (hListThread)
		ShowPopup(TranslateT("Plugin Updater"), TranslateT("List loading already started!"), POPUP_TYPE_INFO);
	else if (hwndDialog) {
		ShowWindow(hwndDialog, SW_SHOW);
		SetForegroundWindow(hwndDialog);
		SetFocus(hwndDialog);
	}
	else hListThread = mir_forkthread(GetList);
}

void UninitListNew()
{
	if (hwndDialog != nullptr)
		DestroyWindow(hwndDialog);
}

static INT_PTR ShowListCommand(WPARAM, LPARAM)
{
	DoGetList();
	return 0;
}

void UnloadListNew()
{
	if (hListThread)
		hListThread = nullptr;
}

static INT_PTR ParseUriService(WPARAM, LPARAM lParam)
{
	wchar_t *arg = (wchar_t *)lParam;
	if (arg == nullptr)
		return 1;

	wchar_t uri[1024];
	wcsncpy_s(uri, arg, _TRUNCATE);

	wchar_t *p = wcschr(uri, ':');
	if (p == nullptr)
		return 1;

	wchar_t pluginPath[MAX_PATH];
	mir_wstrcpy(pluginPath, p + 1);
	p = wcschr(pluginPath, '/');
	if (p) *p = '\\';

	if (GetFileAttributes(pluginPath) != INVALID_FILE_ATTRIBUTES)
		return 0;

	ptrW updateUrl(GetDefaultUrl()), baseUrl;
	SERVLIST hashes(50, CompareHashes);
	if (!ParseHashes(updateUrl, baseUrl, hashes)) {
		hListThread = nullptr;
		return 1;
	}

	ServListEntry *hash = hashes.find((ServListEntry*)&pluginPath);
	if (hash == nullptr)
		return 0;

	VARSW dirName(L"%miranda_path%");
	wchar_t tszPath[MAX_PATH];
	mir_snwprintf(tszPath, L"%s\\%s", dirName.get(), hash->m_name);
	FILEINFO *fileInfo = ServerEntryToFileInfo(*hash, baseUrl, tszPath);

	FILELIST *fileList = new FILELIST(1);
	fileList->insert(fileInfo);
	CallFunctionAsync(LaunchListDialog, fileList);

	return 0;
}

void InitListNew()
{
	CreateServiceFunction(MODULENAME "/ParseUri", ParseUriService);
	CreateServiceFunction(MS_PU_SHOWLIST, ShowListCommand);
}
