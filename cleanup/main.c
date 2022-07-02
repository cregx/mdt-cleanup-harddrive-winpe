/**
 * The application was developed by Christoph Regner.
 * On the web: https://www.cregx.de
 * For further information, please refer to the attached LICENSE.md
 * 
 * The MIT License (MIT)
 * Copyright (c) 2022 Christoph Regner
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
 *
 * MIT License
 * Copyright (c) Christoph Regner July 2022
 **/
#define WIN32_LEAN_AND_MEAN

#define _WIN32_WINNT 0x400
#define _Win32_DCOM
#define MAX_LOADSTRING 512

#include <Windows.h>
#include <CommCtrl.h>
#include <CommDlg.h>				// Dialogs.
#include <tchar.h>
#include <Shlwapi.h>				// e.g. For example for shortening long paths. Needed: Shlwapi.lib static linked.
#include <strsafe.h>				// e.g. Safe string copy.
#include <ShellAPI.h>				// e.g. SHELLEXECUTEINFO.
#include <io.h>					// e.g. _access_s (file exists).

#include <Shobjidl.h>				// COM objects.
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
void ShowLogo(HWND);

static HBITMAP hBitmapLogo;
static HWND hwndLogo;

const TCHAR * g_pstrExePath;				// Full path to the exe.
const TCHAR * g_pstrActionPath;				// Full path to the batch file.
const TCHAR * g_pstrActionInstPath;			// Full path to the file with instructions for the batch file.
action_type g_currentAction;				// Cleanup or terminate action should be performed?

WCHAR szAppDialogTitle[MAX_LOADSTRING];
WCHAR szAppActionRequest[MAX_LOADSTRING];
WCHAR szAppActionExpl[MAX_LOADSTRING];
WCHAR szAppVersion[MAX_LOADSTRING];
WCHAR szRunActionText[MAX_LOADSTRING];
WCHAR szActionWarning[MAX_LOADSTRING];
WCHAR szActionSuccessful[MAX_LOADSTRING];
WCHAR szActionFailed[MAX_LOADSTRING];
WCHAR szActionCancelled[MAX_LOADSTRING];
WCHAR szActionFileNotFound[MAX_LOADSTRING];
WCHAR szBtnActionCaption[MAX_LOADSTRING];
WCHAR szBtnCancelCaption[MAX_LOADSTRING];
WCHAR szAppLang[MAX_LOADSTRING];

// Don't forget to increase the version number in the resource file (cleanup.rc).
#ifdef NLS
const LPCWSTR szAppVer		= TEXT("1.0.3 (%s) / 02. Juli 2022");
#else
const LPCWSTR szAppVer		= TEXT("1.0.3 (%s) / 02. July 2022");
#endif

const LPCWSTR szBatchFileName	= TEXT("action.bat");
const LPCWSTR szBatchParams	= TEXT("diskpart.txt");
const LPCWSTR szRestartExe	= TEXT("wpeutil.exe");
const LPCWSTR szRestartExeParams= TEXT("reboot");

const DWORD RUN_ACTION_SHELLEX_FAILED	= 0xFFFFFFFFFFFFFFFF;		// dec => -1 (Function internal error, use GetLastError().)
const DWORD RUN_ACTION_SHELLEX_FILE_NOT_FOUND = 0xFFFFFFFFFFFFFFFE;	// dec => -2 (Action file not found.)
const DWORD RUN_ACTION_SUCCESSFUL	= 0x400;			// dec => 1024 (Successful processing of the batch file.)
const DWORD RUN_ACTION_CANCELLED_BY_USER= 0xC000013A;			// dec => 3221225786
									// (Cancellation of the batch job by the user,
									// e.g. because the user has clicked the X button.)

// Main function.
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
  LoadString(hInst, IDS_RUN_ACTION_SUCCESSFUL_NLS, szActionSuccessful, MAX_LOADSTRING);
  LoadString(hInst, IDS_RUN_ACTION_FAILED_NLS, szActionFailed, MAX_LOADSTRING);
  LoadString(hInst, IDS_RUN_ACTION_CANCELED_NLS, szActionCancelled, MAX_LOADSTRING);
  LoadString(hInst, IDS_RUN_ACTION_FILE_NOT_FOUND_NLS, szActionFileNotFound, MAX_LOADSTRING);
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
  LoadString(hInst, IDS_RUN_ACTION_SUCCESSFUL, szActionSuccessful, MAX_LOADSTRING);
  LoadString(hInst, IDS_RUN_ACTION_FAILED, szActionFailed, MAX_LOADSTRING);
  LoadString(hInst, IDS_RUN_ACTION_CANCELED, szActionCancelled, MAX_LOADSTRING);
  LoadString(hInst, IDS_RUN_ACTION_FILE_NOT_FOUND, szActionFileNotFound, MAX_LOADSTRING);
#endif

  InitCommonControls();
  hDlg = CreateDialogParam(hInst, MAKEINTRESOURCE(IDD_DIALOG1), 0, DialogProc, 0);
  ShowWindow(hDlg, nCmdShow);

  while((ret = GetMessage(&msg, 0, 0, 0)) != 0) {
    if(ret == -1)
      return -1;

    if(!IsDialogMessage(hDlg, &msg)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }

  return 0;
}

/**
 * Dialog box message procedure.
 */
INT_PTR CALLBACK DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg)
	{
		case WM_INITDIALOG:
			// Create a logo image.
			ShowLogo(hwndDlg);

			onInit(hwndDlg, wParam);
			return TRUE;
		case WM_COMMAND:
			switch(LOWORD(wParam))
			{
				// Click on the action button for cleanup.
				case IDC_BUTTON_CLEANUP:
					onAction(hwndDlg, g_currentAction);
					return TRUE;
				// Click on the action button for cmd run.
				case IDC_BUTTON_CMD:
					//SendDlgItemMessage(hwndDlg, IDC_PICTURE, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hBitmapLogo);
					onClick_ButtonCmd();
					return TRUE;
				// Click on the close button (X).
				case IDCANCEL:
					onClose(hwndDlg); 
					return TRUE;
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
		case WM_DESTROY:
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
	TCHAR szBuffer[1024];
	TCHAR szVersion[128];

	g_currentAction = CLEANUP;

	// Get the module path.
	g_pstrExePath = GetOwnPath();

	// Get the batch file path.
	g_pstrActionPath = GetActionPath((TCHAR *)g_pstrExePath, (TCHAR *)szBatchFileName);

	// Get the full file path for the batch instructions file.
	g_pstrActionInstPath = GetActionPath((TCHAR *)g_pstrExePath, (TCHAR *)szBatchParams);
	
	// Init controls.
	SetWindowText(hwndDlg, szAppDialogTitle);
	SetDlgItemText(hwndDlg, IDC_STATIC_REQUEST, szAppActionRequest);
	SetDlgItemText(hwndDlg, IDC_STATIC_EXPLANATION, szAppActionExpl);
	SetDlgItemText(hwndDlg, IDC_BUTTON_CLEANUP, szBtnActionCaption);
	SetDlgItemText(hwndDlg, IDCANCEL, szBtnCancelCaption);

	// App version.
	_stprintf_s(szVersion, 128, szAppVer, szAppLang);  
	_stprintf_s(szBuffer, 1024, szAppVersion, szVersion);
	SetWindowText(GetDlgItem(hwndDlg, IDC_STATIC_APP_INFO), szBuffer);
}

/**
 * The event that is triggered when the dialog box is to be closed.
 */
void onClose(HWND hwnd)
{
	SendMessage(hwnd, WM_CLOSE, 0, 0);
}

/**
 * Event fired when the action button is clicked.
 */
void onAction(HWND hDlg, action_type action)
{
	#define MAX_BUFFER_SIZE 1024 
	TCHAR szBuffer[MAX_BUFFER_SIZE];
	DWORD rcRunAction = 0;

	memset(szBuffer, 0, sizeof(szBuffer));

	// Cleanup action.
	if (action == CLEANUP)
	{
		if (MessageBox(hDlg, szActionWarning, szRunActionText, MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2) == IDYES)
		{
			// Run action and wait until the task is finished.
			rcRunAction = ActionEx(g_pstrActionPath, TEXT("%s"), g_pstrActionInstPath);

			if (rcRunAction == RUN_ACTION_SUCCESSFUL)
			{
				MessageBox(hDlg, szActionSuccessful, szRunActionText, MB_ICONINFORMATION | MB_OK);
				
				// Restart the system now.
				Action(szRestartExe, szRestartExeParams, SW_HIDE);
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
	int yPos = imgHeight / 2;

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