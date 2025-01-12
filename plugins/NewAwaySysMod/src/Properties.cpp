/*
	New Away System - plugin for Miranda IM
	Copyright (C) 2005-2007 Chervov Dmitry

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

#include "stdafx.h"
#include "Properties.h"

CProtoStates g_ProtoStates;

void ResetContactSettingsOnStatusChange(MCONTACT hContact)
{
	g_plugin.delSetting(hContact, DB_REQUESTCOUNT);
	g_plugin.delSetting(hContact, DB_SENDCOUNT);
	g_plugin.delSetting(hContact, DB_MESSAGECOUNT);
}

void ResetSettingsOnStatusChange(const char *szProto = nullptr, int bResetPersonalMsgs = false, int Status = 0)
{
	if (bResetPersonalMsgs)
		bResetPersonalMsgs = !g_MoreOptPage.GetDBValueCopy(IDC_MOREOPTDLG_SAVEPERSONALMSGS);

	for (auto &hContact : Contacts()) {
		const char *szCurProto;
		if (!szProto || ((szCurProto = GetContactProto(hContact)) && !mir_strcmp(szProto, szCurProto))) {
			ResetContactSettingsOnStatusChange(hContact);
			if (bResetPersonalMsgs)
				CContactSettings(Status, hContact).SetMsgFormat(SMF_PERSONAL, nullptr); // TODO: delete only when SAM dialog opens?
		}
	}
}


CProtoState::CStatus& CProtoState::CStatus::operator=(int Status)
{
	_ASSERT(Status >= ID_STATUS_MIN && Status <= ID_STATUS_MAX);
	if (Status < ID_STATUS_MIN || Status > ID_STATUS_MAX)
		return *this; // ignore status change if the new status is unknown

	bool bModified = false;
	if (m_szProto) {
		if (this->m_status != Status) {
			this->m_status = Status;
			(*m_grandParent)[m_szProto].m_awaySince.Reset();
			ResetSettingsOnStatusChange(m_szProto, true, Status);
			bModified = true;
		}
		if ((*m_grandParent)[m_szProto].TempMsg.IsSet()) {
			(*m_grandParent)[m_szProto].TempMsg.Unset();
			bModified = true;
		}
	}
	else { // global status change
		int bStatusModified = false;
		for (int i = 0; i < m_grandParent->GetSize(); i++) {
			CProtoState &State = (*m_grandParent)[i];
			if (!db_get_b(0, State.GetProto(), "LockMainStatus", 0)) { // if the protocol isn't locked
				if (State.m_status != Status) {
					State.m_status.m_status = Status; // "Status.Status" - changing Status directly to prevent recursive calls to the function
					State.m_awaySince.Reset();
					bModified = true;
					bStatusModified = true;
				}
				if (State.TempMsg.IsSet()) {
					State.TempMsg.Unset();
					bModified = true;
				}
				if (g_MoreOptPage.GetDBValueCopy(IDC_MOREOPTDLG_RESETPROTOMSGS))
					CProtoSettings(State.m_szProto, Status).SetMsgFormat(SMF_PERSONAL, nullptr);
			}
		}
		if (bStatusModified)
			ResetSettingsOnStatusChange(nullptr, true, Status);
	}
	if (bModified && g_SetAwayMsgPage.GetWnd())
		SendMessage(g_SetAwayMsgPage.GetWnd(), UM_SAM_PROTOSTATUSCHANGED, (WPARAM)(char*)m_szProto, 0);
	return *this;
}

void CProtoState::CAwaySince::Reset()
{
	GetLocalTime(&m_awaySince);

	if (m_grandParent && !m_szProto)
		for (int i = 0; i < m_grandParent->GetSize(); i++)
			GetLocalTime((*m_grandParent)[i].m_awaySince);
}


void CContactSettings::SetMsgFormat(int Flags, TCString Message)
{
	if (Flags & SMF_PERSONAL) { // set a personal message for a contact. also it's used to set global status message (hContact = NULL).
		// if Message == nullptr, then the function deletes the message.
		CString DBSetting(StatusToDBSetting(Status, DB_STATUSMSG, IDC_MOREOPTDLG_PERSTATUSPERSONAL));
		if (g_AutoreplyOptPage.GetDBValueCopy(IDC_REPLYDLG_RESETCOUNTERWHENSAMEICON) && GetMsgFormat(SMF_PERSONAL) != (const wchar_t*)Message)
			ResetContactSettingsOnStatusChange(m_hContact);

		if (Message != nullptr)
			db_set_ws(m_hContact, MODULENAME, DBSetting, Message);
		else
			db_unset(m_hContact, MODULENAME, DBSetting);
	}

	if (Flags & (SMF_LAST | SMF_TEMPORARY)) {
		_ASSERT(!m_hContact);
		CProtoSettings().SetMsgFormat(Flags & (SMF_LAST | SMF_TEMPORARY), Message);
	}
}


TCString CContactSettings::GetMsgFormat(int Flags, int *pOrder, char *szProtoOverride)
// returns the requested message; sets Order to the order of the message in the message tree, if available; or to -1 otherwise.
// szProtoOverride is needed only to get status message of the right protocol when the ICQ contact is on list, but not with the same 
// protocol on which it requests the message - this way we can still get contact details.
{
	TCString Message = nullptr;
	if (pOrder)
		*pOrder = -1;

	if (Flags & GMF_PERSONAL) // try getting personal message (it overrides global)
		Message = db_get_s(m_hContact, MODULENAME, StatusToDBSetting(Status, DB_STATUSMSG, IDC_MOREOPTDLG_PERSTATUSPERSONAL), (wchar_t*)NULL);

	if (Flags & (GMF_LASTORDEFAULT | GMF_PROTOORGLOBAL | GMF_TEMPORARY) && Message.IsEmpty()) {
		char *szProto = szProtoOverride ? szProtoOverride : (m_hContact ? GetContactProto(m_hContact) : nullptr);

		// we mustn't pass here by GMF_TEMPORARY flag, as otherwise we'll handle GMF_TEMPORARY | GMF_PERSONAL combination incorrectly, 
		// which is supposed to get only per-contact messages, and at the same time also may be used with NULL contact to get the global status message
		if (Flags & (GMF_LASTORDEFAULT | GMF_PROTOORGLOBAL))
			Message = CProtoSettings(szProto).GetMsgFormat(Flags, pOrder);
		else if (!m_hContact) { // handle global temporary message too
			if (g_ProtoStates[szProto].TempMsg.IsSet())
				Message = g_ProtoStates[szProto].TempMsg;
		}
	}
	return Message;
}

void CProtoSettings::SetMsgFormat(int Flags, TCString Message)
{
	if (Flags & (SMF_TEMPORARY | SMF_PERSONAL) && g_AutoreplyOptPage.GetDBValueCopy(IDC_REPLYDLG_RESETCOUNTERWHENSAMEICON) && GetMsgFormat(Flags & (SMF_TEMPORARY | SMF_PERSONAL)) != (const wchar_t*)Message)
		ResetSettingsOnStatusChange(szProto);

	if (Flags & SMF_TEMPORARY) {
		_ASSERT(!Status || Status == g_ProtoStates[szProto].m_status);
		g_ProtoStates[szProto].TempMsg = (szProto || Message != nullptr) ? Message : CProtoSettings(nullptr, Status).GetMsgFormat(GMF_LASTORDEFAULT);
	}

	if (Flags & SMF_PERSONAL) { // set a "personal" message for a protocol. also it's used to set global status message (hContact = NULL).
		// if Message == NULL, then we'll use the "default" message - i.e. it's either the global message for szProto != NULL (we delete the per-proto DB setting), or it's just a default message for a given status for szProto == NULL.
		g_ProtoStates[szProto].TempMsg.Unset();
		CString DBSetting(ProtoStatusToDBSetting(DB_STATUSMSG, IDC_MOREOPTDLG_PERSTATUSPROTOMSGS));
		if (Message != nullptr)
			g_plugin.setWString(DBSetting, Message);
		else {
			if (!szProto)
				g_plugin.setWString(DBSetting, CProtoSettings(nullptr, Status).GetMsgFormat(GMF_LASTORDEFAULT)); // global message can't be NULL; we can use an empty string instead if it's really necessary
			else
				g_plugin.delSetting(DBSetting);
		}
	}

	if (Flags & SMF_LAST) {
		COptPage MsgTreeData(g_MsgTreePage);
		COptItem_TreeCtrl *TreeCtrl = (COptItem_TreeCtrl*)MsgTreeData.Find(IDV_MSGTREE);
		TreeCtrl->DBToMem(CString(MODULENAME));
		int RecentGroupID = GetRecentGroupID(Status);
		if (RecentGroupID == -1) { // we didn't find the group, it also means that we're using per status messages; so we need to create it
			TreeCtrl->m_value.AddElem(CTreeItem(Status ? Clist_GetStatusModeDescription(Status, 0) : MSGTREE_RECENT_OTHERGROUP, g_Messages_RecentRootID, RecentGroupID = TreeCtrl->GenerateID(), TIF_GROUP));
			TreeCtrl->SetModified(true);
		}
		int i;
		// try to find an identical message in the same group (to prevent saving multiple identical messages), 
		// or at least if we'll find an identical message somewhere else, then we'll use its title for our new message
		TCString Title(L"");
		for (i = 0; i < TreeCtrl->m_value.GetSize(); i++) {
			if (!(TreeCtrl->m_value[i].Flags & TIF_GROUP) && TreeCtrl->m_value[i].User_Str1 == (const wchar_t*)Message) {
				if (TreeCtrl->m_value[i].ParentID == RecentGroupID) { // found it in the same group
					int GroupOrder = TreeCtrl->IDToOrder(RecentGroupID);
					TreeCtrl->m_value.MoveElem(i, (GroupOrder >= 0) ? (GroupOrder + 1) : 0); // now move it to the top of recent messages list
					TreeCtrl->SetModified(true);
					break; // no reason to search for anything else
				}
				if (Title.IsEmpty()) // it's not in the same group, but at least we'll use its title
					Title = TreeCtrl->m_value[i].Title;
			}
		}
		if (i == TreeCtrl->m_value.GetSize()) { // we didn't find an identical message in the same group, so we'll add our new message here
			if (Title.IsEmpty()) { // didn't find a title for our message either
				if (Message.GetLen() > MRM_MAX_GENERATED_TITLE_LEN)
					Title = Message.Left(MRM_MAX_GENERATED_TITLE_LEN - 3) + L"...";
				else
					Title = Message;

				wchar_t *p = Title.GetBuffer();
				while (*p) { // remove "garbage"
					if (!(p = wcspbrk(p, L"\r\n\t")))
						break;

					*p++ = ' ';
				}
				Title.ReleaseBuffer();
			}
			int GroupOrder = TreeCtrl->IDToOrder(RecentGroupID);
			TreeCtrl->m_value.InsertElem(CTreeItem(Title, RecentGroupID, TreeCtrl->GenerateID(), 0, Message), (GroupOrder >= 0) ? (GroupOrder + 1) : 0);
			TreeCtrl->SetModified(true);
		}

		// now clean up here
		int MRMNum = 0;
		int MaxMRMNum = g_MoreOptPage.GetDBValueCopy(IDC_MOREOPTDLG_RECENTMSGSCOUNT);
		for (i = 0; i < TreeCtrl->m_value.GetSize(); i++) {
			if (TreeCtrl->m_value[i].ParentID == RecentGroupID) { // found a child of our group
				if (TreeCtrl->m_value[i].Flags & TIF_GROUP || ++MRMNum > MaxMRMNum) { // what groups are doing here?! :))
					TreeCtrl->m_value.RemoveElem(i);
					TreeCtrl->SetModified(true);
					i--;
				}
			}
		}

		// if we're saving recent messages per status, then remove any messages that were left at the recent messages' root
		if (g_MoreOptPage.GetDBValueCopy(IDC_MOREOPTDLG_PERSTATUSMRM)) {
			for (i = 0; i < TreeCtrl->m_value.GetSize(); i++) {
				if (TreeCtrl->m_value[i].ParentID == g_Messages_RecentRootID) {
					if (!(TreeCtrl->m_value[i].Flags & TIF_GROUP)) {
						TreeCtrl->m_value.RemoveElem(i);
						TreeCtrl->SetModified(true);
						i--;
					}
				}
			}
		}
		TreeCtrl->MemToDB(CString(MODULENAME));
	}
}

// returns the requested message; sets Order to the order of the message in the message tree, if available; or to -1 otherwise.
TCString CProtoSettings::GetMsgFormat(int Flags, int *pOrder)
{
	TCString Message = nullptr;
	if (pOrder)
		*pOrder = -1;

	if (Flags & GMF_TEMPORARY) {
		_ASSERT(!Status || Status == g_ProtoStates[szProto].m_status);
		if (g_ProtoStates[szProto].TempMsg.IsSet()) {
			Message = g_ProtoStates[szProto].TempMsg;
			Flags &= ~GMF_PERSONAL; // don't allow personal message to overwrite our NULL temporary message
		}
	}
	if (Flags & GMF_PERSONAL && Message == nullptr) // try getting personal message (it overrides global)
		Message = db_get_s(0, MODULENAME, ProtoStatusToDBSetting(DB_STATUSMSG, IDC_MOREOPTDLG_PERSTATUSPROTOMSGS), (wchar_t*)nullptr);

	if (Flags & GMF_PROTOORGLOBAL && Message == nullptr) {
		Message = CProtoSettings().GetMsgFormat(GMF_PERSONAL | (Flags & GMF_TEMPORARY), pOrder);
		return (Message == nullptr) ? L"" : Message; // global message can't be NULL
	}

	if (Flags & GMF_LASTORDEFAULT && Message == nullptr) { // try to get the last or default message, depending on current settings
		COptPage MsgTreeData(g_MsgTreePage);
		COptItem_TreeCtrl *TreeCtrl = (COptItem_TreeCtrl*)MsgTreeData.Find(IDV_MSGTREE);
		TreeCtrl->DBToMem(CString(MODULENAME));
		Message = nullptr;
		if (g_MoreOptPage.GetDBValueCopy(IDC_MOREOPTDLG_USELASTMSG)) { // if using last message by default...
			Message = db_get_s(0, MODULENAME, ProtoStatusToDBSetting(DB_STATUSMSG, IDC_MOREOPTDLG_PERSTATUSPROTOMSGS), (wchar_t*)nullptr); // try per-protocol message first
			if (Message.IsEmpty()) {
				Message = nullptr; // to be sure it's NULL, not "" - as we're checking 'Message == NULL' later
				int RecentGroupID = GetRecentGroupID(Status);
				if (RecentGroupID != -1) {
					for (int i = 0; i < TreeCtrl->m_value.GetSize(); i++) { // find first message in the group
						if (TreeCtrl->m_value[i].ParentID == RecentGroupID && !(TreeCtrl->m_value[i].Flags & TIF_GROUP)) {
							Message = TreeCtrl->m_value[i].User_Str1;
							if (pOrder)
								*pOrder = i;
							break;
						}
					}
				}
			}
		} // else, if using default message by default...

		if (Message == nullptr) { // ...or we didn't succeed retrieving the last message
			// get default message for this status
			int DefMsgID = -1;
			static struct {
				int DBSetting, Status;
			}
			DefMsgDlgItems[] = {
				IDS_MESSAGEDLG_DEF_ONL, ID_STATUS_ONLINE,
				IDS_MESSAGEDLG_DEF_AWAY, ID_STATUS_AWAY,
				IDS_MESSAGEDLG_DEF_NA, ID_STATUS_NA,
				IDS_MESSAGEDLG_DEF_OCC, ID_STATUS_OCCUPIED,
				IDS_MESSAGEDLG_DEF_DND, ID_STATUS_DND,
				IDS_MESSAGEDLG_DEF_INV, ID_STATUS_INVISIBLE,
			};

			for (int i = 0; i < _countof(DefMsgDlgItems); i++) {
				if (DefMsgDlgItems[i].Status == Status) {
					DefMsgID = MsgTreeData.GetDBValue(DefMsgDlgItems[i].DBSetting);
					break;
				}
			}
			if (DefMsgID == -1)
				DefMsgID = MsgTreeData.GetDBValue(IDS_MESSAGEDLG_DEF_AWAY); // use away message for unknown statuses

			int Order = TreeCtrl->IDToOrder(DefMsgID); // this will return -1 in any case if something goes wrong
			if (Order >= 0)
				Message = TreeCtrl->m_value[Order].User_Str1;

			if (pOrder)
				*pOrder = Order;
		}
		if (Message == nullptr)
			Message = L""; // last or default message can't be NULL.. otherwise ICQ won't reply to status message requests and won't notify us of status message requests at all
	}
	return Message;
}
