// This file is part of Notepad++ plugin MIME Tools project
// Copyright (C)2023 Don HO <don.h@free.fr>

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// at your option any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "PluginInterface.h"
#include "NppVim.h"


const TCHAR PLUGIN_NAME[] = TEXT("NppVim");
const int nbFunc = 3;

HWND getCurrentScintillaHandle();

HINSTANCE g_hInst = nullptr;;
NppData nppData;
FuncItem funcItem[nbFunc];
HWND g_hAboutDlg = nullptr;

BOOL APIENTRY DllMain(HANDLE hModule, DWORD reasonForCall, LPVOID /*lpReserved*/)
{
	switch (reasonForCall)
	{
	case DLL_PROCESS_ATTACH:
	{
		g_hInst = (HINSTANCE)hModule;
		funcItem[0]._pFunc = ToggleVimMode;
		funcItem[1]._pFunc = NULL;
		funcItem[2]._pFunc = about;

		lstrcpy(funcItem[0]._itemName, TEXT("Toggle Vim mode"));

		lstrcpy(funcItem[1]._itemName, TEXT("-SEPARATOR-"));

		lstrcpy(funcItem[2]._itemName, TEXT("About"));

		funcItem[0]._init2Check = true;
		funcItem[1]._init2Check = false;
		funcItem[2]._init2Check = false;

		// If you don't need the shortcut, you have to make it NULL
		funcItem[0]._pShKey = NULL;
		funcItem[1]._pShKey = NULL;
		funcItem[2]._pShKey = NULL;
	}
	break;

	case DLL_PROCESS_DETACH:
		break;

	case DLL_THREAD_ATTACH:
		break;

	case DLL_THREAD_DETACH:
		break;
	}
	return TRUE;
}

extern "C" __declspec(dllexport) void setInfo(NppData notpadPlusData)
{
	nppData = notpadPlusData;
	HWND hwndEdit = getCurrentScintillaHandle();
	ToggleToNormalMode(hwndEdit); // Set default mode
}


extern "C" __declspec(dllexport) const TCHAR* getName()
{
	return PLUGIN_NAME;
}

extern "C" __declspec(dllexport) FuncItem* getFuncsArray(int* nbF)
{
	*nbF = nbFunc;
	return funcItem;
}

extern "C" __declspec(dllexport) void beNotified(SCNotification* notifyCode)
{
	switch (notifyCode->nmhdr.code)
	{
	case NPPN_DARKMODECHANGED:
	{
		if (g_hAboutDlg)
		{
			::SendMessage(nppData._nppHandle, NPPM_DARKMODESUBCLASSANDTHEME, static_cast<WPARAM>(NppDarkMode::dmfHandleChange), reinterpret_cast<LPARAM>(g_hAboutDlg));
			::SetWindowPos(g_hAboutDlg, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED); // to redraw titlebar
		}
		break;
	}

	case SCN_CHARADDED:
	{
		HWND hwndEdit = getCurrentScintillaHandle();
		HandleKeyPress(hwndEdit, notifyCode->ch);
		break;
	}

	}
}

#ifdef UNICODE
extern "C" __declspec(dllexport) BOOL isUnicode()
{
	return TRUE;
}
#endif //UNICODE

// Here you can process the Npp Messages 
// I will make the messages accessible little by little, according to the need of plugin development.
// Please let me know if you need to access to some messages :
// https://github.com/notepad-plus-plus/notepad-plus-plus/issues
//
extern "C" __declspec(dllexport) LRESULT messageProc(UINT Message, WPARAM wParam, LPARAM lparam)
{
	(void)lparam;
	if (Message == WM_KEYDOWN)
	{
		HWND hwndEdit = getCurrentScintillaHandle();
		HandleKeyPress(hwndEdit, wParam);
	}
	return LRESULT(0);
}


HWND getCurrentScintillaHandle()
{
	int currentEdit;
	::SendMessage(nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&currentEdit);
	return (currentEdit == 0) ? nppData._scintillaMainHandle : nppData._scintillaSecondHandle;
};

void about() {
	::MessageBox(nppData._nppHandle, TEXT("A Notepad++ plugin that adds Vim-style editing and key bindings."), TEXT("Converter Plugin"), MB_OK);
}

bool isNormalMode = true;

void ToggleVimMode() {

	HWND hwndEdit = getCurrentScintillaHandle();
	isNormalMode = !isNormalMode;

	SetCursorStyle(hwndEdit);
	UpdateStatusBar();

}

void UpdateStatusBar() {
	const TCHAR* statusText = isNormalMode ? TEXT("-- NORMAL --") : TEXT("-- INSERT --");
	::SendMessage(nppData._nppHandle, NPPM_SETSTATUSBAR, STATUSBAR_DOC_TYPE, (LPARAM)statusText);
}

void SetCursorStyle(HWND hwndEdit) {
	::SendMessage(hwndEdit, SCI_SETCARETSTYLE, isNormalMode ? CARETSTYLE_BLOCK : CARETSTYLE_LINE, 0);
}

void ToggleToInsertMode(HWND hwndEdit) {
	isNormalMode = false;
	SetCursorStyle(hwndEdit);
	UpdateStatusBar();
}

void ToggleToNormalMode(HWND hwndEdit) {
	isNormalMode = true;
	SetCursorStyle(hwndEdit);
	UpdateStatusBar();
}

void HandleKeyPress(HWND hwndEdit, WPARAM wParam) {
	if (isNormalMode && wParam == 'i') {
		ToggleToInsertMode(hwndEdit);
		::SendMessage(hwndEdit, SCI_DELETEBACK, 0, 0); // Remove inserted 'i'
	}
	else if (!isNormalMode && wParam == VK_ESCAPE) {
		ToggleToNormalMode(hwndEdit);
	}
}
