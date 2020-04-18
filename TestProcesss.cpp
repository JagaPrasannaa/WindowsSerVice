#include<Windows.h>
#include<iostream>
#include <Tchar.h>
#include<wtsapi32.h>
#include<UserEnv.h>
using namespace std;
void ImpersonateActiveUserAndRun(WCHAR* path, WCHAR* args);
int main(int argc, WCHAR** argv)
{
	WCHAR path[] = L"C:\\Windows\\System32\\cmd.exe";
	WCHAR args[] = L"";
	ImpersonateActiveUserAndRun(path,args);
}


void ImpersonateActiveUserAndRun(WCHAR* path,WCHAR* args)
{
	DWORD session_id = -1;
	DWORD session_count = 0;

	WTS_SESSION_INFOW* pSession = NULL;


	if (WTSEnumerateSessions(WTS_CURRENT_SERVER_HANDLE, 0, 1, &pSession, &session_count))
	{
		//log success
	}
	else
	{
		//log error
		return;
	}

	for (DWORD i = 0; i < session_count; i++)
	{
		session_id = pSession[i].SessionId;

		WTS_CONNECTSTATE_CLASS wts_connect_state = WTSDisconnected;
		WTS_CONNECTSTATE_CLASS* ptr_wts_connect_state = NULL;

		DWORD bytes_returned = 0;
		if (WTSQuerySessionInformation(
			WTS_CURRENT_SERVER_HANDLE,
			session_id,
			WTSConnectState,
			reinterpret_cast<LPTSTR*>(&ptr_wts_connect_state),
			&bytes_returned))
		{
			wts_connect_state = *ptr_wts_connect_state;
			::WTSFreeMemory(ptr_wts_connect_state);
			if (wts_connect_state != WTSActive)
				continue;
		}
		else
		{
			//log error
			continue;
		}

		HANDLE hImpersonationToken;

		if (!WTSQueryUserToken(session_id, &hImpersonationToken))
		{
			//log error
			continue;
		}


		//Get real token from impersonation token
		DWORD neededSize1 = 0;
		HANDLE* realToken = new HANDLE;
		if (GetTokenInformation(hImpersonationToken, (::TOKEN_INFORMATION_CLASS) TokenLinkedToken, realToken, sizeof(HANDLE), &neededSize1))
		{
			CloseHandle(hImpersonationToken);
			hImpersonationToken = *realToken;
		}
		else
		{
			//log error
			continue;
		}


		HANDLE hUserToken;

		if (!DuplicateTokenEx(hImpersonationToken,
			//0,
			//MAXIMUM_ALLOWED,
			TOKEN_ASSIGN_PRIMARY | TOKEN_ALL_ACCESS | MAXIMUM_ALLOWED,
			NULL,
			SecurityImpersonation,
			TokenPrimary,
			&hUserToken))
		{
			//log error
			continue;
		}

		// Get user name of this process
		//LPTSTR pUserName = NULL;
		WCHAR* pUserName;
		DWORD user_name_len = 0;

		if (WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, session_id, WTSUserName, &pUserName, &user_name_len))
		{
			//log username contained in pUserName WCHAR string
		}

		//Free memory                         
		if (pUserName) WTSFreeMemory(pUserName);

		ImpersonateLoggedOnUser(hUserToken);

		STARTUPINFOW StartupInfo;
		GetStartupInfoW(&StartupInfo);
		StartupInfo.cb = sizeof(STARTUPINFOW);
		//StartupInfo.lpDesktop = "winsta0\\default";

		PROCESS_INFORMATION processInfo;

		SECURITY_ATTRIBUTES Security1;
		Security1.nLength = sizeof SECURITY_ATTRIBUTES;

		SECURITY_ATTRIBUTES Security2;
		Security2.nLength = sizeof SECURITY_ATTRIBUTES;

		void* lpEnvironment = NULL;

		// Get all necessary environment variables of logged in user
		// to pass them to the new process
		BOOL resultEnv = CreateEnvironmentBlock(&lpEnvironment, hUserToken, FALSE);
		if (!resultEnv)
		{
			//log error
			continue;
		}

		WCHAR PP[1024];
		//path and parameters
		ZeroMemory(PP, 1024 * sizeof WCHAR);
		wcscpy_s(PP, path);
		wcscat_s(PP, L" ");
		wcscat_s(PP, args);
		// Start the process on behalf of the current user 
		BOOL result = CreateProcessAsUserW(hUserToken,NULL,PP,NULL,NULL,FALSE,NORMAL_PRIORITY_CLASS | CREATE_NEW_CONSOLE,NULL,NULL,&StartupInfo,&processInfo);

		if (!result)
		{
			//log error
		}
		else
		{
			//log success
		}

		DestroyEnvironmentBlock(lpEnvironment);

		CloseHandle(hImpersonationToken);
		CloseHandle(hUserToken);
		CloseHandle(realToken);

		RevertToSelf();
	}

	WTSFreeMemory(pSession);
}
