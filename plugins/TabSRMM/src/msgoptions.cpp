/////////////////////////////////////////////////////////////////////////////////////////
// Miranda NG: the free IM client for Microsoft* Windows*
//
// Copyright (C) 2012-19 Miranda NG team,
// Copyright (c) 2000-09 Miranda ICQ/IM project,
// all portions of this codebase are copyrighted to the people
// listed in contributors.txt.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// you should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// part of tabSRMM messaging plugin for Miranda.
//
// (C) 2005-2010 by silvercircle _at_ gmail _dot_ com and contributors
//
// Implementation of the option pages

#include "stdafx.h"
#include "templates.h"

#define DM_GETSTATUSMASK (WM_USER + 10)

HIMAGELIST CreateStateImageList()
{
	HIMAGELIST himlStates = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 4, 0);
	ImageList_AddIcon(himlStates, PluginConfig.g_IconUnchecked); /* IMG_NOCHECK */
	ImageList_AddIcon(himlStates, PluginConfig.g_IconChecked); /* IMG_CHECK */
	ImageList_AddIcon(himlStates, PluginConfig.g_IconGroupOpen); /* IMG_GRPOPEN */
	ImageList_AddIcon(himlStates, PluginConfig.g_IconGroupClose); /* IMG_GRPCLOSED */
	return himlStates;
}

void LoadLogfont(int section, int i, LOGFONTA * lf, COLORREF * colour, char *szModule)
{
	LOGFONT lfResult;
	LoadMsgDlgFont(section, i, &lfResult, colour, szModule);
	if (lf) {
		lf->lfHeight = lfResult.lfHeight;
		lf->lfWidth = lfResult.lfWidth;
		lf->lfEscapement = lfResult.lfEscapement;
		lf->lfOrientation = lfResult.lfOrientation;
		lf->lfWeight = lfResult.lfWeight;
		lf->lfItalic = lfResult.lfItalic;
		lf->lfUnderline = lfResult.lfUnderline;
		lf->lfStrikeOut = lfResult.lfStrikeOut;
		lf->lfCharSet = lfResult.lfCharSet;
		lf->lfOutPrecision = lfResult.lfOutPrecision;
		lf->lfClipPrecision = lfResult.lfClipPrecision;
		lf->lfQuality = lfResult.lfQuality;
		lf->lfPitchAndFamily = lfResult.lfPitchAndFamily;
		mir_snprintf(lf->lfFaceName, "%S", lfResult.lfFaceName);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////

void TreeViewInit(HWND hwndTree, UINT id, DWORD dwFlags, BOOL bFromMem)
{
	TVINSERTSTRUCT tvi = {};
	TOptionListGroup *lvGroups = CTranslator::getGroupTree(id);
	TOptionListItem *lvItems = CTranslator::getTree(id);

	SetWindowLongPtr(hwndTree, GWL_STYLE, GetWindowLongPtr(hwndTree, GWL_STYLE) | (TVS_NOHSCROLL));
	/* Replace image list, destroy old. */
	ImageList_Destroy(TreeView_SetImageList(hwndTree, CreateStateImageList(), TVSIL_NORMAL));

	// fill the list box, create groups first, then add items
	for (int i = 0; lvGroups[i].szName != nullptr; i++) {
		tvi.hParent = nullptr;
		tvi.hInsertAfter = TVI_LAST;
		tvi.item.mask = TVIF_TEXT | TVIF_STATE | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
		tvi.item.pszText = TranslateW(lvGroups[i].szName);
		tvi.item.stateMask = TVIS_EXPANDED | TVIS_BOLD;
		tvi.item.state = TVIS_EXPANDED | TVIS_BOLD;
		tvi.item.iImage = tvi.item.iSelectedImage = IMG_GRPOPEN;
		lvGroups[i].handle = (LRESULT)TreeView_InsertItem(hwndTree, &tvi);
	}

	for (int i = 0; lvItems[i].szName != nullptr; i++) {
		tvi.hParent = (HTREEITEM)lvGroups[lvItems[i].uGroup].handle;
		tvi.hInsertAfter = TVI_LAST;
		tvi.item.pszText = TranslateW(lvItems[i].szName);
		tvi.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
		tvi.item.lParam = i;
		if (bFromMem == FALSE) {
			switch (lvItems[i].uType) {
			case LOI_TYPE_FLAG:
				tvi.item.iImage = tvi.item.iSelectedImage = ((dwFlags & (UINT)lvItems[i].lParam) ? IMG_CHECK : IMG_NOCHECK);
				break;
			case LOI_TYPE_SETTING:
				tvi.item.iImage = tvi.item.iSelectedImage = (M.GetByte((char *)lvItems[i].lParam, lvItems[i].id) ? IMG_CHECK : IMG_NOCHECK);
				break;
			}
		}
		else {
			switch (lvItems[i].uType) {
			case LOI_TYPE_FLAG:
				tvi.item.iImage = tvi.item.iSelectedImage = (((*((UINT*)lvItems[i].lParam)) & lvItems[i].id) ? IMG_CHECK : IMG_NOCHECK);
				break;
			case LOI_TYPE_SETTING:
				tvi.item.iImage = tvi.item.iSelectedImage = ((*((BOOL*)lvItems[i].lParam)) ? IMG_CHECK : IMG_NOCHECK);
				break;
			}
		}
		lvItems[i].handle = (LRESULT)TreeView_InsertItem(hwndTree, &tvi);
	}

}

void TreeViewDestroy(HWND hwndTree)
{
	ImageList_Destroy(TreeView_GetImageList(hwndTree, TVSIL_NORMAL));
}

void TreeViewSetFromDB(HWND hwndTree, UINT id, DWORD dwFlags)
{
	TVITEM item = { 0 };
	TOptionListItem *lvItems = CTranslator::getTree(id);

	for (int i = 0; lvItems[i].szName != nullptr; i++) {
		item.mask = TVIF_HANDLE | TVIF_IMAGE;
		item.hItem = (HTREEITEM)lvItems[i].handle;
		if (lvItems[i].uType == LOI_TYPE_FLAG)
			item.iImage = item.iSelectedImage = ((dwFlags & (UINT)lvItems[i].lParam) ? IMG_CHECK : IMG_NOCHECK);
		else if (lvItems[i].uType == LOI_TYPE_SETTING)
			item.iImage = item.iSelectedImage = (M.GetByte((char *)lvItems[i].lParam, lvItems[i].id) ? IMG_CHECK : IMG_NOCHECK);
		TreeView_SetItem(hwndTree, &item);
	}
}

void TreeViewToDB(HWND hwndTree, UINT id, char *DBPath, DWORD *dwFlags)
{
	TVITEM item = { 0 };
	TOptionListItem *lvItems = CTranslator::getTree(id);

	for (int i = 0; lvItems[i].szName != nullptr; i++) {
		item.mask = TVIF_HANDLE | TVIF_IMAGE;
		item.hItem = (HTREEITEM)lvItems[i].handle;
		TreeView_GetItem(hwndTree, &item);

		switch (lvItems[i].uType) {
		case LOI_TYPE_FLAG:
			if (dwFlags != nullptr)
				(*dwFlags) |= (item.iImage == IMG_CHECK) ? lvItems[i].lParam : 0;
			if (DBPath == nullptr) {
				UINT *tm = (UINT*)lvItems[i].lParam;
				(*tm) = (item.iImage == IMG_CHECK) ? ((*tm) | lvItems[i].id) : ((*tm) & ~lvItems[i].id);
			}
			break;
		case LOI_TYPE_SETTING:
			if (DBPath != nullptr) {
				db_set_b(0, DBPath, (char *)lvItems[i].lParam, (BYTE)((item.iImage == IMG_CHECK) ? 1 : 0));
			}
			else {
				(*((BOOL*)lvItems[i].lParam)) = ((item.iImage == IMG_CHECK) ? TRUE : FALSE);
			}
			break;
		}
	}
}

BOOL TreeViewHandleClick(HWND hwndDlg, HWND hwndTree, WPARAM, LPARAM lParam)
{
	TVITEM item = { 0 };
	TVHITTESTINFO hti;

	switch (((LPNMHDR)lParam)->code) {
	case TVN_KEYDOWN:
		if (((LPNMTVKEYDOWN)lParam)->wVKey != VK_SPACE)
			return FALSE;
		hti.flags = TVHT_ONITEMSTATEICON;
		item.hItem = TreeView_GetSelection(((LPNMHDR)lParam)->hwndFrom);
		break;
	case NM_CLICK:
		hti.pt.x = (short)LOWORD(GetMessagePos());
		hti.pt.y = (short)HIWORD(GetMessagePos());
		ScreenToClient(((LPNMHDR)lParam)->hwndFrom, &hti.pt);
		if (TreeView_HitTest(((LPNMHDR)lParam)->hwndFrom, &hti) == nullptr)
			return FALSE;
		if ((hti.flags & TVHT_ONITEMICON) == 0)
			return FALSE;
		item.hItem = (HTREEITEM)hti.hItem;
		break;

	case TVN_ITEMEXPANDEDW:
		{
			LPNMTREEVIEWW lpnmtv = (LPNMTREEVIEWW)lParam;

			item.mask = TVIF_HANDLE | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
			item.hItem = lpnmtv->itemNew.hItem;
			item.iImage = item.iSelectedImage =
				(lpnmtv->itemNew.state & TVIS_EXPANDED) ? IMG_GRPOPEN : IMG_GRPCLOSED;
			SendMessageW(((LPNMHDR)lParam)->hwndFrom, TVM_SETITEMW, 0, (LPARAM)&item);
		}
		return TRUE;

	default:
		return FALSE;
	}

	item.mask = TVIF_HANDLE | TVIF_IMAGE;
	item.stateMask = TVIS_BOLD;
	SendMessage(hwndTree, TVM_GETITEM, 0, (LPARAM)&item);
	item.mask |= TVIF_SELECTEDIMAGE;
	switch (item.iImage) {
	case IMG_NOCHECK:
		item.iImage = IMG_CHECK;
		break;
	case IMG_CHECK:
		item.iImage = IMG_NOCHECK;
		break;
	case IMG_GRPOPEN:
		item.mask |= TVIF_STATE;
		item.stateMask |= TVIS_EXPANDED;
		item.state = 0;
		item.iImage = IMG_GRPCLOSED;
		break;
	case IMG_GRPCLOSED:
		item.mask |= TVIF_STATE;
		item.stateMask |= TVIS_EXPANDED;
		item.state |= TVIS_EXPANDED;
		item.iImage = IMG_GRPOPEN;
		break;
	}
	item.iSelectedImage = item.iImage;
	SendMessage(hwndTree, TVM_SETITEM, 0, (LPARAM)&item);
	if (item.mask & TVIF_STATE) {
		RedrawWindow(hwndTree, nullptr, nullptr, RDW_INVALIDATE | RDW_NOFRAME | RDW_ERASENOW | RDW_ALLCHILDREN);
		InvalidateRect(hwndTree, nullptr, TRUE);
	}
	else SendMessage(GetParent(hwndDlg), PSM_CHANGED, 0, 0);

	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////////
// options dialog for setting up tab options

/////////////////////////////////////////////////////////////////////////////////////////
// controls to disable when loading or unloading a skin is not possible (because
// of at least one message window being open).

static UINT _ctrls[] = { IDC_SKINNAME, IDC_RESCANSKIN, IDC_RESCANSKIN, IDC_RELOADSKIN, 0 };

class CSkinOptsDlg : public CDlgBase
{
	// mir_free the item extra data (used to store the skin filenames for each entry).
	void FreeComboData()
	{
		LRESULT lr = cmbSkins.GetCount();
		for (int i = 1; i < lr; i++) {
			void *idata = (void *)cmbSkins.GetItemData(i);
			if (idata && idata != (void *)CB_ERR)
				mir_free(idata);
		}
	}

	// scan the skin root folder for subfolder(s).Each folder is supposed to contain a single
	// skin. This function won't dive deeper into the folder structure, so the folder
	// structure for any VALID skin should be:
	// $SKINS_ROOT/skin_folder/skin_name.tsk
	//
	// By default, $SKINS_ROOT is set to %miranda_userdata% or custom folder
	// selected by the folders plugin.

	void RescanSkins()
	{
		wchar_t tszSkinRoot[MAX_PATH], tszFindMask[MAX_PATH];
		wcsncpy_s(tszSkinRoot, M.getSkinPath(), _TRUNCATE);

		SetDlgItemText(m_hwnd, IDC_SKINROOTFOLDER, tszSkinRoot);
		mir_snwprintf(tszFindMask, L"%s*.*", tszSkinRoot);

		cmbSkins.ResetContent();
		cmbSkins.AddString(TranslateT("<no skin>"));

		WIN32_FIND_DATA fd = {};
		HANDLE h = FindFirstFile(tszFindMask, &fd);
		while (h != INVALID_HANDLE_VALUE) {
			if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY && fd.cFileName[0] != '.') {
				wchar_t	tszSubDir[MAX_PATH];
				mir_snwprintf(tszSubDir, L"%s%s\\", tszSkinRoot, fd.cFileName);
				ScanSkinDir(tszSubDir);
			}
			if (FindNextFile(h, &fd) == 0)
				break;
		}
		if (h != INVALID_HANDLE_VALUE)
			FindClose(h);

		ptrW wszCurrSkin(db_get_wsa(0, SRMSGMOD_T, "ContainerSkin"));
		LRESULT lr = cmbSkins.GetCount();
		for (int i = 1; i < lr; i++) {
			wchar_t *idata = (wchar_t *)cmbSkins.GetItemData(i);
			if (idata && idata != (wchar_t *)CB_ERR) {
				if (!mir_wstrcmpi(wszCurrSkin, idata)) {
					cmbSkins.SetCurSel(i);
					return;
				}
			}
		}

		// if no active skin present, set the focus to the first one
		cmbSkins.SetCurSel(0);
	}

	// scan a single skin directory and find the.TSK file.Fill the combobox and set the
	// relative path name as item extra data.
	//
	// If available, read the Name property from the [Global] section and use it in the
	// combo box. If such property is not found, the base filename (without .tsk extension)
	// will be used as the name of the skin.

	void ScanSkinDir(const wchar_t *tszFolder)
	{
		bool fValid = false;
		wchar_t tszMask[MAX_PATH];
		mir_snwprintf(tszMask, L"%s*.*", tszFolder);

		WIN32_FIND_DATA fd = { 0 };
		HANDLE h = FindFirstFile(tszMask, &fd);
		while (h != INVALID_HANDLE_VALUE) {
			if (mir_wstrlen(fd.cFileName) >= 5 && !wcsnicmp(fd.cFileName + mir_wstrlen(fd.cFileName) - 4, L".tsk", 4)) {
				fValid = true;
				break;
			}
			if (FindNextFile(h, &fd) == 0)
				break;
		}
		if (h != INVALID_HANDLE_VALUE)
			FindClose(h);

		if (!fValid)
			return;

		wchar_t tszFinalName[MAX_PATH], tszRel[MAX_PATH], szBuf[255];
		mir_snwprintf(tszFinalName, L"%s%s", tszFolder, fd.cFileName);
		GetPrivateProfileString(L"Global", L"Name", L"None", szBuf, _countof(szBuf), tszFinalName);

		if (!mir_wstrcmp(szBuf, L"None")) {
			fd.cFileName[mir_wstrlen(fd.cFileName) - 4] = 0;
			wcsncpy_s(szBuf, fd.cFileName, _TRUNCATE);
		}

		PathToRelativeW(tszFinalName, tszRel, M.getSkinPath());
		cmbSkins.AddString(szBuf, (LPARAM)mir_wstrdup(tszRel));
	}

	// self - configure the dialog, don't let the user load or unload
	// a skin while a message window is open. Show the warning that all
	// windows must be closed.
	void UpdateControls(CTimer* = nullptr)
	{
		bool fWindowsOpen = (pFirstContainer != nullptr ? true : false);
		for (auto &it : _ctrls)
			Utils::enableDlgControl(m_hwnd, it, !fWindowsOpen);

		Utils::showDlgControl(m_hwnd, IDC_SKIN_WARN, fWindowsOpen ? SW_SHOW : SW_HIDE);
		Utils::showDlgControl(m_hwnd, IDC_SKIN_CLOSENOW, fWindowsOpen ? SW_SHOW : SW_HIDE);
	}

	CTimer m_timer;
	CCtrlCheck chkUseSkin, chkLoadFonts, chkLoadTempl;
	CCtrlCombo cmbSkins;
	CCtrlButton btnClose, btnReload, btnRescan, btnExport, btnImport;
	CCtrlHyperlink m_link1, m_link2;

public:
	CSkinOptsDlg() :
		CDlgBase(g_plugin, IDD_OPT_SKIN),
		m_timer(this, 1000),
		m_link1(this, IDC_GETSKINS, "https://miranda-ng.org/addons/category/19"),
		m_link2(this, IDC_HELP_GENERAL, "https://wiki.miranda-ng.org/index.php?title=Plugin:TabSRMM/en/Using_skins"),
		cmbSkins(this, IDC_SKINNAME),
		btnClose(this, IDC_SKIN_CLOSENOW),
		btnReload(this, IDC_RELOADSKIN),
		btnRescan(this, IDC_RESCANSKIN),
		btnExport(this, IDC_THEMEEXPORT),
		btnImport(this, IDC_THEMEIMPORT),
		chkUseSkin(this, IDC_USESKIN),
		chkLoadFonts(this, IDC_SKIN_LOADFONTS),
		chkLoadTempl(this, IDC_SKIN_LOADTEMPLATES)
	{
		m_timer.OnEvent = Callback(this, &CSkinOptsDlg::UpdateControls);

		btnClose.OnClick = Callback(this, &CSkinOptsDlg::onClick_Close);
		btnReload.OnClick = Callback(this, &CSkinOptsDlg::onClick_Reload);
		btnRescan.OnClick = Callback(this, &CSkinOptsDlg::onClick_Rescan);
		btnExport.OnClick = Callback(this, &CSkinOptsDlg::onClick_Export);
		btnImport.OnClick = Callback(this, &CSkinOptsDlg::onClick_Import);

		chkUseSkin.OnChange = Callback(this, &CSkinOptsDlg::onChange_UseSkin);
		chkLoadFonts.OnChange = Callback(this, &CSkinOptsDlg::onChange_LoadFonts);
		chkLoadTempl.OnChange = Callback(this, &CSkinOptsDlg::onChange_LoadTemplates);

		cmbSkins.OnSelChanged = Callback(this, &CSkinOptsDlg::onSelChange_Skins);
	}

	bool OnInitDialog() override
	{
		RescanSkins();

		chkUseSkin.SetState(M.GetByte("useskin", 0));

		int loadMode = M.GetByte("skin_loadmode", 0);
		CheckDlgButton(m_hwnd, IDC_SKIN_LOADFONTS, loadMode & THEME_READ_FONTS ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(m_hwnd, IDC_SKIN_LOADTEMPLATES, loadMode & THEME_READ_TEMPLATES ? BST_CHECKED : BST_UNCHECKED);

		UpdateControls();
		m_timer.Start(1000);
		return true;
	}

	void OnDestroy() override
	{
		m_timer.Stop();
		FreeComboData();
	}

	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) override
	{
		if (msg == WM_CTLCOLORSTATIC && (HWND)lParam == GetDlgItem(m_hwnd, IDC_SKIN_WARN)) {
			SetTextColor((HDC)wParam, RGB(255, 50, 50));
			return 0;
		}

		return CDlgBase::DlgProc(msg, wParam, lParam);
	}

	void onClick_Close(CCtrlButton *)
	{
		CloseAllContainers();
	}

	void onChange_UseSkin(CCtrlCheck *)
	{
		db_set_b(0, SRMSGMOD_T, "useskin", chkUseSkin.GetState());
	}

	void onChange_LoadFonts(CCtrlCheck *)
	{
		int loadMode = M.GetByte("skin_loadmode", 0);
		loadMode = IsDlgButtonChecked(m_hwnd, IDC_SKIN_LOADFONTS) ? loadMode | THEME_READ_FONTS : loadMode & ~THEME_READ_FONTS;
		db_set_b(0, SRMSGMOD_T, "skin_loadmode", loadMode);
	}

	void onChange_LoadTemplates(CCtrlCheck *)
	{
		int loadMode = M.GetByte("skin_loadmode", 0);
		loadMode = IsDlgButtonChecked(m_hwnd, IDC_SKIN_LOADTEMPLATES) ? loadMode | THEME_READ_TEMPLATES : loadMode & ~THEME_READ_TEMPLATES;
		db_set_b(0, SRMSGMOD_T, "skin_loadmode", loadMode);
	}

	void onClick_Reload(CCtrlButton *)
	{
		Skin->setFileName();
		Skin->Load();
		UpdateControls();
	}

	void onClick_Rescan(CCtrlButton *)
	{
		FreeComboData();
		RescanSkins();
	}

	void onClick_Export(CCtrlButton *)
	{
		const wchar_t *szFilename = GetThemeFileName(1);
		if (szFilename != nullptr)
			WriteThemeToINI(szFilename, nullptr);
	}

	void onClick_Import(CCtrlButton *)
	{
		LRESULT r = CWarning::show(CSkin::m_skinEnabled ? CWarning::WARN_THEME_OVERWRITE : CWarning::WARN_OPTION_CLOSE, MB_YESNOCANCEL | MB_ICONQUESTION);
		if (r == IDNO || r == IDCANCEL)
			return;

		const wchar_t *szFilename = GetThemeFileName(0);
		DWORD dwFlags = THEME_READ_FONTS;

		if (szFilename != nullptr) {
			int result = MessageBox(nullptr, TranslateT("Do you want to also read message templates from the theme?\nCaution: This will overwrite the stored template set which may affect the look of your message window significantly.\nSelect Cancel to not load anything at all."),
				TranslateT("Load theme"), MB_YESNOCANCEL);
			if (result == IDCANCEL)
				return;
			if (result == IDYES)
				dwFlags |= THEME_READ_TEMPLATES;
			ReadThemeFromINI(szFilename, nullptr, 0, dwFlags);
			CacheLogFonts();
			CacheMsgLogIcons();
			PluginConfig.reloadSettings();
			CSkin::setAeroEffect(-1);
			Srmm_Broadcast(DM_OPTIONSAPPLIED, 1, 0);
			Srmm_Broadcast(DM_FORCEDREMAKELOG, 0, 0);
			SendMessage(GetParent(m_hwnd), WM_COMMAND, IDCANCEL, 0);
		}
	}

	void onSelChange_Skins(CCtrlCombo *)
	{
		LRESULT lr = cmbSkins.GetCurSel();
		if (lr != CB_ERR && lr > 0) {
			wchar_t *tszRelPath = (wchar_t *)cmbSkins.GetItemData(lr);
			if (tszRelPath && tszRelPath != (wchar_t *)CB_ERR)
				db_set_ws(0, SRMSGMOD_T, "ContainerSkin", tszRelPath);
			onClick_Reload(0);
		}
		else if (lr == 0) {		// selected the <no skin> entry
			db_unset(0, SRMSGMOD_T, "ContainerSkin");
			Skin->Unload();
			UpdateControls();
		}
	}
};

class CTabConfigDlg : public CDlgBase
{
	CCtrlSpin adjust, border, outerL, outerR, outerT, outerB, width, xpad, ypad;

public:
	CTabConfigDlg() :
		CDlgBase(g_plugin, IDD_TABCONFIG),
		ypad(this, IDC_SPIN1, 10, 1),
		xpad(this, IDC_SPIN3, 10, 1),
		width(this, IDC_TABWIDTHSPIN, 400, 50),
		adjust(this, IDC_BOTTOMTABADJUSTSPIN, 3, -3),
		border(this, IDC_TABBORDERSPIN, 10),
		outerL(this, IDC_TABBORDERSPINOUTER, 50),
		outerR(this, IDC_TABBORDERSPINOUTERRIGHT, 50),
		outerT(this, IDC_TABBORDERSPINOUTERTOP, 40),
		outerB(this, IDC_TABBORDERSPINOUTERBOTTOM, 40)
	{
	}

	bool OnInitDialog() override
	{
		width.SetPosition(PluginConfig.tabConfig.m_fixedwidth);
		adjust.SetPosition(PluginConfig.tabConfig.m_bottomAdjust);

		border.SetPosition(M.GetByte(CSkin::m_skinEnabled ? "S_tborder" : "tborder", 2));
		outerL.SetPosition(M.GetByte(CSkin::m_skinEnabled ? "S_tborder_outer_left" : "tborder_outer_left", 2));
		outerR.SetPosition(M.GetByte(CSkin::m_skinEnabled ? "S_tborder_outer_right" : "tborder_outer_right", 2));
		outerT.SetPosition(M.GetByte(CSkin::m_skinEnabled ? "S_tborder_outer_top" : "tborder_outer_top", 2));
		outerB.SetPosition(M.GetByte(CSkin::m_skinEnabled ? "S_tborder_outer_bottom" : "tborder_outer_bottom", 2));

		xpad.SetPosition(M.GetByte("x-pad", 4));
		ypad.SetPosition(M.GetByte("y-pad", 3));
		return true;
	}

	bool OnApply() override
	{
		db_set_b(0, SRMSGMOD_T, "y-pad", ypad.GetPosition());
		db_set_b(0, SRMSGMOD_T, "x-pad", xpad.GetPosition());
		db_set_b(0, SRMSGMOD_T, CSkin::m_skinEnabled ? "S_tborder" : "tborder", border.GetPosition());
		db_set_b(0, SRMSGMOD_T, CSkin::m_skinEnabled ? "S_tborder_outer_left" : "tborder_outer_left", outerL.GetPosition());
		db_set_b(0, SRMSGMOD_T, CSkin::m_skinEnabled ? "S_tborder_outer_right" : "tborder_outer_right", outerR.GetPosition());
		db_set_b(0, SRMSGMOD_T, CSkin::m_skinEnabled ? "S_tborder_outer_top" : "tborder_outer_top", outerT.GetPosition());
		db_set_b(0, SRMSGMOD_T, CSkin::m_skinEnabled ? "S_tborder_outer_bottom" : "tborder_outer_bottom", outerB.GetPosition());
		db_set_dw(0, SRMSGMOD_T, "bottomadjust", adjust.GetPosition());

		int fixedWidth = width.GetPosition();
		fixedWidth = (fixedWidth < 60 ? 60 : fixedWidth);
		db_set_dw(0, SRMSGMOD_T, "fixedwidth", fixedWidth);
		FreeTabConfig();
		ReloadTabConfig();

		for (TContainerData* p = pFirstContainer; p; p = p->pNext) {
			HWND hwndTab = GetDlgItem(p->m_hwnd, IDC_MSGTABS);
			TabCtrl_SetPadding(hwndTab, xpad.GetPosition(), ypad.GetPosition());
			RedrawWindow(hwndTab, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE);
		}
		return true;
	}
};

/////////////////////////////////////////////////////////////////////////////////////////
// Main options dialog

class COptMainDlg : public CDlgBase
{
	CCtrlSpin spnAvaSize;
	CCtrlCheck chkAvaPreserve;
	CCtrlButton btnReset;
	CCtrlHyperlink urlHelp;

public:
	COptMainDlg() :
		CDlgBase(g_plugin, IDD_OPT_MSGDLG),
		urlHelp(this, IDC_HELP_GENERAL, "https://wiki.miranda-ng.org/index.php?title=Plugin:TabSRMM/en/General_settings"),
		btnReset(this, IDC_RESETWARNINGS),
		spnAvaSize(this, IDC_AVATARSPIN, 150),
		chkAvaPreserve(this, IDC_PRESERVEAVATARSIZE)
	{
		btnReset.OnClick = Callback(this, &COptMainDlg::onClick_Reset);
	}

	bool OnInitDialog() override
	{
		TreeViewInit(GetDlgItem(m_hwnd, IDC_WINDOWOPTIONS), CTranslator::TREE_MSG, 0, FALSE);

		chkAvaPreserve.SetState(M.GetByte("dontscaleavatars", 0));

		spnAvaSize.SetPosition(M.GetDword("avatarheight", 100));
		return true;
	}

	bool OnApply() override
	{
		db_set_dw(0, SRMSGMOD_T, "avatarheight", spnAvaSize.GetPosition());
		db_set_b(0, SRMSGMOD_T, "dontscaleavatars", chkAvaPreserve.GetState());

		// scan the tree view and obtain the options...
		TreeViewToDB(GetDlgItem(m_hwnd, IDC_WINDOWOPTIONS), CTranslator::TREE_MSG, SRMSGMOD_T, nullptr);
		PluginConfig.reloadSettings();
		Srmm_Broadcast(DM_OPTIONSAPPLIED, 1, 0);
		return true;
	}

	void OnDestroy() override
	{
		TreeViewDestroy(GetDlgItem(m_hwnd, IDC_WINDOWOPTIONS));
	}

	void onClick_Reset(CCtrlButton*)
	{
		db_set_dw(0, SRMSGMOD_T, "cWarningsL", 0);
		db_set_dw(0, SRMSGMOD_T, "cWarningsH", 0);
	}

	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) override
	{
		if (msg == WM_NOTIFY && ((LPNMHDR)lParam)->idFrom == IDC_WINDOWOPTIONS)
			return TreeViewHandleClick(m_hwnd, ((LPNMHDR)lParam)->hwndFrom, wParam, lParam);

		return CDlgBase::DlgProc(msg, wParam, lParam);
	}
};

/////////////////////////////////////////////////////////////////////////////////////////

static UINT __ctrls[] = { IDC_INDENTSPIN, IDC_RINDENTSPIN, IDC_INDENTAMOUNT, IDC_RIGHTINDENT, IDC_MODIFY, IDC_RTLMODIFY };

class COptLogDlg : public CDlgBase
{
	CCtrlSpin spnLeft, spnRight, spnLoadCount, spnLoadTime, spnTrim;
	CCtrlCheck chkAlwaysTrim, chkLoadUnread, chkLoadCount, chkLoadTime;
	CCtrlCombo cmbLogDisplay;
	CCtrlButton btnModify, btnRtlModify;

	bool have_ieview, have_hpp;

	// configure the option page - hide most of the settings here when either IEView
	// or H++ is set as the global message log viewer. Showing these options may confuse
	// the user, because they are not working and the user needs to configure the 3rd
	// party plugin.

	void ShowHide()
	{
		LRESULT r = cmbLogDisplay.GetCurSel();
		Utils::showDlgControl(m_hwnd, IDC_EXPLAINMSGLOGSETTINGS, r == 0 ? SW_HIDE : SW_SHOW);
		Utils::showDlgControl(m_hwnd, IDC_LOGOPTIONS, r == 0 ? SW_SHOW : SW_HIDE);

		for (auto &it : __ctrls)
			Utils::enableDlgControl(m_hwnd, it, r == 0);
	}

public:
	COptLogDlg() :
		CDlgBase(g_plugin, IDD_OPT_MSGLOG),
		btnModify(this, IDC_MODIFY),
		btnRtlModify(this, IDC_RTLMODIFY),
		spnTrim(this, IDC_TRIMSPIN, 1000, 5),
		spnLeft(this, IDC_INDENTSPIN, 1000),
		spnRight(this, IDC_RINDENTSPIN, 1000),
		spnLoadTime(this, IDC_LOADTIMESPIN, 24 * 60),
		spnLoadCount(this, IDC_LOADCOUNTSPIN, 100),
		chkLoadTime(this, IDC_LOADTIME),
		chkLoadCount(this, IDC_LOADCOUNT),
		chkAlwaysTrim(this, IDC_ALWAYSTRIM),
		chkLoadUnread(this, IDC_LOADUNREAD),
		cmbLogDisplay(this, IDC_MSGLOGDIDSPLAY)
	{
		btnModify.OnClick = Callback(this, &COptLogDlg::onClick_Modify);
		btnRtlModify.OnClick = Callback(this, &COptLogDlg::onClick_RtlModify);

		cmbLogDisplay.OnChange = Callback(this, &COptLogDlg::onChange_Combo);

		chkAlwaysTrim.OnChange = Callback(this, &COptLogDlg::onChange_Trim);
		chkLoadTime.OnChange = chkLoadCount.OnChange = chkLoadUnread.OnChange = Callback(this, &COptLogDlg::onChange_Load);

		have_ieview = ServiceExists(MS_IEVIEW_WINDOW) != 0;
		have_hpp = ServiceExists("History++/ExtGrid/NewWindow") != 0;
	}

	bool OnInitDialog() override
	{
		DWORD dwFlags = M.GetDword("mwflags", MWF_LOG_DEFAULT);

		switch (g_plugin.getByte(SRMSGSET_LOADHISTORY, SRMSGDEFSET_LOADHISTORY)) {
		case LOADHISTORY_UNREAD:
			chkLoadUnread.SetState(true);
			break;
		case LOADHISTORY_COUNT:
			chkLoadCount.SetState(true);
			Utils::enableDlgControl(m_hwnd, IDC_LOADCOUNTN, true);
			spnLoadCount.Enable(true);
			break;
		case LOADHISTORY_TIME:
			chkLoadTime.SetState(true);
			Utils::enableDlgControl(m_hwnd, IDC_LOADTIMEN, true);
			spnLoadTime.Enable(true);
			Utils::enableDlgControl(m_hwnd, IDC_STMINSOLD, true);
			break;
		}

		TreeViewInit(GetDlgItem(m_hwnd, IDC_LOGOPTIONS), CTranslator::TREE_LOG, dwFlags, FALSE);

		spnLeft.SetPosition(M.GetDword("IndentAmount", 20));
		spnRight.SetPosition(M.GetDword("RightIndent", 20));
		spnLoadCount.SetPosition(g_plugin.getWord(SRMSGSET_LOADCOUNT, SRMSGDEFSET_LOADCOUNT));
		spnLoadTime.SetPosition(g_plugin.getWord(SRMSGSET_LOADTIME, SRMSGDEFSET_LOADTIME));

		DWORD maxhist = M.GetDword("maxhist", 0);
		spnTrim.SetPosition(maxhist);
		spnTrim.Enable(maxhist != 0);
		Utils::enableDlgControl(m_hwnd, IDC_TRIM, maxhist != 0);
		chkAlwaysTrim.SetState(maxhist != 0);

		cmbLogDisplay.AddString(TranslateT("Internal message log"));
		cmbLogDisplay.SetCurSel(0);
		if (have_ieview || have_hpp) {
			if (have_ieview) {
				cmbLogDisplay.AddString(TranslateT("IEView plugin"));
				if (M.GetByte("default_ieview", 0))
					cmbLogDisplay.SetCurSel(1);
			}
			if (have_hpp) {
				cmbLogDisplay.AddString(TranslateT("History++ plugin"));
				if (M.GetByte("default_ieview", 0))
					cmbLogDisplay.SetCurSel(1);
				else if (M.GetByte("default_hpp", 0))
					cmbLogDisplay.SetCurSel(have_ieview ? 2 : 1);
			}
		}
		else cmbLogDisplay.Disable();

		SetDlgItemText(m_hwnd, IDC_EXPLAINMSGLOGSETTINGS, TranslateT("You have chosen to use an external plugin for displaying the message history in the chat window. Most of the settings on this page are for the standard message log viewer only and will have no effect. To change the appearance of the message log, you must configure either IEView or History++."));
		ShowHide();
		return true;
	}

	bool OnApply() override
	{
		LRESULT msglogmode = cmbLogDisplay.GetCurSel();
		DWORD dwFlags = M.GetDword("mwflags", MWF_LOG_DEFAULT);

		dwFlags &= ~(MWF_LOG_ALL);

		if (chkLoadCount.GetState())
			g_plugin.setByte(SRMSGSET_LOADHISTORY, LOADHISTORY_COUNT);
		else if (chkLoadTime.GetState())
			g_plugin.setByte(SRMSGSET_LOADHISTORY, LOADHISTORY_TIME);
		else
			g_plugin.setByte(SRMSGSET_LOADHISTORY, LOADHISTORY_UNREAD);
		g_plugin.setWord(SRMSGSET_LOADCOUNT, spnLoadCount.GetPosition());
		g_plugin.setWord(SRMSGSET_LOADTIME, spnLoadTime.GetPosition());

		db_set_dw(0, SRMSGMOD_T, "IndentAmount", spnLeft.GetPosition());
		db_set_dw(0, SRMSGMOD_T, "RightIndent", spnRight.GetPosition());

		db_set_b(0, SRMSGMOD_T, "default_ieview", 0);
		db_set_b(0, SRMSGMOD_T, "default_hpp", 0);
		switch (msglogmode) {
		case 0:
			break;
		case 1:
			if (have_ieview)
				db_set_b(0, SRMSGMOD_T, "default_ieview", 1);
			else
				db_set_b(0, SRMSGMOD_T, "default_hpp", 1);
			break;
		case 2:
			db_set_b(0, SRMSGMOD_T, "default_hpp", 1);
			break;
		}

		// scan the tree view and obtain the options...
		TreeViewToDB(GetDlgItem(m_hwnd, IDC_LOGOPTIONS), CTranslator::TREE_LOG, SRMSGMOD_T, &dwFlags);
		db_set_dw(0, SRMSGMOD_T, "mwflags", dwFlags);
		if (chkAlwaysTrim.GetState())
			db_set_dw(0, SRMSGMOD_T, "maxhist", spnTrim.GetPosition());
		else
			db_set_dw(0, SRMSGMOD_T, "maxhist", 0);
		PluginConfig.reloadSettings();
		Srmm_Broadcast(DM_OPTIONSAPPLIED, 1, 0);
		return true;
	}

	void OnDestroy() override
	{
		TreeViewDestroy(GetDlgItem(m_hwnd, IDC_LOGOPTIONS));
	}

	void onChange_Trim(CCtrlCheck*)
	{
		bool bEnabled = chkAlwaysTrim.GetState();
		spnTrim.Enable(bEnabled);
		Utils::enableDlgControl(m_hwnd, IDC_TRIM, bEnabled);
	}

	void onChange_Load(CCtrlCheck*)
	{
		bool bEnabled = chkLoadCount.GetState();
		Utils::enableDlgControl(m_hwnd, IDC_LOADCOUNTN, bEnabled);
		Utils::enableDlgControl(m_hwnd, IDC_LOADCOUNTSPIN, bEnabled);

		bEnabled = chkLoadTime.GetState();
		Utils::enableDlgControl(m_hwnd, IDC_LOADTIMEN, bEnabled);
		spnLoadTime.Enable(bEnabled);
		Utils::enableDlgControl(m_hwnd, IDC_STMINSOLD, bEnabled);
	}

	void onClick_Modify(CCtrlButton*)
	{
		CTemplateEditDlg *pDlg = new CTemplateEditDlg(FALSE, m_hwnd);
		pDlg->Show();
	}

	void onClick_RtlModify(CCtrlButton*)
	{
		CTemplateEditDlg *pDlg = new CTemplateEditDlg(TRUE, m_hwnd);
		pDlg->Show();
	}

	void onChange_Combo(CCtrlCombo*)
	{
		ShowHide();
	}

	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) override
	{
		if (msg == WM_NOTIFY && ((LPNMHDR)lParam)->idFrom == IDC_LOGOPTIONS)
			return TreeViewHandleClick(m_hwnd, ((LPNMHDR)lParam)->hwndFrom, wParam, lParam);

		return CDlgBase::DlgProc(msg, wParam, lParam);
	}
};

/////////////////////////////////////////////////////////////////////////////////////////
// typing notify options

class COptTypingDlg : public CDlgBase
{
	HANDLE hItemNew, hItemUnknown;

	CCtrlCheck chkWin, chkNoWin;
	CCtrlCheck chkNotifyPopup, chkNotifyTray, chkShowNotify;
	CCtrlHyperlink urlHelp;

	void ResetCList()
	{
		if (!db_get_b(0, "CList", "UseGroups", SETTING_USEGROUPS_DEFAULT))
			SendDlgItemMessage(m_hwnd, IDC_CLIST, CLM_SETUSEGROUPS, FALSE, 0);
		else
			SendDlgItemMessage(m_hwnd, IDC_CLIST, CLM_SETUSEGROUPS, TRUE, 0);
		SendDlgItemMessage(m_hwnd, IDC_CLIST, CLM_SETHIDEEMPTYGROUPS, 1, 0);
	}

	void RebuildList()
	{
		BYTE defType = g_plugin.getByte(SRMSGSET_TYPINGNEW, SRMSGDEFSET_TYPINGNEW);
		if (hItemNew && defType)
			SendDlgItemMessage(m_hwnd, IDC_CLIST, CLM_SETCHECKMARK, (WPARAM)hItemNew, 1);

		if (hItemUnknown && g_plugin.getByte(SRMSGSET_TYPINGUNKNOWN, SRMSGDEFSET_TYPINGUNKNOWN))
			SendDlgItemMessage(m_hwnd, IDC_CLIST, CLM_SETCHECKMARK, (WPARAM)hItemUnknown, 1);

		for (auto &hContact : Contacts()) {
			HANDLE hItem = (HANDLE)SendDlgItemMessage(m_hwnd, IDC_CLIST, CLM_FINDCONTACT, hContact, 0);
			if (hItem && g_plugin.getByte(hContact, SRMSGSET_TYPING, defType))
				SendDlgItemMessage(m_hwnd, IDC_CLIST, CLM_SETCHECKMARK, (WPARAM)hItem, 1);
		}
	}

	void SaveList()
	{
		if (hItemNew)
			g_plugin.setByte(SRMSGSET_TYPINGNEW, (BYTE)(SendDlgItemMessage(m_hwnd, IDC_CLIST, CLM_GETCHECKMARK, (WPARAM)hItemNew, 0) ? 1 : 0));

		if (hItemUnknown)
			g_plugin.setByte(SRMSGSET_TYPINGUNKNOWN, (BYTE)(SendDlgItemMessage(m_hwnd, IDC_CLIST, CLM_GETCHECKMARK, (WPARAM)hItemUnknown, 0) ? 1 : 0));

		for (auto &hContact : Contacts()) {
			HANDLE hItem = (HANDLE)SendDlgItemMessage(m_hwnd, IDC_CLIST, CLM_FINDCONTACT, hContact, 0);
			if (hItem)
				g_plugin.setByte(hContact, SRMSGSET_TYPING, (BYTE)(SendDlgItemMessage(m_hwnd, IDC_CLIST, CLM_GETCHECKMARK, (WPARAM)hItem, 0) ? 1 : 0));
		}
	}

public:
	COptTypingDlg()
		: CDlgBase(g_plugin, IDD_OPT_MSGTYPE),
		urlHelp(this, IDC_MTN_HELP, "https://wiki.miranda-ng.org/index.php?title=Plugin:TabSRMM/en/Advanced_tweaks"),
		chkWin(this, IDC_TYPEWIN),
		chkNoWin(this, IDC_TYPENOWIN),
		chkNotifyTray(this, IDC_NOTIFYTRAY),
		chkShowNotify(this, IDC_SHOWNOTIFY),
		chkNotifyPopup(this, IDC_NOTIFYPOPUP)
	{
		chkWin.OnChange = chkNoWin.OnChange = Callback(this, &COptTypingDlg::onCheck_Win);

		chkNotifyTray.OnChange = Callback(this, &COptTypingDlg::onCheck_NotifyTray);
		chkShowNotify.OnChange = Callback(this, &COptTypingDlg::onCheck_ShowNotify);
		chkNotifyPopup.OnChange = Callback(this, &COptTypingDlg::onCheck_NotifyPopup);
	}

	bool OnInitDialog() override
	{
		CLCINFOITEM cii = { sizeof(cii) };
		cii.flags = CLCIIF_GROUPFONT | CLCIIF_CHECKBOX;
		cii.pszText = TranslateT("** New contacts **");
		hItemNew = (HANDLE)SendDlgItemMessage(m_hwnd, IDC_CLIST, CLM_ADDINFOITEM, 0, (LPARAM)&cii);
		cii.pszText = TranslateT("** Unknown contacts **");
		hItemUnknown = (HANDLE)SendDlgItemMessage(m_hwnd, IDC_CLIST, CLM_ADDINFOITEM, 0, (LPARAM)&cii);

		SetWindowLongPtr(GetDlgItem(m_hwnd, IDC_CLIST), GWL_STYLE, GetWindowLongPtr(GetDlgItem(m_hwnd, IDC_CLIST), GWL_STYLE) | (CLS_SHOWHIDDEN));
		ResetCList();

		CheckDlgButton(m_hwnd, IDC_SHOWNOTIFY, g_plugin.getByte(SRMSGSET_SHOWTYPING, SRMSGDEFSET_SHOWTYPING) ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(m_hwnd, IDC_TYPEFLASHWIN, g_plugin.getByte(SRMSGSET_SHOWTYPINGWINFLASH, SRMSGDEFSET_SHOWTYPINGWINFLASH) ? BST_CHECKED : BST_UNCHECKED);

		CheckDlgButton(m_hwnd, IDC_TYPENOWIN, g_plugin.getByte(SRMSGSET_SHOWTYPINGNOWINOPEN, 1) ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(m_hwnd, IDC_TYPEWIN, g_plugin.getByte(SRMSGSET_SHOWTYPINGWINOPEN, 1) ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(m_hwnd, IDC_NOTIFYTRAY, g_plugin.getByte(SRMSGSET_SHOWTYPINGCLIST, SRMSGDEFSET_SHOWTYPINGCLIST) ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(m_hwnd, IDC_NOTIFYBALLOON, g_plugin.getByte("ShowTypingBalloon", 0));

		CheckDlgButton(m_hwnd, IDC_NOTIFYPOPUP, g_plugin.getByte("ShowTypingPopup", 0) ? BST_CHECKED : BST_UNCHECKED);

		Utils::enableDlgControl(m_hwnd, IDC_TYPEWIN, IsDlgButtonChecked(m_hwnd, IDC_NOTIFYTRAY) != 0);
		Utils::enableDlgControl(m_hwnd, IDC_TYPENOWIN, IsDlgButtonChecked(m_hwnd, IDC_NOTIFYTRAY) != 0);
		Utils::enableDlgControl(m_hwnd, IDC_NOTIFYBALLOON, IsDlgButtonChecked(m_hwnd, IDC_NOTIFYTRAY) &&
			(IsDlgButtonChecked(m_hwnd, IDC_TYPEWIN) || IsDlgButtonChecked(m_hwnd, IDC_TYPENOWIN)));

		Utils::enableDlgControl(m_hwnd, IDC_TYPEFLASHWIN, IsDlgButtonChecked(m_hwnd, IDC_SHOWNOTIFY) != 0);
		Utils::enableDlgControl(m_hwnd, IDC_MTN_POPUPMODE, IsDlgButtonChecked(m_hwnd, IDC_NOTIFYPOPUP) != 0);

		SendDlgItemMessage(m_hwnd, IDC_MTN_POPUPMODE, CB_INSERTSTRING, -1, (LPARAM)TranslateT("Always"));
		SendDlgItemMessage(m_hwnd, IDC_MTN_POPUPMODE, CB_INSERTSTRING, -1, (LPARAM)TranslateT("Always, but no popup when window is focused"));
		SendDlgItemMessage(m_hwnd, IDC_MTN_POPUPMODE, CB_INSERTSTRING, -1, (LPARAM)TranslateT("Only when no message window is open"));

		SendDlgItemMessage(m_hwnd, IDC_MTN_POPUPMODE, CB_SETCURSEL, (WPARAM)M.GetByte("MTN_PopupMode", 0), 0);
		return true;
	}

	bool OnApply() override
	{
		SaveList();
		g_plugin.setByte(SRMSGSET_SHOWTYPING, (BYTE)IsDlgButtonChecked(m_hwnd, IDC_SHOWNOTIFY));
		g_plugin.setByte(SRMSGSET_SHOWTYPINGWINFLASH, (BYTE)IsDlgButtonChecked(m_hwnd, IDC_TYPEFLASHWIN));
		g_plugin.setByte(SRMSGSET_SHOWTYPINGNOWINOPEN, (BYTE)IsDlgButtonChecked(m_hwnd, IDC_TYPENOWIN));
		g_plugin.setByte(SRMSGSET_SHOWTYPINGWINOPEN, (BYTE)IsDlgButtonChecked(m_hwnd, IDC_TYPEWIN));
		g_plugin.setByte(SRMSGSET_SHOWTYPINGCLIST, (BYTE)IsDlgButtonChecked(m_hwnd, IDC_NOTIFYTRAY));
		g_plugin.setByte("ShowTypingBalloon", (BYTE)IsDlgButtonChecked(m_hwnd, IDC_NOTIFYBALLOON));
		g_plugin.setByte("ShowTypingPopup", (BYTE)IsDlgButtonChecked(m_hwnd, IDC_NOTIFYPOPUP));
		db_set_b(0, SRMSGMOD_T, "MTN_PopupMode", (BYTE)SendDlgItemMessage(m_hwnd, IDC_MTN_POPUPMODE, CB_GETCURSEL, 0, 0));
		PluginConfig.reloadSettings();
		return true;
	}

	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) override
	{
		if (msg == WM_NOTIFY && ((NMHDR*)lParam)->idFrom == IDC_CLIST) {
			switch (((NMHDR*)lParam)->code) {
			case CLN_OPTIONSCHANGED:
				ResetCList();
				break;
			case CLN_CHECKCHANGED:
				SendMessage(m_hwndParent, PSM_CHANGED, 0, 0);
				break;
			case CLN_LISTREBUILT:
				RebuildList();
				break;
			}
		}

		return CDlgBase::DlgProc(msg, wParam, lParam);
	}

	void onCheck_NotifyPopup(CCtrlCheck*)
	{
		Utils::enableDlgControl(m_hwnd, IDC_MTN_POPUPMODE, IsDlgButtonChecked(m_hwnd, IDC_NOTIFYPOPUP) != 0);
	}

	void onCheck_NotifyTray(CCtrlCheck*)
	{
		Utils::enableDlgControl(m_hwnd, IDC_TYPEWIN, IsDlgButtonChecked(m_hwnd, IDC_NOTIFYTRAY) != 0);
		Utils::enableDlgControl(m_hwnd, IDC_TYPENOWIN, IsDlgButtonChecked(m_hwnd, IDC_NOTIFYTRAY) != 0);
		Utils::enableDlgControl(m_hwnd, IDC_NOTIFYBALLOON, IsDlgButtonChecked(m_hwnd, IDC_NOTIFYTRAY) != 0);
	}

	void onCheck_ShowNotify(CCtrlCheck*)
	{
		Utils::enableDlgControl(m_hwnd, IDC_TYPEFLASHWIN, IsDlgButtonChecked(m_hwnd, IDC_SHOWNOTIFY) != 0);
	}

	void onCheck_Win(CCtrlCheck*)
	{
		Utils::enableDlgControl(m_hwnd, IDC_NOTIFYBALLOON, IsDlgButtonChecked(m_hwnd, IDC_NOTIFYTRAY) &&
			(IsDlgButtonChecked(m_hwnd, IDC_TYPEWIN) || IsDlgButtonChecked(m_hwnd, IDC_TYPENOWIN)));
	}
};

/////////////////////////////////////////////////////////////////////////////////////////
// options for tabbed messaging got their own page.. finally :)

class COptTabbedDlg : public CDlgBase
{
	CCtrlEdit edtLimit;
	CCtrlSpin spnLimit;
	CCtrlCombo cmbEscMode;
	CCtrlCheck chkLimit;
	CCtrlButton btnSetup;

public:
	COptTabbedDlg() :
		CDlgBase(g_plugin, IDD_OPT_TABBEDMSG),
		chkLimit(this, IDC_CUT_TABTITLE),
		edtLimit(this, IDC_CUT_TITLEMAX),
		spnLimit(this, IDC_CUT_TITLEMAXSPIN, 20, 5),
		btnSetup(this, IDC_SETUPAUTOCREATEMODES),
		cmbEscMode(this, IDC_ESCMODE)
	{
		btnSetup.OnClick = Callback(this, &COptTabbedDlg::onClick_Setup);

		chkLimit.OnChange = Callback(this, &COptTabbedDlg::onChange_Cut);
	}

	bool OnInitDialog() override
	{
		TreeViewInit(GetDlgItem(m_hwnd, IDC_TABMSGOPTIONS), CTranslator::TREE_TAB, 0, FALSE);

		chkLimit.SetState(M.GetByte("cuttitle", 0));
		spnLimit.SetPosition(db_get_w(0, SRMSGMOD_T, "cut_at", 15));
		onChange_Cut(&chkLimit);

		cmbEscMode.AddString(TranslateT("Normal - close tab, if last tab is closed also close the window"));
		cmbEscMode.AddString(TranslateT("Minimize the window to the task bar"));
		cmbEscMode.AddString(TranslateT("Close or hide window, depends on the close button setting above"));
		cmbEscMode.AddString(TranslateT("Do nothing (ignore Esc key)"));
		cmbEscMode.SetCurSel(PluginConfig.m_EscapeCloses);
		return true;
	}

	bool OnApply() override
	{
		db_set_w(0, SRMSGMOD_T, "cut_at", spnLimit.GetPosition());
		db_set_b(0, SRMSGMOD_T, "cuttitle", chkLimit.GetState());
		db_set_b(0, SRMSGMOD_T, "escmode", cmbEscMode.GetCurSel());

		TreeViewToDB(GetDlgItem(m_hwnd, IDC_TABMSGOPTIONS), CTranslator::TREE_TAB, SRMSGMOD_T, nullptr);

		PluginConfig.reloadSettings();
		Srmm_Broadcast(DM_OPTIONSAPPLIED, 0, 0);
		return true;
	}

	void OnDestroy() override
	{
		TreeViewDestroy(GetDlgItem(m_hwnd, IDC_TABMSGOPTIONS));
	}

	void onClick_Setup(CCtrlButton*)
	{
		CreateDialogParam(g_plugin.getInst(), MAKEINTRESOURCE(IDD_CHOOSESTATUSMODES), m_hwnd, DlgProcSetupStatusModes, M.GetDword("autopopupmask", -1));
	}

	void onChange_Cut(CCtrlCheck*)
	{
		bool bEnabled = chkLimit.GetState() != 0;
		edtLimit.Enable(bEnabled);
		spnLimit.Enable(bEnabled);
	}

	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) override
	{
		if (msg == WM_COMMAND && wParam == DM_STATUSMASKSET)
			db_set_dw(0, SRMSGMOD_T, "autopopupmask", (DWORD)lParam);

		if (msg == WM_NOTIFY && ((LPNMHDR)lParam)->idFrom == IDC_TABMSGOPTIONS)
			return TreeViewHandleClick(m_hwnd, ((LPNMHDR)lParam)->hwndFrom, wParam, lParam);

		return CDlgBase::DlgProc(msg, wParam, lParam);
	}
};

/////////////////////////////////////////////////////////////////////////////////////////
// container options

class COptContainersDlg : public CDlgBase
{
	CCtrlSpin spnNumFlash, spnTabLimit, spnFlashDelay;
	CCtrlCombo cmbAeroEffect;
	CCtrlCheck chkUseAero, chkUseAeroPeek, chkLimits, chkSingle, chkGroup, chkDefault;
	CCtrlHyperlink urlHelp;

	void onChangeAero(CCtrlCheck*)
	{
		Utils::enableDlgControl(m_hwnd, IDC_AEROEFFECT, chkUseAero.GetState() != 0);
	}

	void onChangeLimits(CCtrlCheck*)
	{
		Utils::enableDlgControl(m_hwnd, IDC_TABLIMIT, chkLimits.GetState() != 0);
	}

public:
	COptContainersDlg()
		: CDlgBase(g_plugin, IDD_OPT_CONTAINERS),
		urlHelp(this, IDC_HELP_CONTAINERS, "https://wiki.miranda-ng.org/index.php?title=Plugin:TabSRMM/en/Containers"),
		spnNumFlash(this, IDC_NRFLASHSPIN, 255),
		spnTabLimit(this, IDC_TABLIMITSPIN, 1000, 1),
		spnFlashDelay(this, IDC_FLASHINTERVALSPIN, 10000, 500),
		chkUseAero(this, IDC_USEAERO),
		chkUseAeroPeek(this, IDC_USEAEROPEEK),
		cmbAeroEffect(this, IDC_AEROEFFECT),
		chkLimits(this, IDC_LIMITTABS),
		chkSingle(this, IDC_SINGLEWINDOWMODE),
		chkGroup(this, IDC_CONTAINERGROUPMODE),
		chkDefault(this, IDC_DEFAULTCONTAINERMODE)
	{
		chkUseAero.OnChange = Callback(this, &COptContainersDlg::onChangeAero);
		chkLimits.OnChange = chkSingle.OnChange = chkGroup.OnChange = chkDefault.OnChange = Callback(this, &COptContainersDlg::onChangeLimits);
	}

	bool OnInitDialog() override
	{
		chkGroup.SetState(M.GetByte("useclistgroups", 0));
		chkLimits.SetState(M.GetByte("limittabs", 0));

		spnTabLimit.SetPosition(M.GetDword("maxtabs", 1));
		onChangeLimits(nullptr);

		chkSingle.SetState(M.GetByte("singlewinmode", 0));
		chkDefault.SetState(!(chkGroup.GetState() || chkLimits.GetState() || chkSingle.GetState()));

		spnNumFlash.SetPosition(M.GetByte("nrflash", 4));
		spnFlashDelay.SetPosition(M.GetDword("flashinterval", 1000));

		chkUseAero.SetState(M.GetByte("useAero", 1));
		chkUseAeroPeek.SetState(M.GetByte("useAeroPeek", 1));

		for (int i = 0; i < CSkin::AERO_EFFECT_LAST; i++)
			cmbAeroEffect.InsertString(TranslateW(CSkin::m_aeroEffects[i].tszName), -1);
		cmbAeroEffect.SetCurSel(CSkin::m_aeroEffect);
		cmbAeroEffect.Enable(IsWinVerVistaPlus());

		chkUseAero.Enable(IsWinVerVistaPlus());
		chkUseAeroPeek.Enable(IsWinVer7Plus());
		if (IsWinVerVistaPlus())
			Utils::enableDlgControl(m_hwnd, IDC_AEROEFFECT, chkUseAero.GetState() != 0);
		return true;
	}

	bool OnApply() override
	{
		bool fOldAeroState = M.getAeroState();

		db_set_b(0, SRMSGMOD_T, "useclistgroups", chkGroup.GetState());
		db_set_b(0, SRMSGMOD_T, "limittabs", chkLimits.GetState());
		db_set_dw(0, SRMSGMOD_T, "maxtabs", spnTabLimit.GetPosition());
		db_set_b(0, SRMSGMOD_T, "singlewinmode", chkSingle.GetState());
		db_set_dw(0, SRMSGMOD_T, "flashinterval", spnFlashDelay.GetPosition());
		db_set_b(0, SRMSGMOD_T, "nrflash", spnNumFlash.GetPosition());
		db_set_b(0, SRMSGMOD_T, "useAero", chkUseAero.GetState());
		db_set_b(0, SRMSGMOD_T, "useAeroPeek", chkUseAeroPeek.GetState());

		CSkin::setAeroEffect(cmbAeroEffect.GetCurSel());
		if (M.getAeroState() != fOldAeroState) {
			SendMessage(PluginConfig.g_hwndHotkeyHandler, WM_DWMCOMPOSITIONCHANGED, 0, 0);	// simulate aero state change
			SendMessage(PluginConfig.g_hwndHotkeyHandler, WM_DWMCOLORIZATIONCOLORCHANGED, 0, 0);	// simulate aero state change
		}
		BuildContainerMenu();
		return true;
	}
};

/////////////////////////////////////////////////////////////////////////////////////////
// TabModPlus options

class COptAdvancedDlg : public CDlgBase
{
	CCtrlSpin spnTimeout, spnHistSize;
	CCtrlButton btnRevert;
	CCtrlHyperlink urlHelp;

public:
	COptAdvancedDlg() :
		CDlgBase(g_plugin, IDD_OPTIONS_PLUS),
		urlHelp(this, IDC_PLUS_HELP, "https://wiki.miranda-ng.org/index.php?title=Plugin:TabSRMM/en/Typing_notifications"),
		btnRevert(this, IDC_PLUS_REVERT),
		spnTimeout(this, IDC_TIMEOUTSPIN, 300, SRMSGSET_MSGTIMEOUT_MIN / 1000),
		spnHistSize(this, IDC_HISTORYSIZESPIN, 255, 15)
	{
		btnRevert.OnClick = Callback(this, &COptAdvancedDlg::onClick_Revert);
	}

	bool OnInitDialog() override
	{
		TreeViewInit(GetDlgItem(m_hwnd, IDC_PLUS_CHECKTREE), CTranslator::TREE_MODPLUS, 0, FALSE);

		spnTimeout.SetPosition(PluginConfig.m_MsgTimeout / 1000);
		spnHistSize.SetPosition(M.GetByte("historysize", 0));
		return true;
	}

	bool OnApply() override
	{
		TreeViewToDB(GetDlgItem(m_hwnd, IDC_PLUS_CHECKTREE), CTranslator::TREE_MODPLUS, SRMSGMOD_T, nullptr);

		int msgTimeout = 1000 * spnTimeout.GetPosition();
		PluginConfig.m_MsgTimeout = msgTimeout >= SRMSGSET_MSGTIMEOUT_MIN ? msgTimeout : SRMSGSET_MSGTIMEOUT_MIN;
		g_plugin.setDword(SRMSGSET_MSGTIMEOUT, PluginConfig.m_MsgTimeout);

		db_set_b(0, SRMSGMOD_T, "historysize", spnHistSize.GetPosition());
		PluginConfig.reloadAdv();
		return true;
	}

	void OnDestroy() override
	{
		TreeViewDestroy(GetDlgItem(m_hwnd, IDC_PLUS_CHECKTREE));
	}

	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) override
	{
		if (msg == WM_NOTIFY && ((LPNMHDR)lParam)->idFrom == IDC_PLUS_CHECKTREE)
			return TreeViewHandleClick(m_hwnd, ((LPNMHDR)lParam)->hwndFrom, wParam, lParam);

		return CDlgBase::DlgProc(msg, wParam, lParam);
	}

	void onClick_Revert(CCtrlButton*)
	{
		TOptionListItem *lvItems = CTranslator::getTree(CTranslator::TREE_MODPLUS);

		for (int i = 0; lvItems[i].szName != nullptr; i++)
			if (lvItems[i].uType == LOI_TYPE_SETTING)
				db_set_b(0, SRMSGMOD_T, (char *)lvItems[i].lParam, (BYTE)lvItems[i].id);
		TreeViewSetFromDB(GetDlgItem(m_hwnd, IDC_PLUS_CHECKTREE), CTranslator::TREE_MODPLUS, 0);
	}
};

/////////////////////////////////////////////////////////////////////////////////////////

#define DBFONTF_BOLD       1
#define DBFONTF_ITALIC     2
#define DBFONTF_UNDERLINE  4

#define FONTS_TO_CONFIG MSGDLGFONTCOUNT

#define SAMEASF_FACE   1
#define SAMEASF_SIZE   2
#define SAMEASF_STYLE  4
#define SAMEASF_COLOUR 8
#include <pshpack1.h>

struct
{
	BYTE sameAsFlags, sameAs;
	COLORREF colour;
	char size;
	BYTE style;
	BYTE charset;
	char szFace[LF_FACESIZE];
}
static fontSettings[MSGDLGFONTCOUNT + 1];

#include <poppack.h>

#define SRFONTSETTINGMODULE FONTMODULE

enum
{
	CBVT_NONE,
	CBVT_CHAR,
	CBVT_INT,
	CBVT_BYTE,
	CBVT_DWORD,
	CBVT_BOOL,
};

struct OptCheckBox
{
	UINT idc;

	DWORD defValue;		// should be full combined value for masked items!
	DWORD dwBit;

	BYTE dbType;
	char *dbModule;
	char *dbSetting;

	BYTE valueType;
	union
	{
		void *pValue;

		char *charValue;
		int *intValue;
		BYTE *byteValue;
		DWORD *dwordValue;
		BOOL *boolValue;
	};
};

DWORD OptCheckBox_LoadValue(struct OptCheckBox *cb)
{
	switch (cb->valueType) {
	case CBVT_NONE:
		switch (cb->dbType) {
		case DBVT_BYTE:
			return db_get_b(0, cb->dbModule, cb->dbSetting, cb->defValue);
		case DBVT_WORD:
			return db_get_w(0, cb->dbModule, cb->dbSetting, cb->defValue);
		case DBVT_DWORD:
			return db_get_dw(0, cb->dbModule, cb->dbSetting, cb->defValue);
		}
		break;

	case CBVT_CHAR:
		return *cb->charValue;
	case CBVT_INT:
		return *cb->intValue;
	case CBVT_BYTE:
		return *cb->byteValue;
	case CBVT_DWORD:
		return *cb->dwordValue;
	case CBVT_BOOL:
		return *cb->boolValue;
	}

	return cb->defValue;
}

void OptCheckBox_Load(HWND hwnd, OptCheckBox *cb)
{
	DWORD value = OptCheckBox_LoadValue(cb);
	if (cb->dwBit) value &= cb->dwBit;
	CheckDlgButton(hwnd, cb->idc, value ? BST_CHECKED : BST_UNCHECKED);
}

void OptCheckBox_Save(HWND hwnd, OptCheckBox *cb)
{
	DWORD value = IsDlgButtonChecked(hwnd, cb->idc) == BST_CHECKED;

	if (cb->dwBit) {
		DWORD curValue = OptCheckBox_LoadValue(cb);
		value = value ? (curValue | cb->dwBit) : (curValue & ~cb->dwBit);
	}

	switch (cb->dbType) {
	case DBVT_BYTE:
		db_set_b(0, cb->dbModule, cb->dbSetting, (BYTE)value);
		break;
	case DBVT_WORD:
		db_set_w(0, cb->dbModule, cb->dbSetting, (WORD)value);
		break;
	case DBVT_DWORD:
		db_set_dw(0, cb->dbModule, cb->dbSetting, (DWORD)value);
		break;
	}

	switch (cb->valueType) {
	case CBVT_CHAR:
		*cb->charValue = (char)value;
		break;
	case CBVT_INT:
		*cb->intValue = (int)value;
		break;
	case CBVT_BYTE:
		*cb->byteValue = (BYTE)value;
		break;
	case CBVT_DWORD:
		*cb->dwordValue = (DWORD)value;
		break;
	case CBVT_BOOL:
		*cb->boolValue = (BOOL)value;
		break;
	}
}

INT_PTR CALLBACK DlgProcSetupStatusModes(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	DWORD dwStatusMask = GetWindowLongPtr(hwndDlg, GWLP_USERDATA);
	static DWORD dwNewStatusMask = 0;

	switch (msg) {
	case WM_INITDIALOG:
		TranslateDialogDefault(hwndDlg);
		SetWindowLongPtr(hwndDlg, GWLP_USERDATA, lParam);
		dwStatusMask = lParam;

		SetWindowText(hwndDlg, TranslateT("Choose status modes"));
		{
			for (int i = ID_STATUS_ONLINE; i <= ID_STATUS_MAX; i++) {
				SetDlgItemText(hwndDlg, i, Clist_GetStatusModeDescription(i, 0));
				if (dwStatusMask != -1 && (dwStatusMask & (1 << (i - ID_STATUS_ONLINE))))
					CheckDlgButton(hwndDlg, i, BST_CHECKED);
				Utils::enableDlgControl(hwndDlg, i, dwStatusMask != -1);
			}
		}
		if (dwStatusMask == -1)
			CheckDlgButton(hwndDlg, IDC_ALWAYS, BST_CHECKED);
		ShowWindow(hwndDlg, SW_SHOWNORMAL);
		return TRUE;

	case DM_GETSTATUSMASK:
		if (IsDlgButtonChecked(hwndDlg, IDC_ALWAYS))
			dwNewStatusMask = -1;
		else {
			dwNewStatusMask = 0;
			for (int i = ID_STATUS_ONLINE; i <= ID_STATUS_MAX; i++)
				dwNewStatusMask |= (IsDlgButtonChecked(hwndDlg, i) ? (1 << (i - ID_STATUS_ONLINE)) : 0);
		}
		break;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK:
			SendMessage(hwndDlg, DM_GETSTATUSMASK, 0, 0);
			SendMessage(GetParent(hwndDlg), DM_STATUSMASKSET, 0, (LPARAM)dwNewStatusMask);
			__fallthrough;

		case IDCANCEL:
			DestroyWindow(hwndDlg);
			break;

		case IDC_ALWAYS:
			for (int i = ID_STATUS_ONLINE; i <= ID_STATUS_MAX; i++)
				Utils::enableDlgControl(hwndDlg, i, !IsDlgButtonChecked(hwndDlg, IDC_ALWAYS));
			break;
		}

	case WM_DESTROY:
		SetWindowLongPtr(hwndDlg, GWLP_USERDATA, 0);
		break;
	}
	return FALSE;
}

/////////////////////////////////////////////////////////////////////////////////////////

int OptInitialise(WPARAM wParam, LPARAM lParam)
{
	TN_OptionsInitialize(wParam, lParam);

	// message sessions' options
	OPTIONSDIALOGPAGE odp = {};
	odp.position = 910000000;
	odp.flags = ODPF_BOLDGROUPS;
	odp.szTitle.a = LPGEN("Message sessions");

	odp.szTab.a = LPGEN("General");
	odp.pDialog = new COptMainDlg();
	g_plugin.addOptions(wParam, &odp);

	odp.szTab.a = LPGEN("Tabs and layout");
	odp.pDialog = new COptTabbedDlg();
	g_plugin.addOptions(wParam, &odp);

	odp.szTab.a = LPGEN("Containers");
	odp.pDialog = new COptContainersDlg();
	g_plugin.addOptions(wParam, &odp);

	odp.szTab.a = LPGEN("Message log");
	odp.pDialog = new COptLogDlg();
	g_plugin.addOptions(wParam, &odp);

	odp.szTab.a = LPGEN("Advanced tweaks");
	odp.pDialog = new COptAdvancedDlg();
	g_plugin.addOptions(wParam, &odp);

	odp.szGroup.a = LPGEN("Message sessions");
	odp.szTitle.a = LPGEN("Typing notify");
	odp.pDialog = new COptTypingDlg();
	g_plugin.addOptions(wParam, &odp);

	// skin options
	odp.position = 910000000;
	odp.szGroup.a = LPGEN("Skins");
	odp.szTitle.a = LPGEN("Message window");

	odp.szTab.a = LPGEN("Load and apply");
	odp.pDialog = new CSkinOptsDlg();
	g_plugin.addOptions(wParam, &odp);

	odp.szTab.a = LPGEN("Window layout tweaks");
	odp.pDialog = new CTabConfigDlg();
	g_plugin.addOptions(wParam, &odp);

	// popup options
	Popup_Options(wParam);

	// group chats
	Chat_Options(wParam);
	return 0;
}
