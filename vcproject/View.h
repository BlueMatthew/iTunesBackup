// View.h : interface of the CView class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include <future>
#include <thread>
#include "Core.h"
#include "ViewHelper.h"

class CView : public CDialogImpl<CView>, public CDialogResize<CView>
{
private:
	CSortListViewCtrl		m_appListCtrl;

	std::vector<BackupManifest> m_manifests;

	int m_itemClicked;

	std::future<bool> m_task;

	UINT_PTR m_eventId;

	std::atomic_bool m_cancelled;
	
public:
	enum { IDD = IDD_MAIN_FORM };


	LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		m_itemClicked = -2;
		m_eventId = 0;

		m_cancelled = false;
	
		// Init the CDialogResize code
		DlgResize_Init();

		InitializeAppList();

		CProgressBarCtrl pbc = GetDlgItem(IDC_PROGRESS);
		pbc.SetRange(0, 100);

		// ::PostMessage(GetDlgItem(IDC_EXPORT), BM_CLICK, 0, 0L);

		TCHAR szOutput[MAX_PATH] = { 0 };
		BOOL outputDirFound = FALSE;
#ifndef NDEBUG
		TCHAR szPrevBackup[MAX_PATH] = { 0 };
#endif

		CRegKey rk;
		if (rk.Open(HKEY_CURRENT_USER, TEXT("Software\\iTunesBackup"), KEY_READ) == ERROR_SUCCESS)
		{
			ULONG chars = MAX_PATH;
			if (rk.QueryStringValue(TEXT("OutputDir"), szOutput, &chars) == ERROR_SUCCESS)
			{
				outputDirFound = TRUE;
			}
#ifndef NDEBUG
			chars = MAX_PATH;
			rk.QueryStringValue(TEXT("BackupDir"), szPrevBackup, &chars);
#endif
			rk.Close();
		}

		BOOL iTunesInstalled = FALSE;
		TCHAR iTunesVersion[MAX_PATH] = { 0 };
		CRegKey rkITunes;
		if (rkITunes.Open(HKEY_LOCAL_MACHINE, TEXT("SOFTWARE\\Apple Computer, Inc.\\iTunes"), KEY_READ) == ERROR_SUCCESS)
		{
			ULONG chars = MAX_PATH;
			if (rkITunes.QueryStringValue(TEXT("Version"), iTunesVersion, &chars) == ERROR_SUCCESS)
			{
				iTunesInstalled = TRUE;
			}
			rkITunes.Close();
		}

		// CString iTunesStatus;
		// iTunesInstalled ? iTunesStatus.Format(IDS_ITUNES_VERSION, iTunesVersion) : iTunesStatus.LoadString(IDS_ITUNES_NOT_INSTALLED);
		// m_iTunesLabel.SetWindowText(iTunesStatus);
		// iTunesInstalled ? m_iTunesLabel.SetNormalColors(RGB(0, 0xFF, 0), RGB(0xFF, 0xFF, 0xFF)) : m_iTunesLabel.SetNormalColors(RGB(0xFF, 0, 0), CLR_INVALID);
		// iTunesInstalled ? m_iTunesLabel.SetNormalColors(RGB(0x32, 0xCD, 0x32), CLR_INVALID) : m_iTunesLabel.SetNormalColors(RGB(0xFF, 0, 0), CLR_INVALID);
		// m_iTunesLabel.

		HRESULT result = S_OK;
		if (!outputDirFound)
		{
			result = SHGetFolderPath(NULL, CSIDL_MYDOCUMENTS, NULL, SHGFP_TYPE_CURRENT, szOutput);
		}
		SetDlgItemText(IDC_OUTPUT, szOutput);

		CString backupDir = GetDefaultBackupDir();

		ManifestParser parser((LPCSTR)CW2A(CT2W(backupDir), CP_UTF8), true);
		std::vector<BackupManifest> manifests;
		if (parser.parse(manifests))
		{
			// UpdateBackups(manifests);
		}

#ifndef NDEBUG
		if ((_tcslen(szPrevBackup) > 0) && (_tcscmp(szPrevBackup, backupDir) != 0))
		{
			CW2A backupDirU8(CT2W(szPrevBackup), CP_UTF8);
			ManifestParser parser((LPCSTR)backupDirU8, true);
			parser.parse(manifests);
		}
#endif
		if (!manifests.empty())
		{
			UpdateBackups(manifests);
		}

		return TRUE;
	}

	void OnFinalMessage(HWND hWnd)
	{
		// override to do something, if needed
	}

	BOOL PreTranslateMessage(MSG* pMsg)
	{
		return CWindow::IsDialogMessage(pMsg);
	}

	BEGIN_MSG_MAP(CView)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		CHAIN_MSG_MAP(CDialogResize<CView>)
		MESSAGE_HANDLER(WM_TIMER, OnTimer)
		COMMAND_HANDLER(IDC_CHOOSE_BKP, BN_CLICKED, OnBnClickedChooseBkp)
		COMMAND_HANDLER(IDC_BACKUP, CBN_SELCHANGE, OnBackupSelChange)
		COMMAND_HANDLER(IDC_EXPORT, BN_CLICKED, OnBnClickedExport)
		COMMAND_HANDLER(IDC_CANCEL, BN_CLICKED, OnBnClickedCancel)
		NOTIFY_HANDLER(IDC_APP_LIST, LVN_ITEMCHANGED, OnListItemChanged)
		NOTIFY_CODE_HANDLER(HDN_ITEMSTATEICONCLICK, OnHeaderItemStateIconClick)
		NOTIFY_HANDLER(IDC_APP_LIST, NM_CLICK, OnListClick)
		REFLECT_NOTIFICATIONS()
	END_MSG_MAP()

	BEGIN_DLGRESIZE_MAP(CView)
		DLGRESIZE_CONTROL(IDC_CHOOSE_BKP, DLSZ_MOVE_X)
		DLGRESIZE_CONTROL(IDC_BACKUP, DLSZ_SIZE_X)
		// DLGRESIZE_CONTROL(IDC_CHOOSE_OUTPUT, DLSZ_MOVE_X)
		// DLGRESIZE_CONTROL(IDC_OUTPUT, DLSZ_SIZE_X)
		DLGRESIZE_CONTROL(IDC_APP_LIST, DLSZ_SIZE_X | DLSZ_SIZE_Y)
		DLGRESIZE_CONTROL(IDC_PROGRESS, DLSZ_MOVE_Y)
		DLGRESIZE_CONTROL(IDC_THINIZE, DLSZ_MOVE_X | DLSZ_MOVE_Y)
		DLGRESIZE_CONTROL(IDC_EXPORT, DLSZ_MOVE_X | DLSZ_MOVE_Y)
		DLGRESIZE_CONTROL(IDC_CANCEL, DLSZ_MOVE_X | DLSZ_MOVE_Y)
	END_DLGRESIZE_MAP()


// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)
	LRESULT OnBnClickedChooseBkp(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CString text;
		text.LoadString(IDS_SEL_BACKUP_DIR);

		CFolderDialog folder(NULL, text, BIF_RETURNONLYFSDIRS | BIF_USENEWUI | BIF_NONEWFOLDERBUTTON);
		if (IDOK == folder.DoModal())
		{
			CW2A backupDir(CT2W(folder.m_szFolderPath), CP_UTF8);

			ManifestParser parser((LPCSTR)backupDir, true);
			std::vector<BackupManifest> manifests;
			if (parser.parse(manifests))
			{
				UpdateBackups(manifests);
#ifndef NDEBUG
				CRegKey rk;
				if (rk.Create(HKEY_CURRENT_USER, TEXT("Software\\iTunesBackup"), REG_NONE, REG_OPTION_NON_VOLATILE, KEY_READ | KEY_WRITE) == ERROR_SUCCESS)
				{
					rk.SetStringValue(TEXT("BackupDir"), folder.m_szFolderPath);
				}
#endif
			}
			else
			{
				MsgBox(m_hWnd, IDS_FAILED_TO_LOAD_BKP);
			}
		}

		return 0;
	}

	LRESULT OnBackupSelChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CComboBox cbmBox = GetDlgItem(IDC_BACKUP);
		if (cbmBox.GetCurSel() == -1)
		{
			return 0;
		}

		CListViewCtrl listViewCtrl = GetDlgItem(IDC_APP_LIST);
		// listViewCtrl.SetRedraw(FALSE);
		listViewCtrl.DeleteAllItems();
		// listViewCtrl.SetRedraw(TRUE);

		const BackupManifest& manifest = m_manifests[cbmBox.GetCurSel()];
		if (manifest.isEncrypted())
		{
			MsgBox(m_hWnd, IDS_ENC_BKP_NOT_SUPPORTED);
			return 0;
		}

		TCHAR buffer[MAX_PATH] = { 0 };
		DWORD dwRet = GetCurrentDirectory(MAX_PATH, buffer);
		if (dwRet == 0)
		{
			return 0;
		}

		SendMessage(WM_NEXTDLGCTL, 0, 0);
		CW2A resDir(CT2W(buffer), CP_UTF8);

		const std::vector<BackupManifest::AppInfo>& apps = manifest.getApps();
		for (std::vector<BackupManifest::AppInfo>::const_iterator it = apps.cbegin(); it != apps.cend(); ++it)
		{
			CW2T pszName(CA2W((*it).name.c_str(), CP_UTF8));
			LVITEM lvItem = {};
			lvItem.mask = LVIF_TEXT | LVIF_PARAM;
			lvItem.iItem = listViewCtrl.GetItemCount();
			lvItem.iSubItem = 0;
			lvItem.pszText = TEXT("");
			// lvItem.state = INDEXTOSTATEIMAGEMASK(2);
			// lvItem.stateMask = LVIS_STATEIMAGEMASK;
			int idx = std::distance(apps.cbegin(), it);
			LPARAM lParam = reinterpret_cast<LPARAM>(&(*it));
			lvItem.lParam = lParam;
			int nItem = listViewCtrl.InsertItem(&lvItem);

			listViewCtrl.AddItem(nItem, 1, pszName);
			
			CW2T pszBundleId(CA2W((*it).bundleId.c_str(), CP_UTF8));
			listViewCtrl.AddItem(nItem, 2, pszBundleId);
			
		}

		return 0;
	}

	LRESULT OnBnClickedCancel(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		if (MsgBox(m_hWnd, IDS_CANCEL_PROMPT, MB_YESNO) == IDNO)
		{
			return 0;
		}

		m_cancelled = true;

		return 0;
	}

	LRESULT OnBnClickedExport(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CComboBox cbmBox = GetDlgItem(IDC_BACKUP);
		if (cbmBox.GetCurSel() == -1)
		{
			MsgBox(m_hWnd, IDS_SEL_BACKUP_DIR);
			return 0;
		}

		const BackupManifest& manifest = m_manifests[cbmBox.GetCurSel()];
		if (manifest.isEncrypted())
		{
			MsgBox(m_hWnd, IDS_ENC_BKP_NOT_SUPPORTED);
			return 0;
		}

		CListViewCtrl listViewCtrl = GetDlgItem(IDC_APP_LIST);

		std::vector<std::string> domains;
		const std::vector<BackupManifest::AppInfo>& apps = manifest.getApps();
		for (int nItem = 0; nItem < listViewCtrl.GetItemCount(); nItem++)
		{
			if (!listViewCtrl.GetCheckState(nItem))
			{
				continue;
			}

			BackupManifest::AppInfo* app = reinterpret_cast<BackupManifest::AppInfo*>(listViewCtrl.GetItemData(nItem));
			if (NULL != app)
			{
				domains.push_back("AppDomain-" + app->bundleId);
			}
		}

		if (domains.empty())
		{
			MsgBox(m_hWnd, IDS_NO_SELECTED_APP);
		}

		CString text;
		text.LoadString(IDS_SEL_OUTPUT_DIR);
		CFolderDialog folder(NULL, text, BIF_RETURNONLYFSDIRS | BIF_USENEWUI);

		TCHAR outputDir[MAX_PATH] = { 0 };
		CRegKey rk;
		if (rk.Open(HKEY_CURRENT_USER, TEXT("Software\\iTunesBackup"), KEY_READ) == ERROR_SUCCESS)
		{
			ULONG chars = MAX_PATH;
			if (rk.QueryStringValue(TEXT("OutputDir"), outputDir, &chars) == ERROR_SUCCESS)
			{
				
			}
			rk.Close();
		}
		if (_tcscmp(outputDir, TEXT("")) == 0)
		{
			HRESULT result = SHGetFolderPath(NULL, CSIDL_MYDOCUMENTS, NULL, SHGFP_TYPE_CURRENT, outputDir);
		}
		if (_tcscmp(outputDir, TEXT("")) != 0)
		{
			folder.SetInitialFolder(outputDir);
		}

		if (IDOK != folder.DoModal())
		{
			return 0;
		}

		if (rk.Create(HKEY_CURRENT_USER, TEXT("Software\\iTunesBackup"), REG_NONE, REG_OPTION_NON_VOLATILE, KEY_READ | KEY_WRITE) == ERROR_SUCCESS)
		{
			rk.SetStringValue(TEXT("OutputDir"), folder.m_szFolderPath);
			rk.Close();
		}

		std::string backup = manifest.getPath();

		/*
		GetDlgItemText(IDC_OUTPUT, buffer, MAX_PATH);
		if (!::PathFileExists(buffer))
		{
			MsgBox(IDS_INVALID_OUTPUT_DIR);
			return 0;
		}
		*/
		std::string output((LPCSTR)CW2A(CT2W(folder.m_szFolderPath), CP_UTF8));

		m_cancelled.store(false);
		m_task = std::async(std::launch::async, &CView::exportApps, this, domains, backup, output);

		m_eventId = SetTimer(1, 200);

		EnableInteractiveCtrls(FALSE);
		CProgressBarCtrl progressCtrl = GetDlgItem(IDC_PROGRESS);
		progressCtrl.SetMarquee(TRUE, 0);

		return 0;
	}

	bool exportApps(const std::vector<std::string> domains, const std::string backup, const std::string output)
	{
		bool cancelled = false;
		for (auto it = domains.cbegin(); it != domains.cend(); ++it)
		{
			std::string domainOutput = combinePath(output, *it);
			makeDirectory(domainOutput);

			ITunesDb* iTunesDb = new ITunesDb(backup, "Manifest.db");
			if (iTunesDb->load(*it))
			{
				iTunesDb->enumFiles(std::bind(&CView::handleFile, this, std::cref(domainOutput), std::placeholders::_1, std::placeholders::_2));
			}
			delete iTunesDb;

			cancelled = m_cancelled.load();
			if (cancelled)
			{
				break;
			}
		}

		cancelled = m_cancelled.load();
		return cancelled ? false : true;
	}

	bool exportWechat(const std::string backup, const std::string output)
	{
		// AppDomain-com.tencent.ww
		std::vector<std::string> domains;
		domains.push_back("AppDomain-com.tencent.xin");
		domains.push_back("AppDomainGroup-group.com.tencent.xin");

		return exportApps(domains, backup, output);
	}

	bool handleFile(const std::string& output, const ITunesDb* iTunesDb, ITunesFile* file)
	{
		std::string relativePath = file->relativePath;
		normalizePath(relativePath);
		std::string dest = combinePath(output, relativePath);
		if (!dest.empty())
		{
			if (file->isDir())
			{
				makeDirectory(dest);
			}
			else
			{
				copyFile(iTunesDb->getRealPath(file), dest, true);
			}

			unsigned int modifiedTime = ITunesDb::parseModifiedTime(file->blob);
			if (modifiedTime > 0)
			{
				updateFileTime(dest, modifiedTime);
			}
		}

		bool cancelled = m_cancelled.load();
		return cancelled ? false : true;
	}

	LRESULT OnTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		std::future_status status = m_task.wait_for(std::chrono::seconds(0));
		if (status == std::future_status::ready)
		{
			KillTimer(m_eventId);
			m_eventId = 0;

			CProgressBarCtrl progressCtrl = GetDlgItem(IDC_PROGRESS);
			progressCtrl.SetMarquee(FALSE, 0);
			EnableInteractiveCtrls(TRUE);

			bool cancelled = m_cancelled.load();
			
			MsgBoxTimeout(m_hWnd, cancelled ? IDS_CANCELLED : IDS_FINISHED, 10000);
		}

		return 0;
	}

	LRESULT OnHeaderItemStateIconClick(int idCtrl, LPNMHDR pnmh, BOOL& /*bHandled*/)
	{
		CHeaderCtrl header = m_appListCtrl.GetHeader();
		if (pnmh->hwndFrom == header.m_hWnd)
		{
			LPNMHEADER pnmHeader = (LPNMHEADER)pnmh;
			if (pnmHeader->pitem->mask & HDI_FORMAT && pnmHeader->pitem->fmt & HDF_CHECKBOX)
			{
				CheckAllItems(m_appListCtrl, !(pnmHeader->pitem->fmt & HDF_CHECKED));
				SyncHeaderCheckbox();
				return 1;
			}
		}

		return 0;
	}

	LRESULT OnListItemChanged(int idCtrl, LPNMHDR pnmh, BOOL& /*bHandled*/)
	{
		LPNMLISTVIEW pnmlv = (LPNMLISTVIEW)pnmh;

		if (pnmlv->uChanged & LVIF_STATE)
		{
			if (pnmlv->iItem == m_itemClicked)
			{
				SyncHeaderCheckbox();
				m_itemClicked = -2;
			}

		}
		return 0;
	}

	LRESULT OnListClick(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled)
	{
		LPNMITEMACTIVATE pnmia = (LPNMITEMACTIVATE)pnmh;
		DWORD pos = GetMessagePos();
		POINT pt = { LOWORD(pos), HIWORD(pos) };
		CListViewCtrl listViewCtrl = GetDlgItem(IDC_APP_LIST);

		listViewCtrl.ScreenToClient(&pt);
		UINT flags = 0;
		int nItem = listViewCtrl.HitTest(pt, &flags);
		if (flags == LVHT_ONITEMSTATEICON)
		{
			m_itemClicked = nItem;
			// listViewCtrl.SetCheckState(nItem, !listViewCtrl.GetCheckState(nItem));
			// SetHeaderCheckbox();
			// bHandled = TRUE;
		}

		return 0;
	}

	void SyncHeaderCheckbox()
	{
		// Loop through all of our items.  If any of them are
		// unchecked, we'll want to uncheck the header checkbox.
		CListViewCtrl listViewCtrl = GetDlgItem(IDC_APP_LIST);
		BOOL fChecked = TRUE;
		for (int nItem = 0; nItem < ListView_GetItemCount(listViewCtrl); nItem++)
		{
			if (!ListView_GetCheckState(listViewCtrl, nItem))
			{
				fChecked = FALSE;
				break;
			}
		}

		SetHeaderCheckState(listViewCtrl, fChecked);
	}

	void EnableInteractiveCtrls(BOOL enabled)
	{
		::EnableWindow(GetDlgItem(IDC_BACKUP), enabled);
		::EnableWindow(GetDlgItem(IDC_CHOOSE_BKP), enabled);
		::EnableWindow(GetDlgItem(IDC_CHOOSE_OUTPUT), enabled);
		::EnableWindow(GetDlgItem(IDC_EXPORT), enabled);
		
		::EnableWindow(GetDlgItem(IDC_CANCEL), !enabled);
		//::ShowWindow(GetDlgItem(IDC_CLOSE), enabled ? SW_SHOW : SW_HIDE);
	}

	void UpdateBackups(const std::vector<BackupManifest>& manifests)
	{
		int selectedIndex = -1;
		if (!manifests.empty())
		{
			// Add default backup folder
			for (std::vector<BackupManifest>::const_iterator it = manifests.cbegin(); it != manifests.cend(); ++it)
			{
				std::vector<BackupManifest>::const_iterator it2 = std::find(m_manifests.cbegin(), m_manifests.cend(), *it);
				if (it2 != m_manifests.cend())
				{
					if (selectedIndex == -1)
					{
						selectedIndex = static_cast<int>(std::distance(it2, m_manifests.cbegin()));
					}
				}
				else
				{
					m_manifests.push_back(*it);
					if (selectedIndex == -1)
					{
						selectedIndex = static_cast<int>(m_manifests.size() - 1);
					}
				}
			}

			// update
			CComboBox cmb = GetDlgItem(IDC_BACKUP);
			cmb.SetRedraw(FALSE);
			cmb.ResetContent();
			for (std::vector<BackupManifest>::const_iterator it = m_manifests.cbegin(); it != m_manifests.cend(); ++it)
			{
				std::string itemTitle = it->toString();
				// String* item = [NSString stringWithUTF8String : itemTitle.c_str()];
				CW2T item(CA2W(it->toString().c_str(), CP_UTF8));
				cmb.AddString((LPCTSTR)item);
			}
			if (selectedIndex != -1 && selectedIndex < cmb.GetCount())
			{
				SetComboBoxCurSel(m_hWnd, cmb, selectedIndex);
			}
			cmb.SetRedraw(TRUE);
		}
	}

	void InitializeAppList()
	{
		m_appListCtrl.SubclassWindow(GetDlgItem(IDC_APP_LIST));

		CString strColumn1;
		CString strColumn2;
		CString strColumn3;

		strColumn1.LoadString(IDS_APP_NAME);
		strColumn2.LoadString(IDS_APP_BUNDLEID);
		// strColumn3.LoadString(IDS_SESSION_USER);

		DWORD dwStyle = m_appListCtrl.GetExStyle();
		dwStyle |= LVS_EX_FULLROWSELECT | LVS_EX_LABELTIP | LVS_EX_GRIDLINES | LVS_EX_CHECKBOXES | LVS_EX_DOUBLEBUFFER;
		m_appListCtrl.SetExtendedListViewStyle(dwStyle, dwStyle);

		LVCOLUMN lvc = { 0 };
		ListView_InsertColumn(m_appListCtrl, 0, &lvc);
		lvc.mask = LVCF_TEXT | LVCF_WIDTH;
		lvc.iSubItem++;
		lvc.pszText = (LPTSTR)(LPCTSTR)strColumn1;
		lvc.cx = 392;
		ListView_InsertColumn(m_appListCtrl, 1, &lvc);
		
		
		lvc.iSubItem++;
		lvc.pszText = (LPTSTR)(LPCTSTR)strColumn2;
		lvc.cx = 256;
		ListView_InsertColumn(m_appListCtrl, 2, &lvc);
		
		/*
		lvc.iSubItem++;
		lvc.pszText = (LPTSTR)(LPCTSTR)strColumn3;
		lvc.cx = 192;
		ListView_InsertColumn(m_appListCtrl, 3, &lvc);
		*/

		// Set column widths
		ListView_SetColumnWidth(m_appListCtrl, 0, LVSCW_AUTOSIZE_USEHEADER);
		// ListView_SetColumnWidth(m_appListCtrl, 2, LVSCW_AUTOSIZE_USEHEADER);
		// ListView_SetColumnWidth(listViewCtrl, 1, LVSCW_AUTOSIZE_USEHEADER);
		// ListView_SetColumnWidth(listViewCtrl, 2, LVSCW_AUTOSIZE_USEHEADER);
		// ListView_SetColumnWidth(listViewCtrl, 3, LVSCW_AUTOSIZE_USEHEADER);
		m_appListCtrl.SetColumnSortType(0, LVCOLSORT_NONE);
		// m_appListCtrl.SetColumnSortType(2, LVCOLSORT_LONG);
		// m_appListCtrl.SetColumnSortType(3, LVCOLSORT_NONE);

		HWND header = ListView_GetHeader(m_appListCtrl);
		DWORD dwHeaderStyle = ::GetWindowLong(header, GWL_STYLE);
		dwHeaderStyle |= HDS_CHECKBOXES;
		::SetWindowLong(header, GWL_STYLE, dwHeaderStyle);

		HDITEM hdi = { 0 };
		hdi.mask = HDI_FORMAT;
		Header_GetItem(header, 0, &hdi);
		hdi.fmt |= HDF_CHECKBOX | HDF_FIXEDWIDTH;
		Header_SetItem(header, 0, &hdi);
	}


	CString GetDefaultBackupDir(BOOL bCheckExistence = TRUE)
	{
		CString backupDir;
		TCHAR szPath[2][MAX_PATH] = { { 0 }, { 0 } };

		// Check iTunes Folder
		HRESULT hr = SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, szPath[0]);
		_tcscat(szPath[0], TEXT("\\Apple Computer\\MobileSync\\Backup"));

		// iTunes App from MS Store
		hr = SHGetFolderPath(NULL, CSIDL_PROFILE, NULL, SHGFP_TYPE_CURRENT, szPath[1]);
		_tcscat(szPath[1], TEXT("\\Apple\\MobileSync\\Backup"));

		for (int idx = 0; idx < 2; ++idx)
		{
			DWORD dwAttrib = ::GetFileAttributes(szPath[idx]);
			if (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY))
			{
				backupDir = szPath[idx];
				break;
			}
		}

		if (!bCheckExistence && backupDir.IsEmpty())
		{
			backupDir = szPath[0];
		}

		return backupDir;
	}

};
