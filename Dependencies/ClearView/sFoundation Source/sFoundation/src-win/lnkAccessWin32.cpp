//******************************************************************************
// DESCRIPTION
/**
	\file
	\brief Operating System Specific Programming Interfaces

	Contains utility functions for returning localized error messages and
	operating system infrastructure such as precision time stamps and the
	like.
**/
// PRINCIPLE AUTHORS:
//	DWS
//
// CREATION DATE:
//	02/11/1998 16:34:00
//  06/09/2009 17:39 Refactored from ControlPoint implementation
//
// COPYRIGHT NOTICE:
//	(C)Copyright 1998-2018  Teknic, Inc.  All rights reserved.
//
//	This copyright notice must be reproduced in any copy, modification, 
//	or portion thereof merged into another program. A copy of the 
//	copyright notice must be included in the object library of a user
//	program.
// 																			   *
//******************************************************************************


/// \cond INTERNAL_DOC

//******************************************************************************
// NAME																		   *
// 	lnkAccessWin32.cpp headers
//
#if defined(_MSC_VER)
	#pragma warning(disable:4995)	// Avoid studio warnings
	#pragma comment(lib, "wbemuuid.lib")
	#pragma comment (lib, "Setupapi.lib")
#endif

	// Windows specific headers
	#include <windows.h>
	#include <winbase.h>
	#include <tchar.h>
	#include <strsafe.h>
	#include <stdio.h>
	#include <Shlwapi.h>
	#include <comdef.h>
	#include <Wbemidl.h>
	#include <objbase.h>
	#include <Setupapi.h>
	#include <devguid.h>

	// Our driver headers
	#include "lnkAccessCommon.h"
	#include "lnkAccessAPIwin32.h"
	#include "SerialEx.h"
	#include "sFoundResource.h"
	#include "ErrCodeStr.hpp"
	// Standard library headers
	#include <assert.H>
	#include <sstream>
	// _RPT
	#include <crtdbg.h>
// 																			   *
//******************************************************************************



//******************************************************************************
// NAME																		   *
// 	lnkAccessWin32.cpp constants
// 
//- - - - - - - - - - - - - - - - - - - -
// Registry Key for customizations
//- - - - - - - - - - - - - - - - - - - -
#define CMD_DRIVER_KEY		_T("Software\\Teknic\\MeridianDumpNext")
// Registry Keys
#define CMD_QUEUE_LIM_VAL	_T("QueueLimit")
#define CMD_PRIO_BOOST_VAL	_T("Priority")
#define CMD_DUMP_ON_EXIT	_T("Dump")
#define CMD_RESP_TO			_T("RespTimeout")
#define CMD_SEND_TO			_T("CmdTimeout")
#define CMD_LAST_DUMP	    _T("LastDump")
#define CMD_DIAGS			_T("Diagnostics")

//- - - - - - - - - - - - - - - - - - - -
// TRACING/DEBUG SETUP
//- - - - - - - - - - - - - - - - - - - -
#ifdef _DEBUG
#define T_ON TRUE
#else
#define T_ON FALSE
#endif
#define T_OFF FALSE

#define TRACE_LOW_PRINT			T_OFF	// Print low level activities
#define TRACE_HIGH_LEVEL		T_OFF	// Print high level activities
#define TRACE_HEAP				T_OFF	// Enable heap tracing

//- - - - - - - - - - - - - - - - - - - -
// MISCELLANEOUS
//- - - - - - - - - - - - - - - - - - - -
#define TARGET_RESOLUTION		1U		// 1 ms target for multimedia hack
#define RESPONSE_REAL					// Define to charge overhead to complete functions
#define ERR_CODE_STR_MAX		256U	// Internal string allocation
// 																			  *
//*****************************************************************************


//*****************************************************************************
// NAME																		  *
// 	lnkAccessWin32.cpp imported information
// 
	// Count of specified ports
	extern unsigned SysPortCount;
// 																			  *
//*****************************************************************************


//*****************************************************************************
// NAME																		  *
// 	lnkAccessWin32.cpp function prototypes
// 
		
// Convert last DLL error code into an appropriate mnError
cnErrCode osErrToMnErr();

// Update module statics from registry
void ReadRegistry();				


#ifdef USE_ACTIVEX
	// Release all of our references to the mnClassEx
	void releaseMNref(netaddr cNum);
	void createGIT(void);
	void freeGIT(void);
#endif
// 																			   *
//******************************************************************************


//******************************************************************************
// NAME																		   *
// 	lnkAccessWin32.cpp static variables
//

// Windows DLL information
static HINSTANCE hInst;						// DLL instance handle
static HMODULE hModule;						// DLL module handler
static char m_DLLpath[_MAX_PATH];			// DLL file path
static ULONG DLLfileVersionCode=0;			// File version code
static ULONG nProcAttached=0;				// Count of attached processes
static ULONG nThreadsAttached=0;			// Count of attached thread		
static ErrCodeStr errDictionary;
// Base data for infcCoreTime()	
static UINT wTimerRes;						// MM timer setting
static double perfTicksPerMS;				// # of perf ticks per millisecond
static double perfStartTimeMS;				// Time offset to start of DLL
#ifdef USE_ACTIVEX
	static CComModule _Module;
	// ActiveX references to Network Adapter Objects
	mn1::_NetAdapterObj *mnRef[NET_CONTROLLER_MAX] = {NULL,NULL,NULL};
	static IGlobalInterfaceTable *m_pGIT = NULL;		// Global Interface Table
#endif
static CCCriticalSection *pInitLock;		// Initialization critical section
static bool dllExiting = false;				// Set when DLL is exiting
 
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// These variables are overridden by the registry if values exist.
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
extern BOOL InfcDumpOnExit;						// Dump on exit global
extern unsigned InfcRespTimeOut;				// Timeout setting
extern int InfcPrioBoostFactor;					// Default read thread priority
extern unsigned InfcLastDumpNumber;				// Last dump file number
extern BOOL InfcDiagnosticsOn;					// Inhibits running diagnostics
// Inventory records of where nodes are
extern mnNetInvRecords SysInventory[NET_CONTROLLER_MAX];
//																			   *
//******************************************************************************

#define CO_INIT 0				// NZ will lead to crashes/VB apps

//******************************************************************************
//	NAME
//		DllMain
//
//	DESCRIPTION:
//		This is the function called by the DLL loader. It will create a read
//		and write thread as appropriate.
//
//	RETURNS:
//		TRUE if DLL loaded properly
//
//	SYNOPSIS:
BOOL __stdcall DllMain(
	HINSTANCE hInstDLL,
	DWORD dwReason,
	LPVOID lpNot)
{															
#if TRACE_HEAP
   static _CrtMemState s1, s2;
   static int *pFooLeak;
#endif
	LARGE_INTEGER tickFreq;
	netaddr cNum;
    BYTE* m_pVersionInfo;					// all version info
	DWORD len;
	TIMECAPS tc;
	BOOL retCode = TRUE;
	#if TRACE_HEAP
		int settings;
	#endif

	hInst = hInstDLL;

	// Get the version information for this module
	switch (dwReason) {
		case DLL_PROCESS_ATTACH:
			#if TRACE_HEAP
				settings =_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
				_CrtSetDbgFlag(settings|_CRTDBG_CHECK_ALWAYS_DF);
				_CrtMemCheckpoint(&s1);
				// Test the test!
				pFooLeak = new int(11181);
			#endif

			#if TRACE_LOW_PRINT
				_RPT0(_CRT_WARN, "DllMain: process attach\n");
			#endif
#if CO_INIT
			// Init process for potential WMI access
			CoInitializeEx(0, COINIT_MULTITHREADED);
			HRESULT hres;
			// Setup our security mumbo jumbo for potential WMI calls
			hres =  CoInitializeSecurity(
				NULL,     
				-1,      // COM negotiates service                  
				NULL,    // Authentication services
				NULL,    // Reserved
				RPC_C_AUTHN_LEVEL_DEFAULT,    // authentication
				RPC_C_IMP_LEVEL_IMPERSONATE,  // Impersonation
				NULL,             // Authentication info 
				EOAC_NONE,        // Additional capabilities
				NULL              // Reserved
				);
		#if _DEBUG
		//		assert(SUCCEEDED(hres));
		#endif
#endif

			// Initialize on first instance
			if (++nProcAttached==1) {
				// Set to default value
				InfcPrioBoostFactor = THREAD_PRIORITY_ABOVE_NORMAL;
				pInitLock = new CCCriticalSection;
	
				// get module file name
				len = GetModuleFileNameA(hInstDLL, m_DLLpath, 
											  sizeof(m_DLLpath)/sizeof(m_DLLpath[0]));
				if (len > 0) {
					// We got the name, get the version information

					// read file version info
					DWORD dwDummyHandle; // will always be set to zero
					VS_FIXEDFILEINFO *vInfo;

					len = GetFileVersionInfoSizeA(m_DLLpath, &dwDummyHandle);
					if (len > 0)  {
						m_pVersionInfo = (BYTE *)malloc(len); // allocate version info
						assert(m_pVersionInfo);
						if (!GetFileVersionInfoA(m_DLLpath, 0, len, m_pVersionInfo)) {
							free(m_pVersionInfo);
							return FALSE;
						}
						// Get the version info into module static
						vInfo = (VS_FIXEDFILEINFO *)(m_pVersionInfo+40);
						if (vInfo->dwSignature == 0xfeef04bd) {
							DLLfileVersionCode = ((vInfo->dwFileVersionMS&0xffff0000)<<8) 
										   | ((vInfo->dwFileVersionMS&0xff)<<16)
										   | ((vInfo->dwFileVersionLS&0xffff0000)>>16);
						}
						// Prevent memory leaks
						free(m_pVersionInfo);
						// Load our error strings from XML file
						char foldername[MAX_PATH];
						strncpy(foldername, m_DLLpath, sizeof(foldername));
						PathRemoveFileSpecA(foldername);
						strncat(foldername, LNK_ACCESS_XML_ERR_TXT, MAX_PATH);
						// Load error code defs file
						errDictionary.load(foldername);
					}

					// Get module handle
					hModule = GetModuleHandleA(m_DLLpath);
					// Prevent notifications of thread attach/detach, we 
					// don't care about this.
					#ifndef _DEBUG
					HMODULE xx;
					BOOL ok = GetModuleHandleExA(6, m_DLLpath, &xx);
					if (ok) {
						BOOL disableThreadLib = DisableThreadLibraryCalls(hModule);
						if (!disableThreadLib) {
							DWORD theErr = GetLastError();
							fprintf(stderr, "DllMain: DisableThreadLibraryCalls failed %ld\n", theErr);
						}
						assert(disableThreadLib);
					}
					#endif
				}

				// Calculate # of performance ticks per milliseconds
				QueryPerformanceFrequency(&tickFreq);
				perfTicksPerMS = 1000./(double)tickFreq.QuadPart;
				// Reset the initial time
				perfStartTimeMS = infcCoreTime();
				// Override the defaults from registry if present
				ReadRegistry();
				// Speed up the time quantum, makes for more response
				// application.
				if (timeGetDevCaps(&tc, sizeof(TIMECAPS)) != TIMERR_NOERROR) {
					// Error; application can't continue.
					wTimerRes = 0;
				}
				else {
					wTimerRes = min(max(tc.wPeriodMin, TARGET_RESOLUTION), tc.wPeriodMax);
					timeBeginPeriod(wTimerRes); 
				}
			}
			break;

		case DLL_PROCESS_DETACH:
			#if TRACE_LOW_PRINT
				_RPT0(_CRT_WARN, "DllMain: PROCESS_DETACH\n");
			#endif
			// Destruct items on last one out
			if (--nProcAttached == 0) {
				HKEY keyHndl;
				LONG rErr;
				_RPT1(_CRT_WARN, "%.1f DllMain: shutting down\n", infcCoreTime());
				dllExiting = true;

				// Write out the last dump number
				if (!RegOpenKeyEx(HKEY_CURRENT_USER, CMD_DRIVER_KEY, 0, 
								  KEY_WRITE, &keyHndl)) {
					if (ERROR_SUCCESS != (rErr=RegSetValueEx(keyHndl, CMD_LAST_DUMP, 0, 
									   REG_DWORD, (CONST BYTE *)&InfcLastDumpNumber, sizeof(DWORD)))) {
						_RPT1(_CRT_WARN, "Last dump registry not written: %d\n", rErr);
					}
				}
				// Return the MM timer back to original setting
				if (wTimerRes != 0)
					timeEndPeriod(wTimerRes); 
				// Return all the resources that are opened
				for (cNum=0 ; cNum<SysPortCount ; cNum++) {
					// Kill all the resources in play
					SysInventory[cNum].clearNodes(true);
					#ifdef USE_ACTIVEX
					// insure the net adapter is released from GIT
					releaseMNref(cNum);
					#endif

				} // for (each net)
				#if USE_ACTIVEX
				// Release our ref to GIT
				freeGIT();
				#endif
				if (pInitLock) {
					delete pInitLock;
					pInitLock = NULL;
				}
				#if TRACE_HIGH_LEVEL
					_RPT1(_CRT_WARN, "%.1f DllMain: detached\n", infcCoreTime());
				#endif
				#if TRACE_HEAP
				_CrtMemCheckpoint(&s2);
				{
					_CrtMemState s3;
					if (_CrtMemDifference(&s3, &s1,&s2)) {
						_CrtMemDumpStatistics( &s3 );
					}
				}
				_CrtDumpMemoryLeaks() ;
				#endif

			} //if (--nProcAttached == 0)
			else {
				_RPT0(_CRT_WARN, "skipped cleanup\n");
			}
			// Undo WMI attachment
			#if CO_INIT
				CoUninitialize();
			#endif
			break;
		// These will not be called due to DisableThreadLibraryCalls() API
		case DLL_THREAD_DETACH:
			#if TRACE_LOW_PRINT
				_RPT0(_CRT_WARN, "DllMain: thread detach\n");
			#endif
			// Undo WMI attachment
			#if CO_INIT
				CoUninitialize();
			#endif
			nThreadsAttached--;
			break;
		case DLL_THREAD_ATTACH:
			++nThreadsAttached;
			// Init apartment for potential WMI access
			#if CO_INIT
				CoInitializeEx(0, COINIT_MULTITHREADED);
			#endif
			#if TRACE_LOW_PRINT
				_RPT0(_CRT_WARN, "DllMain: thread attach\n");
			#endif
			break;
	}
	return retCode;
}
//																			    *
//*******************************************************************************



//******************************************************************************
//	NAME																	   *
//		infcCoreTime
//
//	DESCRIPTION:
//		Return the high precision time value.
//
//	RETURNS:
//		double count in milliseconds
//
//	SYNOPSIS:
MN_EXPORT double MN_DECL infcCoreTime(void)
{															
	LARGE_INTEGER timeNow;
	QueryPerformanceCounter(&timeNow);
	return((double)timeNow.QuadPart*perfTicksPerMS - perfStartTimeMS);
}
//																			   *
//******************************************************************************


//******************************************************************************
//	NAME																	   *
//		osErrToMnErr
//
//	DESCRIPTION:
//		Convert specific WIN32 errors into cnErrCode type errors for better
//		understanding by the application writer. Most of this occurs due
//		to the translation of kernel status code to win32 error codes
//
//	RETURNS:
//		cnErrCode
//
//	SYNOPSIS:
cnErrCode osErrToMnErr()
{															
	int sysErr;
	sysErr = GetLastError();					// Get OS error code
	switch (sysErr)	  {
		case ERROR_INVALID_HANDLE:
			return(MN_ERR_PORT_PROBLEM);
	}
	return (cnErrCode)sysErr;
}
//																			  *
//*****************************************************************************



//****************************************************************************
//	NAME																	 *
//		infcVersion
//
//	DESCRIPTION:
//		Return the DLL driver code in 8.8.16 format.
//
//	RETURNS:
//		unsigned long version code
//
//	SYNOPSIS:
MN_EXPORT nodeulong MN_DECL infcVersion(void)
{															
	return(DLLfileVersionCode);
}
//																			 *
//****************************************************************************



//****************************************************************************
//	NAME																	 *
//		infcFilenameA
//
//	DESCRIPTION:
//		Update the ANSI <fname> string up to <len> chars with the file name of 
//		this DLL.
//
//	RETURNS:
//		fname updated
//
//	SYNOPSIS:
MN_EXPORT void MN_DECL infcFileNameA(char *fname, long len)
{
	strncpy(fname, m_DLLpath, len);
}
//																			 *
//****************************************************************************




//*****************************************************************************
//	NAME																	  *
//		infcGetDumpDir
//
//	DESCRIPTION:
///		Return the directory where automatic dump files are created with
///		trailing directory delimitor.  This will be "%TEMP%\Teknic\" 
///
/// 	\param xxx description
///		\return description
/// 
/// 	Detailed description.
//
//	SYNOPSIS:
void infcGetDumpDir(
		char *pStr,						// Ptr to string area
		nodelong maxLen)
{
	// Create a dump file in the temp directory
	GetTempPathA(maxLen, pStr);
	strncat(pStr, "Teknic\\", maxLen);
}
//																			  *
//*****************************************************************************



/*****************************************************************************
 *	NAME
 *		infcTraceDumpA
 *
 *	DESCRIPTION:
 *		Dump and reset the trace log with a ANSI string interface 
 *
 *	RETURNS:
 *		cnErrCode
 *
 *	SYNOPSIS: 															    */
MN_EXPORT cnErrCode MN_DECL infcTraceDumpA(
		netaddr cNum, 
		nodeANSIstr filePath)
{															

	LPWSTR pUniStr;
	int size, uSize;
	cnErrCode err;
	size = (int)strlen((char *)filePath);
	uSize = (1+size)*sizeof(WCHAR);
	pUniStr = (LPWSTR)malloc(uSize);
	if (!pUniStr)
		return(MN_ERR_MEM_LOW);
	// Convert ANSI to Unicode
	MultiByteToWideChar(CP_ACP, MB_COMPOSITE, (LPCSTR)filePath, 
						size, pUniStr, uSize);
	// Insure string is terminated
	pUniStr[size] = 0;
	// Dump using the Unicode interface
	err = infcTraceDump(cNum, (nodeUNIstr)pUniStr);
	free(pUniStr);
	return(err);
}
//																			 *
//****************************************************************************


//****************************************************************************
//	NAME																	 *
//		infcStringGetRes
//
//	DESCRIPTION:
//		Return a BSTR resource allocated from the HEAP for resource ID.
// 		in the current code page.
//
//	RETURNS:
//		nodestr, NULL if insufficient memory
//
//	SYNOPSIS:
MN_EXPORT nodebool MN_DECL infcStringGetRes(
		nodestr *dest, 
		nodeushort resID)
{															
	TCHAR string[255];
	unsigned resStrSize;
	unsigned size=254;

	if (size > 254) 
		size=255;
	
	resStrSize = LoadString(hInst, resID, &string[0], size);
	if (resStrSize > 0)  {
		if (dest)
			return SysReAllocString((BSTR*)dest, string);
			//*dest = string;
		else {
			*dest = (nodestr)(SysAllocString(string));
			//*dest = string;
			return(dest != NULL);
		}
	}
	return(FALSE);
}
//																			 *
//****************************************************************************


//*****************************************************************************
//	NAME																	  *
//		infcUnloadLib
//
//	DESCRIPTION:
///		Unload our DLL for abnormal unload test purposes.
///
/// 	\param xxx description
///		\return description
/// 
/// 	Detailed description.
//
//	SYNOPSIS:
MN_EXPORT bool MN_DECL infcUnloadLib()
{
	bool execOK;
	_RPT1(_CRT_WARN, "%.1f infcUnloadLib\n", infcCoreTime());
	execOK = FreeLibrary(hModule)!=0;
	if (!execOK) {
		_RPT2(_CRT_WARN, "%.1f infcUnloadLib failed code=%d\n", 
				infcCoreTime(), GetLastError());
	}
	return(execOK);
}
//																			  *
//*****************************************************************************



//****************************************************************************
//	NAME
//		ReadRegistry
//
//	DESCRIPTION:
//		Read the registry to override this module's initial statics.
//
//	SYNOPSIS:
void ReadRegistry(void)
{
	HKEY keyHndl;
	DWORD regType, regVal, strSize;
	

	if (!RegOpenKeyEx(HKEY_CURRENT_USER, CMD_DRIVER_KEY, 0, KEY_READ, &keyHndl)) {
		// Adjust response timeout from registry if exist
		if (!RegQueryValueEx(keyHndl, 
							 CMD_RESP_TO, NULL, 
							 &regType, (LPBYTE)&regVal, &strSize))  { 
			// Correct type?
			if (regType == REG_DWORD) {
				InfcRespTimeOut = regVal;
			}
		}

		// Priority booster change
		if (!RegQueryValueEx(keyHndl, 
							 CMD_PRIO_BOOST_VAL, NULL, 
							 &regType, (LPBYTE)&regVal, &strSize))  { 
			// Correct type?
			if (regType == REG_DWORD) {
				InfcPrioBoostFactor = regVal;		// Yes, use it													
			}
		}
		// Prevent dump on exit
		if (!RegQueryValueEx(keyHndl, 
							 CMD_DUMP_ON_EXIT, 0, 
							 &regType, (LPBYTE)&regVal, &strSize))  { 
			// Correct type?
			if (regType == REG_DWORD) {
				// Yes, set active based on the registry setting
				InfcDumpOnExit = (regVal!=0);
			}
		}
		if (!RegQueryValueEx(keyHndl, 
							 CMD_LAST_DUMP, 0, 
							 &regType, (LPBYTE)&regVal, &strSize))  { 
			// Correct type?
			if (regType == REG_DWORD) {
				// Yes, set active based on the registry setting
				InfcLastDumpNumber = regVal;
			}
		}
		if (!RegQueryValueEx(keyHndl, 
							 CMD_DIAGS, 0, 
							 &regType, (LPBYTE)&regVal, &strSize))  { 
			// Correct type?
			if (regType == REG_DWORD) {
				// Yes, set active based on the registry setting
				InfcDiagnosticsOn = regVal;
			}
		}
		RegCloseKey(keyHndl);
	}
}
//																			 *
//****************************************************************************





//*****************************************************************************
//	NAME																	  *
//		infcHeapCheck
//
//	DESCRIPTION:
///		Check the heap for corruption issues.
///
/// 	\param xxx description
///		\return description
/// 
/// 	Detailed description.
//
//	SYNOPSIS:
MN_EXPORT void MN_DECL infcHeapCheck(const char *msg)
{
	#if _DEBUG
		if (!_CrtCheckMemory()) {
			_CrtMemState dbg;
			_RPT1(_CRT_WARN, "infcHeapCheck: %s\n", msg);
			_CrtMemDumpStatistics(&dbg);
		}
	#endif
}
//																			  *
//*****************************************************************************

//*****************************************************************************
//	NAME																	  *
//		infcGetHubPorts
//
//	DESCRIPTION:
///		return the SC Hub USB port adapter name(s)
///
/// 
/// 	Detailed description.
//
//	SYNOPSIS:
MN_EXPORT cnErrCode MN_DECL infcGetHubPorts(std::vector<std::string> &comHubPorts)
{
	
	TCHAR TEKNIC_SC4_HUB_PID[] = _T("VID_2890&PID_0213");
	HDEVINFO deviceInfoSet;
	DWORD regType;
	GUID *guidDev = (GUID*) &GUID_DEVCLASS_PORTS; 
	deviceInfoSet = SetupDiGetClassDevs(guidDev, NULL, NULL, DIGCF_PRESENT | DIGCF_PROFILE);
	TCHAR buffer [4000];
	DWORD buffersize =4000;
	int memberIndex = 0;
	
	comHubPorts.clear();
	
	while (true) {
		SP_DEVINFO_DATA deviceInfoData;
		ZeroMemory(&deviceInfoData, sizeof(SP_DEVINFO_DATA));
		deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
		if (SetupDiEnumDeviceInfo(deviceInfoSet, memberIndex, &deviceInfoData) == FALSE) {
			if (GetLastError() == ERROR_NO_MORE_ITEMS) {
				break;
			}
		}
		DWORD nSize=0 ;
		SetupDiGetDeviceInstanceId (deviceInfoSet, &deviceInfoData, buffer, sizeof(buffer), &nSize);
		
		if (_tcsstr(buffer, TEKNIC_SC4_HUB_PID)) {
			_tprintf (_T("Found Teknic SC4-HUB\n"));
			SetupDiGetDeviceRegistryProperty(deviceInfoSet, &deviceInfoData, SPDRP_FRIENDLYNAME, &regType, (BYTE*)buffer, sizeof(buffer), &nSize);

			_tprintf(_T("Friendly name: %s\n"), buffer);
			LPTSTR  next_token;
			next_token = _tcsstr(buffer, _T("(COM"));

			if (next_token != NULL) {
				next_token = _tcstok_s(next_token + _tcslen(_T("(")), _T(")"), &next_token);
				if (next_token != NULL) {
#ifdef UNICODE
					std::vector<char> buffer;
					size_t size = WideCharToMultiByte(CP_UTF8, 0, next_token, -1, NULL, 0, NULL, NULL);
					if (size > 0) {
						buffer.resize(size);
						WideCharToMultiByte(CP_UTF8, 0, next_token, -1, &buffer[0], int(buffer.size()), NULL, NULL);
					}
					std::string portString(&buffer[0]);
#else
					std::string portString(text);
#endif
					printf("Port number %s\n", portString.c_str());
					comHubPorts.push_back(portString);
				}
			}
		}
		memberIndex++;
	}
	if (deviceInfoSet) {
		SetupDiDestroyDeviceInfoList(deviceInfoSet);
	}
	return MN_OK;
}
//																			  *
//*****************************************************************************

//*****************************************************************************
//	NAME																	  *
//		infcGetPortInfo
//
//	DESCRIPTION:
///		return the port adapter name and manufacturer from the registry
///
/// 
/// 	Detailed description.
//
//	SYNOPSIS:
MN_EXPORT cnErrCode MN_DECL infcGetPortInfo(const char *portName, serPortInfo *portInfo)
{
#if !defined(__GNUC__)
	cnErrCode theErr = MN_ERR_BADARG;
	HRESULT hres;
	if (!portInfo)
		return(MN_ERR_BADARG);
	// Insure failure returns nothing.
	portInfo->adapter[0] = 0;
	portInfo->mfg[0] = 0;
    IEnumWbemClassObject* pEnumerator = NULL;
    IWbemLocator *pLoc = 0;
	IWbemServices *pSvc = 0;
	IWbemClassObject *pclsObj=NULL;
	ULONG uReturn = 0;

	// Obtain the initial locator to Windows Management
    // on a particular host computer.
#if CO_INIT==0
	CoInitializeEx(0, COINIT_MULTITHREADED);
#endif

    hres = CoCreateInstance(
        CLSID_WbemLocator,             
        0, 
        CLSCTX_INPROC_SERVER, 
        IID_IWbemLocator, (LPVOID *) &pLoc);
 
	if (FAILED(hres)) {
		theErr = MN_ERR_OS;			// Program has failed.
        goto exit_pt_inst_rel;     
	}


    // Connect to the root\cimv2 namespace with the
    // current user and obtain pointer pSvc
    // to make IWbemServices calls.
    hres = pLoc->ConnectServer(
        _bstr_t(L"ROOT\\CIMV2"), // WMI namespace
        NULL,                    // User name
        NULL,                    // User password
        NULL,                    // Locale
        0,                    	 // Security flags
        NULL,                    // Authority
        NULL,                    // Context object
        &pSvc                    // IWbemServices proxy
        );                              
    
	if (FAILED(hres)) {
		theErr = MN_ERR_OS;              // Program has failed.
		goto exit_pt_inst_rel;
	}
    
	// Set the IWbemServices proxy so that impersonation
    // of the user (client) occurs.
    hres = CoSetProxyBlanket(
       
       pSvc,                         // the proxy to set
       RPC_C_AUTHN_WINNT,            // authentication service
       RPC_C_AUTHZ_NONE,             // authorization service
       NULL,                         // Server principal name
       RPC_C_AUTHN_LEVEL_CALL,       // authentication level
       RPC_C_IMP_LEVEL_IMPERSONATE,  // impersonation level
       NULL,                         // client identity 
       EOAC_NONE                     // proxy capabilities     
    );

    if (FAILED(hres)) {
		theErr = MN_ERR_OS;              // Program has failed.
		goto exit_pt_svc_rel;
	}

    // Use the IWbemServices pointer to make requests of WMI. 
    // Make requests here:

	// For example, query for all the PnP devices
	// TD - 2/8/2011 - All tested serial ports (Native/USB/PCI/PCIe) have a PnP entry but only
	// some ports have a Win32_SerialPort entry
    hres = pSvc->ExecQuery(
        bstr_t("WQL"), 
        bstr_t("SELECT * FROM Win32_PnPEntity"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, 
        NULL,
        &pEnumerator);
    
    if (FAILED(hres)) {
		theErr = MN_ERR_OS;              // Program has failed.
		goto exit_pt_svc_rel;
	}


	// look at all devices until we find the serial port for this cNum
	while (pEnumerator)
    {
		hres = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
		// no more devices to analyze
		if(0 == uReturn)
			break;

        VARIANT vtProp;
		            // Get the value of the Name property
        hres = pclsObj->Get(L"Name", 0, &vtProp, 0, 0);
		std::string tempStr;
		char * memStr;
		memStr = _strdup(_bstr_t(vtProp.bstrVal));
		tempStr = memStr;
		std::string portKey;
		portKey = "(";
		portKey += portName;
		portKey += ")";
		// _strdup uses malloc - we need to free the memory after use
		free(memStr);
		const char * pch; 	
		pch = strstr(tempStr.c_str(), portKey.c_str());
		if(pch){
			memStr = _strdup(_bstr_t(vtProp.bstrVal));
			strncpy(portInfo->adapter, memStr, sizeof(portInfo->adapter));
			// _strdup uses malloc - we need to free the memory after use
			free(memStr);
			VariantClear(&vtProp);
			
			hres = pclsObj->Get(L"Manufacturer", 0, &vtProp, 0, 0);

			memStr = _strdup(_bstr_t(vtProp.bstrVal));
			strncpy(portInfo->mfg, memStr , sizeof(portInfo->mfg));
			// _strdup uses malloc - we need to free the memory after use
			free(memStr);
			VariantClear(&vtProp);
			pclsObj->Release();
			theErr = MN_OK;
			// We have a winner
			break;
		}
		else {
			if (pclsObj)
				pclsObj->Release();
		}
        VariantClear(&vtProp);
	}
exit_pt_svc_rel:
exit_pt_inst_rel:
	if (pSvc)
		pSvc->Release();
	if (pLoc)
		pLoc->Release();
	if (pEnumerator)
		pEnumerator->Release();
#if CO_INIT==0
	CoUninitialize();
#endif
	return theErr;
#else
	return MN_ERR_NOT_IMPL;
#endif
}

//																			  *
//*****************************************************************************
MN_EXPORT nodelong MN_DECL infcDbgDepth(netaddr cNum) {
#if RECORD_PKT_DEPTH
	if (cNum > NET_CONTROLLER_MAX)
		return(0);
	netStateInfo *pNCS = SysInventory[cNum].pNCS;
	if (pNCS)
		return((nodelong)pNCS->pSerialPort->MaxDepth());
#endif
	return(0);
}


//*****************************************************************************
//	NAME																	  *
//		infcOSversion
//
//	DESCRIPTION:
/**		
	Updates a string with the current OS version.

	Adapted from:
	http://msdn.microsoft.com/en-us/library/ms724429%28v=vs.85%29.aspx

 	\param[in] strSize Size of string to return
 	\param[out] pszOS Ptr to UNICODE string to update.

**/
//	SYNOPSIS:
MN_EXPORT void MN_DECL infcOSversionA(nodeulong strSize, char *pszOS) {
	wchar_t osInfo[100];
	size_t converted;
	infcOSversionW(100, osInfo);
	wcstombs_s(&converted, pszOS, wcslen(osInfo)+1, osInfo, _TRUNCATE);
}

MN_EXPORT void MN_DECL infcOSversionW(nodeulong strSize, nodeUNIstr pszOS)
{
	typedef void (WINAPI *PGNSI)(LPSYSTEM_INFO);
	typedef BOOL (WINAPI *PGPI)(DWORD, DWORD, DWORD, DWORD, PDWORD);
	OSVERSIONINFOEX osvi;
	SYSTEM_INFO si;
	PGNSI pGNSI;
	PGPI pGPI;
	BOOL bOsVersionInfoEx; 
	DWORD dwType;

	ZeroMemory(&si, sizeof(SYSTEM_INFO));
	ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));

	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	bOsVersionInfoEx = GetVersionEx((OSVERSIONINFO*) &osvi);

	if(!bOsVersionInfoEx) {
		StringCchCopy(pszOS, strSize, TEXT("N/A"));
		return;
	}

	// Call GetNativeSystemInfo if supported or GetSystemInfo otherwise.

	pGNSI = (PGNSI) GetProcAddress(
	  GetModuleHandle(TEXT("kernel32.dll")), 
	  "GetNativeSystemInfo");
	if(NULL != pGNSI)
	  pGNSI(&si);
	else GetSystemInfo(&si);

	if ( VER_PLATFORM_WIN32_NT==osvi.dwPlatformId && 
		osvi.dwMajorVersion > 4 )
	{
	  StringCchCopy(pszOS, strSize, TEXT("Microsoft "));

	  // Test for the specific product.

	  if ( osvi.dwMajorVersion == 6 )
	  {
		 if( osvi.dwMinorVersion == 0 )
		 {
			if( osvi.wProductType == VER_NT_WORKSTATION )
				StringCchCat(pszOS, strSize, TEXT("Windows Vista "));
			else StringCchCat(pszOS, strSize, TEXT("Windows Server 2008 " ));
		 }

		 if ( osvi.dwMinorVersion == 1 )
		 {
			if( osvi.wProductType == VER_NT_WORKSTATION )
				StringCchCat(pszOS, strSize, TEXT("Windows 7 "));
			else StringCchCat(pszOS, strSize, TEXT("Windows Server 2008 R2 " ));
		 }
	     
		 pGPI = (PGPI) GetProcAddress(
			GetModuleHandle(TEXT("kernel32.dll")), 
			"GetProductInfo");

		 pGPI( osvi.dwMajorVersion, osvi.dwMinorVersion, 0, 0, &dwType);
// These constants were missing from SDK 6.0. These were found in 
// a MSDN example this was grabbed form
#ifndef PRODUCT_PROFESSIONAL 
#define PRODUCT_PROFESSIONAL 0x30
#endif
#ifndef VER_SUITE_WH_SERVER
#define VER_SUITE_WH_SERVER 0x8000
#endif
		 switch( dwType )
		 {
			case PRODUCT_ULTIMATE:
			   StringCchCat(pszOS, strSize, TEXT("Ultimate Edition" ));
			   break;
			case PRODUCT_PROFESSIONAL:
			   StringCchCat(pszOS, strSize, TEXT("Professional" ));
			   break;
			case PRODUCT_HOME_PREMIUM:
			   StringCchCat(pszOS, strSize, TEXT("Home Premium Edition" ));
			   break;
			case PRODUCT_HOME_BASIC:
			   StringCchCat(pszOS, strSize, TEXT("Home Basic Edition" ));
			   break;
			case PRODUCT_ENTERPRISE:
			   StringCchCat(pszOS, strSize, TEXT("Enterprise Edition" ));
			   break;
			case PRODUCT_BUSINESS:
			   StringCchCat(pszOS, strSize, TEXT("Business Edition" ));
			   break;
			case PRODUCT_STARTER:
			   StringCchCat(pszOS, strSize, TEXT("Starter Edition" ));
			   break;
			case PRODUCT_CLUSTER_SERVER:
			   StringCchCat(pszOS, strSize, TEXT("Cluster Server Edition" ));
			   break;
			case PRODUCT_DATACENTER_SERVER:
			   StringCchCat(pszOS, strSize, TEXT("Datacenter Edition" ));
			   break;
			case PRODUCT_DATACENTER_SERVER_CORE:
			   StringCchCat(pszOS, strSize, TEXT("Datacenter Edition (core installation)" ));
			   break;
			case PRODUCT_ENTERPRISE_SERVER:
			   StringCchCat(pszOS, strSize, TEXT("Enterprise Edition" ));
			   break;
			case PRODUCT_ENTERPRISE_SERVER_CORE:
			   StringCchCat(pszOS, strSize, TEXT("Enterprise Edition (core installation)" ));
			   break;
			case PRODUCT_ENTERPRISE_SERVER_IA64:
			   StringCchCat(pszOS, strSize, TEXT("Enterprise Edition for Itanium-based Systems" ));
			   break;
			case PRODUCT_SMALLBUSINESS_SERVER:
			   StringCchCat(pszOS, strSize, TEXT("Small Business Server" ));
			   break;
			case PRODUCT_SMALLBUSINESS_SERVER_PREMIUM:
			   StringCchCat(pszOS, strSize, TEXT("Small Business Server Premium Edition" ));
			   break;
			case PRODUCT_STANDARD_SERVER:
			   StringCchCat(pszOS, strSize, TEXT("Standard Edition" ));
			   break;
			case PRODUCT_STANDARD_SERVER_CORE:
			   StringCchCat(pszOS, strSize, TEXT("Standard Edition (core installation)" ));
			   break;
			case PRODUCT_WEB_SERVER:
			   StringCchCat(pszOS, strSize, TEXT("Web Server Edition" ));
			   break;
		 }
	  }

	  if ( osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 2 )
	  {
		 if( GetSystemMetrics(SM_SERVERR2) )
			StringCchCat(pszOS, strSize, TEXT( "Windows Server 2003 R2, "));
		 else if ( osvi.wSuiteMask & VER_SUITE_STORAGE_SERVER )
			StringCchCat(pszOS, strSize, TEXT( "Windows Storage Server 2003"));
		 else if ( osvi.wSuiteMask & VER_SUITE_WH_SERVER )
			StringCchCat(pszOS, strSize, TEXT( "Windows Home Server"));
		 else if( osvi.wProductType == VER_NT_WORKSTATION &&
				  si.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_AMD64)
		 {
			StringCchCat(pszOS, strSize, TEXT( "Windows XP Professional x64 Edition"));
		 }
		 else StringCchCat(pszOS, strSize, TEXT("Windows Server 2003, "));

		 // Test for the server type.
		 if ( osvi.wProductType != VER_NT_WORKSTATION )
		 {
			if ( si.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_IA64 )
			{
				if( osvi.wSuiteMask & VER_SUITE_DATACENTER )
				   StringCchCat(pszOS, strSize, TEXT( "Datacenter Edition for Itanium-based Systems" ));
				else if( osvi.wSuiteMask & VER_SUITE_ENTERPRISE )
				   StringCchCat(pszOS, strSize, TEXT( "Enterprise Edition for Itanium-based Systems" ));
			}

			else if ( si.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_AMD64 )
			{
				if( osvi.wSuiteMask & VER_SUITE_DATACENTER )
				   StringCchCat(pszOS, strSize, TEXT( "Datacenter x64 Edition" ));
				else if( osvi.wSuiteMask & VER_SUITE_ENTERPRISE )
				   StringCchCat(pszOS, strSize, TEXT( "Enterprise x64 Edition" ));
				else StringCchCat(pszOS, strSize, TEXT( "Standard x64 Edition" ));
			}

			else
			{
				if ( osvi.wSuiteMask & VER_SUITE_COMPUTE_SERVER )
				   StringCchCat(pszOS, strSize, TEXT( "Compute Cluster Edition" ));
				else if( osvi.wSuiteMask & VER_SUITE_DATACENTER )
				   StringCchCat(pszOS, strSize, TEXT( "Datacenter Edition" ));
				else if( osvi.wSuiteMask & VER_SUITE_ENTERPRISE )
				   StringCchCat(pszOS, strSize, TEXT( "Enterprise Edition" ));
				else if ( osvi.wSuiteMask & VER_SUITE_BLADE )
				   StringCchCat(pszOS, strSize, TEXT( "Web Edition" ));
				else StringCchCat(pszOS, strSize, TEXT( "Standard Edition" ));
			}
		 }
	  }

	  if ( osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 1 )
	  {
		 StringCchCat(pszOS, strSize, TEXT("Windows XP "));
		 if( osvi.wSuiteMask & VER_SUITE_PERSONAL )
			StringCchCat(pszOS, strSize, TEXT( "Home Edition" ));
		 else StringCchCat(pszOS, strSize, TEXT( "Professional" ));
	  }

	  if ( osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 0 )
	  {
		 StringCchCat(pszOS, strSize, TEXT("Windows 2000 "));

		 if ( osvi.wProductType == VER_NT_WORKSTATION )
		 {
			StringCchCat(pszOS, strSize, TEXT( "Professional" ));
		 }
		 else 
		 {
			if( osvi.wSuiteMask & VER_SUITE_DATACENTER )
			   StringCchCat(pszOS, strSize, TEXT( "Datacenter Server" ));
			else if( osvi.wSuiteMask & VER_SUITE_ENTERPRISE )
			   StringCchCat(pszOS, strSize, TEXT( "Advanced Server" ));
			else StringCchCat(pszOS, strSize, TEXT( "Server" ));
		 }
	  }

	   // Include service pack (if any) and build number.

	  if( _tcslen(osvi.szCSDVersion) > 0 )
	  {
		  StringCchCat(pszOS, strSize, TEXT(" ") );
		  StringCchCat(pszOS, strSize, osvi.szCSDVersion);
	  }

	  TCHAR buf[80];

	  StringCchPrintf( buf, 80, TEXT(" (build %d)"), osvi.dwBuildNumber);
	  StringCchCat(pszOS, strSize, buf);

	  if ( osvi.dwMajorVersion >= 6 )
	  {
		 if ( si.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_AMD64 )
			StringCchCat(pszOS, strSize, TEXT( ", 64-bit" ));
		 else if (si.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_INTEL )
			StringCchCat(pszOS, strSize, TEXT(", 32-bit"));
	  }
	  
	  return; 
	}

	else
	{  
		StringCchCopy(pszOS, strSize, TEXT("Microsoft Unknown OS"));
		return;
	}
}
//																			  *
//*****************************************************************************


//****************************************************************************
//	NAME
//		infcThreadID
//
//	DESCRIPTION:
//		Return the thread ID of the currently running thread.
//
//	SYNOPSIS:
MN_EXPORT Uint64 MN_DECL infcThreadID()
{
	return GetCurrentThreadId();
}
//																			  *
//*****************************************************************************





/****************************************************************************/
//						    ACTIVEX INTERFACES
/****************************************************************************/
#ifdef USE_ACTIVEX

//****************************************************************************
//	NAME
//		createGIT
//
//	DESCRIPTION:
//		Create the COM Global Interface Table reference if not already created.
//
//	SYNOPSIS:
void createGIT(void)
{
	HRESULT hr;

	if (m_pGIT != NULL)
		return;

	// Create a reference to the Global Interface Table for in-proc STAs
	hr = CoCreateInstance(CLSID_StdGlobalInterfaceTable, NULL,
						  CLSCTX_INPROC_SERVER, IID_IGlobalInterfaceTable,
						  (void **)&m_pGIT);
	_ASSERT(hr==S_OK);
	_ASSERT(m_pGIT);
	m_pGIT->AddRef();
	#if TRACE_HIGH_LEVEL&0
		_RPT1(_CRT_WARN, "createGIT returned %d\n", hr);
	#endif
//	OLE_INIT
//	if (initRes = S_OK)
//		OLE_UNINIT(999);
}
/****************************************************************************/


//****************************************************************************
//	NAME
//		freeGIT
//
//	DESCRIPTION:
//		Release our hold of the Global Interface Table if it exists.
//
//	SYNOPSIS:
void freeGIT(void)
{
	if (m_pGIT)	 {
		m_pGIT->Release();
		m_pGIT = NULL;
	}
}
/****************************************************************************/


//****************************************************************************
//	NAME
//		infcMarshallNetChange
//
//	DESCRIPTION:
//		Change the network change marshall setting.
//
//	RETURNS:
//		Nothing
//
//	SYNOPSIS:
MN_EXPORT void MN_DECL infcMarshallNetChange(
	netaddr cNum,
	nodebool newState)
{
	if (cNum<SysPortCount)  {
		netStateInfo *pNCS = pNet[cNum];
		if (pNCS)
			pNCS->NetChangeMarshalled = newState;	
	} 
}
//																			 *
//****************************************************************************


//****************************************************************************
//	NAME
//		infcSetNetAdapterRef
//
//	DESCRIPTION:
//		This function register's the VB application's network supervisor
//		to gain access to each network's event sink.
//
//	SYNOPSIS:
MN_EXPORT void MN_DECL infcSetNetAdapterRef(netaddr cNum, 
											mn1::_NetAdapterObj **theNet) {
	// Make sure the GIT reference exists
	netStateInfo *pNCS = pNet[cNum];
	createGIT();
	if (mnRef[cNum]) {
		//_RPT2(_CRT_WARN, "infcSetNetAdapterRef(%d) removed %x\n", cNum, mnRef[cNum]); 
		releaseMNref(cNum);
	}
	// Are they assigning new one?
	if (theNet && *theNet) {
		mnRef[cNum] = *theNet;
		HRESULT hr;
		hr = m_pGIT->RegisterInterfaceInGlobal(mnRef[cNum], 
									   __uuidof(mn1::_NetAdapterObj), 
									   &pNCS->mnCookie);
		pNCS->mnThreadID = GetCurrentThreadId();
		//_RPT2(_CRT_WARN, "infcSetNetAdapterRef(%d) added %x\n", cNum, mnRef[cNum]); 
	}
	else {
		mnRef[cNum] = NULL;
		//_RPT1(_CRT_WARN, "infcSetNetAdaptRef(%d) none set\n", cNum); 
	}
}
//																			  *
//*****************************************************************************

//*****************************************************************************
//	NAME																	  *
//		releaseMNref
//
//	DESCRIPTION:
//		Release any references to the netAdapter we hold.
//
//	SYNOPSIS:
void releaseMNref(netaddr cNum)
{
	netStateInfo *pNCS = pNet[cNum];
	if (mnRef[cNum])  {
		HRESULT hr;
		hr = m_pGIT->RevokeInterfaceFromGlobal(pNCS->mnCookie);
		mnRef[cNum] = NULL;
	}
	#if TRACE_HEAP
		if (!_CrtCheckMemory()) {
			_RPT0(_CRT_WARN, "releaseMNref: memory problems\n");
		}
		else {
			_RPT0(_CRT_WARN, "releaseMNref: memory OK\n");
		}
	#endif
}
//																			  *
//*****************************************************************************


#endif		// ifdef USE_ACTIVEX
/// \endcond 
// end of INTERNAL_DOC

//* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
//         PUBLISHED FUNCTION DOCUMENTATION GOES BELOW THIS LINE 
//* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * 

//****************************************************************************
//	NAME
//		infcErrCodeStrA
//
//	DESCRIPTION:
/**
	Return a descriptive ANSI string for the selected \a cnErrCode in the
	\a resultStr buffer. The message will be truncated at the \a maxLen
	location if too long.
		
	<b>Usage Example</b>
	\code
	// Create buffer for message
	char errString[ERR_CODE_STR_MAX];
	errCode = mnInitializeNets(resetNodes, 1, &controller);
	if(errCode != MN_OK) {
		// Get our error message
		infcErrCodeStrA(errCode, ERR_CODE_STR_MAX, errString);
		// Send the message up as failure reason
		throw errString;
	}
	\endcode	
	\param[in] lookupCode The error code to lookup.
	\param[in] maxLen The number of characters in the \a resultStr buffer.
					\a ERR_CODE_STR_MAX is a good value for this.
	\param[in,out] resultStr A pointer to a \a maxLen buffer that the
				   message will be placed in.

	\return MN_OK if \a resultStr is updated, else failure code.
**/
//	SYNOPSIS:
MN_EXPORT cnErrCode MN_DECL infcErrCodeStrA(
	cnErrCode lookupCode,
	Uint16 maxLen,
	char resultStr[])
{
	WCHAR *rStr = (WCHAR *)malloc(maxLen*sizeof(WCHAR));
	if (rStr==NULL) {
		return(MN_ERR_MEM_LOW);
	}
	infcErrCodeStrW(lookupCode, maxLen, rStr);
	WideCharToMultiByte(CP_ACP, 0, rStr, -1, 
			            resultStr, maxLen, NULL, NULL);
	free(rStr);
	return(MN_OK);
}
//																			 *
//****************************************************************************



//****************************************************************************
//	NAME
//		infcErrCodeStrW
//
//	DESCRIPTION:
/**
	Return a descriptive Unicode string for the selected \a cnErrCode in the
	\a resultStr buffer. The message will be truncated at the \a maxLen
	location if too long.
	
	<b>Usage Example</b>
	\code
	// Create buffer for message
	wchar_t errString[ERR_CODE_STR_MAX];
	errCode = mnInitializeNets(resetNodes, 1, &controller);
	if(errCode != MN_OK) {
		// Get our error message
		infcErrCodeStrW(errCode, ERR_CODE_STR_MAX, errString);
		// Send the message up as failure reason
		throw errString;
	}
	\endcode	

	\param[in] lookupCode The error code to lookup.
	\param[in] maxLen The number of characters in the \a resultStr buffer.
					\a ERR_CODE_STR_MAX is a good value for this.
	\param[in,out] resultStr A pointer to a \a maxLen buffer that the
				   message will be placed in.

	\return MN_OK if \a resultStr is updated, else failure code.
**/
//	SYNOPSIS:
MN_EXPORT cnErrCode MN_DECL infcErrCodeStrW(
	cnErrCode lookupCode,
	Uint16 maxLen,
	WCHAR resultStr[])
{
	int loadRet, dllErr;
	WCHAR rStr[ERR_CODE_STR_MAX], errNumStr[ERR_CODE_STR_MAX];
	StringCbPrintf(errNumStr, ERR_CODE_STR_MAX, L" (0x%X)", lookupCode);
	switch(lookupCode)  {
		case MN_OK:
			loadRet = LoadString(hInst, STR_ERR_OK, resultStr, ERR_CODE_STR_MAX);
			swprintf(resultStr, maxLen, L"OK");
			return(MN_OK);
		case MN_ERR_OS:
			dllErr = GetLastError();
			loadRet = FormatMessage(FORMAT_MESSAGE_FROM_HMODULE|FORMAT_MESSAGE_ARGUMENT_ARRAY,
					  hInst,
					  lookupCode-MN_ERR_BASE+STR_ERR_BASE,
					  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
					  (LPTSTR)rStr,
					  ERR_CODE_STR_MAX,
					  (char **)&dllErr);
									
			break;
		default:
			if (lookupCode >= MN_ERR_BASE && lookupCode < MN_ERR_BASE+0xfff) {
				//_RPT4(_CRT_WARN, "looking up resource %d, code=%x, base=%x, strbase=%d\n", 
				//				lookupCode-MN_ERR_BASE+STR_ERR_BASE, lookupCode, MN_ERR_BASE, STR_ERR_BASE);
				//loadRet = LoadString(hInst, lookupCode-MN_ERR_BASE+STR_ERR_BASE,
				//					 rStr, ERR_CODE_STR_MAX);
				char toStringBuf[10];
				snprintf(toStringBuf, sizeof(toStringBuf), "%d", lookupCode - MN_ERR_BASE + STR_ERR_BASE);
				char* errString = errDictionary.lookup(toStringBuf);
				loadRet = swprintf(rStr, maxLen, L"%hs", errString);
			}
			else if (lookupCode > 0 && lookupCode <= 0x7fffffff) {
				loadRet = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, 
									lookupCode, 0, rStr, ERR_CODE_STR_MAX, 0);
				// Kill the CRLF
				if (loadRet > 2) {
					rStr[loadRet-2] = 0;	
				}
			}
			else
				loadRet = 0;
			break;
	}
	// Lookup work out?
	HRESULT prOK;
	if(loadRet == 0)  {
		prOK = StringCbPrintf(resultStr, maxLen, L"cnErrCode 0x%X", lookupCode); 	
	}
	else {
		prOK = StringCbPrintf(resultStr, maxLen, L"%s%s", rStr, errNumStr); 
	}
	return(prOK == S_OK ? MN_OK : (cnErrCode)prOK);
}
//																			 *
//****************************************************************************

ErrCodeStr::ErrCodeStr()
{
}

//============================================================================= 
//	END OF FILE lnkAccessWin32.cpp
//=============================================================================
