/**
 * The application was developed by Christoph Regner.
 * On the web: https://www.cregx.de
 * For further information, please refer to the attached LICENSE.md
 * 
 * The MIT License (MIT)
 * Copyright (c) 2024-2026 Christoph Regner
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/**
 * This app is based on a skeleton code from
 * https://www.codeproject.com/Articles/227831/A-Dialog-Based-Win32-C-Program-Step-by-Step
 * by (c) Rodrigo Cesar de Freitas Dias https://creativecommons.org/licenses/publicdomain/
 * Thank you for the first class source of inspiration.
 **/

/**
 * Win32-based UI application to erase the primary hard disk as part
 * of a Lite Touch installation (WinPE / MDT). Build with Visual Studio 2010.
 **/
#define WIN32_LEAN_AND_MEAN
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#define _Win32_DCOM
#define MAX_LOADSTRING 512
#define MAX_BUFFER_SIZE 1024
#define MAX_DISKS 10
#define ID_TIMER1 1
#define MIN_LOGICAL_DRIVES_PXE 2	// Specifies the minimum number of logical drives required by the application
									// to perform a cleanup operation within a PXE-based installation.
#define MIN_LOGICAL_DRIVES 3		// minimum number of logical drives required within a Non-PXE-based installation

// Define missing Storage Bus Types.
#if !defined(BusTypeSpaces)
	#define BusTypeSpaces (STORAGE_BUS_TYPE)0x10
#endif
#if !defined(BusTypeNvme)
	#define BusTypeNvme (STORAGE_BUS_TYPE)0x11
#endif
#if !defined(BusTypeScm)
	#define BusTypeScm (STORAGE_BUS_TYPE)0x12
#endif

#include <Windows.h>
#include <CommCtrl.h>
#include <CommDlg.h>				// Dialogs
#include <tchar.h>
#include <Shlwapi.h>				// e.g. For example for shortening long paths. Needed: Shlwapi.lib static linked.
#include <strsafe.h>				// e.g. Safe string copy
#include <ShellAPI.h>				// e.g. SHELLEXECUTEINFO
#include <io.h>					// e.g. _access_s (file exists)
#include <winioctl.h>				// e.g. STORAGE_PROPERTY_QUERY
#include <stddef.h>					// e.g. offsetof

#include <Shobjidl.h>				// COM objects
#include <Objbase.h>

#include "resource.h"

// Enabling visual styles.
#pragma comment(linker, \
  "\"/manifestdependency:type='Win32' "\
  "name='Microsoft.Windows.Common-Controls' "\
  "version='6.0.0.0' "\
  "processorArchitecture='*' "\
  "publicKeyToken='6595b64144ccf1df' "\
  "language='*'\"")

// static link ComCtl32.lib.
#pragma comment(lib, "ComCtl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "propsys.lib")

typedef enum { TERMINATE, CLEANUP } action_type;	// 0 for exit, 1 for cleanup.
typedef struct _DiskInfo {
	int diskIndex;
	ULONGLONG totalBytes;
	TCHAR totalGB[32];
	TCHAR model[256];
	TCHAR busType[64];
	TCHAR serialNumber[256];
	BOOL isRemovable;
	BOOL isSSD;
	BOOL isPhysicalDisk;
	BOOL isCleaned;
} DiskInfo;

INT_PTR CALLBACK DialogProc(HWND, UINT, WPARAM, LPARAM);
void onAction(HWND, action_type);
void onClose(HWND);
void onInit(HWND, WPARAM);
void onClick_ButtonCmd(void);
const TCHAR * GetOwnPath(void);
const TCHAR * GetActionPath(TCHAR *, TCHAR *);
void Action(const TCHAR *, const TCHAR *, int);
DWORD ActionEx(const TCHAR *, TCHAR *, ...);
BOOL FileExists(const wchar_t *);
BOOL DirExists(const TCHAR *);
void ShowLogo(HWND);
const TCHAR * GetLogicalDriveList(DWORD *);
BOOL IsDiskCleaned(const TCHAR *, const TCHAR *);
void SetCancelBtnAsDefault(HWND);
int GetPhysicalDisks(void);
LPCTSTR GetBusTypeName(STORAGE_BUS_TYPE);
BOOL GetPhysicalDiskInfo(int, DiskInfo *);
void TrimTrailingSpaces(TCHAR *);
void InitComboBox_PhysicalDisks(HWND, HWND);
BOOL CreateDiskpartFile(int);

static HBITMAP hBitmapLogo;
static HWND hwndLogo;

const TCHAR * g_pstrExePath;				// Full path to the exe
const TCHAR * g_pstrActionPath;				// Full path to the batch file
const TCHAR * g_pstrActionInstPath;			// Full path to the file with instructions (diskpart.txt) for the batch file
action_type g_currentAction;				// Cleanup or terminate action should be performed?

WCHAR szAppDialogTitle[MAX_LOADSTRING];
WCHAR szAppActionRequest[MAX_LOADSTRING];
WCHAR szAppActionExpl[MAX_LOADSTRING];
WCHAR szAppVersion[MAX_LOADSTRING];
WCHAR szRunActionText[MAX_LOADSTRING];
WCHAR szActionWarning[MAX_LOADSTRING];
WCHAR szActionSuccessful[MAX_LOADSTRING];
WCHAR szActionFailed[MAX_LOADSTRING];
WCHAR szActionDiskSelectionFailed[MAX_LOADSTRING];
WCHAR szActionCancelled[MAX_LOADSTRING];
WCHAR szActionFileNotFound[MAX_LOADSTRING];
WCHAR szBtnActionCaption[MAX_LOADSTRING];
WCHAR szBtnCancelCaption[MAX_LOADSTRING];
WCHAR szBtnCancelCaptionWithTimer[MAX_LOADSTRING];
WCHAR szAppLang[MAX_LOADSTRING];
WCHAR szGrpLogicalDrives[MAX_LOADSTRING];
WCHAR szLogicalDrivesList[MAX_LOADSTRING];
WCHAR szLogicalDrivesListIsEmpty[MAX_LOADSTRING];
WCHAR szNotEnoughLogicalDrivesFound[MAX_LOADSTRING];
WCHAR szNoteDiskAlreadyCleaned[MAX_LOADSTRING];
WCHAR szCheckBoxRestartCaption[MAX_LOADSTRING];
WCHAR szMissingInternalDrive[MAX_LOADSTRING];
WCHAR szGrpPhysicalDrives[MAX_LOADSTRING];
WCHAR szPhysicalDrivesList[MAX_LOADSTRING];

// Don't forget to increase the version number in the resource file (cleanup.rc).
#ifdef NLS
const LPCWSTR szAppVer = TEXT("1.4.1 (%s) / 26. Juni 2026");
#else
const LPCWSTR szAppVer = TEXT("1.4.1 (%s) / 26 June 2026");
#endif

const LPCWSTR szBatchFileName = TEXT("action.bat");
const LPCWSTR szBatchParams	= TEXT("diskpart.txt");
const LPCWSTR szRestartExe	= TEXT("wpeutil.exe");
const LPCWSTR szRestartExeParams= TEXT("reboot");
const LPCWSTR szDiskLetter = TEXT("C:\\");				// Drive letter of the primary disc, usually C:\.
const LPCWSTR szDiskLabel = TEXT("cleaned");				// Temporary label of the primary disc (volume) after 'Cleanup' has cleaned the disk.
									// See also in the diskpart.txt.
const LPCWSTR szPathDeployDir = TEXT("C:\\Deploy");
INT g_iCounter = 30;							// Countdawn timer counter that uses the time (seconds) to automatically exit 'Cleanup'.
BOOL g_bTimerIsCreated;							// Signals whether a timer has been created.

const DWORD RUN_ACTION_SHELLEX_FAILED = 0xFFFFFFFFFFFFFFFF;		// dec => -1 (Function internal error, use GetLastError().)
const DWORD RUN_ACTION_SHELLEX_FILE_NOT_FOUND = 0xFFFFFFFFFFFFFFFE;	// dec => -2 (Action file not found.)
const DWORD RUN_ACTION_SUCCESSFUL = 0x400;			// dec => 1024 (Successful processing of the batch file.)
const DWORD RUN_ACTION_CANCELLED_BY_USER= 0xC000013A;			// dec => 3221225786
const DWORD RUN_ACTION_CREATE_DISKPART_FILE_FAILED = 0xFFFFFFFFFFFFFFFC; // dec => -4 (Action diskpart file could not be created)
									// (Cancellation of the batch job by the user,
									// e.g. because the user has clicked the X button.)

// Main function
int WINAPI _tWinMain(HINSTANCE hInst, HINSTANCE h0, LPTSTR lpCmdLine, int nCmdShow)
{
	HWND hDlg;
	MSG msg;
	BOOL ret;

	#ifdef NLS
	LoadString(hInst, IDS_APP_LANG_NLS, szAppLang, MAX_LOADSTRING);
	LoadString(hInst, IDS_BTN_ACTION_CAPTION_NLS, szBtnActionCaption, MAX_LOADSTRING);
	LoadString(hInst, IDS_BTN_CANCEL_CAPTION_NLS, szBtnCancelCaption, MAX_LOADSTRING);
	LoadString(hInst, IDS_APP_ACTION_EXPLANATION_NLS, szAppActionExpl, MAX_LOADSTRING);
	LoadString(hInst, IDS_APP_ACTION_REQUEST_NLS, szAppActionRequest, MAX_LOADSTRING);
	LoadString(hInst, IDS_TITLE_NLS, szAppDialogTitle, MAX_LOADSTRING);
	LoadString(hInst, IDS_APPV, szAppVersion, MAX_LOADSTRING);
	LoadString(hInst, IDS_RUN_ACTION_TITLE_NLS, szRunActionText, MAX_LOADSTRING);
	LoadString(hInst, IDS_RUN_ACTION_WARNING_NLS, szActionWarning, MAX_LOADSTRING);
	LoadString(hInst, IDS_RUN_ACTION_FAILED_NLS, szActionFailed, MAX_LOADSTRING);
	LoadString(hInst, IDS_RUN_ACTION_DISK_SELECTION_FAILED_NLS, szActionDiskSelectionFailed, MAX_LOADSTRING);
	LoadString(hInst, IDS_RUN_ACTION_CANCELED_NLS, szActionCancelled, MAX_LOADSTRING);
	LoadString(hInst, IDS_RUN_ACTION_FILE_NOT_FOUND_NLS, szActionFileNotFound, MAX_LOADSTRING);
	LoadString(hInst, IDS_GRP_LOGICAL_DRIVES_NLS, szGrpLogicalDrives, MAX_LOADSTRING);
	LoadString(hInst, IDS_LOGICAL_DRIVES_LIST_NLS, szLogicalDrivesList, MAX_LOADSTRING);
	LoadString(hInst, IDS_LOGICAL_DRIVES_LIST_IS_EMPTY_NLS, szLogicalDrivesListIsEmpty, MAX_LOADSTRING);
	LoadString(hInst, IDS_NOT_ENOUGH_LOGICAL_DRIVES_FOUND_NLS,szNotEnoughLogicalDrivesFound, MAX_LOADSTRING);
	LoadString(hInst, IDS_BTN_CANCEL_CAPTION_WITH_TIMER_NLS, szBtnCancelCaptionWithTimer, MAX_LOADSTRING);
	LoadString(hInst, IDS_APP_NOTE_DISK_ALREADY_CLEANED_NLS, szNoteDiskAlreadyCleaned, MAX_LOADSTRING);
	LoadString(hInst, IDS_CB_RESTART_NLS, szCheckBoxRestartCaption, MAX_LOADSTRING);
	LoadString(hInst, IDS_INTERNAL_DRIVE_IS_MISSING_NLS, szMissingInternalDrive, MAX_LOADSTRING);
	LoadString(hInst, IDS_GRP_PHYSICAL_DRIVES_NLS, szGrpPhysicalDrives, MAX_LOADSTRING);
	LoadString(hInst, IDS_PHYSICAL_DRIVES_LIST_NLS, szPhysicalDrivesList, MAX_LOADSTRING);
	#else
	LoadString(hInst, IDS_APP_LANG, szAppLang, MAX_LOADSTRING);
	LoadString(hInst, IDS_BTN_ACTION_CAPTION, szBtnActionCaption, MAX_LOADSTRING);
	LoadString(hInst, IDS_BTN_CANCEL_CAPTION, szBtnCancelCaption, MAX_LOADSTRING);
	LoadString(hInst, IDS_APP_ACTION_EXPLANATION, szAppActionExpl, MAX_LOADSTRING);
	LoadString(hInst, IDS_TITLE, szAppDialogTitle, MAX_LOADSTRING);
	LoadString(hInst, IDS_APP_ACTION_REQUEST, szAppActionRequest, MAX_LOADSTRING);
	LoadString(hInst, IDS_APPV, szAppVersion, MAX_LOADSTRING);
	LoadString(hInst, IDS_RUN_ACTION_TITLE, szRunActionText, MAX_LOADSTRING);
	LoadString(hInst, IDS_RUN_ACTION_WARNING, szActionWarning, MAX_LOADSTRING);
	LoadString(hInst, IDS_RUN_ACTION_FAILED, szActionFailed, MAX_LOADSTRING);
	LoadString(hInst, IDS_RUN_ACTION_DISK_SELECTION_FAILED, szActionDiskSelectionFailed, MAX_LOADSTRING);
	LoadString(hInst, IDS_RUN_ACTION_CANCELED, szActionCancelled, MAX_LOADSTRING);
	LoadString(hInst, IDS_RUN_ACTION_FILE_NOT_FOUND, szActionFileNotFound, MAX_LOADSTRING);
	LoadString(hInst, IDS_GRP_LOGICAL_DRIVES, szGrpLogicalDrives, MAX_LOADSTRING);
	LoadString(hInst, IDS_LOGICAL_DRIVES_LIST, szLogicalDrivesList, MAX_LOADSTRING);
	LoadString(hInst, IDS_LOGICAL_DRIVES_LIST_IS_EMPTY, szLogicalDrivesListIsEmpty, MAX_LOADSTRING);
	LoadString(hInst, IDS_NOT_ENOUGH_LOGICAL_DRIVES_FOUND,szNotEnoughLogicalDrivesFound, MAX_LOADSTRING);
	LoadString(hInst, IDS_BTN_CANCEL_CAPTION_WITH_TIMER, szBtnCancelCaptionWithTimer, MAX_LOADSTRING);
	LoadString(hInst, IDS_APP_NOTE_DISK_ALREADY_CLEANED, szNoteDiskAlreadyCleaned, MAX_LOADSTRING);
	LoadString(hInst, IDS_CB_RESTART, szCheckBoxRestartCaption, MAX_LOADSTRING);
	LoadString(hInst, IDS_INTERNAL_DRIVE_IS_MISSING, szMissingInternalDrive, MAX_LOADSTRING);
	LoadString(hInst, IDS_GRP_PHYSICAL_DRIVES, szGrpPhysicalDrives, MAX_LOADSTRING);
	LoadString(hInst, IDS_PHYSICAL_DRIVES_LIST, szPhysicalDrivesList, MAX_LOADSTRING);
	#endif

	InitCommonControls();
	hDlg = CreateDialogParam(hInst, MAKEINTRESOURCE(IDD_DIALOG1), 0, DialogProc, 0);
	ShowWindow(hDlg, nCmdShow);  // ShowWindows() need usually UpdateWindow() if you need to force the WM_PAINT.

	while((ret = GetMessage(&msg, 0, 0, 0)) != 0) {
		if(ret == -1)
		{
			return -1;
		}

		if(!IsDialogMessage(hDlg, &msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	return 0;
}

/**
 * Dialog box notifications procedure.
 */
INT_PTR CALLBACK DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	TCHAR szBuffer[MAX_LOADSTRING];
	static HBRUSH hBrushStatic;
	static COLORREF colorRef[1] = { RGB(255, 0, 0) };
	static RECT rectIDCANCEL;
		
	switch(uMsg)
	{
		case WM_INITDIALOG:
			// Create a logo image.
			ShowLogo(hwndDlg);
			onInit(hwndDlg, wParam);
			// Get information to redraw the IDCANCEL button when using the timer.  
			GetClientRect(GetDlgItem(hwndDlg, IDCANCEL), &rectIDCANCEL);
			/**
			 * Don't forget to return FALSE in WM_INITDIALOG, as the focus should be set to IDCANCEL
			 * (at least if the clean-up process has taken place before).
			 */
			return FALSE;
		case WM_PAINT:
			// For more information, see under: https://learn.microsoft.com/en-us/windows/win32/gdi/wm-paint
			if (g_bTimerIsCreated == TRUE && g_iCounter >= 0)
			{
				_stprintf_s(szBuffer, MAX_LOADSTRING, szBtnCancelCaptionWithTimer, g_iCounter);
				SetDlgItemText(hwndDlg, IDCANCEL, szBuffer);
			}
			// The timer has expired. The window should be closed now.
			if (g_bTimerIsCreated == TRUE && g_iCounter < 0)
			{
				onClose(hwndDlg);
			}
			return FALSE;
		case WM_COMMAND:
			switch(LOWORD(wParam))
			{
				// Click on the action button for cleanup.
				case IDC_BUTTON_CLEANUP:
					onAction(hwndDlg, g_currentAction);
					return TRUE;
				// Click on the action button for cmd run.
				case IDC_BUTTON_CMD:
					onClick_ButtonCmd();
					return TRUE;
				// Click on the close button (X).
				case IDCANCEL:
					onClose(hwndDlg); 
					return TRUE;
			}
			break;
		case WM_CTLCOLORSTATIC:
			// Color text note for IDC_STATIC_CLEANUP_IMPORTANT_NOTES in red.
			if ((HWND) lParam == GetDlgItem(hwndDlg, IDC_STATIC_CLEANUP_IMPORTANT_NOTES))
			{
				// I use a system brush (GetStockObject()) to display the note text in the color red.
				// This does not then have to be made explicitly free (DeleteObject()).
				// See also under: https://learn.microsoft.com/de-de/windows/win32/controls/wm-ctlcolorstatic
				hBrushStatic = (HBRUSH) GetStockObject(HOLLOW_BRUSH);
				SetBkMode((HDC) wParam, TRANSPARENT);
				SetTextColor((HDC) wParam, colorRef[0]);
				return (INT_PTR) hBrushStatic;
			}
			break;
		case WM_CLOSE:
			// I'm not sure if I need to free up memory on an explicitly
			// allocated memory when I exit the program - so I just do that.
			if (g_pstrExePath != NULL)
			{
				free((void *) g_pstrExePath);
				g_pstrExePath = NULL;
			}
			
			if (g_pstrActionPath != NULL)
			{
				free((void *) g_pstrActionPath);
				g_pstrActionPath = NULL;
			}

			if (g_pstrActionInstPath != NULL)
			{
				free((void *) g_pstrActionInstPath);
				g_pstrActionInstPath = NULL;
			}

			// Close the application.
			DestroyWindow(hwndDlg);
			return TRUE;
		case WM_TIMER:
			// For more information, see under: https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-timer
			switch (wParam)
			{
				case ID_TIMER1:
					g_iCounter--;
					// Cause the IDCANCEL button to be redrawn after the value (countdown) in the timer is updated.
					InvalidateRect(hwndDlg, &rectIDCANCEL, FALSE);
					break;
			}
			return FALSE;
		case WM_DESTROY:
			// Don't forget to kill the timer.
			if (g_bTimerIsCreated == TRUE)
			{
				KillTimer(hwndDlg, ID_TIMER1);
			}
			PostQuitMessage(0);
			return TRUE;
	}
	return FALSE;
}

/**
 * Event that is called when the dialog window is initialized.
 */
void onInit(HWND hwndDlg, WPARAM wParam)
{
	TCHAR szBuffer[MAX_BUFFER_SIZE];
	TCHAR szNotes[MAX_BUFFER_SIZE];
	TCHAR szVersion[128];
	const TCHAR * szLogicalDrives;
	DWORD countLogicalDrives = 0;

	g_currentAction = CLEANUP;

	// Get the list of all logical drives in the system. 
	szLogicalDrives = GetLogicalDriveList(&countLogicalDrives);
	
	// Get the list of all physical disks in the systemn.
	InitComboBox_PhysicalDisks(hwndDlg, GetDlgItem(hwndDlg, IDC_COMBO_PHYSICAL_DISKS));
	
	if (countLogicalDrives > 0)
	{
		_stprintf_s(szBuffer, MAX_BUFFER_SIZE, szLogicalDrivesList, szLogicalDrives);
		// Fix: #5, #6
		if (countLogicalDrives <= 1)
		{
			_stprintf_s(szNotes, MAX_BUFFER_SIZE, szNotEnoughLogicalDrivesFound, countLogicalDrives, MIN_LOGICAL_DRIVES_PXE, MIN_LOGICAL_DRIVES);
			SetDlgItemText(hwndDlg, IDC_STATIC_CLEANUP_IMPORTANT_NOTES, szNotes);
			ShowWindow(GetDlgItem(hwndDlg, IDC_STATIC_CLEANUP_IMPORTANT_NOTES), SW_SHOW);			
			UpdateWindow(hwndDlg);
			EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_CLEANUP), FALSE);
			SetCancelBtnAsDefault(hwndDlg);
		}
		else if (countLogicalDrives == MIN_LOGICAL_DRIVES_PXE)
		{
			/**
			 * PXE-based or USB-/DVD-based deployment:
			 *
			 * Only a minimum of logical drives were detected (see under MIN_LOGICAL_DRIVES_PXE).
			 * We now need to verify the presence of at least 1 internal hard drive.
			 * This is to prevent accidental deletion of the USB drive during Non-PXE-based installations.
			 * The check is performed in such a way that the USB drive is recognized as the first drive
			 * if no internal hard drives are detected and the specific 'Deploy' directory exists on drive C:\.
			 */

			/**
			 * Check if drive C:\ exists, or if it does, whether it contains the directory C:\Deploy,
			 * as this would indicate that no internal logical hard drive is present during a non-PXE installation.
			 */
			if (DirExists(szDiskLetter) == FALSE || DirExists(szPathDeployDir) == TRUE)
			{
				/**
				 * It appears that an internal hard drive is missing.
				 * Therefore, the execution of a cleanup process by disabling the corresponding button is prevented.
				 */
				_stprintf_s(szNotes, MAX_BUFFER_SIZE, szMissingInternalDrive);
				SetDlgItemText(hwndDlg, IDC_STATIC_CLEANUP_IMPORTANT_NOTES, szNotes);
				ShowWindow(GetDlgItem(hwndDlg, IDC_STATIC_CLEANUP_IMPORTANT_NOTES), SW_SHOW);			
				UpdateWindow(hwndDlg);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_CLEANUP), FALSE);
				SetCancelBtnAsDefault(hwndDlg);
			}
		}
	}
	else
	{
		// No logical drives found (should never occur).
		EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_CLEANUP), FALSE);
		_stprintf_s(szBuffer, MAX_BUFFER_SIZE, szLogicalDrivesList, szLogicalDrivesListIsEmpty);
		UpdateWindow(hwndDlg);
	}
	SetDlgItemText(hwndDlg, IDC_STATIC_LOGICAL_DRIVES, szBuffer);
	
	/**
	 * Check if the cleanup action has already taken place.
	 * In this case, the application will automatically exit after a specified period of time.
	 * For this purpose, it is checked whether the label of the volume C:\ has the temporary name "cleaned".
	 * To differentiate: After a (complete) Windows installation, the volume C:\ has the label "windows".
	 */
	if (IsDiskCleaned(szDiskLetter, szDiskLabel) == TRUE)
	{
		// Show notice that Cleanup has already run.
		SetDlgItemText(hwndDlg, IDC_STATIC_CLEANUP_IMPORTANT_NOTES, szNoteDiskAlreadyCleaned);
		ShowWindow(GetDlgItem(hwndDlg, IDC_STATIC_CLEANUP_IMPORTANT_NOTES), SW_SHOW);
		UpdateWindow(hwndDlg);
		SetCancelBtnAsDefault(hwndDlg);

		// Create a timer to automatically close the application.
		// This timer has an interval of 1 second (1000 ms), i.e. it receives a notification of type WM_TIMER 1x per second..
		// For more information, see under: https://learn.microsoft.com/en-us/windows/win32/winmsg/using-timers
		g_bTimerIsCreated = (BOOL)SetTimer(hwndDlg, ID_TIMER1, 1000, (TIMERPROC) NULL);
	}
	
	// Get the module path.
	g_pstrExePath = GetOwnPath();
	
	// Get the batch file path (action.bat).
	g_pstrActionPath = GetActionPath((TCHAR *)g_pstrExePath, (TCHAR *)szBatchFileName);

	// Get the full file path for the batch instructions file (diskpart.txt).
	g_pstrActionInstPath = GetActionPath((TCHAR *)g_pstrExePath, (TCHAR *)szBatchParams);
	
	// Init controls.
	SetWindowText(hwndDlg, szAppDialogTitle);
	SetDlgItemText(hwndDlg, IDC_STATIC_REQUEST, szAppActionRequest);
	SetDlgItemText(hwndDlg, IDC_STATIC_EXPLANATION, szAppActionExpl);
	SetDlgItemText(hwndDlg, IDC_STATIC_GROUP_LOGICAL_DRIVES, szGrpLogicalDrives);
	SetDlgItemText(hwndDlg, IDC_BUTTON_CLEANUP, szBtnActionCaption);
	SetDlgItemText(hwndDlg, IDCANCEL, szBtnCancelCaption);
	SetDlgItemText(hwndDlg, IDC_CHECKBOX_RESTART, szCheckBoxRestartCaption);
	SetDlgItemText(hwndDlg, IDC_STATIC_GROUP_PHYSICAL_DRIVES, szGrpPhysicalDrives);
	SetDlgItemText(hwndDlg, IDC_STATIC_PHYSICAL_DRIVES, szPhysicalDrivesList);

	// Initialization of the restart option checkbox (checked).
	SendMessage(GetDlgItem(hwndDlg, IDC_CHECKBOX_RESTART), BM_SETCHECK, 1, 0L);

	// App version.
	_stprintf_s(szVersion, 128, szAppVer, szAppLang);  
	_stprintf_s(szBuffer, MAX_BUFFER_SIZE, szAppVersion, szVersion);
	SetWindowText(GetDlgItem(hwndDlg, IDC_STATIC_APP_INFO), szBuffer);

	// Release allocated memory. 
	if (szLogicalDrives != NULL)
	{
		free((void *) szLogicalDrives);
		szLogicalDrives = NULL;
	}
}

/**
 * The event that is triggered when the dialog box is to be closed.
 */
void onClose(HWND hwnd)
{
	SendMessage(hwnd, WM_CLOSE, 0, 0L);
}

/**
 * Event fired when the action button is clicked.
 */
void onAction(HWND hDlg, action_type action)
{ 
	TCHAR szBuffer[MAX_BUFFER_SIZE];
	DWORD rcRunAction = 0;
	BOOL bSystemRestart = FALSE;
	int cbPosition = 0;
	int selectedDiskIndex = -1;

	memset(szBuffer, 0, sizeof(szBuffer));

	// Check if the timer is running, then stop it and change the caption of the 'skip' button to the default value.
	if (g_bTimerIsCreated == TRUE)
	{
		KillTimer(hDlg, ID_TIMER1);
		g_bTimerIsCreated = FALSE;
		SetDlgItemText(hDlg, IDCANCEL, szBtnCancelCaption);
		UpdateWindow(GetDlgItem(hDlg, IDCANCEL));
	}

	// Cleanup action.
	if (action == CLEANUP)
	{

		// Create the diskpart.txt based on the selected item in IDC_COMBO_PHYSICAL_DISKS.
		// Get the current selection in the combo box.
		cbPosition = (int)SendMessage(GetDlgItem(hDlg, IDC_COMBO_PHYSICAL_DISKS), CB_GETCURSEL, 0, 0);

		// Check if user has selected a disk.
		if (cbPosition != CB_ERR)
		{
			// Get the number of the selected disk.
			selectedDiskIndex = (int)SendMessage(GetDlgItem(hDlg, IDC_COMBO_PHYSICAL_DISKS), CB_GETITEMDATA, cbPosition, 0);
			
			// Create the diskpart file.
			if (CreateDiskpartFile(selectedDiskIndex) == FALSE)
			{
				// Failed
				_stprintf_s(szBuffer, MAX_BUFFER_SIZE, szActionFailed, RUN_ACTION_CREATE_DISKPART_FILE_FAILED);
				MessageBox(hDlg, szBuffer, szRunActionText, MB_ICONERROR | MB_OK);
				return;
			}
		}
		else 
		{
			// Nothing has been selected (-1).
			_stprintf_s(szBuffer, MAX_BUFFER_SIZE, szActionDiskSelectionFailed, rcRunAction);
			MessageBox(hDlg, szBuffer, szRunActionText, MB_ICONINFORMATION | MB_OK);
			return;
		}

		// Ask the user if the action should be started.
		if (MessageBox(hDlg, szActionWarning, szRunActionText, MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2) == IDYES)
		{			
			// Run action and wait until the task is finished.
			rcRunAction = ActionEx(g_pstrActionPath, TEXT("%s"), g_pstrActionInstPath);

			if (rcRunAction == RUN_ACTION_SUCCESSFUL)
			{
				// Check whether the system should be restarted after the cleanup is complete.
				bSystemRestart = (int) SendMessage(GetDlgItem(hDlg, IDC_CHECKBOX_RESTART), BM_GETCHECK, 0, 0L);

				// Set the appropriate message for the message box.
				#ifdef NLS
				LoadString(NULL, bSystemRestart ? IDS_RUN_ACTION_SUCCESSFUL_WITH_REBOOT_NLS : IDS_RUN_ACTION_SUCCESSFUL_NLS,
							szActionSuccessful, MAX_LOADSTRING);
				#else
				LoadString(NULL, bSystemRestart ? IDS_RUN_ACTION_SUCCESSFUL_WITH_REBOOT : IDS_RUN_ACTION_SUCCESSFUL,
							szActionSuccessful, MAX_LOADSTRING);
				#endif
				// Show the message box with the appropriate message.
				MessageBox(hDlg, szActionSuccessful, szRunActionText, MB_ICONINFORMATION | MB_OK);
				
				// If system restart is required, restart the system now.
				if (bSystemRestart == TRUE)
				{					
					// Restart the system now.
					Action(szRestartExe, szRestartExeParams, SW_HIDE);
				}
				return;
			}
			else if (rcRunAction == RUN_ACTION_CANCELLED_BY_USER)
			{
				// Action cancelled by the user.
				_stprintf_s(szBuffer, MAX_BUFFER_SIZE, szActionCancelled, rcRunAction);
			}
			else if (rcRunAction == RUN_ACTION_SHELLEX_FILE_NOT_FOUND)
			{
				// Action file not found.
				_stprintf_s(szBuffer, MAX_BUFFER_SIZE, szActionFileNotFound, g_pstrActionPath);
			}
			else 
			{
				// ActionEx() failed.
				_stprintf_s(szBuffer, MAX_BUFFER_SIZE, szActionFailed, rcRunAction);
			}
			MessageBox(hDlg, szBuffer, szRunActionText, MB_ICONERROR | MB_OK);
		}
	}
}

/**
 * An event that occurs when the user clicks on the CMD button.
 */
void onClick_ButtonCmd()
{
	// Launch a normal CMD shell (cmd.exe).
	ShellExecute(NULL, TEXT("open"), TEXT("cmd.exe"), NULL, NULL, SW_SHOWNORMAL);
}

/**
 * Returns the absolute path to this module (exe), e.g. c:\programfiles\tool\myapp.exe
 */
const TCHAR * GetOwnPath()
{
	// Allocate memory for the path string. Don't forget to free the memory!
	TCHAR * ptrOwnPath = (TCHAR *) calloc(MAX_PATH, sizeof(TCHAR));

	// If NULL is passed, the handle to the current module (exe) is supplied.
	HMODULE hModule = GetModuleHandle(NULL);
	
	if (hModule != NULL)
	{
		// Get the whole path.
		GetModuleFileName(hModule, ptrOwnPath, MAX_PATH * sizeof(TCHAR));
	}
	return ptrOwnPath;
}

/**
 * Returns an absolute path to the action (batch) file. Based on the data from GetOwnPath().
 */
const TCHAR * GetActionPath(TCHAR * szModulePath, TCHAR * szNewActionFileName)
{
	TCHAR * pszModulePath = szModulePath;
	TCHAR * ptrBatchPath = (TCHAR *) calloc(MAX_PATH, sizeof(TCHAR));	// Don't forget to free the memory!
	TCHAR * pTmp = ptrBatchPath;
	TCHAR * pBackSlash = pTmp;
	HRESULT hResult = 0;

	// Go through the entire module path string until it ends.
	while (*pszModulePath != '\0')
	{
		*pTmp = *pszModulePath;
		// Check if char is a backslash (remember it).
		if (*pTmp == '\\')
		{
			// Note the last backslash as a pointer address.
			pBackSlash = pTmp;
		}
		pTmp++;
		pszModulePath++;
	}

	// Terminate the still temporary batch path string at the last backslash.
	*++pBackSlash = '\0';
	
	// Concatenate the batch file name at the end of the batch string and return it.
	hResult = StringCchCat(ptrBatchPath, MAX_PATH * sizeof(TCHAR), szNewActionFileName); 
	return ptrBatchPath;
}

/**
 * This function executes the actual job and waits until it is finished.
 */
void Action(const TCHAR * szActionFile, const TCHAR * szParameters, int nShow)
{
	SHELLEXECUTEINFO sei = {0};

	ZeroMemory(&sei, sizeof(sei));
	sei.cbSize = sizeof(SHELLEXECUTEINFO);
	sei.fMask = SEE_MASK_NOCLOSEPROCESS;
	sei.hwnd = NULL;
	sei.lpVerb = NULL;
	sei.lpFile = szActionFile;
	sei.lpParameters = szParameters;
	sei.lpDirectory = NULL;
	sei.nShow = nShow;
	sei.hInstApp = NULL;
	
	// Execute the action job.
	ShellExecuteEx(&sei);

	// Wait until the action process is finished. 
	WaitForSingleObject(sei.hProcess, INFINITE);
	CloseHandle(sei.hProcess);
}

/**
 * This function executes the actual action job and waits until it is finished.
 * This function corresponds to the Action() function with the 
 * difference that it takes dynamic parameter lists as last parameter
 * and returns a run code.
 * 
 * Return: If everything was okay,
 * i.e. the batch ran to the end, the return code should be 0x400.
 * Other values always signal an error code.
 * The value SHELL_EXECUTE_EX_FAILED means
 * that an error occurred during the execution of ShellExecuteEx().
 */
DWORD ActionEx(const TCHAR * szActionFile, TCHAR * szFormat, ...)
{
	SHELLEXECUTEINFO sei = {0};
	DWORD exitCode = 0;
	const int SHELL_EXECUTE_EX_FAILED = RUN_ACTION_SHELLEX_FAILED;
	const int SHELL_EXECUTE_EX_FILE_NOT_FOUND = RUN_ACTION_SHELLEX_FILE_NOT_FOUND;
		
	// Dynamic parameter list...
	// See for more information also: https://docs.microsoft.com/de-de/cpp/c-runtime-library/reference/vsnprintf-s-vsnprintf-s-vsnprintf-s-l-vsnwprintf-s-vsnwprintf-s-l?view=msvc-160
	// See also: Book by Charles Petzold, 5th edition Windows Programming, page 42 (SCRNSIZE.C). 
	TCHAR szParameters[2048];
	va_list pArgList;

	// Check if the action file exists.
	if (FileExists(szActionFile) == FALSE)
	{
		return SHELL_EXECUTE_EX_FILE_NOT_FOUND;
	}

	va_start (pArgList, szFormat);
	_vsntprintf_s(szParameters, sizeof(szParameters) / sizeof(TCHAR), _TRUNCATE, szFormat, pArgList); 
	va_end (pArgList);
			
	ZeroMemory(&sei, sizeof(sei));

	sei.cbSize = sizeof(SHELLEXECUTEINFO);
	sei.fMask = SEE_MASK_NOCLOSEPROCESS;
	sei.hwnd = NULL;
	sei.lpVerb = NULL;
	sei.lpFile = szActionFile;
	sei.lpParameters = szParameters;
	sei.lpDirectory = NULL;
	sei.nShow = SW_SHOW;
	sei.hInstApp = NULL;
		
	// Execute the action job.
	if (ShellExecuteEx(&sei) == TRUE)
	{
		// Wait until the action process is finished. 
		WaitForSingleObject(sei.hProcess, INFINITE);

		// The process was finished.
		// Get the value of %errorlevel% from the batch.
		// If everything was okay, i.e. the batch ran to the end, then we get a value of 0x400.
		GetExitCodeProcess(sei.hProcess, &exitCode);
		CloseHandle(sei.hProcess);
		
		return exitCode;
	}
	return SHELL_EXECUTE_EX_FAILED;
}

/**
 * Checks if a file exists and returns TRUE in this case else FALSE.
 * Source: https://docs.microsoft.com/de-de/cpp/c-runtime-library/reference/access-s-waccess-s?view=msvc-160
 */
BOOL FileExists(const wchar_t * szFile)
{
	errno_t err = 0;

	// Check for existence.
	if ((err = _taccess_s(szFile, 0)) == 0)
	{
		return TRUE;
	}
	return FALSE;
}

/**
 * Checks if a directory exists and returns TRUE in this case else FALSE.
 */
BOOL DirExists(const TCHAR * szDir)
{
	DWORD attributes = 0;
	BOOL isDirectory = FALSE;
	attributes = GetFileAttributes(szDir);

	if (attributes == INVALID_FILE_ATTRIBUTES)
	{
		return FALSE;
	}
	isDirectory = ((attributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY);
	return isDirectory;
}

/**
 * Creates a window to display a logo loaded from the resource file.
 */
void ShowLogo(HWND hwndDlg)
{
	RECT rect;
	const int imgWidth = 48;
	const int imgHeight = 48;
	int windowWidth = 0;
	int windowHeight = 0;
	int xPos = 10;
	int yPos = imgHeight / 2 - 3;

	// Get the size of the dialog window.
	if (GetWindowRect(hwndDlg, &rect) != 0)
	{
		windowWidth= rect.right - rect.left;
		windowHeight = rect.bottom - rect.top;
		xPos = (windowWidth / 2) - (imgWidth / 2);
	}

	// https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-loadimagea
	hBitmapLogo = (HBITMAP)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDB_BITMAP2), IMAGE_BITMAP, imgWidth, imgHeight, LR_SHARED);
	hwndLogo = CreateWindow(L"Static", NULL, WS_VISIBLE | WS_CHILD | SS_BITMAP, xPos, yPos, imgWidth, imgHeight, hwndDlg, NULL, NULL, NULL);  
	SendMessage(hwndLogo, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM) hBitmapLogo);
}

/**
 * Determines the logical drives available in the system and returns them in the form of a compound string.
 * dwDrivesCount: [out] Pointer for returning the number of drives found.
 */
const TCHAR * GetLogicalDriveList(DWORD * dwDrivesCount)
{
	DWORD drives = 0, i = 0, counter = 0;

	// Allocate memory for the compound string with all logical drives found.
	// Later, the filled memory will look like this: "C:\ D:\"
	// Don't forget to free the memory!
	TCHAR * ptrDrives = (TCHAR *) calloc(1, sizeof(TCHAR));
	
	// Character array for the pattern of a detected logical drive. This looks like this: "C:\ "
	// The length is 4 characters + 1 character for string termination ('\0').
	TCHAR drivePattern[5];

	// Determine the size of the drive pattern. This is required below for the memory reallocation.
	// The value corresponds to the array size of drivePattern, i.e. the value of 5.
	const size_t sizePattern = sizeof(drivePattern) / sizeof(TCHAR);

	// newSize contains the size for the reallocation of the required memory.
	size_t newSize = 0;

	// dwDrivesCount contains the number of physical drives found in the system, e. g. "C:\", "D:\" == 2.
	*dwDrivesCount = 0;
	
	// Detect all logical drives.
	drives = GetLogicalDrives();

	// Go through all the drive letters and find which drives are available.
	for(i = 0; i < 26; i++)
	{
		// Explanation by example:
		// GetLogicalDrives returns 0x0000000c = 12 (decimal) 
		//                           Drive letter.: .. H G F E D C B A
		//      A bit is set for a drive (example): .. 0 0 0 0 1 1 0 0
		//    Decimal value for the respective bit:         .. 8 4 2 1
		// 1 << 0 = 2 ^ 0 = 1
		// 1 << 1 = 2 ^ 1 = 2
		// 1 << 2 = 2 ^ 2 = 4
		// 1 << 3 = 2 ^ 3 = 8
		if( (drives & ( 1 << i )) != 0)
		{
			// Determine the required memory size for reallocation.
			newSize += (sizePattern) * sizeof(TCHAR);

			// Reallocate memory.
			ptrDrives = (TCHAR *) realloc(ptrDrives, newSize);
			
			// Compose the pattern for a logical drive.
			_stprintf_s(drivePattern, sizeof(drivePattern)/sizeof(TCHAR), TEXT("%c:\\ "), TEXT('A') + i);
			
			// Merge all drives found into one string.
			_tcsncat_s(ptrDrives, newSize, drivePattern, _TRUNCATE);

			// Internal counter for available drives (Fix: #5).
			counter++;
		}
	}

	// Return the number of logical drives found as a pointer (Fix: #5).
	*dwDrivesCount = counter;

	// Return a concatenated string of found drives.
	return ptrDrives;
}

/**
 * Checks whether Cleanup had run.
 * This is recognized by whether the label name of the (primary) volume, usually C:\,
 * is "cleaned" (or as specified under szCompWithLabel (see also in the diskpart.txt)).
 */
BOOL IsDiskCleaned(const TCHAR * szDiskLetter, const TCHAR * szCompWithLabel)
{
	TCHAR volumeName[MAX_PATH + 1] = _T(""), fileSystemName[MAX_PATH + 1] = _T("");
	DWORD maximumComponentLength, fileSystemFlags;
	
	if (szDiskLetter == NULL || szCompWithLabel == NULL)
	{
		return FALSE;
	}

	// Get information about the volume.
	if (GetVolumeInformation(szDiskLetter, volumeName, MAX_PATH + 1, NULL,
		&maximumComponentLength, &fileSystemFlags, fileSystemName, MAX_PATH + 1) == FALSE)
	{
		return FALSE;
	}

	// Compare the determined label name (volumeName) from the szDiskLetter drive with the label name passed via szCompWithLabel.
	// If both match (TRUE), then Cleanup has already been executed.
	return (_tcsicmp(szCompWithLabel, volumeName) == 0);
}

/**
 *  Sets the focus on the IDCANCEL button and designates it as the default button for a dialog window.
 *  This function ensures that the IDCANCEL button receives the default push button style,
 *  making it respond to the Enter key as the default action.
 */
void SetCancelBtnAsDefault(HWND hwndDlg)
{
	// Set the focus on the IDCANCEL button.
	// For this purpose, the dialogue is told that IDCANCEL is the default button.
	// In addition, the OK button should initially be given the style BS_PUSHBUTTON.
	SendDlgItemMessage(hwndDlg, IDOK, BM_SETSTYLE, BS_PUSHBUTTON, (LONG)TRUE);
	SendMessage(hwndDlg, DM_SETDEFID, IDCANCEL, 0L);

	// IDCANCEL itself must also be informed of this.
	SendDlgItemMessage(hwndDlg, IDCANCEL, BM_SETSTYLE, BS_DEFPUSHBUTTON, (LONG)TRUE);		

	// Only then can the focus be successfully set on IDCANCEL.
	SetFocus(GetDlgItem(hwndDlg, IDCANCEL));
}

/**
 * Enumerates all physical disks present in the system and initializes the combo box
 * with their corresponding information.
 */
void InitComboBox_PhysicalDisks(HWND hwndDlg, HWND hwndComboBox)
{
	TCHAR buffer[256];
	int disk = 0;
	int cbPosition = 0;
	BOOL anyDiskCleaned = FALSE;
	int cleanedDiskIndex = -1;
	DiskInfo info[MAX_DISKS];

	memset(info, 0, sizeof(info));

	for (disk; disk < MAX_DISKS; disk++)
	{
		info[disk].diskIndex = disk;
		if (GetPhysicalDiskInfo(disk, &info[disk]) == FALSE)
		{
			break;
		}
		else
		{
			// Check if the volume label starts with "cleaned" (matches first 7 characters).
			// _tcsncmp returns 0 if the specified number of characters match exactly
			// if (_tcsncmp(info[disk].volumeLabel, _T(szDiskLabel), 7) == 0)
			// {
			//	info[disk].isCleaned = TRUE;
			//	anyDiskCleaned = TRUE;
			//	cleanedDiskIndex = info[disk].diskIndex;
			// }
			// else
			// {
			//	info[disk].isCleaned = FALSE;
			// }

			_sntprintf_s(buffer, _countof(buffer), _TRUNCATE,
						_T("Disk: %d => %s (%s, %s GB)"),
						info[disk].diskIndex,
						info[disk].model[0] == _T('\0') ? _T("n/a") : info[disk].model,
						info[disk].busType,
						info[disk].totalGB);
			cbPosition = (int)SendMessage(hwndComboBox, CB_ADDSTRING, 0, (LPARAM)buffer);
			// Save the selected disk value 0, 1 etc. in the combobox item tag.
			SendMessage(hwndComboBox, CB_SETITEMDATA, cbPosition, (LPARAM)info[disk].diskIndex);
		}	
	}






}

/**
 * Extends GetPhysicalDiskInfo by retrieving additional information from the
 * specified physical disk, such as whether the disk is an SSD.
 */
void GetPhysicalDiskInfoEx(HANDLE hDevice, DiskInfo* info)
{
	DWORD bytesReturned = 0;
	DEVICE_SEEK_PENALTY_DESCRIPTOR desc = {0};
	
	STORAGE_PROPERTY_QUERY query;
	query.PropertyId = StorageDeviceSeekPenaltyProperty;
	query.QueryType = PropertyStandardQuery;

	// Is SSD?
	if (DeviceIoControl(hDevice, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), &desc, sizeof(desc), &bytesReturned, NULL))
	{
		info->isSSD = (desc.IncursSeekPenalty == FALSE);
	}
}

/**
 * Determines whether a physical disk exists in the system using the
 * \\.\PhysicalDrive%d syntax. If the disk exists, retrieves information such as
 * disk size, manufacturer, and bus type and stores it in the DiskInfo structure.
 *
 * Returns TRUE if the physical disk exists and the information was retrieved
 * successfully; otherwise, returns FALSE.
 */
BOOL GetPhysicalDiskInfo(int diskIndex, DiskInfo* pInfo)
{
	TCHAR path[64];
	HANDLE hDevice = NULL;
	STORAGE_PROPERTY_QUERY query;
	BYTE buffer[4096];
	DWORD bytesReturned = 0;
	PSTORAGE_DEVICE_DESCRIPTOR pDesc;
	const TCHAR* busTypes[] = { _T("Unknown"), _T("SCSI"), _T("ATAPI"), _T("ATA"), _T("IEEE1394"), _T("SSA"), _T("Fibre"), _T("USB"), _T("RAID"), _T("iSCSI"), _T("SAS"), _T("SATA"), _T("SD"), _T("MMC"), _T("Virtual"), _T("FileBackedVirtual"), _T("NVMe") };
	DISK_GEOMETRY_EX diskGeometry;
	ULONGLONG diskSizeBytes = 0;
	ULONGLONG diskSizeGB = 0;
	BOOL isRealDisk = TRUE;
	
	// Build the full path for the physical drive and verify that it exists.
	_stprintf_s(path, _countof(path), TEXT("\\\\.\\PhysicalDrive%d"), diskIndex);

	// Read device information without requiring elevated privileges.
	hDevice = CreateFile(path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

	if (hDevice == INVALID_HANDLE_VALUE)
		return FALSE;
	
	pInfo->isPhysicalDisk = TRUE;

	// Get the disk size in bytes (64-bit unsigned integer), and also as a string in gigabytes.
	if (DeviceIoControl(hDevice, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, NULL, 0, &diskGeometry, sizeof(diskGeometry), &bytesReturned, NULL))
	{
		// Bytes
		pInfo->totalBytes = diskGeometry.DiskSize.QuadPart;

		// Gigabytes
		_sntprintf_s(pInfo->totalGB, _countof(pInfo->totalGB), _TRUNCATE, TEXT("%.2f"), (double)diskGeometry.DiskSize.QuadPart / (1024.0 * 1024.0 * 1024.0));
	}
		
	// Obtain the device model, bus type, and removable status through IOCTL_STORAGE_QUERY_PROPERTY.
	ZeroMemory(&query, sizeof(query));
	query.PropertyId = StorageDeviceProperty;
	query.QueryType = PropertyStandardQuery;
	ZeroMemory(buffer, sizeof(buffer));

	if (DeviceIoControl(hDevice, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), buffer, sizeof(buffer), &bytesReturned, NULL)) 
	{
		pDesc = (PSTORAGE_DEVICE_DESCRIPTOR)buffer;
			
		// Retrieve  the model name.
		if (pDesc->ProductIdOffset != 0) 
		{
			// Copy a char* string to a TCHAR* string and convert it. Important: use %S when converting from ANSI to Unicode.
			_sntprintf_s(pInfo->model, _countof(pInfo->model), _TRUNCATE, TEXT("%S"), (char*)(buffer + pDesc->ProductIdOffset));
			
			// Trim trailing spaces, tabs, and carriage returns.
			TrimTrailingSpaces(pInfo->model);
		}

		// Retrieve  the serial number.
		// Some device controllers return 0 or -1 if a serial number is not available.
		if (pDesc->SerialNumberOffset != 0 && pDesc->SerialNumberOffset != -1)
		{
			// Copy a char* string to a TCHAR* string and convert it. Important: use %S when converting from ANSI to Unicode.
			_sntprintf_s(pInfo->serialNumber, _countof(pInfo->serialNumber), _TRUNCATE, TEXT("%S"), (char*)(buffer + pDesc->SerialNumberOffset));

			// Trim trailing spaces, tabs, and carriage returns.
			TrimTrailingSpaces(pInfo->serialNumber);
		}
		else
		{
			_tcscpy_s(pInfo->serialNumber, _countof(pInfo->serialNumber), TEXT("n/a"));
		}

		// Check if disk is an SSD.
		GetPhysicalDiskInfoEx(hDevice, pInfo);

		// Get the bus type.
		_stprintf_s(pInfo->busType, _countof(pInfo->busType), TEXT("%s"), GetBusTypeName(pDesc->BusType));

		// Retrieve  the removable status.
		if (pDesc->RemovableMedia ||
			pDesc->BusType == 0x07 ||	// USB
			pDesc->BusType == 0x0C ||	// SD
			pDesc->BusType == 0x0D )	// MMC, eMMC
		{
			pInfo->isRemovable = TRUE;
		}
		else
		{
			pInfo->isRemovable = FALSE;
		}
	}
	CloseHandle(hDevice);
	return TRUE;
}

/**
 * Returns a human-readable string for the specified STORAGE_BUS_TYPE value,
 * such as "NVme", "USB".
 */
LPCTSTR GetBusTypeName(STORAGE_BUS_TYPE busType)
{
	switch (busType)
	{
		case BusTypeUnknown:			return _T("Unknown");
		case BusTypeScsi:				return _T("SCSI");
		case BusTypeAtapi:				return _T("ATAPI");
		case BusTypeAta:				return _T("ATA");
		case BusType1394:				return _T("Firewire");
		case BusTypeSsa:				return _T("SSA");
		case BusTypeFibre:				return _T("Fibre");
		case BusTypeUsb:				return _T("USB");
		case BusTypeRAID:				return _T("RAID");
		case BusTypeiScsi:				return _T("iSCSI");
		case BusTypeSas:				return _T("SAS");
		case BusTypeSata:				return _T("SATA");
		case BusTypeSd:					return _T("SD");
		case BusTypeMmc:				return _T("MMC");
		case BusTypeVirtual:			return _T("Virtual");
		case BusTypeFileBackedVirtual:	return _T("FileBackedVirtual");
		case BusTypeSpaces:				return _T("Storage Spaces");
		case BusTypeNvme:				return _T("NVme");
		case BusTypeScm:				return _T("Storage Class Memory");
		default:
			return _T("Other/Unknown");
	}
}

/**
 * Trims unnecessary trailing characters from the given string, including
 * whitespace and control characters.
 */
void TrimTrailingSpaces(TCHAR* str)
{
	TCHAR c;
	size_t len = 0;
	size_t i = 0;

	if (str == NULL || *str == _T('\0')) return;

	len = _tcslen(str);
	i = len;
	
	// Remove trailing spaces, tabs, and carriage returns from str.
	while(i > 0)
	{
		c = str[i - 1];

		// Only trims spaces, tabs, line breaks, or other invisible control characters (<32).
		if (c == _T(' ') || c == _T('\t') || c == _T('\r') || c == _T('\n') || (unsigned int)c < 32)
		{
			str[i - 1] = _T('\0');
			i--;
		}
		else
		{
			break;
		}
	}
}

/**
 * Dynamically generates a diskpart script file with a modern GPT partition
 * structure and formatting instructions tailored to the selected disk index.
 * 
 * @diskIndex: Index of the disk to be cleaned, partitioned, and formatted.
 * @return: TRUE if the script file was successfully created, FALSE otherwise.
 */
BOOL CreateDiskpartFile(int diskIndex)
{
    // Define the script path based on the build configuration
#ifdef _DEBUG
    // Local test path for development in Visual Studio.
    TCHAR scriptPath[] = _T("c:\\temp\\diskpart.txt");
#else
    // Production path inside the WinPE RAM disk environment.
    TCHAR scriptPath[] = _T("x:\\diskpart.txt");
#endif
		
    FILE* fp = NULL;

    // Try to open the file securely in write mode
    if (_tfopen_s(&fp, scriptPath, _T("w")) == 0)
    {
        // Select the disk specified by the user
        _ftprintf(fp, _T("select disk %d\n"), diskIndex);
        
        // Wipe all existing partition structures and data.
        _fputts(_T("clean\n"), fp);
        
        // Convert the partition style from MBR to the modern GPT schema.
        _fputts(_T("convert gpt\n"), fp);
        
        // Create the main operating system partition (no 'active' flag required for GPT).
        _fputts(_T("create partition primary\n"), fp);
        
        // Format command with the disk label marker "cleaned".
        _fputts(_T("format quick fs=ntfs label=\"cleaned\"\n"), fp);
        
        // Assign a drive letter so Windows can register the volume label.
        _fputts(_T("assign\n"), fp);

        // Exit the diskpart utility
        _fputts(_T("exit\n"), fp);
        
        // Safely close the file handle and flush changes to disk.
        fclose(fp);
        return TRUE;
    }
    
    // Return FALSE if the file could not be created (e.g., directory missing or write protection)
    return FALSE;
}