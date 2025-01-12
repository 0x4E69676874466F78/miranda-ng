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
// implements the "Container" window which acts as a toplevel window
// for message sessions.

#include "stdafx.h"

#define CONTAINER_KEY "TAB_ContainersW"
#define CONTAINER_SUBKEY "containerW"
#define CONTAINER_PREFIX "CNTW_"

TContainerData *pFirstContainer = nullptr;        // the linked list of struct ContainerWindowData
TContainerData *pLastActiveContainer = nullptr;

static TContainerData* TSAPI AppendToContainerList(TContainerData*);
static TContainerData* TSAPI RemoveContainerFromList(TContainerData*);

static bool fForceOverlayIcons = false;

HWND TSAPI GetTabWindow(HWND hwndTab, int i)
{
	if (i < 0)
		return nullptr;

	TCITEM tci;
	tci.mask = TCIF_PARAM;
	return (TabCtrl_GetItem(hwndTab, i, &tci)) ? (HWND)tci.lParam: nullptr;
}

void TContainerData::InitRedraw()
{
	::KillTimer(m_hwnd, (UINT_PTR)this);
	::SetTimer(m_hwnd, (UINT_PTR)this, 100, nullptr);
}

void TContainerData::SetIcon(CTabBaseDlg *pDlg, HICON hIcon)
{
	HICON hIconMsg = PluginConfig.g_IconMsgEvent;
	HICON hIconBig = (pDlg && pDlg->m_cache) ? Skin_LoadProtoIcon(pDlg->m_cache->getProto(), pDlg->m_cache->getStatus(), true) : nullptr;

	if (Win7Taskbar->haveLargeIcons()) {
		if (hIcon == PluginConfig.g_buttonBarIcons[ICON_DEFAULT_TYPING] || hIcon == hIconMsg) {
			Win7Taskbar->setOverlayIcon(m_hwnd, (LPARAM)hIcon);
			if (GetForegroundWindow() != m_hwnd)
				SendMessage(m_hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
			if (hIcon == hIconMsg)
				m_hIconTaskbarOverlay = hIconMsg;
			return;
		}

		if (pDlg) {
			if (pDlg->m_hTaskbarIcon != nullptr) {
				DestroyIcon(pDlg->m_hTaskbarIcon);
				pDlg->m_hTaskbarIcon = nullptr;
			}

			if (pDlg->m_pContainer->m_dwFlags & CNT_AVATARSONTASKBAR)
				pDlg->m_hTaskbarIcon = pDlg->IconFromAvatar();

			if (pDlg->m_hTaskbarIcon) {
				SendMessage(m_hwnd, WM_SETICON, ICON_BIG, (LPARAM)pDlg->m_hTaskbarIcon);
				SendMessage(m_hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
				Win7Taskbar->setOverlayIcon(m_hwnd, (LPARAM)(pDlg->m_hTabIcon ? (LPARAM)pDlg->m_hTabIcon : (LPARAM)hIcon));
			}
			else {
				SendMessage(m_hwnd, WM_SETICON, ICON_BIG, hIconBig ? (LPARAM)hIconBig : (LPARAM)hIcon);
				SendMessage(m_hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
				
				if (pDlg->m_pContainer->m_hIconTaskbarOverlay)
					Win7Taskbar->setOverlayIcon(m_hwnd, (LPARAM)pDlg->m_pContainer->m_hIconTaskbarOverlay);
				else if (Win7Taskbar->haveAlwaysGroupingMode() && fForceOverlayIcons)
					Win7Taskbar->setOverlayIcon(m_hwnd, (LPARAM)hIcon);
				else
					Win7Taskbar->clearOverlayIcon(m_hwnd);
			}
			return;
		}
	}
	
	// default handling (no win7 taskbar)
	if (hIcon == PluginConfig.g_buttonBarIcons[ICON_DEFAULT_TYPING]) {              // always set typing icon, but don't save it...
		SendMessage(m_hwnd, WM_SETICON, ICON_BIG, (LPARAM)PluginConfig.g_IconTypingEventBig);
		SendMessage(m_hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
		return;
	}

	if (hIcon == hIconMsg)
		hIconBig = Skin_LoadIcon(SKINICON_EVENT_MESSAGE, true);

	if (m_hIcon == STICK_ICON_MSG && hIcon != hIconMsg && m_dwFlags & CNT_NEED_UPDATETITLE) {
		hIcon = hIconMsg;
		hIconBig = Skin_LoadIcon(SKINICON_EVENT_MESSAGE, true);
	}
	SendMessage(m_hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
	if (nullptr != hIconBig && reinterpret_cast<HICON>(CALLSERVICE_NOTFOUND) != hIconBig)
		SendMessage(m_hwnd, WM_SETICON, ICON_BIG, LPARAM(hIconBig));
	m_hIcon = (hIcon == hIconMsg) ? STICK_ICON_MSG : 0;
}

void TContainerData::UpdateTabs()
{
	HWND hwndTab = GetDlgItem(m_hwnd, IDC_MSGTABS);
	int nTabs = TabCtrl_GetItemCount(hwndTab);
	for (int i = 0; i < nTabs; i++) {
		HWND hDlg = GetTabWindow(hwndTab, i);
		if (!hDlg)
			continue;

		CTabBaseDlg *dat = (CTabBaseDlg*)GetWindowLongPtr(hDlg, GWLP_USERDATA);
		if (dat)
			dat->m_iTabID = i;
	}
}

void TContainerData::UpdateTitle(MCONTACT hContact, CTabBaseDlg *pDlg)
{
	// pDlg != 0 means sent by a chat window
	if (pDlg) {
		wchar_t szText[512];
		GetWindowText(pDlg->GetHwnd(), szText, _countof(szText));
		szText[_countof(szText) - 1] = 0;
		SetWindowText(m_hwnd, szText);
		SetIcon(pDlg, (pDlg->m_hTabIcon != pDlg->m_hTabStatusIcon) ? pDlg->m_hTabIcon : pDlg->m_hTabStatusIcon);
		return;
	}

	// no hContact given - obtain the hContact for the active tab
	if (hContact == 0) {
		if (m_hwndActive && IsWindow(m_hwndActive))
			pDlg = (CTabBaseDlg*)GetWindowLongPtr(m_hwndActive, GWLP_USERDATA);
	}
	else {
		HWND hwnd = Srmm_FindWindow(hContact);
		if (hwnd != nullptr)
			pDlg = (CTabBaseDlg*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
	}

	if (pDlg) {
		SetIcon(pDlg, pDlg->m_hXStatusIcon ? pDlg->m_hXStatusIcon : pDlg->m_hTabStatusIcon);
		CMStringW szTitle;
		if (pDlg->FormatTitleBar(m_pSettings->szTitleFormat, szTitle))
			SetWindowText(m_hwnd, szTitle);
	}
}

// Windows Vista+
// extend the glassy area to get aero look for the status bar, tab bar, info panel
// and outer margins.

void TSAPI SetAeroMargins(TContainerData *pContainer)
{
	if (!pContainer)
		return;

	if (!M.isAero() || CSkin::m_skinEnabled) {
		pContainer->m_pMenuBar->setAero(false);
		return;
	}

	CTabBaseDlg *dat = (CTabBaseDlg*)GetWindowLongPtr(pContainer->m_hwndActive, GWLP_USERDATA);
	if (!dat)
		return;

	RECT rcWnd;
	if (dat->m_pPanel.isActive())
		GetWindowRect(GetDlgItem(dat->GetHwnd(), IDC_SRMM_LOG), &rcWnd);
	else
		GetWindowRect(dat->GetHwnd(), &rcWnd);

	POINT	pt = { rcWnd.left, rcWnd.top };
	ScreenToClient(pContainer->m_hwnd, &pt);

	MARGINS m;
	m.cyTopHeight = pt.y;
	pContainer->m_pMenuBar->setAero(true);

	// bottom part
	GetWindowRect(dat->GetHwnd(), &rcWnd);
	pt.x = rcWnd.left;

	LONG sbar_left, sbar_right;
	if (!pContainer->m_pSideBar->isActive()) {
		pt.y = rcWnd.bottom + ((pContainer->m_iChilds > 1 || !(pContainer->m_dwFlags & CNT_HIDETABS)) ? pContainer->m_tBorder : 0);
		sbar_left = 0, sbar_right = 0;
	}
	else {
		pt.y = rcWnd.bottom;
		sbar_left = (pContainer->m_pSideBar->getFlags() & CSideBar::SIDEBARORIENTATION_LEFT ? pContainer->m_pSideBar->getWidth() : 0);
		sbar_right = (pContainer->m_pSideBar->getFlags() & CSideBar::SIDEBARORIENTATION_RIGHT ? pContainer->m_pSideBar->getWidth() : 0);
	}
	ScreenToClient(pContainer->m_hwnd, &pt);
	GetClientRect(pContainer->m_hwnd, &rcWnd);
	m.cyBottomHeight = (rcWnd.bottom - pt.y);

	if (m.cyBottomHeight < 0 || m.cyBottomHeight >= rcWnd.bottom)
		m.cyBottomHeight = 0;

	m.cxLeftWidth = pContainer->m_tBorder_outer_left;
	m.cxRightWidth = pContainer->m_tBorder_outer_right;
	m.cxLeftWidth += sbar_left;
	m.cxRightWidth += sbar_right;

	if (memcmp(&m, &pContainer->m_mOld, sizeof(MARGINS)) != 0) {
		pContainer->m_mOld = m;
		CMimAPI::m_pfnDwmExtendFrameIntoClientArea(pContainer->m_hwnd, &m);
	}
}

static LRESULT CALLBACK ContainerWndProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	TContainerData *pContainer = (TContainerData*)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);
	BOOL bSkinned = CSkin::m_skinEnabled ? TRUE : FALSE;

	switch (msg) {
	case WM_NCPAINT:
		if (pContainer && bSkinned) {
			if (CSkin::m_frameSkins) {
				HDC dcFrame = GetDCEx(hwndDlg, nullptr, DCX_WINDOW |/*DCX_INTERSECTRGN|*/0x10000); // GetWindowDC(hwndDlg);
				LONG clip_top, clip_left;
				RECT rcText;
				HDC dcMem = CreateCompatibleDC(pContainer->m_cachedDC ? pContainer->m_cachedDC : dcFrame);

				RECT rcWindow, rcClient;
				POINT pt, pt1;
				GetWindowRect(hwndDlg, &rcWindow);
				GetClientRect(hwndDlg, &rcClient);
				pt.y = 0;
				pt.x = 0;
				ClientToScreen(hwndDlg, &pt);
				pt1.x = rcClient.right;
				pt1.y = rcClient.bottom;
				ClientToScreen(hwndDlg, &pt1);
				clip_top = pt.y - rcWindow.top;
				clip_left = pt.x - rcWindow.left;

				rcWindow.right = rcWindow.right - rcWindow.left;
				rcWindow.bottom = rcWindow.bottom - rcWindow.top;
				rcWindow.left = rcWindow.top = 0;

				HBITMAP hbmMem = CreateCompatibleBitmap(dcFrame, rcWindow.right, rcWindow.bottom);
				HBITMAP hbmOld = (HBITMAP)SelectObject(dcMem, hbmMem);

				ExcludeClipRect(dcFrame, clip_left, clip_top, clip_left + (pt1.x - pt.x), clip_top + (pt1.y - pt.y));
				ExcludeClipRect(dcMem, clip_left, clip_top, clip_left + (pt1.x - pt.x), clip_top + (pt1.y - pt.y));

				CSkin::DrawItem(dcMem, &rcWindow, &SkinItems[pContainer->m_ncActive ? ID_EXTBKFRAME : ID_EXTBKFRAMEINACTIVE]);

				wchar_t szWindowText[512];
				GetWindowText(hwndDlg, szWindowText, _countof(szWindowText));
				szWindowText[511] = 0;

				HFONT hOldFont = (HFONT)SelectObject(dcMem, PluginConfig.hFontCaption);

				TEXTMETRIC tm;
				GetTextMetrics(dcMem, &tm);
				SetTextColor(dcMem, CInfoPanel::m_ipConfig.clrs[IPFONTCOUNT - 1]);
				SetBkMode(dcMem, TRANSPARENT);
				rcText.left = 20 + CSkin::m_SkinnedFrame_left + CSkin::m_bClipBorder + CSkin::m_titleBarLeftOff;//26;
				rcText.right = rcWindow.right - 3 * CSkin::m_titleBarButtonSize.cx - 11 - CSkin::m_titleBarRightOff;
				rcText.top = CSkin::m_captionOffset + CSkin::m_bClipBorder;
				rcText.bottom = rcText.top + tm.tmHeight;
				rcText.left += CSkin::m_captionPadding;
				DrawText(dcMem, szWindowText, -1, &rcText, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
				SelectObject(dcMem, hOldFont);

				// icon
				HICON hIcon = (HICON)SendMessage(hwndDlg, WM_GETICON, ICON_SMALL, 0);
				DrawIconEx(dcMem, 4 + CSkin::m_SkinnedFrame_left + CSkin::m_bClipBorder + CSkin::m_titleBarLeftOff, rcText.top + (rcText.bottom - rcText.top) / 2 - 8, hIcon, 16, 16, 0, nullptr, DI_NORMAL);

				// title buttons
				pContainer->m_rcClose.top = pContainer->m_rcMin.top = pContainer->m_rcMax.top = CSkin::m_titleButtonTopOff;
				pContainer->m_rcClose.bottom = pContainer->m_rcMin.bottom = pContainer->m_rcMax.bottom = CSkin::m_titleButtonTopOff + CSkin::m_titleBarButtonSize.cy;

				pContainer->m_rcClose.right = rcWindow.right - 10 - CSkin::m_titleBarRightOff;
				pContainer->m_rcClose.left = pContainer->m_rcClose.right - CSkin::m_titleBarButtonSize.cx;

				pContainer->m_rcMax.right = pContainer->m_rcClose.left - 2;
				pContainer->m_rcMax.left = pContainer->m_rcMax.right - CSkin::m_titleBarButtonSize.cx;

				pContainer->m_rcMin.right = pContainer->m_rcMax.left - 2;
				pContainer->m_rcMin.left = pContainer->m_rcMin.right - CSkin::m_titleBarButtonSize.cx;

				CSkinItem *item_normal = &SkinItems[ID_EXTBKTITLEBUTTON];
				CSkinItem *item_hot = &SkinItems[ID_EXTBKTITLEBUTTONMOUSEOVER];
				CSkinItem *item_pressed = &SkinItems[ID_EXTBKTITLEBUTTONPRESSED];

				for (int i = 0; i < 3; i++) {
					RECT *pRect = nullptr;

					switch (i) {
					case 0:
						pRect = &pContainer->m_rcMin;
						hIcon = CSkin::m_minIcon;
						break;
					case 1:
						pRect = &pContainer->m_rcMax;
						hIcon = CSkin::m_maxIcon;
						break;
					case 2:
						pRect = &pContainer->m_rcClose;
						hIcon = CSkin::m_closeIcon;
						break;
					}
					if (pRect) {
						CSkinItem *item = pContainer->m_buttons[i].isPressed ? item_pressed : (pContainer->m_buttons[i].isHot ? item_hot : item_normal);
						CSkin::DrawItem(dcMem, pRect, item);
						DrawIconEx(dcMem, pRect->left + ((pRect->right - pRect->left) / 2 - 8), pRect->top + ((pRect->bottom - pRect->top) / 2 - 8), hIcon, 16, 16, 0, nullptr, DI_NORMAL);
					}
				}
				SetBkMode(dcMem, TRANSPARENT);
				BitBlt(dcFrame, 0, 0, rcWindow.right, rcWindow.bottom, dcMem, 0, 0, SRCCOPY);
				SelectObject(dcMem, hbmOld);
				DeleteObject(hbmMem);
				DeleteDC(dcMem);
				ReleaseDC(hwndDlg, dcFrame);
			}
			else mir_callNextSubclass(hwndDlg, ContainerWndProc, msg, wParam, lParam);

			PAINTSTRUCT ps;
			HDC hdcReal = BeginPaint(hwndDlg, &ps);

			RECT rcClient;
			GetClientRect(hwndDlg, &rcClient);
			int width = rcClient.right - rcClient.left;
			int height = rcClient.bottom - rcClient.top;
			if (width != pContainer->m_oldDCSize.cx || height != pContainer->m_oldDCSize.cy) {
				CSkinItem *sbaritem = &SkinItems[ID_EXTBKSTATUSBAR];
				BOOL statusBarSkinnd = !(pContainer->m_dwFlags & CNT_NOSTATUSBAR) && !sbaritem->IGNORED;
				LONG sbarDelta = statusBarSkinnd ? pContainer->m_statusBarHeight : 0;

				pContainer->m_oldDCSize.cx = width;
				pContainer->m_oldDCSize.cy = height;

				if (pContainer->m_cachedDC) {
					SelectObject(pContainer->m_cachedDC, pContainer->m_oldHBM);
					DeleteObject(pContainer->m_cachedHBM);
					DeleteDC(pContainer->m_cachedDC);
				}
				pContainer->m_cachedDC = CreateCompatibleDC(hdcReal);
				pContainer->m_cachedHBM = CreateCompatibleBitmap(hdcReal, width, height);
				pContainer->m_oldHBM = (HBITMAP)SelectObject(pContainer->m_cachedDC, pContainer->m_cachedHBM);

				HDC hdc = pContainer->m_cachedDC;
				if (!CSkin::DrawItem(hdc, &rcClient, &SkinItems[0]))
					FillRect(hdc, &rcClient, GetSysColorBrush(COLOR_3DFACE));

				if (sbarDelta) {
					rcClient.top = rcClient.bottom - sbarDelta;
					CSkin::DrawItem(hdc, &rcClient, sbaritem);
				}
			}
			BitBlt(hdcReal, 0, 0, width, height, pContainer->m_cachedDC, 0, 0, SRCCOPY);
			EndPaint(hwndDlg, &ps);
			return 0;
		}
		break;

	case WM_NCLBUTTONDOWN:
	case WM_NCLBUTTONUP:
	case WM_NCMOUSEHOVER:
	case WM_NCMOUSEMOVE:
		if (pContainer && CSkin::m_frameSkins) {
			POINT pt;
			GetCursorPos(&pt);

			RECT rcWindow;
			GetWindowRect(hwndDlg, &rcWindow);

			memcpy(&pContainer->m_oldbuttons[0], &pContainer->m_buttons[0], sizeof(TitleBtn) * 3);
			memset(&pContainer->m_buttons[0], 0, (sizeof(TitleBtn) * 3));

			if (pt.x >= (rcWindow.left + pContainer->m_rcMin.left) && pt.x <= (rcWindow.left + pContainer->m_rcClose.right) && pt.y < rcWindow.top + 24 && wParam != HTTOPRIGHT) {
				LRESULT result = 0; //DefWindowProc(hwndDlg, msg, wParam, lParam);
				HDC hdc = GetWindowDC(hwndDlg);
				LONG left = rcWindow.left;

				pt.y = 10;
				bool isMin = pt.x >= left + pContainer->m_rcMin.left && pt.x <= left + pContainer->m_rcMin.right;
				bool isMax = pt.x >= left + pContainer->m_rcMax.left && pt.x <= left + pContainer->m_rcMax.right;
				bool isClose = pt.x >= left + pContainer->m_rcClose.left && pt.x <= left + pContainer->m_rcClose.right;

				if (msg == WM_NCMOUSEMOVE) {
					if (isMax)
						pContainer->m_buttons[BTN_MAX].isHot = TRUE;
					else if (isMin)
						pContainer->m_buttons[BTN_MIN].isHot = TRUE;
					else if (isClose)
						pContainer->m_buttons[BTN_CLOSE].isHot = TRUE;
				}
				else if (msg == WM_NCLBUTTONDOWN) {
					if (isMax)
						pContainer->m_buttons[BTN_MAX].isPressed = TRUE;
					else if (isMin)
						pContainer->m_buttons[BTN_MIN].isPressed = TRUE;
					else if (isClose)
						pContainer->m_buttons[BTN_CLOSE].isPressed = TRUE;
				}
				else if (msg == WM_NCLBUTTONUP) {
					if (isMin)
						SendMessage(hwndDlg, WM_SYSCOMMAND, SC_MINIMIZE, 0);
					else if (isMax) {
						if (IsZoomed(hwndDlg))
							PostMessage(hwndDlg, WM_SYSCOMMAND, SC_RESTORE, 0);
						else
							PostMessage(hwndDlg, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
					}
					else if (isClose)
						PostMessage(hwndDlg, WM_SYSCOMMAND, SC_CLOSE, 0);
				}
				for (int i = 0; i < 3; i++) {
					if (pContainer->m_buttons[i].isHot != pContainer->m_oldbuttons[i].isHot) {
						RECT *rc;
						HICON hIcon;

						switch (i) {
						case 0:
							rc = &pContainer->m_rcMin;
							hIcon = CSkin::m_minIcon;
							break;
						case 1:
							rc = &pContainer->m_rcMax;
							hIcon = CSkin::m_maxIcon;
							break;
						case 2:
							rc = &pContainer->m_rcClose;
							hIcon = CSkin::m_closeIcon;
							break;
						default:
							continue; // shall never happen
						}
						if (rc) {
							CSkinItem *item = &SkinItems[pContainer->m_buttons[i].isPressed ? ID_EXTBKTITLEBUTTONPRESSED : (pContainer->m_buttons[i].isHot ? ID_EXTBKTITLEBUTTONMOUSEOVER : ID_EXTBKTITLEBUTTON)];
							CSkin::DrawItem(hdc, rc, item);
							DrawIconEx(hdc, rc->left + ((rc->right - rc->left) / 2 - 8), rc->top + ((rc->bottom - rc->top) / 2 - 8), hIcon, 16, 16, 0, nullptr, DI_NORMAL);
						}
					}
				}
				ReleaseDC(hwndDlg, hdc);
				return result;
			}
			else {
				LRESULT result = DefWindowProc(hwndDlg, msg, wParam, lParam);
				RedrawWindow(hwndDlg, nullptr, nullptr, RDW_INVALIDATE | RDW_FRAME | RDW_UPDATENOW | RDW_NOCHILDREN);
				return result;
			}
		}
		break;

	case WM_SETCURSOR:
		if (CSkin::m_frameSkins && (HWND)wParam == hwndDlg) {
			DefWindowProc(hwndDlg, msg, wParam, lParam);
			RedrawWindow(hwndDlg, nullptr, nullptr, RDW_INVALIDATE | RDW_FRAME | RDW_UPDATENOW | RDW_NOCHILDREN);
			return 1;
		}
		break;

	case WM_NCCALCSIZE:
		if (!CSkin::m_frameSkins)
			break;

		if (wParam) {
			NCCALCSIZE_PARAMS *ncsp = (NCCALCSIZE_PARAMS *)lParam;
			DefWindowProc(hwndDlg, msg, wParam, lParam);

			RECT *rc = &ncsp->rgrc[0];
			rc->left += CSkin::m_realSkinnedFrame_left;
			rc->right -= CSkin::m_realSkinnedFrame_right;
			rc->bottom -= CSkin::m_realSkinnedFrame_bottom;
			rc->top += CSkin::m_realSkinnedFrame_caption;
			return TRUE;
		}

		return DefWindowProc(hwndDlg, msg, wParam, lParam);

	case WM_NCACTIVATE:
		if (pContainer) {
			pContainer->m_ncActive = wParam;
			if (bSkinned && CSkin::m_frameSkins) {
				SendMessage(hwndDlg, WM_NCPAINT, 0, 0);
				return 1;
			}
		}
		break;

	case WM_SETTEXT:
	case WM_SETICON:
		if (CSkin::m_frameSkins) {
			DefWindowProc(hwndDlg, msg, wParam, lParam);
			RedrawWindow(hwndDlg, nullptr, nullptr, RDW_FRAME | RDW_INVALIDATE | RDW_UPDATENOW | RDW_NOCHILDREN);
			return 0;
		}
		break;

	case WM_NCHITTEST:
		if (pContainer && (pContainer->m_dwFlags & CNT_NOTITLE)) {
			RECT r;
			GetWindowRect(hwndDlg, &r);

			POINT pt;
			GetCursorPos(&pt);
			int clip = CSkin::m_bClipBorder;
			if (pt.y <= r.bottom && pt.y >= r.bottom - clip - 6) {
				if (pt.x > r.left + clip + 10 && pt.x < r.right - clip - 10)
					return HTBOTTOM;
				if (pt.x < r.left + clip + 10)
					return HTBOTTOMLEFT;
				if (pt.x > r.right - clip - 10)
					return HTBOTTOMRIGHT;

			}
			else if (pt.y >= r.top && pt.y <= r.top + 6) {
				if (pt.x > r.left + clip + 10 && pt.x < r.right - clip - 10)
					return HTTOP;
				if (pt.x < r.left + clip + 10)
					return HTTOPLEFT;
				if (pt.x > r.right - clip - 10)
					return HTTOPRIGHT;
			}
			else if (pt.x >= r.left && pt.x <= r.left + clip + 6)
				return HTLEFT;
			else if (pt.x >= r.right - clip - 6 && pt.x <= r.right)
				return HTRIGHT;
		}
		break;

	case WM_TIMER:
		if (wParam == (WPARAM)pContainer && pContainer->m_hwndStatus) {
			SendMessage(pContainer->m_hwnd, WM_SIZE, 0, 0);
			SendMessage(pContainer->m_hwndStatus, SB_SETTEXT, (WPARAM)(SBT_OWNERDRAW) | 2, 0);
			InvalidateRect(pContainer->m_hwndStatus, nullptr, TRUE);
			KillTimer(hwndDlg, wParam);
		}
		break;

	case 0xae: // must be some undocumented message - seems it messes with the title bar...
		if (CSkin::m_frameSkins)
			return 0;
	}
	return mir_callNextSubclass(hwndDlg, ContainerWndProc, msg, wParam, lParam);
}

/////////////////////////////////////////////////////////////////////////////////////////
// container window procedure...

static BOOL fHaveTipper = FALSE;

static INT_PTR CALLBACK DlgProcContainer(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	RECT rc;
	POINT pt;
	MCONTACT hContact;
	CTabBaseDlg *dat;

	TContainerData *pContainer = (TContainerData*)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);
	BOOL bSkinned = CSkin::m_skinEnabled ? TRUE : FALSE;
	HWND hwndTab = GetDlgItem(hwndDlg, IDC_MSGTABS);

	switch (msg) {
	case WM_INITDIALOG:
		fHaveTipper = ServiceExists("mToolTip/ShowTip");
		fForceOverlayIcons = M.GetByte("forceTaskBarStatusOverlays", 0) ? true : false;

		pContainer = (TContainerData*)lParam;
		SetWindowLongPtr(hwndDlg, GWLP_USERDATA, (LONG_PTR)pContainer);
		mir_subclassWindow(hwndDlg, ContainerWndProc);

		pContainer->m_hwnd = hwndDlg;
		{
			DWORD dwCreateFlags = pContainer->m_dwFlags;
			pContainer->m_isCloned = (dwCreateFlags & CNT_CREATE_CLONED);
			pContainer->m_fPrivateThemeChanged = FALSE;

			SendMessage(hwndDlg, DM_OPTIONSAPPLIED, 0, 0);          // set options...
			pContainer->m_dwFlags |= dwCreateFlags;

			LoadOverrideTheme(pContainer);
			DWORD ws = GetWindowLongPtr(hwndTab, GWL_STYLE);
			if (pContainer->m_dwFlagsEx & TCF_FLAT)
				ws |= TCS_BUTTONS;

			pContainer->ClearMargins();

			if (pContainer->m_dwFlagsEx & TCF_SINGLEROWTABCONTROL) {
				ws &= ~TCS_MULTILINE;
				ws |= TCS_SINGLELINE;
				ws |= TCS_FIXEDWIDTH;
			}
			else {
				ws &= ~TCS_SINGLELINE;
				ws |= TCS_MULTILINE;
				if (ws & TCS_BUTTONS)
					ws |= TCS_FIXEDWIDTH;
			}
			SetWindowLongPtr(hwndTab, GWL_STYLE, ws);

			pContainer->m_buttonItems = g_ButtonSet.items;

			pContainer->m_dwFlags = ((pContainer->m_dwFlagsEx & (TCF_SBARLEFT | TCF_SBARRIGHT)) ?
				pContainer->m_dwFlags | CNT_SIDEBAR : pContainer->m_dwFlags & ~CNT_SIDEBAR);

			pContainer->m_pSideBar = new CSideBar(pContainer);
			pContainer->m_pMenuBar = new CMenuBar(hwndDlg, pContainer);

			SetClassLongPtr(hwndDlg, GCL_STYLE, GetClassLongPtr(hwndDlg, GCL_STYLE) & ~(CS_VREDRAW | CS_HREDRAW));
			SetClassLongPtr(hwndTab, GCL_STYLE, GetClassLongPtr(hwndTab, GCL_STYLE) & ~(CS_VREDRAW | CS_HREDRAW));

			SetClassLongPtr(hwndDlg, GCL_STYLE, GetClassLongPtr(hwndDlg, GCL_STYLE) & ~CS_DROPSHADOW);

			// additional system menu items...
			HMENU hSysmenu = GetSystemMenu(hwndDlg, FALSE);
			int iMenuItems = GetMenuItemCount(hSysmenu);

			InsertMenu(hSysmenu, iMenuItems++ - 2, MF_BYPOSITION | MF_SEPARATOR, 0, L"");
			InsertMenu(hSysmenu, iMenuItems++ - 2, MF_BYPOSITION | MF_STRING, IDM_STAYONTOP, TranslateT("Stay on top"));
			if (!CSkin::m_frameSkins)
				InsertMenu(hSysmenu, iMenuItems++ - 2, MF_BYPOSITION | MF_STRING, IDM_NOTITLE, TranslateT("Hide title bar"));
			InsertMenu(hSysmenu, iMenuItems++ - 2, MF_BYPOSITION | MF_SEPARATOR, 0, L"");
			InsertMenu(hSysmenu, iMenuItems++ - 2, MF_BYPOSITION | MF_STRING, IDM_MOREOPTIONS, TranslateT("Container options..."));
			SetWindowText(hwndDlg, TranslateT("Message session..."));
			SendMessage(hwndDlg, WM_SETICON, ICON_BIG, (LPARAM)PluginConfig.g_iconContainer);

			// make the tab control the controlling parent window for all message dialogs

			ws = GetWindowLongPtr(hwndTab, GWL_EXSTYLE);
			SetWindowLongPtr(hwndTab, GWL_EXSTYLE, ws | WS_EX_CONTROLPARENT);

			LONG x_pad = M.GetByte("x-pad", 3) + (pContainer->m_dwFlagsEx & TCF_CLOSEBUTTON ? 7 : 0);
			LONG y_pad = M.GetByte("y-pad", 3) + ((pContainer->m_dwFlags & CNT_TABSBOTTOM) ? 1 : 0);

			if (pContainer->m_dwFlagsEx & TCF_FLAT)
				y_pad++; //(pContainer->m_dwFlags & CNT_TABSBOTTOM ? 1 : 2);

			TabCtrl_SetPadding(hwndTab, x_pad, y_pad);

			TabCtrl_SetImageList(hwndTab, PluginConfig.g_hImageList);

			SendMessage(hwndDlg, DM_CONFIGURECONTAINER, 0, 10);

			// tab tooltips...
			if (!fHaveTipper || M.GetByte("d_tooltips", 0) == 0) {
				pContainer->m_hwndTip = CreateWindowEx(0, TOOLTIPS_CLASS, nullptr, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP, CW_USEDEFAULT, CW_USEDEFAULT,
					CW_USEDEFAULT, CW_USEDEFAULT, hwndDlg, nullptr, g_plugin.getInst(), (LPVOID)nullptr);

				if (pContainer->m_hwndTip) {
					SetWindowPos(pContainer->m_hwndTip, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
					TabCtrl_SetToolTips(hwndTab, pContainer->m_hwndTip);
				}
			}
			else pContainer->m_hwndTip = nullptr;

			if (pContainer->m_dwFlags & CNT_CREATE_MINIMIZED) {
				WINDOWPLACEMENT wp = { 0 };
				wp.length = sizeof(wp);

				SetWindowLongPtr(hwndDlg, GWL_STYLE, GetWindowLongPtr(hwndDlg, GWL_STYLE) & ~WS_VISIBLE);
				ShowWindow(hwndDlg, SW_SHOWMINNOACTIVE);
				SendMessage(hwndDlg, DM_RESTOREWINDOWPOS, 0, 0);
				//GetClientRect(hwndDlg, &pContainer->m_rcSaved);
				ShowWindow(hwndDlg, SW_SHOWMINNOACTIVE);
				GetWindowPlacement(hwndDlg, &wp);
				pContainer->m_rcSaved.left = pContainer->m_rcSaved.top = 0;
				pContainer->m_rcSaved.right = wp.rcNormalPosition.right - wp.rcNormalPosition.left;
				pContainer->m_rcSaved.bottom = wp.rcNormalPosition.bottom - wp.rcNormalPosition.top;
			}
			else {
				SendMessage(hwndDlg, DM_RESTOREWINDOWPOS, 0, 0);
				ShowWindow(hwndDlg, SW_SHOWNORMAL);
			}
		}

		// prevent ugly back background being visible while tabbed clients are created
		if (M.isAero()) {
			MARGINS m = { -1 };
			CMimAPI::m_pfnDwmExtendFrameIntoClientArea(hwndDlg, &m);
		}
		return TRUE;

	case DM_RESTOREWINDOWPOS:
		// retrieve the container window geometry information from the database.
		if (pContainer->m_isCloned && pContainer->m_hContactFrom != 0 && !(pContainer->m_dwFlags & CNT_GLOBALSIZE)) {
			if (Utils_RestoreWindowPosition(hwndDlg, pContainer->m_hContactFrom, SRMSGMOD_T, "split")) {
				if (Utils_RestoreWindowPositionNoMove(hwndDlg, pContainer->m_hContactFrom, SRMSGMOD_T, "split"))
					if (Utils_RestoreWindowPosition(hwndDlg, 0, SRMSGMOD_T, "split"))
						if (Utils_RestoreWindowPositionNoMove(hwndDlg, 0, SRMSGMOD_T, "split"))
							SetWindowPos(hwndDlg, nullptr, 50, 50, 450, 300, SWP_NOZORDER | SWP_NOACTIVATE);
			}
		}
		else {
			if (pContainer->m_dwFlags & CNT_GLOBALSIZE) {
				if (Utils_RestoreWindowPosition(hwndDlg, 0, SRMSGMOD_T, "split"))
					if (Utils_RestoreWindowPositionNoMove(hwndDlg, 0, SRMSGMOD_T, "split"))
						SetWindowPos(hwndDlg, nullptr, 50, 50, 450, 300, SWP_NOZORDER | SWP_NOACTIVATE);
			}
			else {
				char szCName[CONTAINER_NAMELEN + 20];
				mir_snprintf(szCName, "%s%d", CONTAINER_PREFIX, pContainer->m_iContainerIndex);
				if (Utils_RestoreWindowPosition(hwndDlg, 0, SRMSGMOD_T, szCName)) {
					if (Utils_RestoreWindowPositionNoMove(hwndDlg, 0, SRMSGMOD_T, szCName))
						if (Utils_RestoreWindowPosition(hwndDlg, 0, SRMSGMOD_T, "split"))
							if (Utils_RestoreWindowPositionNoMove(hwndDlg, 0, SRMSGMOD_T, "split"))
								SetWindowPos(hwndDlg, nullptr, 50, 50, 450, 300, SWP_NOZORDER | SWP_NOACTIVATE);
				}
			}
		}
		return 0;

	case WM_SIZE:
		if (IsIconic(hwndDlg))
			pContainer->m_dwFlags |= CNT_DEFERREDSIZEREQUEST;
		else {
			RECT rcClient, rcUnadjusted;

			GetClientRect(hwndDlg, &rcClient);
			pContainer->m_pMenuBar->getClientRect();

			if (pContainer->m_hwndStatus) {
				dat = (CTabBaseDlg*)GetWindowLongPtr(pContainer->m_hwndActive, GWLP_USERDATA);
				SendMessage(pContainer->m_hwndStatus, WM_USER + 101, 0, (LPARAM)dat);

				RECT rcs;
				GetWindowRect(pContainer->m_hwndStatus, &rcs);
				pContainer->m_statusBarHeight = (rcs.bottom - rcs.top) + 1;
				SendMessage(pContainer->m_hwndStatus, SB_SETTEXT, (WPARAM)(SBT_OWNERDRAW) | 2, 0);
			}
			else pContainer->m_statusBarHeight = 0;

			CopyRect(&pContainer->m_rcSaved, &rcClient);
			rcUnadjusted = rcClient;

			pContainer->m_pMenuBar->Resize(LOWORD(lParam));
			LONG rebarHeight = pContainer->m_pMenuBar->getHeight();
			pContainer->m_pMenuBar->Show((pContainer->m_dwFlags & CNT_NOMENUBAR) ? SW_HIDE : SW_SHOW);

			LONG sbarWidth = pContainer->m_pSideBar->getWidth();
			LONG sbarWidth_left = pContainer->m_pSideBar->getFlags() & CSideBar::SIDEBARORIENTATION_LEFT ? sbarWidth : 0;

			if (lParam) {
				DWORD	dwSWPFlags = SWP_NOACTIVATE | SWP_NOZORDER | SWP_DEFERERASE | SWP_NOCOPYBITS; // | SWP_NOSENDCHANGING  | SWP_ASYNCWINDOWPOS;
				SetWindowPos(hwndTab, nullptr, pContainer->m_tBorder_outer_left + sbarWidth_left, pContainer->m_tBorder_outer_top + rebarHeight,
					(rcClient.right - rcClient.left) - (pContainer->m_tBorder_outer_left + pContainer->m_tBorder_outer_right + sbarWidth),
					(rcClient.bottom - rcClient.top) - pContainer->m_statusBarHeight - (pContainer->m_tBorder_outer_top + pContainer->m_tBorder_outer_bottom) - rebarHeight, dwSWPFlags);
			}

			pContainer->m_pSideBar->resizeScrollWnd(sbarWidth_left ? pContainer->m_tBorder_outer_left : rcClient.right - pContainer->m_tBorder_outer_right - (sbarWidth - 2),
				pContainer->m_tBorder_outer_top + rebarHeight, 0,
				(rcClient.bottom - rcClient.top) - pContainer->m_statusBarHeight - (pContainer->m_tBorder_outer_top + pContainer->m_tBorder_outer_bottom) - rebarHeight);

			AdjustTabClientRect(pContainer, &rcClient);

			BOOL sizeChanged = (((rcClient.right - rcClient.left) != pContainer->m_preSIZE.cx) || ((rcClient.bottom - rcClient.top) != pContainer->m_preSIZE.cy));
			if (sizeChanged) {
				pContainer->m_preSIZE.cx = rcClient.right - rcClient.left;
				pContainer->m_preSIZE.cy = rcClient.bottom - rcClient.top;
			}

			// we care about all client sessions, but we really resize only the active tab (hwndActive)
			// we tell inactive tabs to resize theirselves later when they get activated (DM_CHECKSIZE
			// just queues a resize request)
			int nCount = TabCtrl_GetItemCount(hwndTab);

			for (int i = 0; i < nCount; i++) {
				HWND hDlg = GetTabWindow(hwndTab, i);
				if (hDlg == pContainer->m_hwndActive) {
					SetWindowPos(hDlg, nullptr, rcClient.left, rcClient.top, (rcClient.right - rcClient.left), (rcClient.bottom - rcClient.top),
						SWP_NOSENDCHANGING | SWP_NOACTIVATE/*|SWP_NOCOPYBITS*/);
					if (!pContainer->m_bSizingLoop && sizeChanged) {
						dat = (CTabBaseDlg*)GetWindowLongPtr(pContainer->m_hwndActive, GWLP_USERDATA);
						if (dat)
							dat->DM_ScrollToBottom(0, 1);
					}
				}
				else if (sizeChanged)
					SendMessage(hDlg, DM_CHECKSIZE, 0, 0);
			}
			pContainer->m_pSideBar->scrollIntoView();

			if (!M.isAero()) {					// aero mode uses buffered paint, no forced redraw needed
				RedrawWindow(hwndTab, nullptr, nullptr, RDW_INVALIDATE | (pContainer->m_bSizingLoop ? RDW_ERASE : 0));
				RedrawWindow(hwndDlg, nullptr, nullptr, (bSkinned ? RDW_FRAME : 0) | RDW_INVALIDATE | ((pContainer->m_bSizingLoop || wParam == SIZE_RESTORED) ? RDW_ERASE : 0));
			}

			if (pContainer->m_hwndStatus)
				InvalidateRect(pContainer->m_hwndStatus, nullptr, FALSE);

			if ((CSkin::m_bClipBorder != 0 || CSkin::m_bRoundedCorner) && CSkin::m_frameSkins) {
				HRGN rgn;
				int clip = CSkin::m_bClipBorder;

				RECT rcWindow;
				GetWindowRect(hwndDlg, &rcWindow);

				if (CSkin::m_bRoundedCorner)
					rgn = CreateRoundRectRgn(clip, clip, (rcWindow.right - rcWindow.left) - clip + 1,
						(rcWindow.bottom - rcWindow.top) - clip + 1, CSkin::m_bRoundedCorner + clip, CSkin::m_bRoundedCorner + clip);
				else
					rgn = CreateRectRgn(clip, clip, (rcWindow.right - rcWindow.left) - clip, (rcWindow.bottom - rcWindow.top) - clip);
				SetWindowRgn(hwndDlg, rgn, TRUE);
			}
			else if (CSkin::m_frameSkins)
				SetWindowRgn(hwndDlg, nullptr, TRUE);
		}
		break;

	case WM_NOTIFY:
		if (pContainer == nullptr)
			break;
		if (pContainer->m_pMenuBar) {
			LRESULT processed = pContainer->m_pMenuBar->processMsg(msg, wParam, lParam);
			if (processed != -1) {
				SetWindowLongPtr(hwndDlg, DWLP_MSGRESULT, processed);
				return(processed);
			}
		}

		if (pContainer->m_hwndStatus != nullptr && ((LPNMHDR)lParam)->hwndFrom == pContainer->m_hwndStatus) {
			switch (((LPNMHDR)lParam)->code) {
			case NM_CLICK:
			case NM_RCLICK:
				NMMOUSE *nm = (NMMOUSE*)lParam;
				int nPanel;
				if (nm->dwItemSpec == 0xFFFFFFFE) {
					nPanel = 2;
					SendMessage(pContainer->m_hwndStatus, SB_GETRECT, nPanel, (LPARAM)&rc);
					if (nm->pt.x > rc.left && nm->pt.x < rc.right)
						goto panel_found;
					else
						return FALSE;
				}
				else nPanel = nm->dwItemSpec;
panel_found:
				if (nPanel == 2) {
					dat = (CTabBaseDlg*)GetWindowLongPtr(pContainer->m_hwndActive, GWLP_USERDATA);
					SendMessage(pContainer->m_hwndStatus, SB_GETRECT, nPanel, (LPARAM)&rc);
					if (dat)
						dat->CheckStatusIconClick(nm->pt, rc, 2, ((LPNMHDR)lParam)->code);
				}
				else if (((LPNMHDR)lParam)->code == NM_RCLICK) {
					GetCursorPos(&pt);
					hContact = 0;
					SendMessage(pContainer->m_hwndActive, DM_QUERYHCONTACT, 0, (LPARAM)&hContact);
					if (hContact) {
						int iSel = 0;
						HMENU hMenu = Menu_BuildContactMenu(hContact);
						iSel = TrackPopupMenu(hMenu, TPM_RETURNCMD, pt.x, pt.y, 0, hwndDlg, nullptr);
						if (iSel)
							Clist_MenuProcessCommand(LOWORD(iSel), MPCF_CONTACTMENU, hContact);
						DestroyMenu(hMenu);
					}
				}
				return TRUE;
			}
			break;
		}

		switch (((LPNMHDR)lParam)->code) {
		case TCN_SELCHANGE:
			if (HWND hDlg = GetTabWindow(hwndTab, TabCtrl_GetCurSel(hwndTab))) {
				if (hDlg != pContainer->m_hwndActive)
					if (pContainer->m_hwndActive && IsWindow(pContainer->m_hwndActive))
						ShowWindow(pContainer->m_hwndActive, SW_HIDE);

				pContainer->m_hwndActive = hDlg;
				SendMessage(hDlg, DM_SAVESIZE, 0, 1);
				ShowWindow(hDlg, SW_SHOW);
				if (!IsIconic(hwndDlg))
					SetFocus(pContainer->m_hwndActive);
			}
			SendMessage(hwndTab, EM_VALIDATEBOTTOM, 0, 0);
			return 0;

		// tooltips
		case NM_RCLICK:
			bool fFromSidebar = false;

			GetCursorPos(&pt);
			HMENU subMenu = GetSubMenu(PluginConfig.g_hMenuContext, 0);

			HWND hDlg = nullptr;
			dat = nullptr;
			if (((LPNMHDR)lParam)->idFrom == IDC_MSGTABS) {
				hDlg = GetTabWindow(hwndTab, GetTabItemFromMouse(hwndTab, &pt));
				if (hDlg && IsWindow(hDlg))
					dat = (CTabBaseDlg*)GetWindowLongPtr(hDlg, GWLP_USERDATA);
			}
			// sent from a sidebar button (RMB click) instead of the tab control
			else if (((LPNMHDR)lParam)->idFrom == 5000) {
				TSideBarNotify* n = reinterpret_cast<TSideBarNotify *>(lParam);
				dat = n->dat;
				fFromSidebar = true;
			}

			if (dat)
				dat->MsgWindowUpdateMenu(subMenu, MENU_TABCONTEXT);

			int iSelection = TrackPopupMenu(subMenu, TPM_RETURNCMD, pt.x, pt.y, 0, hwndDlg, nullptr);
			if (iSelection >= IDM_CONTAINERMENU) {
				char szIndex[10];
				itoa(iSelection - IDM_CONTAINERMENU, szIndex, 10);
				if (iSelection - IDM_CONTAINERMENU >= 0) {
					ptrW tszName(db_get_wsa(0, CONTAINER_KEY, szIndex));
					if (tszName != nullptr)
						SendMessage(hDlg, DM_CONTAINERSELECTED, 0, tszName);
				}
				return 1;
			}
			
			switch (iSelection) {
			case ID_TABMENU_CLOSETAB:
				if (fFromSidebar && dat)
					SendMessage(dat->GetHwnd(), WM_CLOSE, 1, 0);
				else
					SendMessage(hwndDlg, DM_CLOSETABATMOUSE, 0, (LPARAM)&pt);
				break;
			case ID_TABMENU_CLOSEOTHERTABS:
				if (dat)
					CloseOtherTabs(hwndTab, *dat);
				break;
			case ID_TABMENU_SAVETABPOSITION:
				if (dat)
					db_set_dw(dat->m_hContact, SRMSGMOD_T, "tabindex", dat->m_iTabID * 100);
				break;
			case ID_TABMENU_CLEARSAVEDTABPOSITION:
				if (dat)
					db_unset(dat->m_hContact, SRMSGMOD_T, "tabindex");
				break;
			case ID_TABMENU_LEAVECHATROOM:
				if (dat && dat->isChat() && dat->m_hContact) {
					char *szProto = GetContactProto(dat->m_hContact);
					if (szProto)
						CallProtoService(szProto, PS_LEAVECHAT, dat->m_hContact, 0);
				}
				break;
			case ID_TABMENU_ATTACHTOCONTAINER:
				hDlg = GetTabWindow(hwndTab, GetTabItemFromMouse(hwndTab, &pt));
				if (hDlg)
					CreateDialogParam(g_plugin.getInst(), MAKEINTRESOURCE(IDD_SELECTCONTAINER), hwndDlg, SelectContainerDlgProc, (LPARAM)hDlg);
				break;
			case ID_TABMENU_CONTAINEROPTIONS:
				if (pContainer->m_hWndOptions == nullptr)
					CreateDialogParam(g_plugin.getInst(), MAKEINTRESOURCE(IDD_CONTAINEROPTIONS), hwndDlg, DlgProcContainerOptions, (LPARAM)pContainer);
				break;
			case ID_TABMENU_CLOSECONTAINER:
				SendMessage(hwndDlg, WM_CLOSE, 0, 0);
				break;
			}
			InvalidateRect(hwndTab, nullptr, FALSE);
			return 1;
		}
		break;

	case WM_COMMAND:
		{
			bool fProcessContactMenu = pContainer->m_pMenuBar->isContactMenu();
			bool fProcessMainMenu = pContainer->m_pMenuBar->isMainMenu();
			pContainer->m_pMenuBar->Cancel();

			dat = (CTabBaseDlg*)GetWindowLongPtr(pContainer->m_hwndActive, GWLP_USERDATA);
			DWORD dwOldFlags = pContainer->m_dwFlags;

			if (dat) {
				if (fProcessContactMenu)
					return Clist_MenuProcessCommand(LOWORD(wParam), MPCF_CONTACTMENU, dat->m_hContact);
				if (fProcessMainMenu)
					return Clist_MenuProcessCommand(LOWORD(wParam), MPCF_MAINMENU, 0);
				if (dat->MsgWindowMenuHandler(LOWORD(wParam), MENU_PICMENU) == 1)
					break;
			}
			SendMessage(pContainer->m_hwndActive, DM_QUERYHCONTACT, 0, (LPARAM)&hContact);
			if (LOWORD(wParam) == IDC_TBFIRSTUID - 1)
				break;

			switch (LOWORD(wParam)) {
			case IDC_TOGGLESIDEBAR:
				GetWindowRect(hwndDlg, &rc);
				{
					LONG dwNewLeft;
					bool fVisible = pContainer->m_pSideBar->isVisible();
					if (fVisible) {
						dwNewLeft = pContainer->m_pSideBar->getWidth();
						pContainer->m_pSideBar->setVisible(false);
					}
					else {
						pContainer->m_pSideBar->setVisible(true);
						dwNewLeft = -(pContainer->m_pSideBar->getWidth());
					}

					pContainer->m_preSIZE.cx = pContainer->m_preSIZE.cy = 0;
					pContainer->m_oldDCSize.cx = pContainer->m_oldDCSize.cy = 0;
				}

				PostMessage(hwndDlg, WM_SIZE, 0, 1);
				break;

			case IDC_SIDEBARDOWN:
			case IDC_SIDEBARUP:
				{
					HWND hwnd = GetFocus();
					pContainer->m_pSideBar->processScrollerButtons(LOWORD(wParam));
					SetFocus(hwnd);
				}
				break;

			case IDC_CLOSE:
				SendMessage(hwndDlg, WM_SYSCOMMAND, SC_CLOSE, 0);
				break;
			
			case IDC_MINIMIZE:
				PostMessage(hwndDlg, WM_SYSCOMMAND, SC_MINIMIZE, 0);
				break;
			
			case IDC_MAXIMIZE:
				SendMessage(hwndDlg, WM_SYSCOMMAND, IsZoomed(hwndDlg) ? SC_RESTORE : SC_MAXIMIZE, 0);
				break;
			
			case IDOK:
				SendMessage(pContainer->m_hwndActive, WM_COMMAND, wParam, lParam);      // pass the IDOK command to the active child - fixes the "enter not working
				break;
			
			case ID_FILE_SAVEMESSAGELOGAS:
				if (dat)
					dat->DM_SaveLogAsRTF();
				break;
			
			case ID_FILE_CLOSEMESSAGESESSION:
				PostMessage(pContainer->m_hwndActive, WM_CLOSE, 0, 1);
				break;
			
			case ID_FILE_CLOSE:
				PostMessage(hwndDlg, WM_CLOSE, 0, 1);
				break;
			
			case ID_VIEW_SHOWSTATUSBAR:
				ApplyContainerSetting(pContainer, CNT_NOSTATUSBAR, pContainer->m_dwFlags & CNT_NOSTATUSBAR ? 0 : 1, true);
				break;
			
			case ID_VIEW_VERTICALMAXIMIZE:
				ApplyContainerSetting(pContainer, CNT_VERTICALMAX, pContainer->m_dwFlags & CNT_VERTICALMAX ? 0 : 1, false);
				break;
			
			case ID_VIEW_BOTTOMTOOLBAR:
				ApplyContainerSetting(pContainer, CNT_BOTTOMTOOLBAR, pContainer->m_dwFlags & CNT_BOTTOMTOOLBAR ? 0 : 1, false);
				Srmm_Broadcast(DM_CONFIGURETOOLBAR, 0, 1);
				return 0;
			
			case ID_VIEW_SHOWTOOLBAR:
				ApplyContainerSetting(pContainer, CNT_HIDETOOLBAR, pContainer->m_dwFlags & CNT_HIDETOOLBAR ? 0 : 1, false);
				Srmm_Broadcast(DM_CONFIGURETOOLBAR, 0, 1);
				return 0;
			
			case ID_VIEW_SHOWMENUBAR:
				ApplyContainerSetting(pContainer, CNT_NOMENUBAR, pContainer->m_dwFlags & CNT_NOMENUBAR ? 0 : 1, true);
				break;
			
			case ID_VIEW_SHOWTITLEBAR:
				ApplyContainerSetting(pContainer, CNT_NOTITLE, pContainer->m_dwFlags & CNT_NOTITLE ? 0 : 1, true);
				break;
			
			case ID_VIEW_TABSATBOTTOM:
				ApplyContainerSetting(pContainer, CNT_TABSBOTTOM, pContainer->m_dwFlags & CNT_TABSBOTTOM ? 0 : 1, false);
				break;
			
			case ID_VIEW_SHOWMULTISENDCONTACTLIST:
				SendMessage(pContainer->m_hwndActive, WM_COMMAND, MAKEWPARAM(IDC_SENDMENU, ID_SENDMENU_SENDTOMULTIPLEUSERS), 0);
				break;
			
			case ID_VIEW_STAYONTOP:
				SendMessage(hwndDlg, WM_SYSCOMMAND, IDM_STAYONTOP, 0);
				break;
			
			case ID_CONTAINER_CONTAINEROPTIONS:
				SendMessage(hwndDlg, WM_SYSCOMMAND, IDM_MOREOPTIONS, 0);
				break;
			
			case ID_EVENTPOPUPS_DISABLEALLEVENTPOPUPS:
				ApplyContainerSetting(pContainer, (CNT_DONTREPORT | CNT_DONTREPORTUNFOCUSED | CNT_DONTREPORTFOCUSED | CNT_ALWAYSREPORTINACTIVE), 0, false);
				return 0;
			
			case ID_EVENTPOPUPS_SHOWPOPUPSIFWINDOWISMINIMIZED:
				ApplyContainerSetting(pContainer, CNT_DONTREPORT, pContainer->m_dwFlags & CNT_DONTREPORT ? 0 : 1, false);
				return 0;
			
			case ID_EVENTPOPUPS_SHOWPOPUPSIFWINDOWISUNFOCUSED:
				ApplyContainerSetting(pContainer, CNT_DONTREPORTUNFOCUSED, pContainer->m_dwFlags & CNT_DONTREPORTUNFOCUSED ? 0 : 1, false);
				return 0;
			
			case ID_EVENTPOPUPS_SHOWPOPUPSIFWINDOWISFOCUSED:
				ApplyContainerSetting(pContainer, CNT_DONTREPORTFOCUSED, pContainer->m_dwFlags & CNT_DONTREPORTFOCUSED ? 0 : 1, false);
				return 0;
			
			case ID_EVENTPOPUPS_SHOWPOPUPSFORALLINACTIVESESSIONS:
				ApplyContainerSetting(pContainer, CNT_ALWAYSREPORTINACTIVE, pContainer->m_dwFlags & CNT_ALWAYSREPORTINACTIVE ? 0 : 1, false);
				return 0;
			
			case ID_WINDOWFLASHING_DISABLEFLASHING:
				ApplyContainerSetting(pContainer, CNT_NOFLASH, 1, false);
				ApplyContainerSetting(pContainer, CNT_FLASHALWAYS, 0, false);
				return 0;
			
			case ID_WINDOWFLASHING_FLASHUNTILFOCUSED:
				ApplyContainerSetting(pContainer, CNT_NOFLASH, 0, false);
				ApplyContainerSetting(pContainer, CNT_FLASHALWAYS, 1, false);
				return 0;
			
			case ID_WINDOWFLASHING_USEDEFAULTVALUES:
				ApplyContainerSetting(pContainer, (CNT_NOFLASH | CNT_FLASHALWAYS), 0, false);
				return 0;
			
			case ID_OPTIONS_SAVECURRENTWINDOWPOSITIONASDEFAULT:
				{
					WINDOWPLACEMENT wp = { 0 };
					wp.length = sizeof(wp);
					if (GetWindowPlacement(hwndDlg, &wp)) {
						db_set_dw(0, SRMSGMOD_T, "splitx", wp.rcNormalPosition.left);
						db_set_dw(0, SRMSGMOD_T, "splity", wp.rcNormalPosition.top);
						db_set_dw(0, SRMSGMOD_T, "splitwidth", wp.rcNormalPosition.right - wp.rcNormalPosition.left);
						db_set_dw(0, SRMSGMOD_T, "splitheight", wp.rcNormalPosition.bottom - wp.rcNormalPosition.top);
					}
				}
				return 0;

			case ID_VIEW_INFOPANEL:
				if (dat) {
					GetWindowRect(pContainer->m_hwndActive, &rc);
					pt.x = rc.left + 10;
					pt.y = rc.top + dat->m_pPanel.getHeight() - 10;
					dat->m_pPanel.invokeConfigDialog(pt);
				}
				return 0;

				// commands from the message log popup will be routed to the
				// message log menu handler
			case ID_MESSAGELOGSETTINGS_FORTHISCONTACT:
			case ID_MESSAGELOGSETTINGS_GLOBAL:
				if (dat) {
					dat->MsgWindowMenuHandler((int)LOWORD(wParam), MENU_LOGMENU);
					return 1;
				}
				break;
			}

			if (pContainer->m_dwFlags != dwOldFlags)
				SendMessage(hwndDlg, DM_CONFIGURECONTAINER, 0, 0);
		}
		break;

	case WM_ENTERSIZEMOVE:
		GetClientRect(hwndTab, &rc);
		{
			SIZE sz;
			sz.cx = rc.right - rc.left;
			sz.cy = rc.bottom - rc.top;
			pContainer->m_oldSize = sz;
			pContainer->m_bSizingLoop = TRUE;
		}
		break;

	case WM_EXITSIZEMOVE:
		GetClientRect(hwndTab, &rc);
		if (!((rc.right - rc.left) == pContainer->m_oldSize.cx && (rc.bottom - rc.top) == pContainer->m_oldSize.cy)) {
			dat = (CTabBaseDlg*)GetWindowLongPtr(pContainer->m_hwndActive, GWLP_USERDATA);
			if (dat)
				dat->DM_ScrollToBottom(0, 0);
			SendMessage(pContainer->m_hwndActive, WM_SIZE, 0, 0);
		}
		pContainer->m_bSizingLoop = FALSE;
		break;

		// determine minimum and maximum size limits
		// 1) for maximizing the window when the "vertical maximize" option is set
		// 2) to limit the minimum height when manually resizing the window
		// (this avoids overlapping of controls inside the window and ensures
		// that at least 2 lines of the message log are always visible).
	case WM_GETMINMAXINFO:
		RECT rcWindow;
		{
			RECT rcClient = { 0 };

			MINMAXINFO *mmi = (MINMAXINFO *)lParam;
			mmi->ptMinTrackSize.x = 275;
			mmi->ptMinTrackSize.y = 130;
			GetClientRect(hwndTab, &rc);
			if (pContainer->m_hwndActive)								// at container creation time, there is no hwndActive yet..
				GetClientRect(pContainer->m_hwndActive, &rcClient);
			GetWindowRect(hwndDlg, &rcWindow);
			pt.y = rc.top;
			TabCtrl_AdjustRect(hwndTab, FALSE, &rc);
			// uChildMinHeight holds the min height for the client window only
			// so let's add the container's vertical padding (title bar, tab bar,
			// window border, status bar) to this value
			if (pContainer->m_hwndActive)
				mmi->ptMinTrackSize.y = pContainer->m_uChildMinHeight + (pContainer->m_hwndActive ? ((rcWindow.bottom - rcWindow.top) - rcClient.bottom) : 0);

			if (pContainer->m_dwFlags & CNT_VERTICALMAX || (GetKeyState(VK_CONTROL) & 0x8000)) {
				RECT rcDesktop = { 0 };
				BOOL fDesktopValid = FALSE;
				int monitorXOffset = 0;
				WINDOWPLACEMENT wp = { 0 };

				HMONITOR hMonitor = MonitorFromWindow(hwndDlg, 2);
				if (hMonitor) {
					MONITORINFO mi = { 0 };
					mi.cbSize = sizeof(mi);
					GetMonitorInfoA(hMonitor, &mi);
					rcDesktop = mi.rcWork;
					OffsetRect(&rcDesktop, -mi.rcMonitor.left, -mi.rcMonitor.top);
					monitorXOffset = mi.rcMonitor.left;
					fDesktopValid = TRUE;
				}
				if (!fDesktopValid)
					SystemParametersInfo(SPI_GETWORKAREA, 0, &rcDesktop, 0);

				wp.length = sizeof(wp);
				GetWindowPlacement(hwndDlg, &wp);
				mmi->ptMaxSize.y = rcDesktop.bottom - rcDesktop.top;
				mmi->ptMaxSize.x = wp.rcNormalPosition.right - wp.rcNormalPosition.left;
				mmi->ptMaxPosition.x = wp.rcNormalPosition.left - monitorXOffset;
				mmi->ptMaxPosition.y = 0;
				if (IsIconic(hwndDlg)) {
					mmi->ptMaxPosition.x += rcDesktop.left;
					mmi->ptMaxPosition.y += rcDesktop.top;
				}

				// protect against invalid values...
				if (mmi->ptMinTrackSize.y < 50 || mmi->ptMinTrackSize.y > rcDesktop.bottom)
					mmi->ptMinTrackSize.y = 130;
			}
		}
		return 0;

	case WM_TIMER:
		if (wParam == TIMERID_HEARTBEAT) {
			if (GetForegroundWindow() != hwndDlg && (pContainer->m_pSettings->autoCloseSeconds > 0) && !pContainer->m_bHidden) {
				BOOL fResult = TRUE;
				BroadCastContainer(pContainer, DM_CHECKAUTOHIDE, (WPARAM)pContainer->m_pSettings->autoCloseSeconds, (LPARAM)&fResult);

				if (fResult && nullptr == pContainer->m_hWndOptions)
					PostMessage(hwndDlg, WM_CLOSE, 1, 0);
			}

			dat = (CTabBaseDlg*)GetWindowLongPtr(pContainer->m_hwndActive, GWLP_USERDATA);
			if (dat && !dat->isChat()) {
				if (dat->m_idle && pContainer->m_hwndActive && IsWindow(pContainer->m_hwndActive))
					dat->m_pPanel.Invalidate(TRUE);
			}
			else if (dat)
				dat->UpdateStatusBar();
		}
		break;

	case WM_SYSCOMMAND:
		switch (wParam) {
		case IDM_STAYONTOP:
			SetWindowPos(hwndDlg, (pContainer->m_dwFlags & CNT_STICKY) ? HWND_NOTOPMOST : HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
			CheckMenuItem(GetSystemMenu(hwndDlg, FALSE), IDM_STAYONTOP, (pContainer->m_dwFlags & CNT_STICKY) ? MF_BYCOMMAND | MF_UNCHECKED : MF_BYCOMMAND | MF_CHECKED);
			ApplyContainerSetting(pContainer, CNT_STICKY, pContainer->m_dwFlags & CNT_STICKY ? 0 : 1, false);
			break;
		case IDM_NOTITLE:
			pContainer->m_oldSize.cx = 0;
			pContainer->m_oldSize.cy = 0;

			CheckMenuItem(GetSystemMenu(hwndDlg, FALSE), IDM_NOTITLE, (pContainer->m_dwFlags & CNT_NOTITLE) ? MF_BYCOMMAND | MF_UNCHECKED : MF_BYCOMMAND | MF_CHECKED);
			ApplyContainerSetting(pContainer, CNT_NOTITLE, pContainer->m_dwFlags & CNT_NOTITLE ? 0 : 1, true);
			break;
		case IDM_MOREOPTIONS:
			if (IsIconic(pContainer->m_hwnd))
				SendMessage(pContainer->m_hwnd, WM_SYSCOMMAND, SC_RESTORE, 0);
			if (pContainer->m_hWndOptions == nullptr)
				CreateDialogParam(g_plugin.getInst(), MAKEINTRESOURCE(IDD_CONTAINEROPTIONS), hwndDlg, DlgProcContainerOptions, (LPARAM)pContainer);
			break;
		case SC_MAXIMIZE:
			pContainer->m_oldSize.cx = pContainer->m_oldSize.cy = 0;
			break;
		case SC_RESTORE:
			pContainer->m_oldSize.cx = pContainer->m_oldSize.cy = 0;
			pContainer->ClearMargins();
			break;
		case SC_MINIMIZE:
			dat = (CTabBaseDlg*)(GetWindowLongPtr(pContainer->m_hwndActive, GWLP_USERDATA));
			if (dat) {
				GetWindowRect(pContainer->m_hwndActive, &pContainer->m_rcLogSaved);
				pContainer->m_ptLogSaved.x = pContainer->m_rcLogSaved.left;
				pContainer->m_ptLogSaved.y = pContainer->m_rcLogSaved.top;
				ScreenToClient(hwndDlg, &pContainer->m_ptLogSaved);
			}
		}
		break;

	case DM_SELECTTAB:
		switch (wParam) {
		case DM_SELECT_BY_HWND:
			ActivateTabFromHWND(hwndTab, (HWND)lParam);
			break;

		case DM_SELECT_NEXT:
		case DM_SELECT_PREV:
		case DM_SELECT_BY_INDEX:
			int iItems = TabCtrl_GetItemCount(hwndTab);
			if (iItems == 1)
				break;

			int iCurrent = TabCtrl_GetCurSel(hwndTab), iNewTab;

			if (wParam == DM_SELECT_PREV)
				iNewTab = iCurrent ? iCurrent - 1 : iItems - 1;     // cycle if current is already the leftmost tab..
			else if (wParam == DM_SELECT_NEXT)
				iNewTab = (iCurrent == (iItems - 1)) ? 0 : iCurrent + 1;
			else {
				if ((int)lParam > iItems)
					break;
				iNewTab = lParam - 1;
			}

			if (iNewTab != iCurrent) {
				if (HWND hDlg = GetTabWindow(hwndTab, iNewTab)) {
					TabCtrl_SetCurSel(hwndTab, iNewTab);
					ShowWindow(pContainer->m_hwndActive, SW_HIDE);
					pContainer->m_hwndActive = hDlg;
					ShowWindow(hDlg, SW_SHOW);
					SetFocus(pContainer->m_hwndActive);
				}
			}
			break;
		}
		break;

	case WM_INITMENUPOPUP:
		pContainer->m_pMenuBar->setActive(reinterpret_cast<HMENU>(wParam));
		break;

	case WM_LBUTTONDOWN:
		if (pContainer->m_dwFlags & CNT_NOTITLE) {
			GetCursorPos(&pt);
			return SendMessage(hwndDlg, WM_SYSCOMMAND, SC_MOVE | HTCAPTION, MAKELPARAM(pt.x, pt.y));
		}
		break;

		// pass the WM_ACTIVATE msg to the active message dialog child
	case WM_NCACTIVATE:
		if (IsWindowVisible(hwndDlg))
			pContainer->m_bHidden = false;
		break;

	case WM_ACTIVATE:
		if (pContainer == nullptr)
			break;

		if (LOWORD(wParam == WA_INACTIVE))
			BroadCastContainer(pContainer, DM_CHECKINFOTIP, wParam, lParam);

		if (LOWORD(wParam == WA_INACTIVE) && (HWND)lParam != PluginConfig.g_hwndHotkeyHandler && GetParent((HWND)lParam) != hwndDlg) {
			BOOL fTransAllowed = !bSkinned || IsWinVerVistaPlus();

			if (pContainer->m_dwFlags & CNT_TRANSPARENCY && fTransAllowed) {
				SetLayeredWindowAttributes(hwndDlg, Skin->getColorKey(), (BYTE)HIWORD(pContainer->m_pSettings->dwTransparency), (pContainer->m_dwFlags & CNT_TRANSPARENCY ? LWA_ALPHA : 0));
			}
		}
		pContainer->m_hwndSaved = nullptr;

		if (LOWORD(wParam) != WA_ACTIVE) {
			pContainer->m_pMenuBar->Cancel();

			if (HWND hDlg = GetTabWindow(hwndTab, TabCtrl_GetCurSel(hwndTab)))
				SendMessage(hDlg, WM_ACTIVATE, WA_INACTIVE, 0);
			break;
		}

	case WM_MOUSEACTIVATE:
		if (pContainer != nullptr) {
			BOOL fTransAllowed = !bSkinned || IsWinVerVistaPlus();

			FlashContainer(pContainer, 0, 0);
			pContainer->m_dwFlashingStarted = 0;
			pLastActiveContainer = pContainer;
			if (pContainer->m_dwFlags & CNT_DEFERREDTABSELECT) {
				pContainer->m_dwFlags &= ~CNT_DEFERREDTABSELECT;
				SendMessage(hwndDlg, WM_SYSCOMMAND, SC_RESTORE, 0);

				NMHDR nmhdr = { hwndTab, IDC_MSGTABS, TCN_SELCHANGE };
				SendMessage(hwndDlg, WM_NOTIFY, 0, (LPARAM)&nmhdr);     // do it via a WM_NOTIFY / TCN_SELCHANGE to simulate user-activation
			}
			if (pContainer->m_dwFlags & CNT_DEFERREDSIZEREQUEST) {
				pContainer->m_dwFlags &= ~CNT_DEFERREDSIZEREQUEST;
				SendMessage(hwndDlg, WM_SIZE, 0, 0);
			}

			if (pContainer->m_dwFlags & CNT_TRANSPARENCY && fTransAllowed) {
				DWORD trans = LOWORD(pContainer->m_pSettings->dwTransparency);
				SetLayeredWindowAttributes(hwndDlg, Skin->getColorKey(), (BYTE)trans, (pContainer->m_dwFlags & CNT_TRANSPARENCY ? LWA_ALPHA : 0));
			}
			if (pContainer->m_dwFlags & CNT_NEED_UPDATETITLE) {
				pContainer->m_dwFlags &= ~CNT_NEED_UPDATETITLE;
				if (pContainer->m_hwndActive) {
					hContact = 0;
					SendMessage(pContainer->m_hwndActive, DM_QUERYHCONTACT, 0, (LPARAM)&hContact);
					if (hContact)
						pContainer->UpdateTitle(hContact);
				}
			}
			
			HWND hDlg = GetTabWindow(hwndTab, TabCtrl_GetCurSel(hwndTab));
			if (pContainer->m_dwFlags & CNT_DEFERREDCONFIGURE && hDlg) {
				pContainer->m_dwFlags &= ~CNT_DEFERREDCONFIGURE;
				pContainer->m_hwndActive = hDlg;
				SendMessage(hwndDlg, WM_SYSCOMMAND, SC_RESTORE, 0);
				if (pContainer->m_hwndActive != nullptr && IsWindow(pContainer->m_hwndActive)) {
					ShowWindow(pContainer->m_hwndActive, SW_SHOW);
					SetFocus(pContainer->m_hwndActive);
					SendMessage(pContainer->m_hwndActive, WM_ACTIVATE, WA_ACTIVE, 0);
					RedrawWindow(pContainer->m_hwndActive, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
				}
			}
			else if (hDlg)
				SendMessage(hDlg, WM_ACTIVATE, WA_ACTIVE, 0);
		}
		break;

	case WM_MOUSEMOVE:
		// wine: fix for erase/paint tab on mouse enter/leave tab.
		GetCursorPos(&pt);
		ScreenToClient(hwndTab, &pt);
		SendMessage(hwndTab, WM_MOUSEMOVE, wParam, (LPARAM)&pt);
		break;

	case DM_CLOSETABATMOUSE:
		if (HWND hDlg = GetTabWindow(hwndTab, GetTabItemFromMouse(hwndTab, (POINT*)lParam))) {
			if (hDlg != pContainer->m_hwndActive) {
				pContainer->m_bDontSmartClose = TRUE;
				SendMessage(hDlg, WM_CLOSE, 0, 1);
				RedrawWindow(hwndDlg, nullptr, nullptr, RDW_INVALIDATE);
				pContainer->m_bDontSmartClose = FALSE;
			}
			else SendMessage(hDlg, WM_CLOSE, 0, 1);
		}
		break;

	case WM_PAINT:
		if (bSkinned || M.isAero()) {
			PAINTSTRUCT ps;
			BeginPaint(hwndDlg, &ps);
			EndPaint(hwndDlg, &ps);
			return 0;
		}
		break;

	case WM_ERASEBKGND:
		// avoid flickering of the menu bar when aero is active
		if (pContainer) {
			HDC hdc = (HDC)wParam;
			GetClientRect(hwndDlg, &rc);

			if (M.isAero()) {
				HDC hdcMem;
				HANDLE  hbp = CMimAPI::m_pfnBeginBufferedPaint(hdc, &rc, BPBF_TOPDOWNDIB, nullptr, &hdcMem);
				FillRect(hdcMem, &rc, CSkin::m_BrushBack);
				CSkin::FinalizeBufferedPaint(hbp, &rc);
			}
			else {
				if (CSkin::m_skinEnabled)
					CSkin::DrawItem(hdc, &rc, &SkinItems[ID_EXTBKCONTAINER]);
				else {
					CSkin::FillBack(hdc, &rc);
					if (pContainer->m_pSideBar->isActive() && pContainer->m_pSideBar->isVisible()) {

						HPEN hPen = ::CreatePen(PS_SOLID, 1, PluginConfig.m_cRichBorders ? PluginConfig.m_cRichBorders : ::GetSysColor(COLOR_3DSHADOW));
						HPEN hOldPen = reinterpret_cast<HPEN>(::SelectObject(hdc, hPen));
						LONG x = (pContainer->m_pSideBar->getFlags() & CSideBar::SIDEBARORIENTATION_LEFT ? pContainer->m_pSideBar->getWidth() - 2 + pContainer->m_tBorder_outer_left :
							rc.right - pContainer->m_pSideBar->getWidth() + 1 - pContainer->m_tBorder_outer_right);
						::MoveToEx(hdc, x, rc.top, nullptr);
						::LineTo(hdc, x, rc.bottom);
						::SelectObject(hdc, hOldPen);
						::DeleteObject(hPen);
					}
				}
			}
			SetWindowLongPtr(hwndDlg, DWLP_MSGRESULT, 1);
			return TRUE;
		}
		break;

	case DM_OPTIONSAPPLIED:
		char szCname[40];
		wchar_t szTitleFormat[200];
		{
			wchar_t *szThemeName = nullptr;
			DBVARIANT dbv = { 0 };

			szTitleFormat[0] = 0;

			if (pContainer->m_isCloned && pContainer->m_hContactFrom != 0) {
				pContainer->m_pSettings = &PluginConfig.globalContainerSettings;

				pContainer->m_szRelThemeFile[0] = pContainer->m_szAbsThemeFile[0] = 0;
				mir_snprintf(szCname, "%s_theme", CONTAINER_PREFIX);
				if (!db_get_ws(pContainer->m_hContactFrom, SRMSGMOD_T, szCname, &dbv))
					szThemeName = dbv.pwszVal;
			}
			else {
				Utils::ReadPrivateContainerSettings(pContainer);
				if (szThemeName == nullptr) {
					mir_snprintf(szCname, "%s%d_theme", CONTAINER_PREFIX, pContainer->m_iContainerIndex);
					if (!db_get_ws(0, SRMSGMOD_T, szCname, &dbv))
						szThemeName = dbv.pwszVal;
				}
			}
			Utils::SettingsToContainer(pContainer);

			if (szThemeName != nullptr) {
				PathToAbsoluteW(szThemeName, pContainer->m_szAbsThemeFile, M.getDataPath());
				wcsncpy_s(pContainer->m_szRelThemeFile, szThemeName, _TRUNCATE);
				db_free(&dbv);
			}
			else pContainer->m_szAbsThemeFile[0] = pContainer->m_szRelThemeFile[0] = 0;

			pContainer->m_ltr_templates = pContainer->m_rtl_templates = nullptr;
		}
		break;

	case DM_STATUSBARCHANGED:
		SendMessage(hwndDlg, WM_SIZE, 0, 0);

		GetWindowRect(hwndDlg, &rc);
		SetWindowPos(hwndDlg, nullptr, rc.left, rc.top, rc.right - rc.left, (rc.bottom - rc.top) + 1, SWP_NOZORDER | SWP_NOACTIVATE);
		SetWindowPos(hwndDlg, nullptr, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, SWP_NOZORDER | SWP_NOACTIVATE);
		RedrawWindow(hwndDlg, nullptr, nullptr, RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN);

		if (pContainer->m_hwndStatus != nullptr && pContainer->m_hwndActive != nullptr)
			PostMessage(pContainer->m_hwndActive, DM_STATUSBARCHANGED, 0, 0);
		return 0;

	case DM_CONFIGURECONTAINER:
		UINT sBarHeight;
		{
			HMENU hSysmenu = GetSystemMenu(hwndDlg, FALSE);

			DWORD wsold, ws = wsold = GetWindowLongPtr(hwndDlg, GWL_STYLE);
			if (!CSkin::m_frameSkins) {
				ws = (pContainer->m_dwFlags & CNT_NOTITLE) ?
					((IsWindowVisible(hwndDlg) ? WS_VISIBLE : 0) | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_CLIPCHILDREN | WS_THICKFRAME | (CSkin::m_frameSkins ? WS_SYSMENU : WS_SYSMENU | WS_SIZEBOX)) :
					ws | WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
			}

			SetWindowLongPtr(hwndDlg, GWL_STYLE, ws);

			pContainer->m_tBorder = M.GetByte((bSkinned ? "S_tborder" : "tborder"), 2);
			pContainer->m_tBorder_outer_left = g_ButtonSet.left + M.GetByte((bSkinned ? "S_tborder_outer_left" : "tborder_outer_left"), 2);
			pContainer->m_tBorder_outer_right = g_ButtonSet.right + M.GetByte((bSkinned ? "S_tborder_outer_right" : "tborder_outer_right"), 2);
			pContainer->m_tBorder_outer_top = g_ButtonSet.top + M.GetByte((bSkinned ? "S_tborder_outer_top" : "tborder_outer_top"), 2);
			pContainer->m_tBorder_outer_bottom = g_ButtonSet.bottom + M.GetByte((bSkinned ? "S_tborder_outer_bottom" : "tborder_outer_bottom"), 2);
			sBarHeight = (UINT)M.GetByte((bSkinned ? "S_sbarheight" : "sbarheight"), 0);

			BOOL fTransAllowed = !bSkinned || IsWinVerVistaPlus();

			DWORD exold, ex = exold = GetWindowLongPtr(hwndDlg, GWL_EXSTYLE);
			ex = (pContainer->m_dwFlags & CNT_TRANSPARENCY && (!CSkin::m_skinEnabled || fTransAllowed)) ? ex | WS_EX_LAYERED : ex & ~(WS_EX_LAYERED);

			SetWindowLongPtr(hwndDlg, GWL_EXSTYLE, ex);
			if (pContainer->m_dwFlags & CNT_TRANSPARENCY && fTransAllowed) {
				DWORD trans = LOWORD(pContainer->m_pSettings->dwTransparency);
				SetLayeredWindowAttributes(hwndDlg, Skin->getColorKey(), (BYTE)trans, (/* pContainer->m_bSkinned ? LWA_COLORKEY : */ 0) | (pContainer->m_dwFlags & CNT_TRANSPARENCY ? LWA_ALPHA : 0));
			}

			if (!CSkin::m_frameSkins)
				CheckMenuItem(hSysmenu, IDM_NOTITLE, (pContainer->m_dwFlags & CNT_NOTITLE) ? MF_BYCOMMAND | MF_CHECKED : MF_BYCOMMAND | MF_UNCHECKED);

			CheckMenuItem(hSysmenu, IDM_STAYONTOP, pContainer->m_dwFlags & CNT_STICKY ? MF_BYCOMMAND | MF_CHECKED : MF_BYCOMMAND | MF_UNCHECKED);
			SetWindowPos(hwndDlg, (pContainer->m_dwFlags & CNT_STICKY) ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOREDRAW | SWP_NOCOPYBITS);
			if (ws != wsold) {
				GetWindowRect(hwndDlg, &rc);
				if ((ws & WS_CAPTION) != (wsold & WS_CAPTION)) {
					SetWindowPos(hwndDlg, nullptr, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_NOCOPYBITS);
					RedrawWindow(hwndDlg, nullptr, nullptr, RDW_INVALIDATE | RDW_FRAME | RDW_UPDATENOW);
					if (pContainer->m_hwndActive != nullptr) {
						dat = (CTabBaseDlg*)GetWindowLongPtr(pContainer->m_hwndActive, GWLP_USERDATA);
						dat->DM_ScrollToBottom(0, 0);
					}
				}
			}

			pContainer->m_dwFlags = ((pContainer->m_dwFlagsEx & (TCF_SBARLEFT | TCF_SBARRIGHT)) ?
				pContainer->m_dwFlags | CNT_SIDEBAR : pContainer->m_dwFlags & ~CNT_SIDEBAR);

			pContainer->m_pSideBar->Init();

			ws = wsold = GetWindowLongPtr(hwndTab, GWL_STYLE);
			if (pContainer->m_dwFlags & CNT_TABSBOTTOM)
				ws |= (TCS_BOTTOM);
			else
				ws &= ~(TCS_BOTTOM);
			if ((ws & (TCS_BOTTOM | TCS_MULTILINE)) != (wsold & (TCS_BOTTOM | TCS_MULTILINE))) {
				SetWindowLongPtr(hwndTab, GWL_STYLE, ws);
				RedrawWindow(hwndTab, nullptr, nullptr, RDW_INVALIDATE);
			}

			if (pContainer->m_dwFlags & CNT_NOSTATUSBAR) {
				if (pContainer->m_hwndStatus) {
					DestroyWindow(pContainer->m_hwndStatus);
					pContainer->m_hwndStatus = nullptr;
					pContainer->m_statusBarHeight = 0;
					SendMessage(hwndDlg, DM_STATUSBARCHANGED, 0, 0);
				}
			}
			else if (pContainer->m_hwndStatus == nullptr) {
				pContainer->m_hwndStatus = CreateWindowEx(0, L"TSStatusBarClass", nullptr, SBT_TOOLTIPS | WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwndDlg, nullptr, g_plugin.getInst(), nullptr);

				if (sBarHeight && bSkinned)
					SendMessage(pContainer->m_hwndStatus, SB_SETMINHEIGHT, sBarHeight, 0);
			}
			if (pContainer->m_hwndActive != nullptr) {
				hContact = 0;
				SendMessage(pContainer->m_hwndActive, DM_QUERYHCONTACT, 0, (LPARAM)&hContact);
				if (hContact)
					pContainer->UpdateTitle(hContact);
			}
			SendMessage(hwndDlg, WM_SIZE, 0, 1);
			BroadCastContainer(pContainer, DM_CONFIGURETOOLBAR, 0, 1);
		}
		return 0;

	// search the first and most recent unread events in all client tabs...
	// return all information via a RECENTINFO structure (tab indices,
	// window handles and timestamps).
	case DM_QUERYRECENT:
		DWORD dwTimestamp;
		{
			int iItems = TabCtrl_GetItemCount(hwndTab);

			RECENTINFO *ri = (RECENTINFO *)lParam;
			ri->iFirstIndex = ri->iMostRecent = -1;
			ri->dwFirst = ri->dwMostRecent = 0;
			ri->hwndFirst = ri->hwndMostRecent = nullptr;

			for (int i = 0; i < iItems; i++) {
				HWND hDlg = GetTabWindow(hwndTab, i);
				SendMessage(hDlg, DM_QUERYLASTUNREAD, 0, (LPARAM)&dwTimestamp);
				if (dwTimestamp > ri->dwMostRecent) {
					ri->dwMostRecent = dwTimestamp;
					ri->iMostRecent = i;
					ri->hwndMostRecent = hDlg;
					if (ri->iFirstIndex == -1) {
						ri->iFirstIndex = i;
						ri->dwFirst = dwTimestamp;
						ri->hwndFirst = hDlg;
					}
				}
			}
		}
		return 0;

		// search tab with either next or most recent unread message and select it
	case DM_QUERYPENDING:
		RECENTINFO ri;
		{
			SendMessage(hwndDlg, DM_QUERYRECENT, 0, (LPARAM)&ri);

			NMHDR nmhdr;
			nmhdr.code = TCN_SELCHANGE;

			if (wParam == DM_QUERY_NEXT && ri.iFirstIndex != -1) {
				TabCtrl_SetCurSel(hwndTab, ri.iFirstIndex);
				SendMessage(hwndDlg, WM_NOTIFY, 0, (LPARAM)&nmhdr);
			}
			if (wParam == DM_QUERY_MOSTRECENT && ri.iMostRecent != -1) {
				TabCtrl_SetCurSel(hwndTab, ri.iMostRecent);
				SendMessage(hwndDlg, WM_NOTIFY, 0, (LPARAM)&nmhdr);
			}
		}
		return 0;

	case WM_DRAWITEM:
		{
			DRAWITEMSTRUCT *dis = (DRAWITEMSTRUCT *)lParam;
			if (dis->hwndItem == pContainer->m_hwndStatus && !(pContainer->m_dwFlags & CNT_NOSTATUSBAR)) {
				dat = (CTabBaseDlg*)GetWindowLongPtr(pContainer->m_hwndActive, GWLP_USERDATA);
				if (dat)
					dat->DrawStatusIcons(dis->hDC, dis->rcItem, 2);
				return TRUE;
			}
		}
		return Menu_DrawItem(lParam);

	case WM_MEASUREITEM:
		return Menu_MeasureItem(lParam);

	case DM_QUERYCLIENTAREA:
		{
			RECT *pRect = (RECT*)lParam;
			if (pRect) {
				if (!IsIconic(hwndDlg))
					GetClientRect(hwndDlg, pRect);
				else
					CopyRect(pRect, &pContainer->m_rcSaved);
				AdjustTabClientRect(pContainer, pRect);
			}
		}
		return 0;

	case WM_DESTROY:
		pContainer->m_hwnd = nullptr;
		pContainer->m_hwndActive = nullptr;
		if (pContainer->m_hwndStatus)
			DestroyWindow(pContainer->m_hwndStatus);

		// mir_free private theme...
		if (pContainer->m_theme.isPrivate) {
			mir_free(pContainer->m_ltr_templates);
			mir_free(pContainer->m_rtl_templates);
			mir_free(pContainer->m_theme.logFonts);
			mir_free(pContainer->m_theme.fontColors);
			mir_free(pContainer->m_theme.rtfFonts);
		}

		if (pContainer->m_hwndTip)
			DestroyWindow(pContainer->m_hwndTip);
		RemoveContainerFromList(pContainer);
		if (pContainer->m_cachedDC) {
			SelectObject(pContainer->m_cachedDC, pContainer->m_oldHBM);
			DeleteObject(pContainer->m_cachedHBM);
			DeleteDC(pContainer->m_cachedDC);
		}
		if (pContainer->m_cachedToolbarDC) {
			SelectObject(pContainer->m_cachedToolbarDC, pContainer->m_oldhbmToolbarBG);
			DeleteObject(pContainer->m_hbmToolbarBG);
			DeleteDC(pContainer->m_cachedToolbarDC);
		}
		return 0;

	case WM_NCDESTROY:
		if (pContainer) {
			delete pContainer->m_pMenuBar;
			delete pContainer->m_pSideBar;
			if (pContainer->m_pSettings != &PluginConfig.globalContainerSettings)
				mir_free(pContainer->m_pSettings);
			mir_free(pContainer);
		}
		SetWindowLongPtr(hwndDlg, GWLP_USERDATA, 0);
		break;

	case WM_CLOSE:
		if (PluginConfig.m_bHideOnClose && !lParam) {
			ShowWindow(hwndDlg, SW_HIDE);
			pContainer->m_bHidden = true;
		}
		else {
			if (TabCtrl_GetItemCount(hwndTab) > 1) {
				LRESULT res = CWarning::show(CWarning::WARN_CLOSEWINDOW, MB_YESNOCANCEL | MB_ICONQUESTION);
				if (IDNO == res || IDCANCEL == res)
					break;
			}

			// dont ask if container is empty (no tabs)
			if (lParam == 0 && TabCtrl_GetItemCount(hwndTab) > 0) {
				int clients = TabCtrl_GetItemCount(hwndTab), iOpenJobs = 0;

				for (int i = 0; i < clients; i++) {
					HWND hDlg = GetTabWindow(hwndTab, i);
					if (hDlg && IsWindow(hDlg))
						SendMessage(hDlg, DM_CHECKQUEUEFORCLOSE, 0, (LPARAM)&iOpenJobs);
				}
				if (iOpenJobs && pContainer) {
					if (pContainer->m_exFlags & CNT_EX_CLOSEWARN)
						return TRUE;

					pContainer->m_exFlags |= CNT_EX_CLOSEWARN;
					LRESULT result = SendQueue::WarnPendingJobs(iOpenJobs);
					pContainer->m_exFlags &= ~CNT_EX_CLOSEWARN;
					if (result == IDNO)
						return TRUE;
				}
			}

			// save geometry information to the database...
			if (!(pContainer->m_dwFlags & CNT_GLOBALSIZE)) {
				WINDOWPLACEMENT wp = { 0 };
				wp.length = sizeof(wp);
				if (GetWindowPlacement(hwndDlg, &wp) != 0) {
					if (pContainer->m_isCloned && pContainer->m_hContactFrom != 0) {
						HWND hDlg = GetTabWindow(hwndTab, TabCtrl_GetCurSel(hwndTab));
						SendMessage(hDlg, DM_QUERYHCONTACT, 0, (LPARAM)&hContact);
						db_set_b(hContact, SRMSGMOD_T, "splitmax", (BYTE)((wp.showCmd == SW_SHOWMAXIMIZED) ? 1 : 0));

						for (int i = 0; i < TabCtrl_GetItemCount(hwndTab); i++) {
							if (hDlg = GetTabWindow(hwndTab, i)) {
								SendMessage(hDlg, DM_QUERYHCONTACT, 0, (LPARAM)&hContact);
								db_set_dw(hContact, SRMSGMOD_T, "splitx", wp.rcNormalPosition.left);
								db_set_dw(hContact, SRMSGMOD_T, "splity", wp.rcNormalPosition.top);
								db_set_dw(hContact, SRMSGMOD_T, "splitwidth", wp.rcNormalPosition.right - wp.rcNormalPosition.left);
								db_set_dw(hContact, SRMSGMOD_T, "splitheight", wp.rcNormalPosition.bottom - wp.rcNormalPosition.top);
							}
						}
					}
					else {
						char szCName[40];
						mir_snprintf(szCName, "%s%dx", CONTAINER_PREFIX, pContainer->m_iContainerIndex);
						db_set_dw(0, SRMSGMOD_T, szCName, wp.rcNormalPosition.left);
						mir_snprintf(szCName, "%s%dy", CONTAINER_PREFIX, pContainer->m_iContainerIndex);
						db_set_dw(0, SRMSGMOD_T, szCName, wp.rcNormalPosition.top);
						mir_snprintf(szCName, "%s%dwidth", CONTAINER_PREFIX, pContainer->m_iContainerIndex);
						db_set_dw(0, SRMSGMOD_T, szCName, wp.rcNormalPosition.right - wp.rcNormalPosition.left);
						mir_snprintf(szCName, "%s%dheight", CONTAINER_PREFIX, pContainer->m_iContainerIndex);
						db_set_dw(0, SRMSGMOD_T, szCName, wp.rcNormalPosition.bottom - wp.rcNormalPosition.top);

						db_set_b(0, SRMSGMOD_T, "splitmax", (BYTE)((wp.showCmd == SW_SHOWMAXIMIZED) ? 1 : 0));
					}
				}
			}

			// clear temp flags which should NEVER be saved...
			if (pContainer->m_isCloned && pContainer->m_hContactFrom != 0) {
				
				pContainer->m_dwFlags &= ~(CNT_DEFERREDCONFIGURE | CNT_CREATE_MINIMIZED | CNT_DEFERREDSIZEREQUEST | CNT_CREATE_CLONED);
				for (int i = 0; i < TabCtrl_GetItemCount(hwndTab); i++) {
					if (HWND hDlg = GetTabWindow(hwndTab, i)) {
						SendMessage(hDlg, DM_QUERYHCONTACT, 0, (LPARAM)&hContact);

						char szCName[40];
						mir_snprintf(szCName, "%s_theme", CONTAINER_PREFIX);
						if (mir_wstrlen(pContainer->m_szRelThemeFile) > 1) {
							if (pContainer->m_fPrivateThemeChanged == TRUE) {
								PathToRelativeW(pContainer->m_szRelThemeFile, pContainer->m_szAbsThemeFile, M.getDataPath());
								db_set_ws(hContact, SRMSGMOD_T, szCName, pContainer->m_szRelThemeFile);
								pContainer->m_fPrivateThemeChanged = FALSE;
							}
						}
						else {
							db_unset(hContact, SRMSGMOD_T, szCName);
							pContainer->m_fPrivateThemeChanged = FALSE;
						}
					}
				}
			}
			else Utils::SaveContainerSettings(pContainer, CONTAINER_PREFIX);
			DestroyWindow(hwndDlg);
		}
	}
	return FALSE;
}

/////////////////////////////////////////////////////////////////////////////////////////
// CreateContainer MUST allocate a ContainerWindowData and pass its address
// to CreateDialogParam() via the LPARAM. It also adds the struct to the linked list
// of containers.
//
// The WM_DESTROY handler of the container DlgProc is responsible for mir_free()'ing the
// pointer and for removing the struct from the linked list.

TContainerData* TSAPI CreateContainer(const wchar_t *name, int iTemp, MCONTACT hContactFrom)
{
	if (CMimAPI::m_shutDown)
		return nullptr;

	TContainerData *pContainer = (TContainerData*)mir_calloc(sizeof(TContainerData));
	if (pContainer == nullptr)
		return nullptr;
	wcsncpy(pContainer->m_wszName, name, CONTAINER_NAMELEN + 1);
	AppendToContainerList(pContainer);

	if (M.GetByte("limittabs", 0) && !mir_wstrcmp(name, L"default"))
		iTemp |= CNT_CREATEFLAG_CLONED;

	// save container name to the db
	if (!M.GetByte("singlewinmode", 0)) {
		int iFirstFree = -1, iFound = FALSE, i = 0;
		do {
			char szCounter[10];
			itoa(i, szCounter, 10);
			ptrW tszName(db_get_wsa(0, CONTAINER_KEY, szCounter));
			if (tszName == nullptr) {
				if (iFirstFree != -1) {
					pContainer->m_iContainerIndex = iFirstFree;
					itoa(iFirstFree, szCounter, 10);
				}
				else pContainer->m_iContainerIndex = i;

				db_set_ws(0, CONTAINER_KEY, szCounter, name);
				BuildContainerMenu();
				break;
			}

			if (!wcsncmp(tszName, name, CONTAINER_NAMELEN)) {
				pContainer->m_iContainerIndex = i;
				iFound = TRUE;
			}
			else if (!wcsncmp(tszName, L"**free**", CONTAINER_NAMELEN))
				iFirstFree = i;
		} while (++i && iFound == FALSE);
	}
	else {
		iTemp |= CNT_CREATEFLAG_CLONED;
		pContainer->m_iContainerIndex = 1;
	}

	if (iTemp & CNT_CREATEFLAG_MINIMIZED)
		pContainer->m_dwFlags = CNT_CREATE_MINIMIZED;
	if (iTemp & CNT_CREATEFLAG_CLONED) {
		pContainer->m_dwFlags |= CNT_CREATE_CLONED;
		pContainer->m_hContactFrom = hContactFrom;
	}
	pContainer->m_hwnd = CreateDialogParam(g_plugin.getInst(), MAKEINTRESOURCE(IDD_MSGCONTAINER), nullptr, DlgProcContainer, (LPARAM)pContainer);
	return pContainer;
}

// search the list of tabs and return the tab (by index) which "belongs" to the given
// hwnd. The hwnd is the handle of a message dialog childwindow. At creation,
// the dialog handle is stored in the TCITEM.lParam field, because we need
// to know the owner of the tab.
//
// hwndTab: handle of the tab control itself.
// hwnd: handle of a message dialog.
//
// returns the tab index (zero based), -1 if no tab is found (which SHOULD not
// really happen, but who knows... ;))

int TSAPI GetTabIndexFromHWND(HWND hwndTab, HWND hwnd)
{
	int iItems = TabCtrl_GetItemCount(hwndTab);

	for (int i = 0; i < iItems; i++) {
		HWND pDlg = GetTabWindow(hwndTab, i);
		if (pDlg == hwnd)
			return i;
	}
	return -1;
}

// activates the tab belonging to the given client HWND (handle of the actual
// message window.

int TSAPI ActivateTabFromHWND(HWND hwndTab, HWND hwnd)
{
	int iItem = GetTabIndexFromHWND(hwndTab, hwnd);
	if (iItem >= 0) {
		TabCtrl_SetCurSel(hwndTab, iItem);

		NMHDR nmhdr = {};
		nmhdr.code = TCN_SELCHANGE;
		SendMessage(GetParent(hwndTab), WM_NOTIFY, 0, (LPARAM)&nmhdr);     // do it via a WM_NOTIFY / TCN_SELCHANGE to simulate user-activation
		return iItem;
	}
	return -1;
}

// enumerates tabs and closes all of them, but the one in dat
void TSAPI CloseOtherTabs(HWND hwndTab, CTabBaseDlg &dat)
{
	for (int idxt = 0; idxt < dat.m_pContainer->m_iChilds;) {
		HWND otherTab = GetTabWindow(hwndTab, idxt);
		if (otherTab != nullptr && otherTab != dat.GetHwnd())
			SendMessage(otherTab, WM_CLOSE, 1, 0);
		else
			++idxt;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////
// cut off contact name to the option value set via Options->Tabbed messaging
// some people were requesting this, because really long contact list names
// are causing extraordinary wide tabs and these are looking ugly and wasting
// screen space.
//
// size = max length of target string

int TSAPI CutContactName(const wchar_t *oldname, wchar_t *newname, size_t size)
{
	if ((int)mir_wstrlen(oldname) <= PluginConfig.m_iTabNameLimit)
		wcsncpy_s(newname, size, oldname, _TRUNCATE);
	else {
		wchar_t fmt[30];
		mir_snwprintf(fmt, L"%%%d.%ds...", PluginConfig.m_iTabNameLimit, PluginConfig.m_iTabNameLimit);
		mir_snwprintf(newname, size, fmt, oldname);
	}
	return 0;
}

// functions for handling the linked list of struct ContainerWindowData *foo

static TContainerData* TSAPI AppendToContainerList(TContainerData *pContainer)
{
	if (!pFirstContainer) {
		pFirstContainer = pContainer;
		pFirstContainer->pNext = nullptr;
		return pFirstContainer;
	}

	TContainerData *p = pFirstContainer;
	while (p->pNext != nullptr)
		p = p->pNext;
	p->pNext = pContainer;
	pContainer->pNext = nullptr;
	return p;
}

TContainerData* TSAPI FindContainerByName(const wchar_t *name)
{
	if (name == nullptr || mir_wstrlen(name) == 0)
		return nullptr;

	if (M.GetByte("singlewinmode", 0)) // single window mode - always return 0 and force a new container
		return nullptr;

	for (TContainerData *p = pFirstContainer; p; p = p->pNext)
		if (!wcsncmp(p->m_wszName, name, CONTAINER_NAMELEN))
			return p;

	// error, didn't find it.
	return nullptr;
}

static TContainerData* TSAPI RemoveContainerFromList(TContainerData *pContainer)
{
	if (pContainer == pFirstContainer) {
		if (pContainer->pNext != nullptr)
			pFirstContainer = pContainer->pNext;
		else
			pFirstContainer = nullptr;

		if (pLastActiveContainer == pContainer)     // make sure, we don't reference this container anymore
			pLastActiveContainer = pFirstContainer;

		return pFirstContainer;
	}

	for (TContainerData *p = pFirstContainer; p; p = p->pNext) {
		if (p->pNext == pContainer) {
			p->pNext = p->pNext->pNext;

			if (pLastActiveContainer == pContainer)     // make sure, we don't reference this container anymore
				pLastActiveContainer = pFirstContainer;

			return nullptr;
		}
	}
	return nullptr;
}

// calls the TabCtrl_AdjustRect to calculate the "real" client area of the tab.
// also checks for the option "hide tabs when only one tab open" and adjusts
// geometry if necessary
// rc is the RECT obtained by GetClientRect(hwndTab)

void TSAPI AdjustTabClientRect(TContainerData *pContainer, RECT *rc)
{
	HWND hwndTab = GetDlgItem(pContainer->m_hwnd, IDC_MSGTABS);
	DWORD tBorder = pContainer->m_tBorder;
	DWORD dwStyle = GetWindowLongPtr(hwndTab, GWL_STYLE);

	RECT rcTab, rcTabOrig;
	GetClientRect(hwndTab, &rcTab);
	if (!(pContainer->m_dwFlags & CNT_SIDEBAR) && (pContainer->m_iChilds > 1 || !(pContainer->m_dwFlags & CNT_HIDETABS))) {
		rcTabOrig = rcTab;
		TabCtrl_AdjustRect(hwndTab, FALSE, &rcTab);
		DWORD dwTopPad = rcTab.top - rcTabOrig.top;

		rc->left += tBorder;
		rc->right -= tBorder;

		if (dwStyle & TCS_BUTTONS) {
			if (pContainer->m_dwFlags & CNT_TABSBOTTOM) {
				int nCount = TabCtrl_GetItemCount(hwndTab);
				if (nCount > 0) {
					RECT rcItem;
					TabCtrl_GetItemRect(hwndTab, nCount - 1, &rcItem);
					rc->bottom = rcItem.top;
				}
			}
			else {
				rc->top += (dwTopPad - 2);
				rc->bottom = rcTabOrig.bottom;
			}
		}
		else {
			if (pContainer->m_dwFlags & CNT_TABSBOTTOM)
				rc->bottom = rcTab.bottom + 2;
			else {
				rc->top += (dwTopPad - 2);
				rc->bottom = rcTabOrig.bottom;
			}
		}

		rc->top += tBorder;
		rc->bottom -= tBorder;
	}
	else {
		rc->bottom = rcTab.bottom;
		rc->top = rcTab.top;
	}
	rc->right -= (pContainer->m_tBorder_outer_left + pContainer->m_tBorder_outer_right);
	if (pContainer->m_pSideBar->isVisible())
		rc->right -= pContainer->m_pSideBar->getWidth();
}

// retrieve the container name for the given contact handle.
// if none is assigned, return the name of the default container

void TSAPI GetContainerNameForContact(MCONTACT hContact, wchar_t *szName, int iNameLen)
{
	// single window mode using cloned (temporary) containers
	if (M.GetByte("singlewinmode", 0)) {
		wcsncpy_s(szName, iNameLen, L"Message Session", _TRUNCATE);
		return;
	}

	// use clist group names for containers...
	if (M.GetByte("useclistgroups", 0)) {
		wcsncpy_s(szName, iNameLen, ptrW(db_get_wsa(hContact, "CList", "Group", L"default")), _TRUNCATE);
		return;
	}

	wcsncpy_s(szName, iNameLen, ptrW(db_get_wsa(hContact, SRMSGMOD_T, CONTAINER_SUBKEY, L"default")), _TRUNCATE);
}

void TSAPI DeleteContainer(int iIndex)
{
	char szIndex[10];
	itoa(iIndex, szIndex, 10);
	ptrW tszContainerName(db_get_wsa(0, CONTAINER_KEY, szIndex));
	if (tszContainerName == nullptr)
		return;

	db_set_ws(0, CONTAINER_KEY, szIndex, L"**free**");

	for (auto &hContact : Contacts()) {
		ptrW tszValue(db_get_wsa(hContact, SRMSGMOD_T, CONTAINER_SUBKEY));
		if (!mir_wstrcmp(tszValue, tszContainerName))
			db_unset(hContact, SRMSGMOD_T, CONTAINER_SUBKEY);
	}

	char szSetting[CONTAINER_NAMELEN + 30];
	mir_snprintf(szSetting, "%s%d_Flags", CONTAINER_PREFIX, iIndex);
	db_unset(0, SRMSGMOD_T, szSetting);
	mir_snprintf(szSetting, "%s%d_Trans", CONTAINER_PREFIX, iIndex);
	db_unset(0, SRMSGMOD_T, szSetting);
	mir_snprintf(szSetting, "%s%dwidth", CONTAINER_PREFIX, iIndex);
	db_unset(0, SRMSGMOD_T, szSetting);
	mir_snprintf(szSetting, "%s%dheight", CONTAINER_PREFIX, iIndex);
	db_unset(0, SRMSGMOD_T, szSetting);
	mir_snprintf(szSetting, "%s%dx", CONTAINER_PREFIX, iIndex);
	db_unset(0, SRMSGMOD_T, szSetting);
	mir_snprintf(szSetting, "%s%dy", CONTAINER_PREFIX, iIndex);
	db_unset(0, SRMSGMOD_T, szSetting);
}

void TSAPI RenameContainer(int iIndex, const wchar_t *szNew)
{
	if (mir_wstrlen(szNew) == 0)
		return;

	char szIndex[10];
	itoa(iIndex, szIndex, 10);
	ptrW tszContainerName(db_get_wsa(0, CONTAINER_KEY, szIndex));
	if (tszContainerName == nullptr)
		return;

	db_set_ws(0, CONTAINER_KEY, szIndex, szNew);

	for (auto &hContact : Contacts()) {
		ptrW tszValue(db_get_wsa(hContact, SRMSGMOD_T, CONTAINER_SUBKEY));
		if (!mir_wstrcmp(tszValue, tszContainerName))
			db_set_ws(hContact, SRMSGMOD_T, CONTAINER_SUBKEY, szNew);
	}
}

HMENU TSAPI BuildContainerMenu()
{
	if (PluginConfig.g_hMenuContainer != nullptr) {
		HMENU submenu = GetSubMenu(PluginConfig.g_hMenuContext, 0);
		RemoveMenu(submenu, 6, MF_BYPOSITION);
		DestroyMenu(PluginConfig.g_hMenuContainer);
		PluginConfig.g_hMenuContainer = nullptr;
	}

	// no container attach menu, if we are using the "clist group mode"
	if (M.GetByte("useclistgroups", 0) || M.GetByte("singlewinmode", 0))
		return nullptr;

	HMENU hMenu = CreateMenu();
	int i = 0;
	while (true) {
		char szCounter[10];
		itoa(i, szCounter, 10);
		ptrW tszName(db_get_wsa(0, CONTAINER_KEY, szCounter));
		if (tszName == nullptr)
			break;

		if (wcsncmp(tszName, L"**free**", CONTAINER_NAMELEN))
			AppendMenu(hMenu, MF_STRING, IDM_CONTAINERMENU + i, !mir_wstrcmp(tszName, L"default") ? TranslateT("Default container") : tszName);
		i++;
	}

	InsertMenu(PluginConfig.g_hMenuContext, ID_TABMENU_ATTACHTOCONTAINER, MF_BYCOMMAND | MF_POPUP, (UINT_PTR)hMenu, TranslateT("Attach to"));
	PluginConfig.g_hMenuContainer = hMenu;
	return hMenu;
}

// flashes the container
// iMode != 0: turn on flashing
// iMode == 0: turn off flashing

void TSAPI FlashContainer(TContainerData *pContainer, int iMode, int iCount)
{
	if (pContainer->m_dwFlags & CNT_NOFLASH)                  // container should never flash
		return;

	FLASHWINFO fwi;
	fwi.cbSize = sizeof(fwi);
	fwi.uCount = 0;

	if (iMode) {
		fwi.dwFlags = FLASHW_ALL;
		if (pContainer->m_dwFlags & CNT_FLASHALWAYS)
			fwi.dwFlags |= FLASHW_TIMER;
		else
			fwi.uCount = (iCount == 0) ? M.GetByte("nrflash", 4) : iCount;
		fwi.dwTimeout = M.GetDword("flashinterval", 1000);

	}
	else fwi.dwFlags = FLASHW_STOP;

	fwi.hwnd = pContainer->m_hwnd;
	pContainer->m_dwFlashingStarted = GetTickCount();
	FlashWindowEx(&fwi);
}

void TSAPI ReflashContainer(TContainerData *pContainer)
{
	DWORD dwStartTime = pContainer->m_dwFlashingStarted;

	if (pContainer->IsActive())       // dont care about active windows
		return;

	if (pContainer->m_dwFlags & CNT_NOFLASH || pContainer->m_dwFlashingStarted == 0)
		return;                                                                                 // dont care about containers which should never flash

	if (pContainer->m_dwFlags & CNT_FLASHALWAYS)
		FlashContainer(pContainer, 1, 0);
	else {
		// recalc the remaining flashes
		DWORD dwInterval = M.GetDword("flashinterval", 1000);
		int iFlashesElapsed = (GetTickCount() - dwStartTime) / dwInterval;
		DWORD dwFlashesDesired = M.GetByte("nrflash", 4);
		if (iFlashesElapsed < (int)dwFlashesDesired)
			FlashContainer(pContainer, 1, dwFlashesDesired - iFlashesElapsed);
		else {
			BOOL isFlashed = FlashWindow(pContainer->m_hwnd, TRUE);
			if (!isFlashed)
				FlashWindow(pContainer->m_hwnd, TRUE);
		}
	}
	pContainer->m_dwFlashingStarted = dwStartTime;
}

// broadcasts a message to all child windows (tabs/sessions)

void TSAPI BroadCastContainer(const TContainerData *pContainer, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (pContainer == nullptr)
		return;
	HWND hwndTab = GetDlgItem(pContainer->m_hwnd, IDC_MSGTABS);

	int nCount = TabCtrl_GetItemCount(hwndTab);
	for (int i = 0; i < nCount; i++) {
		HWND hDlg = GetTabWindow(hwndTab, i);
		if (IsWindow(hDlg))
			SendMessage(hDlg, message, wParam, lParam);
	}
}

void TSAPI CloseAllContainers()
{
	bool fOldHideSetting = PluginConfig.m_bHideOnClose;

	while (pFirstContainer != nullptr) {
		if (!IsWindow(pFirstContainer->m_hwnd))
			pFirstContainer = pFirstContainer->pNext;
		else {
			PluginConfig.m_bHideOnClose = false;
			::SendMessage(pFirstContainer->m_hwnd, WM_CLOSE, 0, 1);
		}
	}

	PluginConfig.m_bHideOnClose = fOldHideSetting;
}
