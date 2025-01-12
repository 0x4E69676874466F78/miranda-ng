/*
Plugin of Miranda IM for communicating with users of the MSN Messenger protocol.

Copyright (c) 2012-2019 Miranda NG team
Copyright (c) 2006-2012 Boris Krasnovskiy.
Copyright (c) 2003-2005 George Hazan.
Copyright (c) 2002-2003 Richard Hughes (original version).

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "stdafx.h"
#include "msn_proto.h"

MCONTACT CMsnProto::MSN_HContactFromEmail(const char* wlid, const char* msnNick, bool addIfNeeded, bool temporary)
{
	MCONTACT hContact = NULL;

	char *szEmail, *szNet = nullptr;
	parseWLID(NEWSTR_ALLOCA(wlid), &szNet, &szEmail, nullptr);

	MsnContact *msc = Lists_Get(szEmail);
	if (msc && msc->hContact)
		hContact = msc->hContact;

	if (hContact == NULL && addIfNeeded) {
		int netId = (msc && msc->netId)?msc->netId:(szNet?atoi(szNet):NETID_MSN);
		hContact = db_add_contact();
		Proto_AddToContact(hContact, m_szModuleName);
		if (netId != NETID_SKYPE) setString(hContact, "e-mail", szEmail);
		setStringUtf(hContact, "Nick", msnNick ? msnNick : szEmail);
		setWord(hContact, "netId", netId);
		setString(hContact, "wlid", szEmail);
		if (temporary)
			db_set_b(hContact, "CList", "NotOnList", 1);

		Lists_Add(0, szNet?atoi(szNet):NETID_MSN, szEmail, hContact);
	}

	return hContact;
}

MCONTACT CMsnProto::MSN_HContactFromChatID(const char* wlid)
{
	for (auto &hContact : AccContacts()) {
		if (isChatRoom(hContact) != 0) {
			DBVARIANT dbv;
			if (getString(hContact, "ChatRoomID", &dbv) == 0) {
				if (mir_strcmp(dbv.pszVal, wlid) == 0) {
					db_free(&dbv);
					return hContact;
				}
				db_free(&dbv);
			}
		}
	}
	return NULL;
}


void CMsnProto::MSN_SetContactDb(MCONTACT hContact, const char *szEmail)
{
	MsnContact *cont = Lists_Get(szEmail);
	if (cont == nullptr)
		return;
	const int listId = cont->list;

	if (listId & LIST_FL) {
		if (db_get_b(hContact, "CList", "NotOnList", 0) == 1) {
			db_unset(hContact, "CList", "NotOnList");
			db_unset(hContact, "CList", "Hidden");
		}

		if (listId & (LIST_BL | LIST_AL)) {
			WORD tApparentMode = getWord(hContact, "ApparentMode", 0);
			if ((listId & LIST_BL) && tApparentMode == 0)
				setWord(hContact, "ApparentMode", ID_STATUS_OFFLINE);
			else if ((listId & LIST_AL) && tApparentMode != 0)
				delSetting(hContact, "ApparentMode");
		}

		if (cont->netId == NETID_MOB) {
			setWord(hContact, "Status", ID_STATUS_ONLINE);
			setString(hContact, "MirVer", "SMS");
		}
	}
	if (listId & LIST_LL)
		setByte(hContact, "LocalList", 1);
	else
		delSetting(hContact, "LocalList");
}


void CMsnProto::AddDelUserContList(const char* email, const int list, const int netId, const bool del)
{
	if (del)
		Lists_Remove(list, email);
	else
		Lists_Add(list, netId, email);
}


/////////////////////////////////////////////////////////////////////////////////////////
// MSN_AddUser - adds a e-mail address to one of the MSN server lists

bool CMsnProto::MSN_AddUser(MCONTACT hContact, const char* email, int netId, int flags, const char *msg)
{
	bool needRemove = (flags & LIST_REMOVE) != 0;
	bool leaveHotmail = (flags & LIST_REMOVENH) == LIST_REMOVENH;
	flags &= 0xFF;

	if (needRemove != Lists_IsInList(flags, email))
		return true;

	bool res = false;
	if (MyOptions.netId == NETID_SKYPE) {
		if (flags & LIST_BL) {
			if (needRemove) return MSN_SKYABBlockContact(email, "unblock");
			return MSN_SKYABBlockContact(email, "block");
		}
		else if ((flags & LIST_FL)) {
			if (!needRemove) return MSN_SKYABAuthRq(email, msg);
			if (!leaveHotmail) return MSN_SKYABDeleteContact(email);
			return true;
		}
		else if ((flags & LIST_AL) && !needRemove) return MSN_SKYABAuthRsp(email, "accept");
		else if ((flags & LIST_RL) && !needRemove) return MSN_SKYABAuthRsp(email, "decline");
	} else {
		if (flags == LIST_FL) {
			if (needRemove) {
				if (hContact == NULL) {
					hContact = MSN_HContactFromEmail(email);
					if (hContact == NULL)
						return false;
				}

				char id[MSN_GUID_LEN];
				if (!db_get_static(hContact, m_szModuleName, "ID", id, _countof(id))) {
					int netId2 = Lists_GetNetId(email);
					if (leaveHotmail)
						res = MSN_ABAddRemoveContact(id, netId2, false);
					else
						res = MSN_ABAddDelContactGroup(id, nullptr, "ABContactDelete");
					if (res)
						AddDelUserContList(email, flags, netId2, true);

					delSetting(hContact, "GroupID");
					delSetting(hContact, "ID");
					MSN_RemoveEmptyGroups();
				}
			}
			else {
				DBVARIANT dbv = { 0 };
				if (!mir_strcmp(email, MyOptions.szEmail))
					getStringUtf("Nick", &dbv);

				unsigned res1 = MSN_ABContactAdd(email, dbv.pszVal, netId, msg, false);
				if (netId == NETID_MSN && res1 == 2) {
					netId = NETID_LCS;
					res = MSN_ABContactAdd(email, dbv.pszVal, netId, msg, false) == 0;
				}
				else if (netId == NETID_MSN && res1 == 3) {
					char szContactID[100];
					hContact = MSN_HContactFromEmail(email);
					if (db_get_static(hContact, m_szModuleName, "ID", szContactID, sizeof(szContactID)) == 0) {
						MSN_ABAddRemoveContact(szContactID, netId, true);
						res = true;
					}
				}
				else res = (res1 == 0);

				if (res) {
					DBVARIANT dbv2;
					if (!db_get_utf(hContact, "CList", "Group", &dbv2)) {
						MSN_MoveContactToGroup(hContact, dbv2.pszVal);
						db_free(&dbv2);
					}

					char szContactID[100];
					if (db_get_static(hContact, m_szModuleName, "ID", szContactID, sizeof(szContactID)) == 0)
						MSN_ABFind("ABFindByContacts", szContactID);

					MSN_SharingFindMembership(true);
					AddDelUserContList(email, flags, netId, false);
				}

				db_free(&dbv);
			}
		}
		else if (flags == LIST_LL) {
			if (needRemove)
				Lists_Remove(LIST_LL, email);
			else
				Lists_Add(LIST_LL, NETID_MSN, email);
		}
		else {
			if (netId == 0)
				netId = Lists_GetNetId(email);
			res = MSN_SharingAddDelMember(email, flags, netId, needRemove ? "DeleteMember" : "AddMember");
			AddDelUserContList(email, flags, netId, needRemove);

			if ((flags & LIST_PL) && needRemove)
				MSN_AddUser(hContact, email, netId, LIST_RL);
		}
	}
	return res;
}

bool CMsnProto::MSN_RefreshContactList(void)
{
	Lists_Wipe();
	Lists_Populate();

	/*
	if (MyOptions.netId != NETID_SKYPE)
	{
		// Get your own profile info (does not work anymore, byrdr died)
		//MSN_SharingFindMembership();

		if (m_iDesiredStatus == ID_STATUS_OFFLINE) return false;

		// Get "classic" MSN contacts (does not work anymore, byrdr died)
		// MSN_ABFind("ABFindContactsPaged", NULL);

		// Get Skype contacts on linked profiles
		if (!MSN_ABRefreshClist()) return false;

		if (m_iDesiredStatus == ID_STATUS_OFFLINE) return false;

		// Remove contacts not on server etc.
		MSN_CleanupLists();

		if (m_iDesiredStatus == ID_STATUS_OFFLINE) return false;

		msnLoggedIn = true;

		// Populate Contact list on MSN network to get status updates of contacts
		MSN_CreateContList();

		// Read my own profile from SOAP AB (does not work anymore, byrdr died)
		//MSN_StoreGetProfile();
	}
	else 
	*/
	{
		// Refresh Skype contact list
		MSN_SKYABRefreshClist();
		
		// Populate Contact list on MSN network to get status updates of contacts
		MSN_CreateContList();
	}

	// Refresh Threads which are also part of the contact list
	if (msnP24Ver>1) MSN_GCRefreshThreadsInfo();

	return true;
}
