#include <Windows.h>
#include <tchar.h>
#include <fstream>
#include <iostream>
#include <time.h>
#include <UserEnv.h>
#include <WtsApi32.h>

#pragma comment (lib,"wtsapi32")
#pragma comment (lib,"userenv")
using namespace std;
SERVICE_STATUS        g_ServiceStatus = { 0 };
SERVICE_STATUS_HANDLE g_StatusHandle = NULL;
HANDLE                g_ServiceStopEvent = INVALID_HANDLE_VALUE;

VOID WINAPI ServiceMain(DWORD argc, LPTSTR* argv);
VOID WINAPI ServiceCtrlHandler(DWORD);
DWORD WINAPI ServiceWorkerThread(LPVOID lpParam);
void ImpersonateActiveUserAndRun(WCHAR* path, WCHAR* args);
#define SERVICE_NAME  _T("My Sample Service")

int _tmain(int argc, TCHAR* argv[])
{
    OutputDebugString(_T("My Sample Service: Main: Entry"));

    SERVICE_TABLE_ENTRY ServiceTable[] =
    {
        {SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION)ServiceMain},
        {NULL, NULL}
    };

    if (StartServiceCtrlDispatcher(ServiceTable) == FALSE)
    {
        OutputDebugString(_T("My Sample Service: Main: StartServiceCtrlDispatcher returned error"));
        return GetLastError();
    }

    OutputDebugString(_T("My Sample Service: Main: Exit"));
    return 0;
}


VOID WINAPI ServiceMain(DWORD argc, LPTSTR* argv)
{
    DWORD Status = E_FAIL;

    OutputDebugString(_T("My Sample Service: ServiceMain: Entry"));

    g_StatusHandle = RegisterServiceCtrlHandler(SERVICE_NAME, ServiceCtrlHandler);

    if (g_StatusHandle == NULL)
    {
        OutputDebugString(_T("My Sample Service: ServiceMain: RegisterServiceCtrlHandler returned error"));
        goto EXIT;
    }

    // Tell the service controller we are starting
    ZeroMemory(&g_ServiceStatus, sizeof(g_ServiceStatus));
    g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_ServiceStatus.dwControlsAccepted = 0;
    g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    g_ServiceStatus.dwWin32ExitCode = 0;
    g_ServiceStatus.dwServiceSpecificExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 0;

    if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
    {
        OutputDebugString(_T("My Sample Service: ServiceMain: SetServiceStatus returned error"));
    }

    /*
     * Perform tasks neccesary to start the service here
     */
    OutputDebugString(_T("My Sample Service: ServiceMain: Performing Service Start Operations"));

    // Create stop event to wait on later.
    g_ServiceStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (g_ServiceStopEvent == NULL)
    {
        OutputDebugString(_T("My Sample Service: ServiceMain: CreateEvent(g_ServiceStopEvent) returned error"));

        g_ServiceStatus.dwControlsAccepted = 0;
        g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        g_ServiceStatus.dwWin32ExitCode = GetLastError();
        g_ServiceStatus.dwCheckPoint = 1;

        if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
        {
            OutputDebugString(_T("My Sample Service: ServiceMain: SetServiceStatus returned error"));
        }
        goto EXIT;
    }

    // Tell the service controller we are started
    g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    g_ServiceStatus.dwWin32ExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 0;

    if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
    {
        OutputDebugString(_T("My Sample Service: ServiceMain: SetServiceStatus returned error"));
    }

    // Start the thread that will perform the main task of the service
    HANDLE hThread = CreateThread(NULL, 0, ServiceWorkerThread, NULL, 0, NULL);

    OutputDebugString(_T("My Sample Service: ServiceMain: Waiting for Worker Thread to complete"));

    // Wait until our worker thread exits effectively signaling that the service needs to stop
    WaitForSingleObject(hThread, INFINITE);

    OutputDebugString(_T("My Sample Service: ServiceMain: Worker Thread Stop Event signaled"));


    /*
     * Perform any cleanup tasks
     */
    OutputDebugString(_T("My Sample Service: ServiceMain: Performing Cleanup Operations"));

    CloseHandle(g_ServiceStopEvent);

    g_ServiceStatus.dwControlsAccepted = 0;
    g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    g_ServiceStatus.dwWin32ExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 3;

    if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
    {
        OutputDebugString(_T("My Sample Service: ServiceMain: SetServiceStatus returned error"));
    }

EXIT:
    OutputDebugString(_T("My Sample Service: ServiceMain: Exit"));

    return;
}


VOID WINAPI ServiceCtrlHandler(DWORD CtrlCode)
{
    OutputDebugString(_T("My Sample Service: ServiceCtrlHandler: Entry"));

    switch (CtrlCode)
    {
    case SERVICE_CONTROL_STOP:

        OutputDebugString(_T("My Sample Service: ServiceCtrlHandler: SERVICE_CONTROL_STOP Request"));

        if (g_ServiceStatus.dwCurrentState != SERVICE_RUNNING)
            break;

        /*
         * Perform tasks neccesary to stop the service here
         */

        g_ServiceStatus.dwControlsAccepted = 0;
        g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
        g_ServiceStatus.dwWin32ExitCode = 0;
        g_ServiceStatus.dwCheckPoint = 4;

        if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
        {
            OutputDebugString(_T("My Sample Service: ServiceCtrlHandler: SetServiceStatus returned error"));
        }

        // This will signal the worker thread to start shutting down
        SetEvent(g_ServiceStopEvent);

        break;

    default:
        break;
    }

    OutputDebugString(_T("My Sample Service: ServiceCtrlHandler: Exit"));
}


DWORD WINAPI ServiceWorkerThread(LPVOID lpParam)
{
    OutputDebugString(_T("My Sample Service: ServiceWorkerThread: Entry"));

    //  Periodically check if the service has been requested to stop
    while (WaitForSingleObject(g_ServiceStopEvent, 0) != WAIT_OBJECT_0)
    {
        /*
         * Perform main service function here
         */
        /*std::ofstream myfile("C:\\Users\\jagap\\source\\repos\\ServiceTest\\Debug\\file3.txt");
        time_t now = time(0);
        char* dt = ctime(&now);
        myfile << "the service time " << dt;
        myfile.close();*/
         //  Simulate some work by sleeping
        WCHAR path[] = L"C:\\Windows\\System32\\cmd.exe";
        WCHAR args[] = L"";
        ImpersonateActiveUserAndRun(path, args);
        Sleep(3000);
    }

    OutputDebugString(_T("My Sample Service: ServiceWorkerThread: Exit"));

    return ERROR_SUCCESS;
}



void ImpersonateActiveUserAndRun(WCHAR* path, WCHAR* args)
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
            std::cout << GetLastError() << endl;
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

            cout << GetLastError() << endl;
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
        WCHAR type[] = L"winsta0\\default";
        StartupInfo.lpDesktop = type;

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
        BOOL result = CreateProcessAsUserW(hUserToken, NULL, PP, NULL, NULL, FALSE, NORMAL_PRIORITY_CLASS | CREATE_NEW_CONSOLE, NULL, NULL, &StartupInfo, &processInfo);

        if (!result)
        {
            //log error

            cout << GetLastError() << endl;
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
