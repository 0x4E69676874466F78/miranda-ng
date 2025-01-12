/*
    Cool Scrollbar Library Version 1.2

    Module: coolscroll.c
	Copyright (c) J Brown 2001
	  
	This code is freeware, however, you may not publish
	this code elsewhere or charge any money for it. This code
	is supplied as-is. I make no guarantees about the suitability
	of this code - use at your own risk.
	
	It would be nice if you credited me, in the event
	that you use this code in a product.
	
	VERSION HISTORY:

	 V1.2: TreeView problem fixed by Diego Tartara
		   Small problem in thumbsize calculation also fixed (thanks Diego!)

	 V1.1: Added support for Right-left windows
	       Changed calling convention of APIs to WINAPI (__stdcall)
		   Completely standalone (no need for c-runtime)
		   Now supports ALL windows with appropriate USER32.DLL patching
		    (you provide!!)

	 V1.0: Apr 2001: Initial Version

  IMPORTANT:
	 This whole library is based around code for a horizontal scrollbar.
	 All "vertical" scrollbar drawing / mouse interaction uses the
	 horizontal scrollbar functions, but uses a trick to convert the vertical
	 scrollbar coordinates into horizontal equivelants. When I started this project,
	 I quickly realised that the code for horz/vert bars was IDENTICAL, apart
	 from the fact that horizontal code uses left/right coords, and vertical code
	 uses top/bottom coords. On entry to a "vertical" drawing function, for example,
	 the coordinates are "rotated" before the horizontal function is called, and
	 then rotated back once the function has completed. When something needs to
	 be drawn, the coords are converted back again before drawing.
	 
     This trick greatly reduces the amount of code required, and makes
	 maintanence much simpler. This way, only one function is needed to draw
	 a scrollbar, but this can be used for both horizontal and vertical bars
	 with careful thought.
*/

#include "stdafx.h"
#include "coolscroll.h"
#include "userdefs.h"
#include "coolsb_internal.h"

//define some values if the new version of common controls
//is not available.
#ifndef NM_CUSTOMDRAW
#define NM_CUSTOMDRAW (NM_FIRST-12)
#define CDRF_DODEFAULT		0x0000
#define CDRF_SKIPDEFAULT	0x0004
#define CDDS_PREPAINT		0x0001
#define CDDS_POSTPAINT		0x0002
#endif

//
//	Special thumb-tracking variables
//
//
static UINT uCurrentScrollbar = COOLSB_NONE;	//SB_HORZ / SB_VERT
static UINT uCurrentScrollPortion = HTSCROLL_NONE;
static UINT uCurrentButton = 0;

static RECT rcThumbBounds;		//area that the scroll thumb can travel in
static int  nThumbSize;			//(pixels)
static int  nThumbPos;			//(pixels)
static int  nThumbMouseOffset;	//(pixels)
static int  nLastPos = -1;		//(scrollbar units)
static int  nThumbPos0;			//(pixels) initial thumb position

//
//	Temporary state used to auto-generate timer messages
//
static UINT_PTR uMouseOverId = 0;
static UINT uMouseOverScrollbar = COOLSB_NONE;
static UINT uHitTestPortion = HTSCROLL_NONE;
static UINT uLastHitTestPortion = HTSCROLL_NONE;
static RECT MouseOverRect;

static UINT uScrollTimerMsg = 0;
static UINT uScrollTimerPortion = HTSCROLL_NONE;
static UINT_PTR uScrollTimerId = 0;
static HWND hwndCurCoolSB = nullptr;

extern int  CustomDrawScrollBars(NMCSBCUSTOMDRAW *nmcsbcd);

LRESULT CALLBACK CoolSBWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

//
//	Provide this so there are NO dependencies on CRT
//
static void CoolSB_ZeroMemory(void *ptr, DWORD bytes)
{
	BYTE *bptr = (BYTE *)ptr;

	while(bytes--) *bptr++ = 0;
}

BOOL WINAPI CoolSB_IsThumbTracking(HWND hwnd)	
{ 
	SCROLLWND *sw;

	if ((sw = GetScrollWndFromHwnd(hwnd)) == nullptr)
		return FALSE;
	else
		return sw->fThumbTracking; 
}

//
//	swap the rectangle's x coords with its y coords
//
static void __stdcall RotateRect(RECT *rect)
{
	int temp;
	temp = rect->left;
	rect->left = rect->top;
	rect->top = temp;

	temp = rect->right;
	rect->right = rect->bottom;
	rect->bottom = temp;
}

//
//	swap the coords if the scrollbar is a SB_VERT
//
static void __stdcall RotateRect0(SCROLLBAR *sb, RECT *rect)
{
	if (sb->nBarType == SB_VERT)
		RotateRect(rect);
}

//
//	Calculate if the SCROLLINFO members produce
//  an enabled or disabled scrollbar
//
static BOOL IsScrollInfoActive(SCROLLINFO *si)
{
	if ((si->nPage > (UINT)si->nMax
		|| si->nMax <= si->nMin || si->nMax == 0))
		return FALSE;
	else
		return TRUE;
}

//
//	Return if the specified scrollbar is enabled or not
//
static BOOL IsScrollbarActive(SCROLLBAR *sb)
{
	SCROLLINFO *si = &sb->scrollInfo;
	if (((sb->fScrollFlags & ESB_DISABLE_BOTH) == ESB_DISABLE_BOTH) ||
		!(sb->fScrollFlags & CSBS_THUMBALWAYS) && !IsScrollInfoActive(si))
		return FALSE;
	else
		return TRUE;
}

//
//	Draw a standard scrollbar arrow
//
static int DrawScrollArrow(SCROLLBAR *sbar, HDC hdc, RECT *rect, UINT arrow, BOOL fMouseDown, BOOL fMouseOver)
{
	UINT ret;
	UINT flags = arrow;

	//HACKY bit so this routine can be called by vertical and horizontal code
	if (sbar->nBarType == SB_VERT)
	{
		if (flags & DFCS_SCROLLLEFT)		flags = flags & ~DFCS_SCROLLLEFT  | DFCS_SCROLLUP;
		if (flags & DFCS_SCROLLRIGHT)	flags = flags & ~DFCS_SCROLLRIGHT | DFCS_SCROLLDOWN;
	}

	if (fMouseDown) flags |= (DFCS_FLAT | DFCS_PUSHED);

#ifdef FLAT_SCROLLBARS
	if (sbar->fFlatScrollbar != CSBS_NORMAL)
	{
		HDC hdcmem1, hdcmem2;
		HBITMAP hbm1, oldbm1;
		HBITMAP hbm2, oldbm2;
		RECT rc;
		int width, height;

		rc = *rect;
		width  = rc.right-rc.left;
		height = rc.bottom-rc.top;
		SetRect(&rc, 0, 0, width, height);

		//MONOCHROME bitmap to convert the arrow to black/white mask
		hdcmem1 = CreateCompatibleDC(hdc);
		hbm1    = CreateBitmap(width, height, 1, 1, nullptr);
		UnrealizeObject(hbm1);
		oldbm1  = reinterpret_cast<HBITMAP>(SelectObject(hdcmem1, hbm1));
		

		//NORMAL bitmap to draw the arrow into
		hdcmem2 = CreateCompatibleDC(hdc);
		hbm2    = CreateCompatibleBitmap(hdc, width, height);
		UnrealizeObject(hbm2);
		oldbm2  = reinterpret_cast<HBITMAP>(SelectObject(hdcmem2, hbm2));
		

		flags = flags & ~DFCS_PUSHED | DFCS_FLAT;	//just in case
		DrawFrameControl(hdcmem2, &rc, DFC_SCROLL, flags);


#ifndef HOT_TRACKING
		if (fMouseDown)
		{
			//uncomment these to make the cool scrollbars
			//look like the common controls flat scrollbars
			//fMouseDown = FALSE;
			//fMouseOver = TRUE;
		}
#endif
		//draw a flat monochrome version of a scrollbar arrow (dark)
		if (fMouseDown)
		{
			SetBkColor(hdcmem2, GetSysColor(COLOR_BTNTEXT));
			BitBlt(hdcmem1, 0, 0, width, height, hdcmem2, 0, 0, SRCCOPY);
			SetBkColor(hdc, 0x00ffffff);
			SetTextColor(hdc, GetSysColor(COLOR_3DDKSHADOW));
			BitBlt(hdc, rect->left, rect->top, width, height, hdcmem1, 0, 0, SRCCOPY);
		}
		//draw a flat monochrome version of a scrollbar arrow (grey)
		else if (fMouseOver)
		{
			SetBkColor(hdcmem2, GetSysColor(COLOR_BTNTEXT));
			FillRect(hdcmem1, &rc, reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
			BitBlt(hdcmem1, 0, 0, width, height, hdcmem2, 0, 0, SRCINVERT);

			SetBkColor(hdc, GetSysColor(COLOR_3DSHADOW));
			SetTextColor(hdc, 0x00ffffff);
			BitBlt(hdc, rect->left, rect->top, width, height, hdcmem1, 0, 0, SRCCOPY);
		}
		//draw the arrow normally
		else
		{
			BitBlt(hdc, rect->left, rect->top, width, height, hdcmem2, 0, 0, SRCCOPY);
		}

		SelectObject(hdcmem1, oldbm1);
		SelectObject(hdcmem2, oldbm2);
		DeleteObject(hbm1);
		DeleteObject(hbm2);
		DeleteDC(hdcmem1);
		DeleteDC(hdcmem2);

		ret = 0;
	}
	else
#endif
	ret = DrawFrameControl(hdc, rect, DFC_SCROLL, flags);

	return ret;
}

//
//	Return the size in pixels for the specified scrollbar metric,
//  for the specified scrollbar
//
static int GetScrollMetric(SCROLLBAR *sbar, int metric)
{
	if (sbar->nBarType == SB_HORZ)
	{
		if (metric == SM_CXHORZSB)
		{
			if (sbar->nArrowLength < 0)
				return -sbar->nArrowLength * GetSystemMetrics(SM_CXHSCROLL);
			else
				return sbar->nArrowLength;
		}
		else
		{
			if (sbar->nArrowWidth < 0)
				return -sbar->nArrowWidth * GetSystemMetrics(SM_CYHSCROLL);
			else
				return sbar->nArrowWidth;
		}
	}
	else if (sbar->nBarType == SB_VERT)
	{
		if (metric == SM_CYVERTSB)
		{
			if (sbar->nArrowLength < 0)
				return -sbar->nArrowLength * GetSystemMetrics(SM_CYVSCROLL);
			else
				return sbar->nArrowLength;
		}
		else
		{
			if (sbar->nArrowWidth < 0)
				return -sbar->nArrowWidth * GetSystemMetrics(SM_CXVSCROLL);
			else
				return sbar->nArrowWidth;
		}
	}

	return 0;
}

//
//	
//
static COLORREF GetSBForeColor(void)
{
	COLORREF c1 = GetSysColor(COLOR_3DHILIGHT);
	COLORREF c2 = GetSysColor(COLOR_WINDOW);

	if (c1 != 0xffffff && c1 == c2)
	{
		return GetSysColor(COLOR_BTNFACE);
	}
	else
	{
		return GetSysColor(COLOR_3DHILIGHT);
	}
}

static COLORREF GetSBBackColor(void)
{
	return GetSysColor(COLOR_SCROLLBAR);
}

//
//	Paint a checkered rectangle, with each alternate
//	pixel being assigned a different colour
//
static void DrawCheckedRect(HDC hdc, RECT *rect, COLORREF fg, COLORREF bg)
{
	static WORD wCheckPat[8] = 
	{ 
		0xaaaa, 0x5555, 0xaaaa, 0x5555, 0xaaaa, 0x5555, 0xaaaa, 0x5555 
	};

	HBITMAP hbmp;
	HBRUSH  hbr, hbrold;
	COLORREF fgold, bgold;

	hbmp = CreateBitmap(8, 8, 1, 1, wCheckPat);
	hbr  = CreatePatternBrush(hbmp);

	UnrealizeObject(hbr);
	SetBrushOrgEx(hdc, rect->left, rect->top, nullptr);

	hbrold = (HBRUSH)SelectObject(hdc, hbr);

	fgold = SetTextColor(hdc, fg);
	bgold = SetBkColor(hdc, bg);
	
	PatBlt(hdc, rect->left, rect->top, 
				rect->right - rect->left, 
				rect->bottom - rect->top, 
				PATCOPY);
	
	SetBkColor(hdc, bgold);
	SetTextColor(hdc, fgold);
	
	SelectObject(hdc, hbrold);
	DeleteObject(hbr);
	DeleteObject(hbmp);
}

//
//	Fill the specifed rectangle using a solid colour
//
static void PaintRect(HDC hdc, RECT *rect, COLORREF color)
{
	COLORREF oldcol = SetBkColor(hdc, color);
	ExtTextOutA(hdc, 0, 0, ETO_OPAQUE, rect, "", 0, nullptr);
	SetBkColor(hdc, oldcol);
}

//
//	Draw a simple blank scrollbar push-button. Can be used
//	to draw a push button, or the scrollbar thumb
//	drawflag - could set to BF_FLAT to make flat scrollbars
//
void DrawBlankButton(HDC hdc, const RECT *rect, UINT drawflag)
{
	RECT rc = *rect;
		
#ifndef FLAT_SCROLLBARS	
	drawflag &= ~BF_FLAT;
#endif
	
	DrawEdge(hdc, &rc, EDGE_RAISED, BF_RECT | drawflag | BF_ADJUST);
	FillRect(hdc, &rc, GetSysColorBrush(COLOR_BTNFACE));
}

//
//	Send a WM_VSCROLL or WM_HSCROLL message
//
static void SendScrollMessage(HWND hwnd, UINT scrMsg, UINT scrId, UINT pos)
{
	SendMessage(hwnd, scrMsg, MAKEWPARAM(scrId, pos), 0);
}

//
//	Calculate the screen coordinates of the area taken by
//  the horizontal scrollbar. Take into account the size
//  of the window borders
//
static BOOL GetHScrollRect(SCROLLWND *sw, HWND hwnd, RECT *rect)
{
	GetWindowRect(hwnd, rect);
	
	if (sw->fLeftScrollbar)
	{
		rect->left  += sw->cxLeftEdge + (sw->sbarVert.fScrollVisible ? 
					GetScrollMetric(&sw->sbarVert, SM_CXVERTSB) : 0);
		rect->right -= sw->cxRightEdge;
	}
	else
	{
		rect->left   += sw->cxLeftEdge;					//left window edge
	
		rect->right  -= sw->cxRightEdge +				//right window edge
					(sw->sbarVert.fScrollVisible ? 
					GetScrollMetric(&sw->sbarVert, SM_CXVERTSB) : 0);
	}
	
	rect->bottom -= sw->cyBottomEdge;				//bottom window edge
	
	rect->top	  = rect->bottom -
					(sw->sbarHorz.fScrollVisible ?
					GetScrollMetric(&sw->sbarHorz, SM_CYHORZSB) : 0);
	
	return TRUE;
}

//
//	Calculate the screen coordinates of the area taken by the
//  vertical scrollbar
//
static BOOL GetVScrollRect(SCROLLWND *sw, HWND hwnd, RECT *rect)
{
	GetWindowRect(hwnd, rect);
	rect->top	 += sw->cyTopEdge;						//top window edge
	
	rect->bottom -= sw->cyBottomEdge + 
					(sw->sbarHorz.fScrollVisible ?		//bottom window edge
					GetScrollMetric(&sw->sbarHorz, SM_CYHORZSB) : 0);

	if (sw->fLeftScrollbar)
	{
		rect->left	+= sw->cxLeftEdge;
		rect->right = rect->left + (sw->sbarVert.fScrollVisible ?
					GetScrollMetric(&sw->sbarVert, SM_CXVERTSB) : 0);
	}
	else
	{
		rect->right  -= sw->cxRightEdge;
		rect->left    = rect->right - (sw->sbarVert.fScrollVisible ?	
					GetScrollMetric(&sw->sbarVert, SM_CXVERTSB) : 0);
	}

	return TRUE;
}

//	Depending on what type of scrollbar nBar refers to, call the
//  appropriate Get?ScrollRect function
//
BOOL GetScrollRect(SCROLLWND *sw, UINT nBar, HWND hwnd, RECT *rect)
{
	if (nBar == SB_HORZ)
		return GetHScrollRect(sw, hwnd, rect);
	else if (nBar == SB_VERT)
		return GetVScrollRect(sw, hwnd, rect);
	else
		return FALSE;
}

//
//	Work out the scrollbar width/height for either type of scrollbar (SB_HORZ/SB_VERT)
//	rect - coords of the scrollbar.
//	store results into *thumbsize and *thumbpos
//
static int CalcThumbSize(SCROLLBAR *sbar, const RECT *rect, int *pthumbsize, int *pthumbpos)
{
	int scrollsize;			//total size of the scrollbar including arrow buttons
	int workingsize;		//working area (where the thumb can slide)
	int siMaxMin;
	int butsize;
	int startcoord;
	int thumbpos = 0, thumbsize = 0;

	static int count=0;

	//work out the width (for a horizontal) or the height (for a vertical)
	//of a standard scrollbar button
	butsize = GetScrollMetric(sbar, SM_SCROLL_LENGTH);

	scrollsize = rect->right - rect->left;
	startcoord = rect->left;

	SCROLLINFO *si = &sbar->scrollInfo;
	siMaxMin = si->nMax - si->nMin + 1;
	workingsize = scrollsize - butsize * 2;

	//
	// Work out the scrollbar thumb SIZE
	//
	if (si->nPage == 0)
	{
		thumbsize = butsize;
	}
	else if (siMaxMin > 0)
	{
		thumbsize = MulDiv(si->nPage, workingsize, siMaxMin);

		if (thumbsize < sbar->nMinThumbSize)
			thumbsize = sbar->nMinThumbSize;
	}

	//
	// Work out the scrollbar thumb position
	//
	if (siMaxMin > 0)
	{
		int pagesize = max(1, si->nPage);
		thumbpos = MulDiv(si->nPos - si->nMin, workingsize-thumbsize, siMaxMin - pagesize);
		
		if (thumbpos < 0)						
			thumbpos = 0;

		if (thumbpos >= workingsize-thumbsize)	
			thumbpos = workingsize-thumbsize;
	}

	thumbpos += startcoord + butsize;

	*pthumbpos  = thumbpos;
	*pthumbsize = thumbsize;

	return 1;
}

//
//	return a hit-test value for whatever part of the scrollbar x,y is located in
//	rect, x, y: SCREEN coordinates
//	the rectangle must not include space for any inserted buttons 
//	(i.e, JUST the scrollbar area)
//
static UINT GetHorzScrollPortion(SCROLLBAR *sbar, const RECT *rect, int x, int y)
{
	int thumbwidth, thumbpos;
	int butwidth = GetScrollMetric(sbar, SM_SCROLL_LENGTH);
	int scrollwidth  = rect->right-rect->left;
	int workingwidth = scrollwidth - butwidth*2;

	if (y < rect->top || y >= rect->bottom)
		return HTSCROLL_NONE;

	CalcThumbSize(sbar, rect, &thumbwidth, &thumbpos);

	//if we have had to scale the buttons to fit in the rect,
	//then adjust the button width accordingly
	if (scrollwidth <= butwidth * 2)
	{
		butwidth = scrollwidth / 2;	
	}

	//check for left button click
	if (x >= rect->left && x < rect->left + butwidth)
	{
		return HTSCROLL_LEFT;	
	}
	//check for right button click
	else if (x >= rect->right-butwidth && x < rect->right)
	{
		return HTSCROLL_RIGHT;
	}
	
	//if the thumb is too big to fit (i.e. it isn't visible)
	//then return a NULL scrollbar area
	if (thumbwidth >= workingwidth)
		return HTSCROLL_NONE;
	
	//check for point in the thumbbar
	if (x >= thumbpos && x < thumbpos+thumbwidth)
	{
		return HTSCROLL_THUMB;
	}	
	//check for left margin
	else if (x >= rect->left+butwidth && x < thumbpos)
	{
		return HTSCROLL_PAGELEFT;
	}
	else if (x >= thumbpos+thumbwidth && x < rect->right-butwidth)
	{
		return HTSCROLL_PAGERIGHT;
	}
	
	return HTSCROLL_NONE;
}

//
//	For vertical scrollbars, rotate all coordinates by -90 degrees
//	so that we can use the horizontal version of this function
//
static UINT GetVertScrollPortion(SCROLLBAR *sb, RECT *rect, int x, int y)
{
	UINT r;
	
	RotateRect(rect);
	r = GetHorzScrollPortion(sb, rect, y, x);
	RotateRect(rect);
	return r;
}

//
//	CUSTOM DRAW support
//	
static LRESULT PostCustomPrePostPaint(HWND hwnd, HDC hdc, SCROLLBAR *sb, UINT dwStage)
{
#ifdef CUSTOM_DRAW
	NMCSBCUSTOMDRAW	nmcd;

	CoolSB_ZeroMemory(&nmcd, sizeof nmcd);
	nmcd.hdr.hwndFrom = hwnd;
	nmcd.hdr.idFrom   = GetWindowLongPtr(hwnd, GWLP_ID);
	nmcd.hdr.code     = NM_COOLSB_CUSTOMDRAW;
	nmcd.nBar		  = sb->nBarType;
	nmcd.dwDrawStage  = dwStage;
	nmcd.hdc		  = hdc;

	hwnd = GetParent(hwnd);
	return CustomDrawScrollBars(&nmcd);
#else
	return 0;
#endif
}

static LRESULT PostCustomDrawNotify(HWND hwnd, HDC hdc, UINT nBar, RECT *prect, UINT nItem, BOOL fMouseDown, BOOL fMouseOver, BOOL fInactive)
{
#ifdef CUSTOM_DRAW
	NMCSBCUSTOMDRAW	nmcd;

    //fill in the standard header
	nmcd.hdr.hwndFrom = hwnd;
	nmcd.hdr.idFrom   = GetWindowLongPtr(hwnd, GWLP_ID);
	nmcd.hdr.code     = NM_COOLSB_CUSTOMDRAW;

	nmcd.dwDrawStage  = CDDS_ITEMPREPAINT;
	nmcd.nBar		  = nBar;
	nmcd.rect		  = *prect;
	nmcd.uItem		  = nItem;
	nmcd.hdc		  = hdc;

	if (fMouseDown) 
		nmcd.uState		  = CDIS_SELECTED;
	else if (fMouseOver)
		nmcd.uState		  = CDIS_HOT;
	else if (fInactive)
		nmcd.uState		  = CDIS_DISABLED;
	else
		nmcd.uState		  = CDIS_DEFAULT;

	hwnd = GetParent(hwnd);
	return CustomDrawScrollBars(&nmcd);
#else
	return 0;
#endif
}

// Depending on if we are supporting custom draw, either define
// a macro to the function name, or to nothing at all. If custom draw
// is turned off, then we can save ALOT of code space by binning all 
// calls to the custom draw support.

/*
#ifdef CUSTOM_DRAW
#define PostCustomDrawNotify	PostCustomDrawNotify0
#define PostCustomPrePostPaint	PostCustomPrePostPaint0
#else
#define PostCustomDrawNotify	1 ? (void)0 : PostCustomDrawNotify0
#define PostCustomPrePostPaint	1 ? (void)0 : PostCustomPrePostPaint0
#endif
*/

static LRESULT PostMouseNotify0(HWND hwnd, UINT nBar, RECT *prect, UINT nCmdId, POINT pt)
{
#ifdef NOTIFY_MOUSE
	NMCOOLBUTMSG	nmcb;

	//fill in the standard header
	nmcb.hdr.hwndFrom	= hwnd;
	nmcb.hdr.idFrom		= GetWindowLongPtr(hwnd, GWLP_ID);
	nmcb.hdr.code		= NM_CLICK;

	nmcb.nBar			= nBar;
	nmcb.uCmdId			= nCmdId;
	nmcb.uState			= 0;
	nmcb.rect			= *prect;
	nmcb.pt				= pt;

	hwnd = GetParent(hwnd);
	return SendMessage(hwnd, WM_NOTIFY, nmcb.hdr.idFrom, (LPARAM)&nmcb);
#else
	return 0;
#endif
}

#ifdef NOTIFY_MOUSE
#define PostMouseNotify			PostMouseNotify0
#else
#define PostMouseNotify			1 ? (void)0 : PostMouseNotify0
#endif



//
//	Draw a complete HORIZONTAL scrollbar in the given rectangle
//	Don't draw any inserted buttons in this procedure
//	
//	uDrawFlags - hittest code, to say if to draw the
//  specified portion in an active state or not.
//
//
static LRESULT NCDrawHScrollbar(SCROLLBAR *sb, HWND hwnd, HDC hdc, const RECT *rect, UINT uDrawFlags)
{
	SCROLLINFO *si;
	RECT ctrl, thumb;
	RECT sbm;
	int butwidth	 = GetScrollMetric(sb, SM_SCROLL_LENGTH);
	int scrollwidth  = rect->right-rect->left;
	int workingwidth = scrollwidth - butwidth*2;
	int thumbwidth   = 0, thumbpos = 0;
	int siMaxMin;

	BOOL fCustomDraw = 0;

	BOOL fMouseDownL = 0, fMouseOverL = 0, fBarHot = 0;
	BOOL fMouseDownR = 0, fMouseOverR = 0;

	COLORREF crCheck1   = GetSBForeColor();
	COLORREF crCheck2   = GetSBBackColor();
	COLORREF crInverse1 = InvertCOLORREF(crCheck1);
	COLORREF crInverse2 = InvertCOLORREF(crCheck2);

	UINT uDEFlat  = sb->fFlatScrollbar ? BF_FLAT   : 0;

	//drawing flags to modify the appearance of the scrollbar buttons
	UINT uLeftButFlags  = DFCS_SCROLLLEFT;
	UINT uRightButFlags = DFCS_SCROLLRIGHT;

	if (scrollwidth <= 0)
		return 0;

	si = &sb->scrollInfo;
	siMaxMin = si->nMax - si->nMin;

	if (hwnd != hwndCurCoolSB)
		uDrawFlags = HTSCROLL_NONE;
	//
	// work out the thumb size and position
	//
	CalcThumbSize(sb, rect, &thumbwidth, &thumbpos);
	
	if (sb->fScrollFlags & ESB_DISABLE_LEFT)		uLeftButFlags  |= DFCS_INACTIVE;
	if (sb->fScrollFlags & ESB_DISABLE_RIGHT)	uRightButFlags |= DFCS_INACTIVE;

	//if we need to grey the arrows because there is no data to scroll
	if ( !IsScrollInfoActive(si) && !(sb->fScrollFlags & CSBS_THUMBALWAYS))
	{
		uLeftButFlags  |= DFCS_INACTIVE;
		uRightButFlags |= DFCS_INACTIVE;
	}

	if (hwnd == hwndCurCoolSB)
	{
#ifdef FLAT_SCROLLBARS	
		BOOL ldis = !(uLeftButFlags & DFCS_INACTIVE);
		BOOL rdis = !(uRightButFlags & DFCS_INACTIVE);

		fBarHot = (sb->nBarType == (int)uMouseOverScrollbar && sb->fFlatScrollbar == CSBS_HOTTRACKED);
		
		fMouseOverL = uHitTestPortion == HTSCROLL_LEFT && fBarHot && ldis;		
		fMouseOverR = uHitTestPortion == HTSCROLL_RIGHT && fBarHot && rdis;
#endif
		fMouseDownL = (uDrawFlags == HTSCROLL_LEFT);
		fMouseDownR = (uDrawFlags == HTSCROLL_RIGHT);
	}


//#ifdef CUSTOM_DRAW
	fCustomDraw = ((PostCustomPrePostPaint(hwnd, hdc, sb, CDDS_PREPAINT)) == CDRF_SKIPDEFAULT);
//#endif

	//
	// Draw the scrollbar now
	//
	if (scrollwidth > butwidth*2)
	{
		//LEFT ARROW
		SetRect(&ctrl, rect->left, rect->top, rect->left + butwidth, rect->bottom);

		RotateRect0(sb, &ctrl);

		if (fCustomDraw)
			PostCustomDrawNotify(hwnd, hdc, sb->nBarType, &ctrl, SB_LINELEFT, fMouseDownL, fMouseOverL, uLeftButFlags & DFCS_INACTIVE);
		else
			DrawScrollArrow(sb, hdc, &ctrl, uLeftButFlags, fMouseDownL, fMouseOverL);

		RotateRect0(sb, &ctrl);

		//MIDDLE PORTION
		//if we can fit the thumbbar in, then draw it
		if (thumbwidth > 0 && thumbwidth <= workingwidth
			&& IsScrollInfoActive(si) && ((sb->fScrollFlags & ESB_DISABLE_BOTH) != ESB_DISABLE_BOTH))
		{	
			//Draw the scrollbar margin above the thumb
			SetRect(&sbm, rect->left + butwidth, rect->top, thumbpos, rect->bottom);
			
			RotateRect0(sb, &sbm);
			
			if (fCustomDraw)
			{
				PostCustomDrawNotify(hwnd, hdc, sb->nBarType, &sbm, SB_PAGELEFT, uDrawFlags == HTSCROLL_PAGELEFT, FALSE, FALSE);
			}
			else
			{
				if (uDrawFlags == HTSCROLL_PAGELEFT)
					DrawCheckedRect(hdc, &sbm, crInverse1, crInverse2);
				else
					DrawCheckedRect(hdc, &sbm, crCheck1, crCheck2);

			}

			RotateRect0(sb, &sbm);			
			
			//Draw the margin below the thumb
			sbm.left = thumbpos+thumbwidth;
			sbm.right = rect->right - butwidth;
			
			RotateRect0(sb, &sbm);
			if (fCustomDraw)
			{
				PostCustomDrawNotify(hwnd, hdc, sb->nBarType, &sbm, SB_PAGERIGHT, uDrawFlags == HTSCROLL_PAGERIGHT, 0, 0);
			}
			else
			{
				if (uDrawFlags == HTSCROLL_PAGERIGHT)
					DrawCheckedRect(hdc, &sbm, crInverse1, crInverse2);
				else
					DrawCheckedRect(hdc, &sbm, crCheck1, crCheck2);
			
			}
			RotateRect0(sb, &sbm);
			
			//Draw the THUMB finally
			SetRect(&thumb, thumbpos, rect->top, thumbpos+thumbwidth, rect->bottom);

			RotateRect0(sb, &thumb);			

			if (fCustomDraw)
			{
				PostCustomDrawNotify(hwnd, hdc, sb->nBarType, &thumb, SB_THUMBTRACK, uDrawFlags==HTSCROLL_THUMB, uHitTestPortion == HTSCROLL_THUMB && fBarHot, FALSE);
			}
			else
			{

#ifdef FLAT_SCROLLBARS	
				if (hwnd == hwndCurCoolSB && sb->fFlatScrollbar && (uDrawFlags == HTSCROLL_THUMB || 
				(uHitTestPortion == HTSCROLL_THUMB && fBarHot)))
				{	
					PaintRect(hdc, &thumb, GetSysColor(COLOR_3DSHADOW));
				}
				else
#endif
				{
					DrawBlankButton(hdc, &thumb, uDEFlat);
				}
			}
			RotateRect0(sb, &thumb);

		}
		//otherwise, just leave that whole area blank
		else
		{
			OffsetRect(&ctrl, butwidth, 0);
			ctrl.right = rect->right - butwidth;

			//if we always show the thumb covering the whole scrollbar,
			//then draw it that way
			if ( !IsScrollInfoActive(si)	&& (sb->fScrollFlags & CSBS_THUMBALWAYS) 
				&& ctrl.right - ctrl.left > sb->nMinThumbSize)
			{
				//leave a 1-pixel gap between the thumb + right button
				ctrl.right --;
				RotateRect0(sb, &ctrl);

				if (fCustomDraw)
					PostCustomDrawNotify(hwnd, hdc, sb->nBarType, &ctrl, SB_THUMBTRACK, fMouseDownL, FALSE, FALSE);
				else
				{
#ifdef FLAT_SCROLLBARS	
					if (sb->fFlatScrollbar == CSBS_HOTTRACKED && uDrawFlags == HTSCROLL_THUMB)
						PaintRect(hdc, &ctrl, GetSysColor(COLOR_3DSHADOW));
					else
#endif
						DrawBlankButton(hdc, &ctrl, uDEFlat);

				}
				RotateRect0(sb, &ctrl);

				//draw the single-line gap
				ctrl.left = ctrl.right;
				ctrl.right += 1;
				
				RotateRect0(sb, &ctrl);
				
				if (fCustomDraw)
					PostCustomDrawNotify(hwnd, hdc, sb->nBarType, &ctrl, SB_PAGERIGHT, 0, 0, 0);
				else
					PaintRect(hdc, &ctrl, GetSysColor(COLOR_SCROLLBAR));

				RotateRect0(sb, &ctrl);
			}
			//otherwise, paint a blank if the thumb doesn't fit in
			else
			{
				RotateRect0(sb, &ctrl);
	
				if (fCustomDraw)
					PostCustomDrawNotify(hwnd, hdc, sb->nBarType, &ctrl, SB_PAGERIGHT, 0, 0, 0);
				else
					DrawCheckedRect(hdc, &ctrl, crCheck1, crCheck2);
				
				RotateRect0(sb, &ctrl);
			}
		}

		//RIGHT ARROW
		SetRect(&ctrl, rect->right - butwidth, rect->top, rect->right, rect->bottom);

		RotateRect0(sb, &ctrl);

		if (fCustomDraw)
			PostCustomDrawNotify(hwnd, hdc, sb->nBarType, &ctrl, SB_LINERIGHT, fMouseDownR, fMouseOverR, uRightButFlags & DFCS_INACTIVE);
		else
			DrawScrollArrow(sb, hdc, &ctrl, uRightButFlags, fMouseDownR, fMouseOverR);

		RotateRect0(sb, &ctrl);
	}
	//not enough room for the scrollbar, so just draw the buttons (scaled in size to fit)
	else
	{
		butwidth = scrollwidth / 2;

		//LEFT ARROW
		SetRect(&ctrl, rect->left, rect->top, rect->left + butwidth, rect->bottom);

		RotateRect0(sb, &ctrl);
		if (fCustomDraw)
			PostCustomDrawNotify(hwnd, hdc, sb->nBarType, &ctrl, SB_LINELEFT, fMouseDownL, fMouseOverL, uLeftButFlags & DFCS_INACTIVE);
		else	
			DrawScrollArrow(sb, hdc, &ctrl, uLeftButFlags, fMouseDownL, fMouseOverL);
		RotateRect0(sb, &ctrl);

		//RIGHT ARROW
		OffsetRect(&ctrl, scrollwidth - butwidth, 0);
		
		RotateRect0(sb, &ctrl);
		if (fCustomDraw)
			PostCustomDrawNotify(hwnd, hdc, sb->nBarType, &ctrl, SB_LINERIGHT, fMouseDownR, fMouseOverR, uRightButFlags & DFCS_INACTIVE);
		else
			DrawScrollArrow(sb, hdc, &ctrl, uRightButFlags, fMouseDownR, fMouseOverR);		
		RotateRect0(sb, &ctrl);

		//if there is a gap between the buttons, fill it with a solid color
		//if (butwidth & 0x0001)
		if (ctrl.left != rect->left + butwidth)
		{
			ctrl.left --;
			ctrl.right -= butwidth;
			RotateRect0(sb, &ctrl);
			
			if (fCustomDraw)
				PostCustomDrawNotify(hwnd, hdc, sb->nBarType, &ctrl, SB_PAGERIGHT, 0, 0, 0);
			else
				DrawCheckedRect(hdc, &ctrl, crCheck1, crCheck2);

			RotateRect0(sb, &ctrl);
		}
			
	}

//#ifdef CUSTOM_DRAW
	PostCustomPrePostPaint(hwnd, hdc, sb, CDDS_POSTPAINT);
//#endif

	return fCustomDraw;
}

//
//	Draw a vertical scrollbar using the horizontal draw routine, but
//	with the coordinates adjusted accordingly
//
static LRESULT NCDrawVScrollbar(SCROLLBAR *sb, HWND hwnd, HDC hdc, const RECT *rect, UINT uDrawFlags)
{
	LRESULT ret;
	RECT rc;

	rc = *rect;
	RotateRect(&rc);
	ret = NCDrawHScrollbar(sb, hwnd, hdc, &rc, uDrawFlags);
	RotateRect(&rc);
	
	return ret;
}

//
//	Generic wrapper function for the scrollbar drawing
//
static LRESULT NCDrawScrollbar(SCROLLBAR *sb, HWND hwnd, HDC hdc, const RECT *rect, UINT uDrawFlags)
{
	if (sb->nBarType == SB_HORZ)
		return NCDrawHScrollbar(sb, hwnd, hdc, rect, uDrawFlags);
	else
		return NCDrawVScrollbar(sb, hwnd, hdc, rect, uDrawFlags);
}

//
//	Define these two for proper processing of NCPAINT
//	NOT needed if we don't bother to mask the scrollbars we draw
//	to prevent the old window procedure from accidently drawing over them
//
HDC CoolSB_GetDC(HWND hwnd, WPARAM)
{
	// I just can't figure out GetDCEx, so I'll just use this:
	return GetWindowDC(hwnd);
}

static LRESULT NCPaint(SCROLLWND *sw, HWND hwnd, WPARAM wParam, LPARAM lParam)
{
	SCROLLBAR *sb;
	HDC hdc;
	RECT winrect, rect;
	HRGN clip = nullptr;
	BOOL fCustomDraw = FALSE;
	LRESULT ret;
	DWORD dwStyle;

	GetWindowRect(hwnd, &winrect);
	
	HRGN hrgn = (HRGN)wParam;
	
	//hdc = GetWindowDC(hwnd);
	hdc = CoolSB_GetDC(hwnd, wParam);

	//
	//	Only draw the horizontal scrollbar if the window is tall enough
	//
	sb = &sw->sbarHorz;
	if (sb->fScrollVisible)
	{
		//get the screen coordinates of the whole horizontal scrollbar area
		GetHScrollRect(sw, hwnd, &rect);

		//make the coordinates relative to the window for drawing
		OffsetRect(&rect, -winrect.left, -winrect.top);

		if (uCurrentScrollbar == SB_HORZ)
			fCustomDraw |= NCDrawHScrollbar(sb, hwnd, hdc, &rect, uScrollTimerPortion);
		else
			fCustomDraw |= NCDrawHScrollbar(sb, hwnd, hdc, &rect, HTSCROLL_NONE);
	}

	//
	// Only draw the vertical scrollbar if the window is wide enough to accomodate it
	//
	sb = &sw->sbarVert;
	if (sb->fScrollVisible)
	{
		//get the screen cooridinates of the whole horizontal scrollbar area
		GetVScrollRect(sw, hwnd, &rect);

		//make the coordinates relative to the window for drawing
		OffsetRect(&rect, -winrect.left, -winrect.top);

		if (uCurrentScrollbar == SB_VERT)
			fCustomDraw |= NCDrawVScrollbar(sb, hwnd, hdc, &rect, uScrollTimerPortion);
		else
			fCustomDraw |= NCDrawVScrollbar(sb, hwnd, hdc, &rect, HTSCROLL_NONE);
	}

	//Call the default window procedure for WM_NCPAINT, with the
	//new window region. ** region must be in SCREEN coordinates **
	dwStyle = GetWindowLongPtr(hwnd, GWL_STYLE);

	// If the window has WS_(H-V)SCROLL bits set, we should reset them
	// to avoid windows taking the scrollbars into account.
	// We temporarily set a flag preventing the subsecuent 
	// WM_STYLECHANGING/WM_STYLECHANGED to be forwarded to 
	// the original window procedure
	if ( dwStyle & (WS_VSCROLL|WS_HSCROLL))
	{
		sw->bPreventStyleChange = TRUE;
		SetWindowLongPtr(hwnd, GWL_STYLE, dwStyle & ~(WS_VSCROLL|WS_HSCROLL));
	}

	ret = mir_callNextSubclass(hwnd, CoolSBWndProc, WM_NCPAINT, (WPARAM)hrgn, lParam);

	if ( dwStyle & (WS_VSCROLL|WS_HSCROLL))
	{
		SetWindowLongPtr(hwnd, GWL_STYLE, dwStyle);
		sw->bPreventStyleChange = FALSE;
	}

	// DRAW THE DEAD AREA
	// only do this if the horizontal and vertical bars are visible
	if (sw->sbarHorz.fScrollVisible && sw->sbarVert.fScrollVisible)
	{
		GetWindowRect(hwnd, &rect);
		OffsetRect(&rect, -winrect.left, -winrect.top);

		rect.bottom -= sw->cyBottomEdge;
		rect.top  = rect.bottom - GetScrollMetric(&sw->sbarHorz, SM_CYHORZSB);

		if (sw->fLeftScrollbar)
		{
			rect.left += sw->cxLeftEdge;
			rect.right = rect.left + GetScrollMetric(&sw->sbarVert, SM_CXVERTSB);
		}
		else
		{
			rect.right -= sw->cxRightEdge;
			rect.left = rect.right  - GetScrollMetric(&sw->sbarVert, SM_CXVERTSB);
		}

		if (fCustomDraw)
			PostCustomDrawNotify(hwnd, hdc, SB_BOTH, &rect, 32, 0, 0, 0);
		else
		{
			//calculate the position of THIS window's dead area
			//with the position of the PARENT window's client rectangle.
			//if THIS window has been positioned such that its bottom-right
			//corner sits in the parent's bottom-right corner, then we should
			//show the sizing-grip.
			//Otherwise, assume this window is not in the right place, and
			//just draw a blank rectangle
			RECT parent;
			RECT rect2;
			HWND hwndParent = GetParent(hwnd);

			GetClientRect(hwndParent, &parent);
			MapWindowPoints(hwndParent, nullptr, (POINT *)&parent, 2);

			CopyRect(&rect2, &rect);
			OffsetRect(&rect2, winrect.left, winrect.top);

			if ( !sw->fLeftScrollbar && parent.right == rect2.right+sw->cxRightEdge && parent.bottom == rect2.bottom+sw->cyBottomEdge
				|| sw->fLeftScrollbar && parent.left  == rect2.left -sw->cxLeftEdge  && parent.bottom == rect2.bottom+sw->cyBottomEdge)
				DrawFrameControl(hdc, &rect, DFC_SCROLL, sw->fLeftScrollbar ? DFCS_SCROLLSIZEGRIPRIGHT : DFCS_SCROLLSIZEGRIP );
			else
				PaintRect(hdc, &rect, GetSysColor(COLOR_3DFACE));
		}
	}

	UNREFERENCED_PARAMETER(clip);
	ReleaseDC(hwnd, hdc);
	return ret;
}

//
// Need to detect if we have clicked in the scrollbar region or not
//
static LRESULT NCHitTest(SCROLLWND *sw, HWND hwnd, WPARAM wParam, LPARAM lParam)
{
	RECT hrect;
	RECT vrect;
	POINT pt;

	pt.x = LOWORD(lParam);
	pt.y = HIWORD(lParam);
	
	//work out exactly where the Horizontal and Vertical scrollbars are
	GetHScrollRect(sw, hwnd, &hrect);
	GetVScrollRect(sw, hwnd, &vrect);
	
	//Clicked in the horizontal scrollbar area
	if (sw->sbarHorz.fScrollVisible && PtInRect(&hrect, pt))
		return HTHSCROLL;

	//Clicked in the vertical scrollbar area
	if (sw->sbarVert.fScrollVisible && PtInRect(&vrect, pt))
		return HTVSCROLL;

	//clicked somewhere else
	return mir_callNextSubclass(hwnd, CoolSBWndProc, WM_NCHITTEST, wParam, lParam);
}

//
//	Return a HT* value indicating what part of the scrollbar was clicked
//	Rectangle is not adjusted
//
static UINT GetHorzPortion(SCROLLBAR *sb, RECT *rect, int x, int y)
{
	RECT rc = *rect;

	if (y < rc.top || y >= rc.bottom)
		return HTSCROLL_NONE;

	// Now we have the rectangle for the scrollbar itself, so work out
	// what part we clicked on.
	return GetHorzScrollPortion(sb, &rc, x, y);
}

//
//	Just call the horizontal version, with adjusted coordinates
//
static UINT GetVertPortion(SCROLLBAR *sb, RECT *rect, int x, int y)
{
	UINT ret;
	RotateRect(rect);
	ret = GetHorzPortion(sb, rect, y, x);
	RotateRect(rect);
	return ret;
}

//
//	Wrapper function for GetHorzPortion and GetVertPortion
//
static UINT GetPortion(SCROLLBAR *sb, RECT *rect, int x, int y)
{
	if (sb->nBarType == SB_HORZ)
		return GetHorzPortion(sb, rect, x, y);
	else if (sb->nBarType == SB_VERT)
		return GetVertPortion(sb, rect, x, y);
	else
		return HTSCROLL_NONE;
}

//
//	Input: rectangle of the total scrollbar area
//	Output: adjusted to take the inserted buttons into account
//
static void GetRealHorzScrollRect(SCROLLBAR *sb, RECT *rect)
{
	if (sb->fButVisibleBefore) rect->left += sb->nButSizeBefore;
	if (sb->fButVisibleAfter)  rect->right -= sb->nButSizeAfter;
}

//
//	Input: rectangle of the total scrollbar area
//	Output: adjusted to take the inserted buttons into account
//
static void GetRealVertScrollRect(SCROLLBAR *sb, RECT *rect)
{
	if (sb->fButVisibleBefore) rect->top += sb->nButSizeBefore;
	if (sb->fButVisibleAfter)  rect->bottom -= sb->nButSizeAfter;
}

//
//	Decide which type of scrollbar we have before calling
//  the real function to do the job
//
static void GetRealScrollRect(SCROLLBAR *sb, RECT *rect)
{
	if (sb->nBarType == SB_HORZ)
	{
		GetRealHorzScrollRect(sb, rect);
	}
	else if (sb->nBarType == SB_VERT)
	{
		GetRealVertScrollRect(sb, rect);
	}
}

//
//	Left button click in the non-client area
//
static LRESULT NCLButtonDown(SCROLLWND *sw, HWND hwnd, WPARAM wParam, LPARAM lParam)
{
	RECT rect, winrect;
	HDC hdc;
	SCROLLBAR *sb;
	POINT pt;

	pt.x = LOWORD(lParam);
	pt.y = HIWORD(lParam);

	hwndCurCoolSB = hwnd;

	//
	//	HORIZONTAL SCROLLBAR PROCESSING
	//
	if (wParam == HTHSCROLL)
	{
		uScrollTimerMsg = WM_HSCROLL;
		uCurrentScrollbar = SB_HORZ;
		sb = &sw->sbarHorz;

		//get the total area of the normal Horz scrollbar area
		GetHScrollRect(sw, hwnd, &rect);
		uCurrentScrollPortion = GetHorzPortion(sb, &rect, LOWORD(lParam), HIWORD(lParam));
	}
	//
	//	VERTICAL SCROLLBAR PROCESSING
	//
	else if (wParam == HTVSCROLL)
	{
		uScrollTimerMsg = WM_VSCROLL;
		uCurrentScrollbar = SB_VERT;
		sb = &sw->sbarVert;

		//get the total area of the normal Horz scrollbar area
		GetVScrollRect(sw, hwnd, &rect);
		uCurrentScrollPortion = GetVertPortion(sb, &rect, LOWORD(lParam), HIWORD(lParam));
	}
	//
	//	NORMAL PROCESSING
	//
	else
	{
		uCurrentScrollPortion = HTSCROLL_NONE;
		return mir_callNextSubclass(hwnd, CoolSBWndProc, WM_NCLBUTTONDOWN, wParam, lParam);
	}

	//
	// we can now share the same code for vertical
	// and horizontal scrollbars
	//
	switch(uCurrentScrollPortion) {
	case HTSCROLL_THUMB: 

		//if the scrollbar is disabled, then do no further processing
		if ( !IsScrollbarActive(sb))
			return 0;
		
		GetRealScrollRect(sb, &rect);
		RotateRect0(sb, &rect);
		CalcThumbSize(sb, &rect, &nThumbSize, &nThumbPos);
		RotateRect0(sb, &rect);
		
		//remember the bounding rectangle of the scrollbar work area
		rcThumbBounds = rect;
		
		sw->fThumbTracking = TRUE;
		sb->scrollInfo.nTrackPos = sb->scrollInfo.nPos;
		
		if (wParam == HTVSCROLL) 
			nThumbMouseOffset = pt.y - nThumbPos;
		else
			nThumbMouseOffset = pt.x - nThumbPos;

		nLastPos = -sb->scrollInfo.nPos;
		nThumbPos0 = nThumbPos;
	
		//if (sb->fFlatScrollbar)
		//{
			GetWindowRect(hwnd, &winrect);
			OffsetRect(&rect, -winrect.left, -winrect.top);
			hdc = GetWindowDC(hwnd);
			NCDrawScrollbar(sb, hwnd, hdc, &rect, HTSCROLL_THUMB);
			ReleaseDC(hwnd, hdc);
		//}

		break;

		//Any part of the scrollbar
	case HTSCROLL_LEFT:  
		if (sb->fScrollFlags & ESB_DISABLE_LEFT)		return 0;
		else										goto target1;
	
	case HTSCROLL_RIGHT: 
		if (sb->fScrollFlags & ESB_DISABLE_RIGHT)	return 0;
		else										goto target1;

		goto target1;	

	case HTSCROLL_PAGELEFT:  case HTSCROLL_PAGERIGHT:

		target1:

		//if the scrollbar is disabled, then do no further processing
		if ( !IsScrollbarActive(sb))
			break;

		//ajust the horizontal rectangle to NOT include
		//any inserted buttons
		GetRealScrollRect(sb, &rect);

		SendScrollMessage(hwnd, uScrollTimerMsg, uCurrentScrollPortion, 0);

		// Check what area the mouse is now over :
		// If the scroll thumb has moved under the mouse in response to 
		// a call to SetScrollPos etc, then we don't hilight the scrollbar margin
		if (uCurrentScrollbar == SB_HORZ)
			uScrollTimerPortion = GetHorzScrollPortion(sb, &rect, pt.x, pt.y);
		else
			uScrollTimerPortion = GetVertScrollPortion(sb, &rect, pt.x, pt.y);

		GetWindowRect(hwnd, &winrect);
		OffsetRect(&rect, -winrect.left, -winrect.top);
		hdc = GetWindowDC(hwnd);
			
#ifndef HOT_TRACKING
		//if we aren't hot-tracking, then don't highlight 
		//the scrollbar thumb unless we click on it
		if (uScrollTimerPortion == HTSCROLL_THUMB)
			uScrollTimerPortion = HTSCROLL_NONE;
#endif
		NCDrawScrollbar(sb, hwnd, hdc, &rect, uScrollTimerPortion);
		ReleaseDC(hwnd, hdc);

		//Post the scroll message!!!!
		uScrollTimerPortion = uCurrentScrollPortion;

		//set a timer going on the first click.
		//if this one expires, then we can start off a more regular timer
		//to generate the auto-scroll behaviour
		uScrollTimerId = SetTimer(hwnd, COOLSB_TIMERID1, COOLSB_TIMERINTERVAL1, nullptr);
		break;
	default:
		return mir_callNextSubclass(hwnd, CoolSBWndProc, WM_NCLBUTTONDOWN, wParam, lParam);
		//return 0;
	}
		
	SetCapture(hwnd);
	return 0;
}

//
//	Left button released
//
static LRESULT LButtonUp(SCROLLWND *sw, HWND hwnd, WPARAM wParam, LPARAM lParam)
{
	RECT rect;
	//UINT thisportion;
	HDC hdc;
	POINT pt;
	RECT winrect;
	
	//current scrollportion is the button that we clicked down on
	if (uCurrentScrollPortion != HTSCROLL_NONE)
	{
		SCROLLBAR *sb = &sw->sbarHorz;
		lParam = GetMessagePos();
		ReleaseCapture();

		GetWindowRect(hwnd, &winrect);
		pt.x = LOWORD(lParam);
		pt.y = HIWORD(lParam);

		//emulate the mouse input on a scrollbar here...
		if (uCurrentScrollbar == SB_HORZ)
		{
			//get the total area of the normal Horz scrollbar area
			sb = &sw->sbarHorz;
			GetHScrollRect(sw, hwnd, &rect);
		}
		else if (uCurrentScrollbar == SB_VERT)
		{
			//get the total area of the normal Horz scrollbar area
			sb = &sw->sbarVert;
			GetVScrollRect(sw, hwnd, &rect);
		}

		//we need to do different things depending on if the
		//user is activating the scrollbar itself, or one of
		//the inserted buttons
		switch(uCurrentScrollPortion) {
		//The scrollbar is active
		case HTSCROLL_LEFT:  case HTSCROLL_RIGHT: 
		case HTSCROLL_PAGELEFT:  case HTSCROLL_PAGERIGHT: 
		case HTSCROLL_NONE:
			
			KillTimer(hwnd, uScrollTimerId);

		case HTSCROLL_THUMB: 
	
			//In case we were thumb tracking, make sure we stop NOW
			if (sw->fThumbTracking == TRUE)
			{
				SendScrollMessage(hwnd, uScrollTimerMsg, SB_THUMBPOSITION, nLastPos);
				sw->fThumbTracking = FALSE;
			}

			//send the SB_ENDSCROLL message now that scrolling has finished
			SendScrollMessage(hwnd, uScrollTimerMsg, SB_ENDSCROLL, 0);

			//adjust the total scroll area to become where the scrollbar
			//really is (take into account the inserted buttons)
			GetRealScrollRect(sb, &rect);
			OffsetRect(&rect, -winrect.left, -winrect.top);
			hdc = GetWindowDC(hwnd);
			
			//draw whichever scrollbar sb is
			NCDrawScrollbar(sb, hwnd, hdc, &rect, HTSCROLL_NORMAL);

			ReleaseDC(hwnd, hdc);
			break;
		}

		//reset our state to default
		uCurrentScrollPortion = HTSCROLL_NONE;
		uScrollTimerPortion	  = HTSCROLL_NONE;
		uScrollTimerId		  = 0;

		uScrollTimerMsg       = 0;
		uCurrentScrollbar     = COOLSB_NONE;

		return 0;
	}
	else
	{
		/*
		// Can't remember why I did this!
		if (GetCapture() == hwnd)
		{
			ReleaseCapture();
		}*/
	}

	return mir_callNextSubclass(hwnd, CoolSBWndProc, WM_LBUTTONUP, wParam, lParam);
}

//
//	This function is called whenever the mouse is moved and 
//  we are dragging the scrollbar thumb about.
//
static LRESULT ThumbTrackHorz(SCROLLBAR *sbar, HWND hwnd, int x, int y)
{
	POINT pt;
	RECT rc, winrect, rc2;
	COLORREF crCheck1 = GetSBForeColor();
	COLORREF crCheck2 = GetSBBackColor();
	HDC hdc;
	int thumbpos = nThumbPos;
	int pos;
	int siMaxMin = 0;
	UINT flatflag = sbar->fFlatScrollbar ? BF_FLAT : 0;
	BOOL fCustomDraw = FALSE;

	SCROLLINFO *si;
	si = &sbar->scrollInfo;

	pt.x = x;
	pt.y = y;

	//draw the thumb at whatever position
	rc = rcThumbBounds;

	SetRect(&rc2, rc.left -  THUMBTRACK_SNAPDIST*2, rc.top -    THUMBTRACK_SNAPDIST, 
				  rc.right + THUMBTRACK_SNAPDIST*2, rc.bottom + THUMBTRACK_SNAPDIST);

	rc.left +=  GetScrollMetric(sbar, SM_CXHORZSB);
	rc.right -= GetScrollMetric(sbar, SM_CXHORZSB);

	//if the mouse is not in a suitable distance of the scrollbar,
	//then "snap" the thumb back to its initial position
#ifdef SNAP_THUMB_BACK
	if ( !PtInRect(&rc2, pt))
	{
		thumbpos = nThumbPos0;
	}
	//otherwise, move the thumb to where the mouse is
	else
#endif //SNAP_THUMB_BACK
	{
		//keep the thumb within the scrollbar limits
		thumbpos = pt.x - nThumbMouseOffset;
		if (thumbpos < rc.left) thumbpos = rc.left;
		if (thumbpos > rc.right - nThumbSize) thumbpos = rc.right - nThumbSize;
	}

	GetWindowRect(hwnd, &winrect);

	if (sbar->nBarType == SB_VERT)
		RotateRect(&winrect);
	
	hdc = GetWindowDC(hwnd);
		
//#ifdef CUSTOM_DRAW
	fCustomDraw = PostCustomPrePostPaint(hwnd, hdc, sbar, CDDS_PREPAINT) == CDRF_SKIPDEFAULT;
//#endif

	OffsetRect(&rc, -winrect.left, -winrect.top);
	thumbpos -= winrect.left;

	//draw the margin before the thumb
	SetRect(&rc2, rc.left, rc.top, thumbpos, rc.bottom);
	RotateRect0(sbar, &rc2);

	if (fCustomDraw)
		PostCustomDrawNotify(hwnd, hdc, sbar->nBarType, &rc2, SB_PAGELEFT, 0, 0, 0);
	else
		DrawCheckedRect(hdc, &rc2, crCheck1, crCheck2);
	
	RotateRect0(sbar, &rc2);

	//draw the margin after the thumb 
	SetRect(&rc2, thumbpos+nThumbSize, rc.top, rc.right, rc.bottom);
	
	RotateRect0(sbar, &rc2);
	
	if (fCustomDraw)
		PostCustomDrawNotify(hwnd, hdc, sbar->nBarType, &rc2, SB_PAGERIGHT, 0, 0, 0);
	else
		DrawCheckedRect(hdc, &rc2, crCheck1, crCheck2);
	
	RotateRect0(sbar, &rc2);
	
	//finally draw the thumb itelf. This is how it looks on win2000, anyway
	SetRect(&rc2, thumbpos, rc.top, thumbpos+nThumbSize, rc.bottom);
	
	RotateRect0(sbar, &rc2);

	if (fCustomDraw)
		PostCustomDrawNotify(hwnd, hdc, sbar->nBarType, &rc2, SB_THUMBTRACK, TRUE, TRUE, FALSE);
	else
	{

#ifdef FLAT_SCROLLBARS	
		if (sbar->fFlatScrollbar)
			PaintRect(hdc, &rc2, GetSysColor(COLOR_3DSHADOW));
		else
#endif
		{
				DrawBlankButton(hdc, &rc2, flatflag);
		}
	}

	RotateRect0(sbar, &rc2);
	ReleaseDC(hwnd, hdc);

	//post a SB_TRACKPOS message!!!
	siMaxMin = si->nMax - si->nMin;

	if (siMaxMin > 0)
		pos = MulDiv(thumbpos-rc.left, siMaxMin-si->nPage + 1, rc.right-rc.left-nThumbSize);
	else
		pos = thumbpos - rc.left;

	if (pos != nLastPos)
	{
		si->nTrackPos = pos;	
		SendScrollMessage(hwnd, uScrollTimerMsg, SB_THUMBTRACK, pos);
	}

	nLastPos = pos;

//#ifdef CUSTOM_DRAW
	PostCustomPrePostPaint(hwnd, hdc, sbar, CDDS_POSTPAINT);
//#endif
	
	return 0;
}

//
//	remember to rotate the thumb bounds rectangle!!
//
static LRESULT ThumbTrackVert(SCROLLBAR *sb, HWND hwnd, int x, int y)
{
	//sw->swapcoords = TRUE;
	RotateRect(&rcThumbBounds);
	ThumbTrackHorz(sb, hwnd, y, x);
	RotateRect(&rcThumbBounds);
	//sw->swapcoords = FALSE;

	return 0;
}

//
//	Called when we have set the capture from the NCLButtonDown(...)
//	
static LRESULT MouseMove(SCROLLWND *sw, HWND hwnd, WPARAM wParam, LPARAM lParam)
{
	RECT rect;
	UINT thisportion;
	HDC hdc;
	static UINT lastportion = 0;
	static UINT lastbutton = 0;
	POINT pt;
	RECT winrect;
	UINT buttonIdx = 0;

	if (sw->fThumbTracking == TRUE)
	{
		int x, y;
		lParam = GetMessagePos();
		x = LOWORD(lParam);
		y = HIWORD(lParam);

		if (uCurrentScrollbar == SB_HORZ)
			return ThumbTrackHorz(&sw->sbarHorz, hwnd, x,y);

		else if (uCurrentScrollbar == SB_VERT)
			return ThumbTrackVert(&sw->sbarVert, hwnd, x,y);
	}

	if (uCurrentScrollPortion == HTSCROLL_NONE)
		return mir_callNextSubclass(hwnd, CoolSBWndProc, WM_MOUSEMOVE, wParam, lParam);

	LPARAM nlParam;
	SCROLLBAR *sb = &sw->sbarHorz;

	nlParam = GetMessagePos();

	GetWindowRect(hwnd, &winrect);

	pt.x = LOWORD(nlParam);
	pt.y = HIWORD(nlParam);

	//emulate the mouse input on a scrollbar here...
	if (uCurrentScrollbar == SB_HORZ)
	{
		sb = &sw->sbarHorz;
	}
	else if (uCurrentScrollbar == SB_VERT)
	{
		sb = &sw->sbarVert;
	}

	//get the total area of the normal scrollbar area
	GetScrollRect(sw, sb->nBarType, hwnd, &rect);
		
	//see if we clicked in the inserted buttons / normal scrollbar
	//thisportion = GetPortion(sb, hwnd, &rect, LOWORD(lParam), HIWORD(lParam));
	thisportion = GetPortion(sb, &rect, pt.x, pt.y);
		
	//we need to do different things depending on if the
	//user is activating the scrollbar itself, or one of
	//the inserted buttons
	switch(uCurrentScrollPortion) {

	//The scrollbar is active
	case HTSCROLL_LEFT:		 case HTSCROLL_RIGHT:case HTSCROLL_THUMB: 
	case HTSCROLL_PAGELEFT:  case HTSCROLL_PAGERIGHT: 
	case HTSCROLL_NONE:
		//adjust the total scroll area to become where the scrollbar
		//really is (take into account the inserted buttons)
		GetRealScrollRect(sb, &rect);

		OffsetRect(&rect, -winrect.left, -winrect.top);
		hdc = GetWindowDC(hwnd);
		
		if (thisportion != uCurrentScrollPortion)
		{
			uScrollTimerPortion = HTSCROLL_NONE;

			if (lastportion != thisportion)
				NCDrawScrollbar(sb, hwnd, hdc, &rect, HTSCROLL_NORMAL);
		}
		//otherwise, draw the button in its depressed / clicked state
		else
		{
			uScrollTimerPortion = uCurrentScrollPortion;

			if (lastportion != thisportion)
				NCDrawScrollbar(sb, hwnd, hdc, &rect, thisportion);
		}

		ReleaseDC(hwnd, hdc);
		break;
	}

	lastportion = thisportion;
	lastbutton  = buttonIdx;
	return 0;
}

//
//	We must allocate from in the non-client area for our scrollbars
//	Call the default window procedure first, to get the borders (if any)
//	allocated some space, then allocate the space for the scrollbars
//	if they fit
//
static LRESULT NCCalcSize(SCROLLWND *sw, HWND hwnd, WPARAM wParam, LPARAM lParam)
{
	NCCALCSIZE_PARAMS *nccsp;
	RECT *rect;
	RECT oldrect;
	SCROLLBAR *sb;
	LRESULT ret;
	DWORD dwStyle;

	//Regardless of the value of fCalcValidRects, the first rectangle 
	//in the array specified by the rgrc structure member of the 
	//NCCALCSIZE_PARAMS structure contains the coordinates of the window,
	//so we can use the exact same code to modify this rectangle, when
	//wParam is TRUE and when it is FALSE.
	nccsp = (NCCALCSIZE_PARAMS *)lParam;
	rect = &nccsp->rgrc[0];
	oldrect = *rect;

	dwStyle = GetWindowLongPtr(hwnd, GWL_STYLE);

	// TURN OFF SCROLL-STYLES.
    if ( dwStyle & (WS_VSCROLL|WS_HSCROLL))
    {
        sw->bPreventStyleChange = TRUE;
        SetWindowLongPtr(hwnd, GWL_STYLE, dwStyle & ~(WS_VSCROLL|WS_HSCROLL));
    }
	
	//call the default procedure to get the borders allocated
	ret = mir_callNextSubclass(hwnd, CoolSBWndProc, WM_NCCALCSIZE, wParam, lParam);

	// RESTORE PREVIOUS STYLES (if present at all)
    if ( dwStyle & (WS_VSCROLL|WS_HSCROLL))
    {
        SetWindowLongPtr(hwnd, GWL_STYLE, dwStyle);
        sw->bPreventStyleChange = FALSE;
    }

	// calculate what the size of each window border is,
	sw->cxLeftEdge   = rect->left     - oldrect.left;
	sw->cxRightEdge  = oldrect.right  - rect->right;
	sw->cyTopEdge    = rect->top      - oldrect.top;
	sw->cyBottomEdge = oldrect.bottom - rect->bottom;

	sb = &sw->sbarHorz;

	//if there is room, allocate some space for the horizontal scrollbar
	//NOTE: Change the ">" to a ">=" to make the horz bar totally fill the
	//window before disappearing
	if ((sb->fScrollFlags & CSBS_VISIBLE) && rect->bottom - rect->top > GetScrollMetric(sb, SM_CYHORZSB))
	{
		rect->bottom -= GetScrollMetric(sb, SM_CYHORZSB);
		sb->fScrollVisible = TRUE;
	}
	else sb->fScrollVisible = FALSE;

	sb = &sw->sbarVert;

	//if there is room, allocate some space for the vertical scrollbar
	if ((sb->fScrollFlags & CSBS_VISIBLE) && 
		rect->right - rect->left >= GetScrollMetric(sb, SM_CXVERTSB))
	{
		if (sw->fLeftScrollbar)
			rect->left  += GetScrollMetric(sb, SM_CXVERTSB);
		else
			rect->right -= GetScrollMetric(sb, SM_CXVERTSB);

		sb->fScrollVisible = TRUE;
	}
	else sb->fScrollVisible = FALSE;

	//don't return a value unless we actually modify the other rectangles
	//in the NCCALCSIZE_PARAMS structure. In this case, we return 0
	//no matter what the value of fCalcValidRects is
	return ret;//FALSE;
}

//
//	used for hot-tracking over the scroll buttons
//
static LRESULT NCMouseMove(SCROLLWND *sw, HWND hwnd, WPARAM wHitTest, LPARAM lParam)
{
	//install a timer for the mouse-over events, if the mouse moves
	//over one of the scrollbars
#ifdef HOT_TRACKING
	hwndCurCoolSB = hwnd;
	if (wHitTest == HTHSCROLL)
	{
		if (uMouseOverScrollbar == SB_HORZ)
			return mir_callNextSubclass(hwnd, CoolSBWndProc, WM_NCMOUSEMOVE, wHitTest, lParam);

		uLastHitTestPortion = HTSCROLL_NONE;
		uHitTestPortion     = HTSCROLL_NONE;
		GetScrollRect(sw, SB_HORZ, hwnd, &MouseOverRect);
		uMouseOverScrollbar = SB_HORZ;
		uMouseOverId = SetTimer(hwnd, COOLSB_TIMERID3, COOLSB_TIMERINTERVAL3, nullptr);

		NCPaint(sw, hwnd, 1, 0);
	}
	else if (wHitTest == HTVSCROLL)
	{
		if (uMouseOverScrollbar == SB_VERT)
			return mir_callNextSubclass(hwnd, CoolSBWndProc, WM_NCMOUSEMOVE, wHitTest, lParam);

		uLastHitTestPortion = HTSCROLL_NONE;
		uHitTestPortion     = HTSCROLL_NONE;
		GetScrollRect(sw, SB_VERT, hwnd, &MouseOverRect);
		uMouseOverScrollbar = SB_VERT;
		uMouseOverId = SetTimer(hwnd, COOLSB_TIMERID3, COOLSB_TIMERINTERVAL3, nullptr);

		NCPaint(sw, hwnd, 1, 0);
	}

#endif //HOT_TRACKING
	return mir_callNextSubclass(hwnd, CoolSBWndProc, WM_NCMOUSEMOVE, wHitTest, lParam);
}

//
//	Timer routine to generate scrollbar messages
//
static LRESULT CoolSB_Timer(SCROLLWND *swnd, HWND hwnd, WPARAM wTimerId, LPARAM lParam)
{
	//let all timer messages go past if we don't have a timer installed ourselves
	if (uScrollTimerId == 0 && uMouseOverId == 0)
		return mir_callNextSubclass(hwnd, CoolSBWndProc, WM_TIMER, wTimerId, lParam);

#ifdef HOT_TRACKING
	//mouse-over timer
	if (wTimerId == COOLSB_TIMERID3)
	{
		POINT pt;
		RECT rect, winrect;
		HDC hdc;
		SCROLLBAR *sbar;

		if (swnd->fThumbTracking)
			return 0;

		//if the mouse moves outside the current scrollbar,
		//then kill the timer..
		GetCursorPos(&pt);

		if ( !PtInRect(&MouseOverRect, pt))
		{
			KillTimer(hwnd, uMouseOverId);
			uMouseOverId = 0;
			uMouseOverScrollbar = COOLSB_NONE;
			uLastHitTestPortion = HTSCROLL_NONE;

			uHitTestPortion = HTSCROLL_NONE;
			NCPaint(swnd, hwnd, 1, 0);
		}
		else
		{
			if (uMouseOverScrollbar == SB_HORZ)
			{
				sbar = &swnd->sbarHorz;
				uHitTestPortion = GetHorzPortion(sbar, &MouseOverRect, pt.x, pt.y);
			}
			else
			{
				sbar = &swnd->sbarVert;
				uHitTestPortion = GetVertPortion(sbar, &MouseOverRect, pt.x, pt.y);
			}

			if (uLastHitTestPortion != uHitTestPortion)
			{
				rect = MouseOverRect;
				GetRealScrollRect(sbar, &rect);

				GetWindowRect(hwnd, &winrect);
				OffsetRect(&rect, -winrect.left, -winrect.top);

				hdc = GetWindowDC(hwnd);
				NCDrawScrollbar(sbar, hwnd, hdc, &rect, HTSCROLL_NONE);
				ReleaseDC(hwnd, hdc);
			}
			
			uLastHitTestPortion = uHitTestPortion;
		}

		return 0;
	}
#endif // HOT_TRACKING

	//if the first timer goes off, then we can start a more
	//regular timer interval to auto-generate scroll messages
	//this gives a slight pause between first pressing the scroll arrow, and the
	//actual scroll starting
	if (wTimerId == COOLSB_TIMERID1)
	{
		KillTimer(hwnd, uScrollTimerId);
		uScrollTimerId = SetTimer(hwnd, COOLSB_TIMERID2, COOLSB_TIMERINTERVAL2, nullptr);
		return 0;
	}
	//send the scrollbar message repeatedly
	else if (wTimerId == COOLSB_TIMERID2)
	{
		//need to process a spoof WM_MOUSEMOVE, so that
		//we know where the mouse is each time the scroll timer goes off.
		//This is so we can stop sending scroll messages if the thumb moves
		//under the mouse.
		POINT pt;
		GetCursorPos(&pt);
		ScreenToClient(hwnd, &pt);
		
		MouseMove(swnd, hwnd, MK_LBUTTON, MAKELPARAM(pt.x, pt.y));

		if (uScrollTimerPortion != HTSCROLL_NONE)
			SendScrollMessage(hwnd, uScrollTimerMsg, uScrollTimerPortion, 0);
		
		return 0;
	}

	return mir_callNextSubclass(hwnd, CoolSBWndProc, WM_TIMER, wTimerId, lParam);
}

//
//	We must intercept any calls to SetWindowLongPtr, to check if
//  left-scrollbars are taking effect or not
//
static LRESULT CoolSB_StyleChange(SCROLLWND *swnd, HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	STYLESTRUCT *ss = (STYLESTRUCT *)lParam;

	if (wParam == GWL_EXSTYLE)
	{
		if (ss->styleNew & WS_EX_LEFTSCROLLBAR)
			swnd->fLeftScrollbar = TRUE;
		else
			swnd->fLeftScrollbar = FALSE;
	}

	return mir_callNextSubclass(hwnd, CoolSBWndProc, msg, wParam, lParam);
}

static UINT curTool = -1;
static LRESULT CoolSB_Notify(SCROLLWND* /*swnd*/, HWND hwnd, WPARAM wParam, LPARAM lParam)
{
#ifdef COOLSB_TOOLTIPS

	NMTTDISPINFO *nmdi = (NMTTDISPINFO *)lParam;

	if (nmdi->hdr.hwndFrom == swnd->hwndToolTip &&
		nmdi->hdr.code == TTN_GETDISPINFO)
	{
		//convert the tooltip notify from a "ISHWND" style
		//request to an id-based request. 
		//We do this because our tooltip is a window-style
		//tip, with no tools, and the GETDISPINFO request must
		//indicate which button to retrieve the text for
		//nmdi->hdr.idFrom = curTool;
		nmdi->hdr.idFrom = curTool;
		nmdi->hinst = GetModuleHandle(0);
		nmdi->uFlags &= ~TTF_IDISHWND;
	}
#endif	//COOLSB_TOOLTIPS

	return mir_callNextSubclass(hwnd, CoolSBWndProc, WM_NOTIFY, wParam, lParam);	
}

static LRESULT SendToolTipMessage0(HWND hwndTT, UINT message, WPARAM wParam, LPARAM lParam)
{
	return SendMessage(hwndTT, message, wParam, lParam);
}

#ifdef COOLSB_TOOLTIPS
#define SendToolTipMessage		SendToolTipMessage0
#else
#define SendToolTipMessage		1 ? (void)0 : SendToolTipMessage0
#endif

//
//	Send the specified message to the tooltip control
//
static void __stdcall RelayMouseEvent(HWND hwnd, HWND hwndToolTip, UINT event)
{
#ifdef COOLSB_TOOLTIPS
	MSG msg;

	CoolSB_ZeroMemory(&msg, sizeof(MSG));
	msg.hwnd = hwnd;
	msg.message = event;

	SendMessage(hwndToolTip, TTM_RELAYEVENT, 0, (LONG)&msg);
#else
	UNREFERENCED_PARAMETER(hwnd);
	UNREFERENCED_PARAMETER(hwndToolTip);
	UNREFERENCED_PARAMETER(event);
#endif
}


//
//  CoolScrollbar subclass procedure.
//	Handle all messages needed to mimick normal windows scrollbars
//
LRESULT CALLBACK CoolSBWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	SCROLLWND *swnd = GetScrollWndFromHwnd(hwnd);
	static int count;

	switch(message) {
	case WM_NCDESTROY:
		//this should NEVER be called, because the user
		//should have called Uninitialize() themselves.

		//However, if the user tries to call Uninitialize().. 
		//after this window is destroyed, this window's entry in the lookup
		//table will not be there, and the call will fail
		UninitializeCoolSB(hwnd);
		
		//we must call the original window procedure, otherwise it
		//will never get the WM_NCDESTROY message, and it wouldn't
		//be able to clean up etc.
		return mir_callNextSubclass(hwnd, CoolSBWndProc, message, wParam, lParam);

    case WM_NCCALCSIZE:
		return NCCalcSize(swnd, hwnd, wParam, lParam);

	case WM_NCPAINT:
		return NCPaint(swnd, hwnd, wParam, lParam);	

	case WM_NCHITTEST:
		return NCHitTest(swnd, hwnd, wParam, lParam);

	case WM_NCRBUTTONDOWN: case WM_NCRBUTTONUP: 
	case WM_NCMBUTTONDOWN: case WM_NCMBUTTONUP: 
		RelayMouseEvent(hwnd, swnd->hwndToolTip, (WM_MOUSEMOVE-WM_NCMOUSEMOVE) + (message));
		if (wParam == HTHSCROLL || wParam == HTVSCROLL) 
			return 0;
		else 
			break;

	case WM_NCLBUTTONDBLCLK:
		//TRACE("WM_NCLBUTTONDBLCLK %d\n", count++);
		if (wParam == HTHSCROLL || wParam == HTVSCROLL)
			return NCLButtonDown(swnd, hwnd, wParam, lParam);
		else
			break;

	case WM_NCLBUTTONDOWN:
		//TRACE("WM_NCLBUTTONDOWN%d\n", count++);
		RelayMouseEvent(hwnd, swnd->hwndToolTip, WM_LBUTTONDOWN);
		return NCLButtonDown(swnd, hwnd, wParam, lParam);


	case WM_LBUTTONUP:
		//TRACE("WM_LBUTTONUP %d\n", count++);
		RelayMouseEvent(hwnd, swnd->hwndToolTip, WM_LBUTTONUP);
		return LButtonUp(swnd, hwnd, wParam, lParam);

	case WM_NOTIFY:
		return CoolSB_Notify(swnd, hwnd, wParam, lParam);

	//Mouse moves are received when we set the mouse capture,
	//even when the mouse moves over the non-client area
	case WM_MOUSEMOVE: 
		//TRACE("WM_MOUSEMOVE %d\n", count++);
		return MouseMove(swnd, hwnd, wParam, lParam);
	
	case WM_TIMER:
		return CoolSB_Timer(swnd, hwnd, wParam, lParam);

	//case WM_STYLECHANGING:
	//	return CoolSB_StyleChange(swnd, hwnd, WM_STYLECHANGING, wParam, lParam);
	case WM_STYLECHANGED:

		if (swnd->bPreventStyleChange)
			// the NCPAINT handler has told us to eat this message!
			return 0;
		else
			return CoolSB_StyleChange(swnd, hwnd, WM_STYLECHANGED, wParam, lParam);

	case WM_NCMOUSEMOVE: 
		{
			static LONG_PTR lastpos = -1;

			//TRACE("WM_NCMOUSEMOVE %d\n", count++);

			//The problem with NCMOUSEMOVE is that it is sent continuously
			//even when the mouse is stationary (under win2000 / win98)
			//
			//Tooltips don't like being sent a continous stream of mouse-moves
			//if the cursor isn't moving, because they will think that the mouse
			//is moving position, and the internal timer will never expire
			//
			if (lastpos != lParam)
			{
				RelayMouseEvent(hwnd, swnd->hwndToolTip, WM_MOUSEMOVE);
				lastpos = lParam;
			}
		}

		return NCMouseMove(swnd, hwnd, wParam, lParam);

	case WM_CAPTURECHANGED:
		break;

	default:
		break;
	}
	
	return mir_callNextSubclass(hwnd, CoolSBWndProc, message, wParam, lParam);
}

