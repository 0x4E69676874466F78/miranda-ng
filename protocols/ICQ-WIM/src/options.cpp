// -----------------------------------------------------------------------------
// ICQ plugin for Miranda NG
// -----------------------------------------------------------------------------
// Copyright � 2018-19 Miranda NG team
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
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
// -----------------------------------------------------------------------------

#include "stdafx.h"

struct CIcqRegistrationDlg : public CProtoDlgBase<CIcqProto>
{
	CMStringA szTrans, szMsisdn;
	int iErrorCode = 0;

	CCtrlEdit edtPhone, edtCode;
	CCtrlButton btnSendSms;

	CIcqRegistrationDlg(CIcqProto *ppro) :
		CProtoDlgBase<CIcqProto>(ppro, IDD_REGISTER),
		edtPhone(this, IDC_PHONE),
		edtCode(this, IDC_CODE),
		btnSendSms(this, IDC_SENDSMS)
	{
		btnSendSms.OnClick = Callback(this, &CIcqRegistrationDlg::onClick_SendSms);
		edtPhone.OnChange = Callback(this, &CIcqRegistrationDlg::onChange_Phone);
	}

	bool OnApply() override
	{
		auto *pReq = new AsyncHttpRequest(CONN_MAIN, REQUEST_GET, "https://www.icq.com/smsreg/loginWithPhoneNumber.php", &CIcqProto::OnLoginViaPhone);
		pReq << CHAR_PARAM("locale", "en") << CHAR_PARAM("msisdn", szMsisdn) << CHAR_PARAM("trans_id", szTrans) << CHAR_PARAM("k", ICQ_APP_ID)
			<< CHAR_PARAM("r", pReq->m_reqId) << CHAR_PARAM("f", "json") << WCHAR_PARAM("sms_code", ptrW(edtCode.GetText())) << INT_PARAM("create_account", 1);
		pReq->pUserInfo = this;
		
		SetCursor(LoadCursor(0, IDC_WAIT));
		m_proto->ExecuteRequest(pReq);
		SetCursor(LoadCursor(0, IDC_ARROW));

		if (iErrorCode != 200)
			return false;

		EndDialog(m_hwnd, 1);
		return true;
	}

	void onChange_Phone(CCtrlEdit*)
	{
		auto *pReq = new AsyncHttpRequest(CONN_MAIN, REQUEST_GET, "https://clientapi.icq.net/fcgi-bin/smsphoneinfo", &CIcqProto::OnCheckPhone);
		pReq << CHAR_PARAM("service", "icq_registration") << CHAR_PARAM("info", "typing_check,score,iso_country_code")
			<< WCHAR_PARAM("phone", ptrW(edtPhone.GetText())) << CHAR_PARAM("id", pReq->m_reqId);
		pReq->pUserInfo = this;
		m_proto->Push(pReq);
	}

	void onClick_SendSms(CCtrlButton*)
	{
		auto *pReq = new AsyncHttpRequest(CONN_MAIN, REQUEST_GET, "https://www.icq.com/smsreg/requestPhoneValidation.php", &CIcqProto::OnValidateSms);
		pReq << CHAR_PARAM("locale", "en") << CHAR_PARAM("msisdn", szMsisdn) << CHAR_PARAM("r", pReq->m_reqId)
			<< CHAR_PARAM("smsFormatType", "human") << CHAR_PARAM("k", ICQ_APP_ID);
		pReq->pUserInfo = this;
		m_proto->Push(pReq);
	}
};

void CIcqProto::OnCheckPhone(NETLIBHTTPREQUEST *pReply, AsyncHttpRequest *pReq)
{
	if (pReply == nullptr || pReply->resultCode != 200)
		return;

	CIcqRegistrationDlg *pDlg = (CIcqRegistrationDlg*)pReq->pUserInfo;
	pDlg->btnSendSms.Disable();
	pDlg->edtCode.Disable();

	JSONROOT root(pReply->pData);
	CMStringW wszStatus((*root)["status"].as_mstring());
	if (wszStatus != L"OK") {
		pDlg->edtCode.SetText((*root)["printable"].as_mstring());
		return;
	}

	CMStringA szPhoneNumber((*root)["typing_check"]["modified_phone_number"].as_mstring());
	CMStringA szPrefix((*root)["modified_prefix"].as_mstring());

	auto *pNew = new AsyncHttpRequest(CONN_MAIN, REQUEST_GET, "https://www.icq.com/smsreg/normalizePhoneNumber.php", &CIcqProto::OnNormalizePhone);
	pNew << CHAR_PARAM("countryCode", szPrefix) << CHAR_PARAM("phoneNumber", szPhoneNumber.c_str() + szPrefix.GetLength())
		<< CHAR_PARAM("k", ICQ_APP_ID) << CHAR_PARAM("r", pReq->m_reqId);
	pNew->pUserInfo = pDlg;
	Push(pNew);
}

void CIcqProto::OnNormalizePhone(NETLIBHTTPREQUEST *pReply, AsyncHttpRequest *pReq)
{
	CIcqRegistrationDlg *pDlg = (CIcqRegistrationDlg*)pReq->pUserInfo;

	JsonReply root(pReply);
	pDlg->iErrorCode = root.error();
	if (root.error() != 200)
		return;

	const JSONNode &data = root.data();
	pDlg->szMsisdn = data["msisdn"].as_mstring();
	pDlg->btnSendSms.Enable();
}

void CIcqProto::OnValidateSms(NETLIBHTTPREQUEST *pReply, AsyncHttpRequest *pReq)
{
	JsonReply root(pReply);
	if (root.error() != 200)
		return;

	CIcqRegistrationDlg *pDlg = (CIcqRegistrationDlg*)pReq->pUserInfo;
	const JSONNode &data = root.data();
	pDlg->szTrans = data["trans_id"].as_mstring();

	pDlg->edtCode.Enable();
	pDlg->edtCode.SetText(L"");
}

void CIcqProto::OnLoginViaPhone(NETLIBHTTPREQUEST *pReply, AsyncHttpRequest *pReq)
{
	CIcqRegistrationDlg *pDlg = (CIcqRegistrationDlg*)pReq->pUserInfo;

	JsonReply root(pReply);
	pDlg->iErrorCode = root.error();
	if (root.error() != 200)
		return;

	const JSONNode &data = root.data();
	m_szAToken = data["token"]["a"].as_mstring();
	mir_urlDecode(m_szAToken.GetBuffer());
	setString(DB_KEY_ATOKEN, m_szAToken);

	int srvTS = data["hostTime"].as_int();
	m_iTimeShift = (srvTS) ? time(0) - srvTS : 0;

	m_szSessionKey = data["sessionKey"].as_mstring();
	setString(DB_KEY_SESSIONKEY, m_szSessionKey);

	m_szOwnId = data["loginId"].as_mstring();
	setByte(DB_KEY_PHONEREG, 1);
}

/////////////////////////////////////////////////////////////////////////////////////////

class CIcqOptionsDlg : public CProtoDlgBase<CIcqProto>
{
	CCtrlEdit edtUin, edtPassword;
	CCtrlCheck chkHideChats, chkTrayIcon, chkLaunchMailbox, chkShowErrorPopups;
	CCtrlButton btnCreate;
	CMStringW wszOldPass;

public:
	CIcqOptionsDlg(CIcqProto *ppro, int iDlgID, bool bFullDlg) :
		CProtoDlgBase<CIcqProto>(ppro, iDlgID),
		edtUin(this, IDC_UIN),
		btnCreate(this, IDC_REGISTER),
		edtPassword(this, IDC_PASSWORD),
		chkTrayIcon(this, IDC_USETRAYICON),
		chkHideChats(this, IDC_HIDECHATS),
		chkLaunchMailbox(this, IDC_LAUNCH_MAILBOX),
		chkShowErrorPopups(this, IDC_SHOWERRORPOPUPS)
	{
		btnCreate.OnClick = Callback(this, &CIcqOptionsDlg::onClick_Register);

		CreateLink(edtUin, ppro->m_szOwnId);
		CreateLink(edtPassword, ppro->m_szPassword);
		if (bFullDlg) {
			CreateLink(chkHideChats, ppro->m_bHideGroupchats);
			CreateLink(chkTrayIcon, ppro->m_bUseTrayIcon);
			CreateLink(chkLaunchMailbox, ppro->m_bLaunchMailbox);
			CreateLink(chkShowErrorPopups, ppro->m_bErrorPopups);

			chkTrayIcon.OnChange = Callback(this, &CIcqOptionsDlg::onChange_Tray);
		}

		wszOldPass = ppro->m_szPassword;
	}

	bool OnInitDialog() override
	{
		onChange_Tray(0);
		return true;
	}

	bool OnApply() override
	{
		if (wszOldPass != ptrW(edtPassword.GetText())) {
			m_proto->delSetting(DB_KEY_ATOKEN);
			m_proto->delSetting(DB_KEY_SESSIONKEY);
			m_proto->delSetting(DB_KEY_PHONEREG);
		}
		return true;
	}

	void onChange_Tray(CCtrlCheck*)
	{
		chkLaunchMailbox.Enable(chkTrayIcon.GetState());
	}

	void onClick_Register(CCtrlButton*)
	{
		CIcqRegistrationDlg dlg(m_proto);
		dlg.SetParent(m_hwnd);
		if (dlg.DoModal()) // force exit to avoid data corruption
			PostMessage(m_hwndParent, WM_COMMAND, MAKELONG(IDCANCEL, BN_CLICKED), 0);
	}
};

/////////////////////////////////////////////////////////////////////////////////////////
// Advanced options

class CIcqOptionsAdv : public CProtoDlgBase<CIcqProto>
{
	CCtrlEdit edtDiff1, edtDiff2;
	CCtrlSpin spin1, spin2;
	CCtrlCombo cmbStatus1, cmbStatus2;

public:
	CIcqOptionsAdv(CIcqProto *ppro) :
		CProtoDlgBase<CIcqProto>(ppro, IDD_OPTIONS_ADV),
		spin1(this, IDC_SPIN1, 32000),
		spin2(this, IDC_SPIN2, 32000),
		edtDiff1(this, IDC_DIFF1),
		edtDiff2(this, IDC_DIFF2),
		cmbStatus1(this, IDC_STATUS1),
		cmbStatus2(this, IDC_STATUS2)
	{
		edtDiff1.OnChange = Callback(this, &CIcqOptionsAdv::onChange_Timeout1);
		edtDiff2.OnChange = Callback(this, &CIcqOptionsAdv::onChange_Timeout2);

		CreateLink(spin1, ppro->m_iTimeDiff1);
		CreateLink(spin2, ppro->m_iTimeDiff2);
	}

	bool OnInitDialog() override
	{
		if (cmbStatus1.GetHwnd()) {
			for (DWORD iStatus = ID_STATUS_OFFLINE; iStatus <= ID_STATUS_MAX; iStatus++) {
				int idx = cmbStatus1.AddString(Clist_GetStatusModeDescription(iStatus, 0));
				cmbStatus1.SetItemData(idx, iStatus);
				if (iStatus == m_proto->m_iStatus1)
					cmbStatus1.SetCurSel(idx);

				idx = cmbStatus2.AddString(Clist_GetStatusModeDescription(iStatus, 0));
				cmbStatus2.SetItemData(idx, iStatus);
				if (iStatus == m_proto->m_iStatus2)
					cmbStatus2.SetCurSel(idx);
			}

			onChange_Timeout1(0);
		}

		return true;
	}

	bool OnApply() override
	{
		if (cmbStatus1.GetHwnd()) {
			m_proto->m_iStatus1 = cmbStatus1.GetItemData(cmbStatus1.GetCurSel());
			m_proto->m_iStatus2 = cmbStatus2.GetItemData(cmbStatus2.GetCurSel());
		}

		return true;
	}

	void onChange_Timeout1(CCtrlEdit*)
	{
		bool bEnabled = edtDiff1.GetInt() != 0;
		spin2.Enable(bEnabled);
		edtDiff2.Enable(bEnabled);
		cmbStatus1.Enable(bEnabled);
		cmbStatus2.Enable(bEnabled && edtDiff2.GetInt() != 0);
	}

	void onChange_Timeout2(CCtrlEdit*)
	{
		bool bEnabled = edtDiff2.GetInt() != 0;
		cmbStatus2.Enable(bEnabled);
	}
};

/////////////////////////////////////////////////////////////////////////////////////////
// Services

INT_PTR CIcqProto::CreateAccMgrUI(WPARAM, LPARAM hwndParent)
{
	CIcqOptionsDlg *pDlg = new CIcqOptionsDlg(this, IDD_OPTIONS_ACCMGR, false);
	pDlg->SetParent((HWND)hwndParent);
	pDlg->Create();
	return (INT_PTR)pDlg->GetHwnd();
}

int CIcqProto::OnOptionsInit(WPARAM wParam, LPARAM)
{
	OPTIONSDIALOGPAGE odp = {};
	odp.szTitle.w = m_tszUserName;
	odp.flags = ODPF_UNICODE | ODPF_BOLDGROUPS;
	odp.szGroup.w = LPGENW("Network");
	odp.position = 1;

	odp.szTab.w = LPGENW("General");
	odp.pDialog = new CIcqOptionsDlg(this, IDD_OPTIONS_FULL, true);
	g_plugin.addOptions(wParam, &odp);

	odp.szTab.w = LPGENW("Advanced");
	odp.pDialog = new CIcqOptionsAdv(this);
	g_plugin.addOptions(wParam, &odp);
	return 0;
}
