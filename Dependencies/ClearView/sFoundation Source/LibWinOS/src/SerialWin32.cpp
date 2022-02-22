// $Workfile: SerialWin32.cpp $  
// $Archive: /ClearPath SC-1.0.123/User Driver/win/src/SerialWin32.cpp $
// $Date: 01/23/2017 11:25 $
//
//	Serial.cpp - Implementation of the CSerial class
//
//	Copyright (C) 1999-2003 Ramon de Klein (Ramon.de.Klein@ict.nl)
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
// 
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#define USE_FTDI_LIB			0		// Use the FTDI library for setup
#define USE_EXAR_SPEEDUP		0	// Reset Exar latency timer (TODO)
extern "C" double __stdcall infcCoreTime(void);

//////////////////////////////////////////////////////////////////////
// Include the standard header files

#define STRICT
#include <crtdbg.h>
#include <tchar.h>
#include <windows.h>


//////////////////////////////////////////////////////////////////////
// Include module headerfile

#include "SerialEx.h"
#include <string.h>
#include <stdlib.h>
#if USE_FTDI_LIB
	#include "ftd2xx.h"
#endif


//////////////////////////////////////////////////////////////////////
// Disable warning C4127: conditional expression is constant, which
// is generated when using the _RPTF and _ASSERTE macros.
#if defined(_MSC_VER)
#pragma warning(disable: 4127)
#endif

#ifdef _DEBUG
#define T_ON TRUE
#else
#define T_ON FALSE
#endif
#define T_OFF FALSE

#define TRACE_THREAD	T_OFF	// Print port IDs
#define TRACE_LOW		T_OFF
#define TRACE_PORT		T_OFF


//////////////////////////////////////////////////////////////////////
// Enable debug memory manager

#ifdef _DEBUG

#ifdef THIS_FILE
#undef THIS_FILE
#endif

static const char THIS_FILE[] = __FILE__;
//#define new DEBUG_NEW

#endif


//////////////////////////////////////////////////////////////////////
// Helper methods
CSerial::SERAPI_ERR translateOSerrToSerErr(DWORD errNum)
{
	switch (errNum) {
	case ERROR_FILE_NOT_FOUND:
	case ERROR_ACCESS_DENIED:
	case ERROR_GEN_FAILURE:
		// The specified COM-port does not exist
		return CSerial::API_ERROR_PORT_UNAVAILABLE;

	default:
		// Something else is wrong
		return CSerial::SERAPI_ERR(errNum);
	}
}


inline void CSerial::CheckRequirements (LPOVERLAPPED lpOverlapped, DWORD dwTimeout) const
{
#ifdef SERIAL_NO_OVERLAPPED

	// Check if an overlapped structure has been specified
	if (lpOverlapped || (dwTimeout != INFINITE))
	{
		// Quit application
		::MessageBox(0,_T("Overlapped I/O and time-outs are not supported, when overlapped I/O is disabled."),_T("Serial library"), MB_ICONERROR | MB_TASKMODAL);
		::DebugBreak();
		::ExitProcess(0xFFFFFFF);
	}

#endif

#ifdef SERIAL_NO_CANCELIO

	// Check if 0 or INFINITE time-out has been specified, because
	// the communication I/O cannot be cancelled.
	if ((dwTimeout != 0) && (dwTimeout != INFINITE))
	{
		// Quit application
		::MessageBox(0,_T("Timeouts are not supported, when SERIAL_NO_CANCELIO is defined"),_T("Serial library"), MB_ICONERROR | MB_TASKMODAL);
		::DebugBreak();
		::ExitProcess(0xFFFFFFF);
	}

#endif	// SERIAL_NO_CANCELIO

	// Avoid warnings
	(void) dwTimeout;
	(void) lpOverlapped;
}

CSerial::SERAPI_ERR  CSerial::CancelCommIo (void)
{
	BOOL lastErr;
	#if TRACE_LOW
		_RPT1(_CRT_WARN, "%.1f CSerial::CancelCommIo\n", infcCoreTime());
	#endif
	// Cancel the I/O request
	// 9/20/11 DS Attempts to find out why re-open fails on serial port.
	// The theory was that the I/O remained pending when port is closed.
	// DIDN'T HELP onder Win7 lastErr = ::CancelIoEx(m_hFile, &m_rdOverlapper);
	lastErr = ::CancelIo(m_hFile);
#if 1
	#if TRACE_LOW
		_RPT1(_CRT_WARN, "%.1f CSerial::ForceCommEvent\n", infcCoreTime());
	#endif
	ForceCommEvent();
	
#else
	// 9/20/11 DS This was suggested by MSDN notes found, didn't
	// seem to help over TD's method.
	// Wait for I/O to cancel
	if (lastErr || ::GetLastError() != ERROR_NOT_FOUND) {
		// Wait for the I/O to actually stop
		DWORD nRead;
		lastErr = ::GetOverlappedResult(m_hFile, &m_rdOverlapper, 
										&nRead, TRUE);
		//#ifdef _DEBUG
		//_RPT1(_CRT_WARN, "CancelIo / wait returns %d\n", lastErr);
		//#endif
	}
#endif
	return lastErr ? API_ERROR_SUCCESS 
				   : (CSerial::SERAPI_ERR)::GetLastError(); 
}
// 4/4/11 DS Code copied from Exar e-mail to turn off the latency timer
// when the port speed is > 40K baud.
#if USE_EXAR_SPEEDUP
#include "WinIoCtl.h"
#define FILE_DEVICE_XRPORT  0x00008005
#define XRPORT_IOCTL_INDEX  0x805
#define EXAR_LATENCY_REG	4
 
#define IOCTL_XRUSBPORT_READ_GENERIC_REG  CTL_CODE(FILE_DEVICE_UNKNOWN , \
XRPORT_IOCTL_INDEX + 16,  \
METHOD_BUFFERED,     \
FILE_ANY_ACCESS)
 
#define IOCTL_XRUSBPORT_WRITE_GENERIC_REG CTL_CODE(FILE_DEVICE_UNKNOWN , \
XRPORT_IOCTL_INDEX + 17,  \
METHOD_BUFFERED,     \
FILE_ANY_ACCESS)
 
typedef struct
{
    unsigned short      wReg;
    unsigned short      wData;
 
} XRUSB_CUSTOM_RW_GENERIC, *PXRUSB_CUSTOM_RW_GENERIC;
// function definition for writing to Exar USB-UART register
BOOL WriteExarReg(HANDLE hDevice, USHORT Reg, USHORT Data)
{
      XRUSB_CUSTOM_RW_GENERIC Data2Driver;
 
      Data2Driver.wReg = Reg;
      Data2Driver.wData = Data;
 
      if(hDevice != INVALID_HANDLE_VALUE) // hDevice is the port handle obtained from CreateFile("\\\\.\\COMx"...
      {
            BOOL Status;
            DWORD  cbReturned;
 
            Status =    DeviceIoControl(hDevice,
                               IOCTL_XRUSBPORT_WRITE_GENERIC_REG,
                               &Data2Driver,
                               sizeof(XRUSB_CUSTOM_RW_GENERIC),
                               NULL,
                               0,
                               &cbReturned,
                               0);
 
            if (!Status)
            {          
                  // you will get a error message box whenever there is an error
                  _RPT0(_CRT_WARN, _T("Error in Writing to USB_UART Reg!\n"));

               return FALSE;
            }
else 
return TRUE;
        }
  else
            return FALSE;
}
 
// function definition for reading from Exar USB-UART register
BOOL ReadExarReg(HANDLE hDevice, USHORT Reg, USHORT *pwValue)
{
      XRUSB_CUSTOM_RW_GENERIC Data2Driver;
      USHORT Value;
 
      Data2Driver.wReg = Reg;
 
      if(hDevice != INVALID_HANDLE_VALUE) // hDevice is the port handle obtained from CreateFile("\\\\.\\COMx"...
      {
            BOOL Status;
            DWORD  cbReturned;
 
            Status =    DeviceIoControl(hDevice,
                               IOCTL_XRUSBPORT_READ_GENERIC_REG,
                               &Data2Driver,
                               sizeof(XRUSB_CUSTOM_RW_GENERIC),
                               &Value,
                               sizeof(USHORT),
                               &cbReturned,
                               0);
            if (!Status)
            {         
                  // you will get a error message box whenever there is an error
				  _RPT0(_CRT_WARN,_T("Error in Reading from USB_UART Reg!\n"));
                  return FALSE;                 
}
else
{
*pwValue = Value;
                  return TRUE;
}
      }
      else
      {
// you will get a error message box whenever there is an error
            _RPT0(_CRT_WARN, _T("The COM Port is not opened for r/w registers!\n"));
            return FALSE;
      }
}

#endif

//*****************************************************************************
//	NAME																	  *
//		CSerial::CSerial
//
//	AUTHOR:
//		Ramon de Klein (Ramon.de.Klein@ict.nl)
//
//	DESCRIPTION:
///		Construction/Destruction
///
/// 	\param xxx description
///		\return description
/// 
/// 	Detailed description.
//
//	SYNOPSIS:
CSerial::CSerial ()
	: m_lLastError(API_ERROR_SUCCESS)
	, m_hFile(0)
	, m_eEvent(EEventNone)
	, m_dwEventMask(0)
#ifndef SERIAL_NO_OVERLAPPED
	, m_DTRBit(EDTRClear)
	, m_RTSBit(ERTSClear)
#endif
{
	#if (defined(_WIN32)||defined(_WIN64))
		memset(&m_rdOverlapper, 0, sizeof(OVERLAPPED));
		m_rdOverlapper.hEvent = m_rdEvent.GetHandle();

		memset(&m_waitCommOverlapper,0,sizeof(m_waitCommOverlapper));
		m_waitCommOverlapper.hEvent = m_commEvent.GetHandle();
	#endif
	m_lLastRdError = m_lLastWrError = API_ERROR_UNKNOWN;
}

CSerial::~CSerial ()
{
	// If the device is already closed,
	// then we don't need to do anything.
	if (m_hFile) {
		// Display a warning
		//_RPTF0(_CRT_WARN,"CSerial::~CSerial - Serial port not closed\n");

		// Close implicitly
		Close();
	}
#if THREAD_THREAD
	_RPTF0(_CRT_WARN,"CSerial::~CSerial (destroyed)\n");
#endif
}
//																			 *
//****************************************************************************

CSerial::EPort CSerial::CheckPort (LPCTSTR lpszDevice)
{

	HANDLE hFile = ::CreateFile(lpszDevice, 
						   GENERIC_READ|GENERIC_WRITE, 
						   0, 
						   0, 
						   OPEN_EXISTING, 
						   0,
						   0);

	#if TRACE_PORT
		_RPTF1(_CRT_WARN, "CSerial::CheckPort port = hndl=0x%x\n", hFile);
	#endif
	// Check if we could open the device
	if (hFile == INVALID_HANDLE_VALUE)
	{
		// Display error
		switch (::GetLastError())
		{
		case ERROR_FILE_NOT_FOUND:
			// The specified COM-port does not exist
			return EPortNotAvailable;

		case ERROR_ACCESS_DENIED:
			// The specified COM-port is in use
			return EPortInUse;

		default:
			// Something else is wrong
			return EPortUnknownError;
		}
	}

	// Close handle
	#if TRACE_PORT
		_RPTF1(_CRT_WARN, "CSerial::CheckPort port(close) = hndl=0x%x\n", hFile);
	#endif
	::CloseHandle(hFile);
	hFile = 0;

	// Port is available
	return EPortAvailable;
}
//																			 *
//****************************************************************************

CSerial::SERAPI_ERR CSerial::Open (
	DWORD portNumber,						// the raw port number for 
	DWORD dwInQueue, 
	DWORD dwOutQueue, 
	bool fOverlapped)
{
	WCHAR str[MAX_PATH];
	// Create windows based port name
	//wsprintf(str, _T("\\\\.\\COM%d"), portNumber);
	wsprintf(str, L"\\\\.\\COM%d", portNumber);
	
#if USE_FTDI_LIB

	FT_STATUS ftStatus; 
	FT_HANDLE ftHandleTemp; 
	DWORD numDevs; 
	LONG thisPortNumber;
	// Insure the FTDI port is setup correctly.
	
	// Find out how many devices?
	ftStatus = FT_CreateDeviceInfoList(&numDevs); 
	if (ftStatus == FT_OK) { 
		//_RPT1(_CRT_WARN, "Number of devices is %d\n",numDevs); 
		for (DWORD pNum = 0 ; pNum<numDevs; pNum++) {
			ftStatus = FT_Open(pNum, &ftHandleTemp);
			if (ftStatus == FT_OK) {
				ftStatus = FT_GetComPortNumber(ftHandleTemp, &thisPortNumber);
				// Is this us?
				if (ftStatus == FT_OK && thisPortNumber==portNumber) {
					////char manuBuf[32];
					////char manuID[16];
					////char descr[64];
					////char sernum[16];
					////FT_PROGRAM_DATA myData;
					////myData.Signature1 = 0;
					////myData.Signature2 = 0xffffffff;
					////myData.Version = 4;
					////myData.Manufacturer = manuBuf;
					////myData.ManufacturerId = manuID;
					////myData.Description = descr;
					////myData.SerialNumber = sernum;
					////ftStatus = FT_EE_Read(ftHandleTemp, &myData);
					// Yes, change latency timer to 1
					//#ifdef _DEBUG
					UCHAR tt;
					ftStatus = FT_GetLatencyTimer(ftHandleTemp, &tt);
					_RPT2(_CRT_WARN, "SerialWin32 COM%d latency now %d\n", portNumber, tt);
					//#endif
					// Set to minimum
					ftStatus = FT_SetLatencyTimer(ftHandleTemp, 2);
					#ifdef _DEBUG
						if (ftStatus != FT_OK) 
							_RPT2(_CRT_WARN, "SerialWin32 COM%d failed set latency %d\n", portNumber, ftStatus); 
						ftStatus = FT_GetLatencyTimer(ftHandleTemp, &tt);
						_RPT2(_CRT_WARN, "SerialWin32 COM%d after set is now %d\n", portNumber, tt);
					#endif
				}
				FT_Close(ftHandleTemp);
			}
		}
	}
#endif
	// Open the port via 
	return(Open(str, dwInQueue, dwOutQueue, fOverlapped));
}

CSerial::SERAPI_ERR CSerial::Open (
	LPCTSTR lpszDevice, 
	DWORD dwInQueue, 
	DWORD dwOutQueue, 
	bool fOverlapped)
{
	// Reset error state
	m_lLastError = API_ERROR_SUCCESS;

	// Check if the port isn't already opened
	if (m_hFile)
	{
		m_lLastError = CSerial::SERAPI_ERR(ERROR_ALREADY_INITIALIZED);
		_RPTF0(_CRT_WARN,"CSerial::Open - Port already opened\n");
		return m_lLastError;
	}

	// Open the device
	m_hFile = ::CreateFile(lpszDevice,
						   GENERIC_READ|GENERIC_WRITE,
						   0,
						   0,
						   OPEN_EXISTING,
						   fOverlapped?FILE_FLAG_OVERLAPPED:0,
						   0);
	#if TRACE_PORT
		_RPTF1(_CRT_WARN, "CSerial::Open port = hndl=0x%x\n", m_hFile);
	#endif
	if (m_hFile == INVALID_HANDLE_VALUE) {
		// Reset file handle
		m_hFile = 0;
		// Display error
		m_lLastError = CSerial::SERAPI_ERR(::GetLastError());
		_RPTF1(_CRT_WARN, "CSerial::Open - Unable to open port err=%d\n", m_lLastError);
		return m_lLastError;
	}
	#if USE_EXAR_SPEEDUP
		// Enable low latency mode
		if(WriteExarReg(m_hFile, EXAR_LATENCY_REG, 1)) 
			_RPT0(_CRT_WARN, "Failed to set Exar options\n");
	#endif
	// Clear out old errors
	//Purge(); 2016-03-10 JS: This causes the Release UserDriver to not work
	// Setup outputs used for brakes to engage brake
	SetDTR(m_DTRBit==EDTRSet);
	SetRTS(m_RTSBit==ERTSSet);

	// Setup the COM-port
	if (dwInQueue || dwOutQueue)
	{
		// Make sure the queue-sizes are reasonable sized. Win9X systems crash
		// if the input queue-size is zero. Both queues need to be at least
		// 16 bytes large.
		_ASSERTE(dwInQueue >= 16);
		_ASSERTE(dwOutQueue >= 16);

		if (!::SetupComm(m_hFile,dwInQueue,dwOutQueue))
		{
			// Display a warning
			long lLastError = ::GetLastError();
			_RPTF0(_CRT_WARN,"CSerial::Open - Unable to setup the COM-port\n");

			// Close the port
			Close();

			// Save last error from SetupComm
			m_lLastError = CSerial::SERAPI_ERR(lLastError);
			return m_lLastError;	
		}
	}
	
	// Setup the default communication mask
	SetEventMask();

	// Non-blocking reads is default
	SetupReadTimeouts(EReadTimeoutNonblocking);

	// Setup the device for default settings
 	COMMCONFIG commConfig = {0};
	DWORD dwSize = sizeof(commConfig);
	commConfig.dwSize = dwSize;
	if (::GetDefaultCommConfig(lpszDevice,&commConfig,&dwSize))

	{
		// Set the default communication configuration
		if (!::SetCommConfig(m_hFile,&commConfig,dwSize))
		{
			// Display a warning
			_RPTF0(_CRT_WARN,"CSerial::Open - Unable to set default communication configuration.\n");
		}
	}
	else
	{
		// Display a warning
		//_RPTF0(_CRT_WARN,"CSerial::Open - Unable to obtain default communication configuration.\n");
	}

	// Return successful
	return m_lLastError;
}

CSerial::SERAPI_ERR CSerial::Close (void)
{
	// Reset error state
	m_lLastError = API_ERROR_SUCCESS;

	// If the device is already closed,
	// then we don't need to do anything.
	if (m_hFile == 0)
	{
		// Display a warning
		//_RPTF0(_CRT_WARN,"CSerial::Close - Method called when device is not open\n");
		return m_lLastError;
	}
	// Insure all open work has stopped
	CancelCommIo();
	// Yield quanta now to let it happen
	Sleep(10);

	// Close COM port
	#if TRACE_PORT
		_RPTF2(_CRT_WARN, "%.1f CSerial::Close port; hndl=0x%x\n", infcCoreTime(), m_hFile);
	#endif
#if TRACE_PORT
	BOOL closeErr =	::CloseHandle(m_hFile);
	if (!closeErr) {
		closeErr = ::GetLastError();
	}
	_RPTF2(_CRT_WARN, "%.1f CSerial::Close port; err=%d\n", infcCoreTime(), closeErr);
#else
	::CloseHandle(m_hFile);
	m_hFile = 0;
#endif
	// 9/20/11 DS+TD Mystery sleep required.
	// If less than 100 will cause re-open after this
	// to fail with access errors (GetLastError()==5)
	::Sleep(200);

	// Return successful
	return m_lLastError;
}
//																			 *
//****************************************************************************

CSerial::SERAPI_ERR CSerial::Setup (
		nodeulong eBaudrate, 
		EDataBits eDataBits, 
		EParity eParity, 
		EStopBits eStopBits,
		EDTR eDTRBit,
		ERTS eRTSBit)
{
	// Reset error state
	m_lLastError = API_ERROR_SUCCESS;

	//_RPT1(_CRT_WARN, "CSerial::Setup: baud=%ld\n", eBaudrate);
	// Check if the device is open
	if (m_hFile == 0)
	{
		// Set the internal error code
		m_lLastError = API_ERROR_INVALID_HANDLE;

		// Issue an error and quit
		_RPTF0(_CRT_WARN,"CSerial::Setup - Device is not opened\n");
		return m_lLastError;
	}

	// Obtain the DCB structure for the device
	CDCB dcb;
	if (!::GetCommState(m_hFile,&dcb))
	{
		// Obtain the error code
		m_lLastError = translateOSerrToSerErr(::GetLastError());
		// Display a warning
		_RPTF0(_CRT_WARN,"CSerial::Setup - Unable to obtain DCB information\n");
		return m_lLastError;
	}

	// Set the new data
	dcb.BaudRate = DWORD(eBaudrate);
	dcb.ByteSize = BYTE(eDataBits);
	dcb.Parity   = BYTE(eParity);
	dcb.StopBits = BYTE(eStopBits);

	// Determine if parity is used
	dcb.fParity  = (eParity != EParNone);
	dcb.fDtrControl=eDTRBit;
	dcb.fRtsControl=eRTSBit;

	// Set the new DCB structure
	if (!::SetCommState(m_hFile,&dcb))
	{
		// Obtain the error code
		m_lLastError = translateOSerrToSerErr(::GetLastError());

		// Display a warning
		_RPTF1(_CRT_WARN,"CSerial::Setup - Unable to set DCB information, err=%d\n", m_lLastError);
		return m_lLastError;
	}
	// Return successful
	return m_lLastError;
}
//																			 *
//****************************************************************************


CSerial::SERAPI_ERR CSerial::SetEventMask (DWORD dwEventMask)
{
	// Reset error state
	m_lLastError = API_ERROR_SUCCESS;

	// Check if the device is open
	if (m_hFile == 0)
	{
		// Set the internal error code
		m_lLastError = API_ERROR_INVALID_HANDLE;

		// Issue an error and quit
		_RPTF0(_CRT_WARN,"CSerial::SetMask - Device is not opened\n");
		return m_lLastError;
	}

	// Set the new mask. Note that this will generate an EEventNone
	// if there is an asynchronous WaitCommEvent pending.
	if (!::SetCommMask(m_hFile,dwEventMask))
	{
		// Obtain the error code
		m_lLastError = CSerial::SERAPI_ERR(::GetLastError());

		// Display a warning
		_RPTF0(_CRT_WARN,"CSerial::SetMask - Unable to set event mask\n");
		return m_lLastError;
	}

	// Save event mask and return successful
	m_dwEventMask = dwEventMask;
	return m_lLastError;
}
//																			 *
//****************************************************************************


//*****************************************************************************
//	NAME																	  *
//		CSerial::CommEventWaitInitiate
//
//	DESCRIPTION:
///		The WaitEvent method initiates a wait operation for one of the events 
///		that are enabled via SetMask. The <evt> will be signaled when
///		one of these events has occurred.
///
/// 	\param xxx description
///		\return description
/// 
/// 	Detailed description.
//
//	SYNOPSIS:
CSerial::SERAPI_ERR CSerial::CommEventWaitInitiate()
{
	if (HasOverlappedIoCompleted(&m_waitCommOverlapper)) {
		// Setup our own overlapped structure
		#if !(defined(_WIN32)||defined(_WIN64))
		memset(&m_waitCommOverlapper,0,sizeof(m_waitCommOverlapper));
		m_waitCommOverlapper.hEvent = m_commEvent.GetHandle();
		#endif
		// Assume not signalled until event occurs
		//m_commEvent.ResetEvent();		SM/DS 9/12/17 Remove???
		if (!::WaitCommEvent(m_hFile, LPDWORD(&m_eEvent), &m_waitCommOverlapper)) {
			// Set the internal error code
			DWORD lLastError = ::GetLastError();
			if (lLastError != ERROR_IO_PENDING) {
				// Save the error
				m_lLastError = CSerial::SERAPI_ERR(lLastError);

				// Issue an error and quit
#ifdef _DEBUG
				if (m_hFile)
					_RPTF0(_CRT_WARN,"CSerial::WaitEvent - Unable to wait for COM event\n");
#endif
				return m_lLastError;
			}
		}
		else {
			// We have events now.
			DWORD nBytes;
			if(::GetOverlappedResult(m_hFile, &m_waitCommOverlapper, &nBytes, TRUE)) {
				if (nBytes != sizeof(m_eEvent)) {
					_RPTF0(_CRT_WARN,"CSerial::WaitEvent - returned wrong size!\n");
				}
				else
					_RPTF0(_CRT_WARN,"CSerial::WaitEvent - Get Overlapped!\n");
			}
		}
	}
	// We are OK, let a thread wait for notification
	return(API_ERROR_SUCCESS);
}
//																			  *
//*****************************************************************************


CSerial::EEvent CSerial::GetEventType (void)
{
//#ifdef _DEBUG
//	// Check if the event is within the mask
//	if ((m_eEvent & m_dwEventMask) == 0)
//		_RPTF2(_CRT_WARN,"CSerial::GetEventType - Event %08Xh not within mask %08Xh.\n", m_eEvent, m_dwEventMask);
//#endif

	// Obtain the event (mask unwanted events out)
//	EEvent eEvent = EEvent(m_eEvent & m_dwEventMask);
	EEvent eEvent = m_eEvent;

	// Reset internal event type
	m_eEvent = EEventNone;

	// Return the current cause
	return eEvent;
}
//																			 *
//****************************************************************************


LONG CSerial::SetupReadTimeouts (EReadTimeout eReadTimeout)
{
	// Reset error state
	m_lLastError = API_ERROR_SUCCESS;

	// Check if the device is open
	if (m_hFile == 0)
	{
		// Set the internal error code
		m_lLastError = API_ERROR_INVALID_HANDLE;

		// Issue an error and quit
		_RPTF0(_CRT_WARN,"CSerial::SetupReadTimeouts - Device is not opened\n");
		return m_lLastError;
	}

	// Determine the time-outs
	COMMTIMEOUTS cto;
	if (!::GetCommTimeouts(m_hFile,&cto))
	{
		// Obtain the error code
		m_lLastError = CSerial::SERAPI_ERR(::GetLastError());

		// Display a warning
		_RPTF0(_CRT_WARN,"CSerial::SetupReadTimeouts - Unable to obtain timeout information\n");
		return m_lLastError;
	}

	// Set the new timeouts
	switch (eReadTimeout)
	{
	case EReadTimeoutBlocking:
		cto.ReadIntervalTimeout = 0;
		cto.ReadTotalTimeoutConstant = 0;
		cto.ReadTotalTimeoutMultiplier = 0;
		break;
	case EReadTimeoutNonblocking:
		cto.ReadIntervalTimeout = MAXDWORD;
		cto.ReadTotalTimeoutConstant = 0;
		cto.ReadTotalTimeoutMultiplier = 0;
		break;
	default:
		// This shouldn't be possible
		_ASSERTE(false);
		m_lLastError = API_ERROR_INVALID_ARG;
		return m_lLastError;
	}

	// Set the new DCB structure
	if (!::SetCommTimeouts(m_hFile,&cto))
	{
		// Obtain the error code
		m_lLastError = CSerial::SERAPI_ERR(::GetLastError());

		// Display a warning
		_RPTF0(_CRT_WARN,"CSerial::SetupReadTimeouts - Unable to set timeout information\n");
		return m_lLastError;
	}

	// Return successful
	return m_lLastError;
}
//																			 *
//****************************************************************************

nodeulong CSerial::GetBaudrate (void)
{
	// Reset error state
	m_lLastError = API_ERROR_SUCCESS;

	// Check if the device is open
	if (m_hFile == 0)
	{
		// Set the internal error code
		m_lLastError = API_ERROR_INVALID_HANDLE;

		// Issue an error and quit
		_RPTF0(_CRT_WARN,"CSerial::GetBaudrate - Device is not opened\n");
		return EBaudUnknown;
	}

	// Obtain the DCB structure for the device
	CDCB dcb;
	if (!::GetCommState(m_hFile,&dcb))
	{
		// Obtain the error code
		m_lLastError = CSerial::SERAPI_ERR(::GetLastError());

		// Display a warning
		_RPTF0(_CRT_WARN,"CSerial::GetBaudrate - Unable to obtain DCB information\n");
		return EBaudUnknown;
	}

	// Return the appropriate baudrate
	return (nodeulong(dcb.BaudRate));
}
//																			 *
//****************************************************************************


bool CSerial::GetDTR(void)
{
	return m_DTRBit == EDTRSet;
}

void CSerial::SetDTR(bool EDTRBit)
{
	if(EDTRBit) {
		m_DTRBit = EDTRSet;
		EscapeCommFunction(m_hFile,SETDTR);
	}
	else {
		m_DTRBit = EDTRClear;
		EscapeCommFunction(m_hFile,CLRDTR);
	}
}

bool CSerial::GetRTS(void)
{
	return m_RTSBit == ERTSSet;
}

void CSerial::SetRTS(bool ERTSBit)
{
	if(ERTSBit) {
		m_RTSBit = ERTSSet;
		EscapeCommFunction(m_hFile,SETRTS);
	}
	else {
		m_RTSBit = ERTSClear;
		EscapeCommFunction(m_hFile,CLRRTS);
	}
}
//																			 *
//****************************************************************************

DWORD CSerial::GetEventMask (void)
{
	// Reset error state
	m_lLastError = API_ERROR_SUCCESS;

	// Check if the device is open
	if (m_hFile == 0)
	{
		// Set the internal error code
		m_lLastError = API_ERROR_INVALID_HANDLE;

		// Issue an error and quit
		_RPTF0(_CRT_WARN,"CSerial::GetEventMask - Device is not opened\n");
		return 0;
	}

	// Return the event mask
	return m_dwEventMask;
}
//																			 *
//****************************************************************************


//*****************************************************************************
//	NAME																	  *
//		CSerial Write  
//
//	DESCRIPTION:							
//		Write data to port.
//
//	SYNOPSIS:
CSerial::SERAPI_ERR CSerial::Write (const void* pData, size_t iLen, 
					 DWORD* pdwWritten, DWORD dwTimeout)
{
	// Overlapped operation should specify the pdwWritten variable
	_ASSERTE(pdwWritten);

	// Reset error state
	m_lLastWrError = API_ERROR_SUCCESS;

	// Use our own variable for read count
	DWORD dwWritten;
	if (pdwWritten == 0) {
		pdwWritten = &dwWritten;
	}

	// Reset the number of bytes written
	*pdwWritten = 0;

	// Check if the device is open
	if (m_hFile == 0) {
		// Set the internal error code
		m_lLastWrError = API_ERROR_INVALID_HANDLE;

		// Issue an error and quit
		_RPTF0(_CRT_WARN,"CSerial::Write - Device is not opened\n");
		return m_lLastError;
	}

	// Wait for the event to happen
	OVERLAPPED ovInternal;
	CCEvent overLapEvt;
	// Setup our own overlapped structure
	memset(&ovInternal, 0, sizeof(ovInternal));
	ovInternal.hEvent = overLapEvt.GetHandle();

	// Make sure the overlapped structure isn't busy and we have good internal 
	// pointers
	_ASSERTE(pdwWritten);
	// Write the data to the port
	// Win64: limited writes to 2^31 OK
	if (!::WriteFile(m_hFile, pData, (DWORD)iLen, pdwWritten, &ovInternal)) {
		// Set the internal error code
		DWORD lLastError = ::GetLastError();

		// Overlapped operation in progress is not an actual error
		if (lLastError != ERROR_IO_PENDING) {
			// Save the error
			m_lLastWrError = CSerial::SERAPI_ERR(lLastError);

			// Issue an error and quit
			_RPTF0(_CRT_WARN,"CSerial::Write - Unable to write the data\n");
			return m_lLastWrError;
		}

		// Wait for write to complete
		if (overLapEvt.WaitFor(dwTimeout)) {
			// The overlapped operation has completed
			if (!::GetOverlappedResult(m_hFile,&ovInternal,pdwWritten,TRUE)) {
				// Set the internal error code
				m_lLastWrError = CSerial::SERAPI_ERR(::GetLastError());

				_RPTF0(_CRT_WARN,"CSerial::Write - Overlapped completed without result\n");
				return m_lLastWrError;
			}
		}
	}
	else {
		// The operation completed immediatly. Just to be sure
		// we'll set the overlapped structure's event handle.
		overLapEvt.SetEvent();
	}


	// Return successfully
	return m_lLastWrError;
}

CSerial::SERAPI_ERR CSerial::Write (LPCSTR pString, DWORD* pdwWritten, DWORD dwTimeout)
{
	// Determine the length of the string
	return Write(pString,strlen(pString),pdwWritten,dwTimeout);
}
//																			 *
//****************************************************************************


//*****************************************************************************
//	NAME																	  *
//		CSerialEx::GetCharsAvailable
//
//	DESCRIPTION:
///		Return the count of characters available for reading.
///
/// 	\param xxx description
///		\return description
/// 
/// 	Detailed description.
//
//	SYNOPSIS:
LONG CSerialEx::GetCharsAvailable(void)
{
	//return m_rdBuffer.charsInBuf();
	COMSTAT portProps;
	DWORD errors;
	if (::ClearCommError(m_hFile, &errors, &portProps)) {
		return(portProps.cbInQue);
	}
	return(0);
}

//																			  *
//*****************************************************************************


//*****************************************************************************
//	NAME																	  *
//		CSerial::Read
//
//	DESCRIPTION:
///		Wait for data a return it.
///
/// 	\param xxx description
///		\return 0 if SUCCESS, else an error code.
/// 
/// 	Detailed description.
//
//	SYNOPSIS:
CSerial::SERAPI_ERR CSerial::Read(void* pData, size_t iLen, DWORD* pdwRead, DWORD dwTimeout)
{
	// Overlapped operation should specify the pdwRead variable
	_ASSERTE(pdwRead || pdwRead);

	// Reset error state
	m_lLastRdError = API_ERROR_SUCCESS;

	// Reset the number of bytes read
	*pdwRead = 0;
	bool waitingOnRead = false;

	// Check if the device is open
	if (m_hFile == 0) {
		// Set the internal error code
		m_lLastRdError = API_ERROR_INVALID_HANDLE;

		// Issue an error and quit
		_RPTF0(_CRT_WARN, "CSerial::Read - Device is not opened\n");
		return m_lLastRdError;
	}

	ReadLock.Lock();
	// (TD/DS 1/20/2012) Commented the assert out - the assert condition is not good but probably will
	// not cause problems 
	//_ASSERTE(HasOverlappedIoCompleted(&m_rdOverlapper));
	// Make sure the overlapped structure isn't busy
	OVERLAPPED rdOverlapper = { 0 };
	rdOverlapper.hEvent = m_rdEvent.GetHandle();
	// SM 8/31/2016 Reset is automatically performed by ReadFile
	//m_rdEvent.ResetEvent();
	// Read the data / overlap
	if (!::ReadFile(m_hFile, pData, (DWORD)iLen, pdwRead, &rdOverlapper)) {
		// Set the internal error code
		DWORD lLastError = ::GetLastError();
		// Overlapped operation in progress is not an actual error
		// (voodoo-sometime lLastError was "no error"!)
		if (lLastError != ERROR_IO_PENDING) {
			// Save the error
			m_lLastRdError = CSerial::SERAPI_ERR(lLastError);
			// Issue an error and quit
			_RPTF1(_CRT_WARN, "CSerial::Read - Unable to read the data err=%d\n",
				lLastError);
#if 0
			WCHAR foo[100];	   // TO_KILL
			wsprintf(foo, _T("rd: failed, err=0x%x\n"), m_lLastRdError);
			OutputDebugStr(foo);
#endif
			//waitingOnRead = false;
			ReadLock.Unlock();
			return m_lLastRdError;
		}
		else {
			waitingOnRead = true;
		}
	}

	if (waitingOnRead) {
		// If the handle was opened with non-blocking reads this should never happen
		// Wait as long as <dwTimeout> for data to arrive
		if (m_rdEvent.WaitFor(dwTimeout)) {
			//lLastError = ::GetOverlappedResult(m_hFile, &m_rdOverlapper, pdwRead, FALSE);
			DWORD lLastError = ::GetOverlappedResult(m_hFile, &rdOverlapper, pdwRead, FALSE);
			if (!lLastError) {
				lLastError = ::GetLastError();
				#if TRACE_LOW
					_RPT2(_CRT_WARN, "%.1f CSerial::Read - GetOverlappedResult err=%d\n", 
									  infcCoreTime(), lLastError);
				#endif
#if 0
				{
					WCHAR foo[100];	   // TO_KILL
					wsprintf(foo, _T("rd: failed overlap, err=0x%x\n"), lLastError);
					OutputDebugStr(foo);
				}
#endif
				ReadLock.Unlock();
				return(CSerial::SERAPI_ERR(lLastError));
			}
			waitingOnRead = false;
			ReadLock.Unlock();
			return(API_ERROR_SUCCESS);
		}
		#if TRACE_LOW
		_RPT1(_CRT_WARN, "%.1f CSerial::Read - time-out\n", infcCoreTime());
		#endif
		ReadLock.Unlock();
		return(API_ERROR_SUCCESS);
		//m_lLastRdError = API_ERROR_TIMEOUT;
	}
	else {
		#if TRACE_LOW
		_RPT2(_CRT_WARN, "%.1f CSerial::Read - completed: pdwRead=%d\n", 
								infcCoreTime(), *pdwRead);
		#endif
		// Completed already, say OK
		//_RPTF0(_CRT_WARN, "CSerial::Read - completed?\n");
		ReadLock.Unlock();
		return(API_ERROR_SUCCESS);
	}	
		
	// Failed, cancel the I/O operation
	CancelCommIo();
	ReadLock.Unlock();
	// Return successfully
	return m_lLastRdError;
}
//																			 *
//****************************************************************************

CSerial::SERAPI_ERR CSerial::Purge()
{
	// Reset error state
	m_lLastError = API_ERROR_SUCCESS;

	// Check if the device is open
	if (m_hFile == 0) {
		// Set the internal error code
		m_lLastError = API_ERROR_INVALID_HANDLE;

		// Issue an error and quit
		_RPTF0(_CRT_WARN,"CSerial::Purge - Device is not opened\n");
		return m_lLastError;
	}
	ReadLock.Lock();
	// Clear any break that maybe in progress
	if (!::ClearCommBreak(m_hFile)) {
		m_lLastError = CSerial::SERAPI_ERR(::GetLastError());
		_RPTF1(_CRT_WARN,"CSerial::Purge - ClearCpmmBreak failed result %d\n", m_lLastError);
	}
	// Kill pending I/O
	if (!::PurgeComm(m_hFile, PURGE_RXCLEAR|PURGE_RXABORT|PURGE_TXABORT|PURGE_TXCLEAR)) {
		// Set the internal error code
		m_lLastError = CSerial::SERAPI_ERR(::GetLastError());
		_RPTF1(_CRT_WARN,"CSerial::Purge - PurgeComm failed result %d\n", m_lLastError);
	}
	DWORD theErrors;
	COMSTAT stats;
	DWORD cErr;

	cErr = ::ClearCommError(m_hFile, &theErrors, &stats);
	if (!cErr) {
		m_lLastError = CSerial::SERAPI_ERR(::GetLastError());
		_RPTF1(_CRT_WARN,"CSerial::Purge - ClearCommError failed result %d\n", m_lLastError);
#if _DEBUG
		if (theErrors) {
			_RPT1(_CRT_WARN, "CSerial::Purge Cleared Errors=0x%x\n", theErrors);
		}
#endif
	}
	ReadLock.Unlock();
	// Not sure here, clear any OS pending items 12/14/10 DS
	char junk[4000]; DWORD nRead;
#if _DEBUG
	int rErr = Read(junk, sizeof(junk), &nRead);
	if (rErr) 
		_RPT1(_CRT_WARN, "CSerial::Purge post Read returned err=%d\n", ::GetLastError());
#else
	Read(junk, sizeof(junk), &nRead);
#endif
	if (nRead) {
		_RPT0(_CRT_WARN, "cleared junk from read\n");
	}
	// Return successfully
	return m_lLastError;
}
//																			 *
//****************************************************************************

CSerial::SERAPI_ERR CSerial::Break (DWORD breakDurationMs)
{
	// Reset error state
	m_lLastError = API_ERROR_SUCCESS;

	// Check if the device is open
	if (m_hFile == 0)
	{
		// Set the internal error code
		m_lLastError = API_ERROR_INVALID_HANDLE;

		// Issue an error and quit
		_RPTF0(_CRT_WARN,"CSerial::Break - Device is not opened\n");
		return m_lLastError;
	}

    // Set the RS-232 port in break mode for a little while
	if(!::SetCommBreak(m_hFile)) {
		m_lLastError = API_ERROR_PORT_UNAVAILABLE;
		return(m_lLastError);
	}
    ::Sleep(breakDurationMs);
	if(!::ClearCommBreak(m_hFile)) {
		m_lLastError = API_ERROR_PORT_UNAVAILABLE;
		return(m_lLastError);
	}
	// Return successfully
	return m_lLastError;
}
//																			 *
//****************************************************************************


CSerial::EError CSerial::GetError (void)
{
	// Reset error state
	m_lLastError = API_ERROR_SUCCESS;

	// Check if the device is open
	if (m_hFile == 0)
	{
		// Set the internal error code
		m_lLastError = API_ERROR_INVALID_HANDLE;

		// Issue an error and quit
		_RPTF0(_CRT_WARN,"CSerial::GetError - Device is not opened\n");
		return EErrorUnknown;
	}

	// Obtain COM status
	DWORD dwErrors = 0;
	if (!::ClearCommError(m_hFile,&dwErrors,0))
	{
		// Set the internal error code
		m_lLastError = CSerial::SERAPI_ERR(::GetLastError());

		// Issue an error and quit
		_RPTF0(_CRT_WARN,"CSerial::GetError - Unable to obtain COM status\n");
		return EErrorUnknown;
	}

	// Return the error
	return EError(dwErrors);
}
//																			 *
//****************************************************************************

bool CSerial::GetCTS (void)
{
	// Reset error state
	m_lLastError = API_ERROR_SUCCESS;

	// Obtain the modem status
	DWORD dwModemStat = 0;
	if (!::GetCommModemStatus(m_hFile,&dwModemStat))
	{
		// Obtain the error code
		m_lLastError = (SERAPI_ERR)::GetLastError();

		// Display a warning
		_RPTF0(_CRT_WARN,"CSerial::GetCTS - Unable to obtain the modem status\n");
		return false;
	}

	// Determine if CTS is on
	return (dwModemStat & MS_CTS_ON) != 0;
}
//																			 *
//****************************************************************************

bool CSerial::GetDSR (void)
{
	// Reset error state
	m_lLastError = API_ERROR_SUCCESS;

	// Obtain the modem status
	DWORD dwModemStat = 0;
	if (!::GetCommModemStatus(m_hFile,&dwModemStat))
	{
		// Obtain the error code
		m_lLastError = (SERAPI_ERR)::GetLastError();

		// Display a warning
		_RPTF0(_CRT_WARN,"CSerial::GetDSR - Unable to obtain the modem status\n");
		return false;
	}

	// Determine if CTS is on
	return (dwModemStat & MS_DSR_ON) != 0;
}
//																			 *
//****************************************************************************

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//						FULL API IMPLEMENTATION							  //
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

#ifdef SERIAL_FULL_API

bool CSerial::GetDSR (void)
{
	// Reset error state
	m_lLastError = API_ERROR_SUCCESS;

	// Obtain the modem status
	DWORD dwModemStat = 0;
	if (!::GetCommModemStatus(m_hFile,&dwModemStat))
	{
		// Obtain the error code
		m_lLastError = ::GetLastError();

		// Display a warning
		_RPTF0(_CRT_WARN,"CSerial::GetDSR - Unable to obtain the modem status\n");
		return false;
	}

	// Determine if DSR is on
	return (dwModemStat & MS_DSR_ON) != 0;
}

bool CSerial::GetRing (void)
{
	// Reset error state
	m_lLastError = API_ERROR_SUCCESS;

	// Obtain the modem status
	DWORD dwModemStat = 0;
	if (!::GetCommModemStatus(m_hFile,&dwModemStat))
	{
		// Obtain the error code
		m_lLastError = ::GetLastError();

		// Display a warning
		_RPTF0(_CRT_WARN,"CSerial::GetRing - Unable to obtain the modem status");
		return false;
	}

	// Determine if Ring is on
	return (dwModemStat & MS_RING_ON) != 0;
}

LONG CSerial::SetEventChar (BYTE bEventChar, bool fAdjustMask)
{
	// Reset error state
	m_lLastError = API_ERROR_SUCCESS;

	// Check if the device is open
	if (m_hFile == 0)
	{
		// Set the internal error code
		m_lLastError = API_ERROR_INVALID_HANDLE;

		// Issue an error and quit
		_RPTF0(_CRT_WARN,"CSerial::SetEventChar - Device is not opened\n");
		return m_lLastError;
	}

	// Obtain the DCB structure for the device
	CDCB dcb;
	if (!::GetCommState(m_hFile,&dcb))
	{
		// Obtain the error code
		m_lLastError = ::GetLastError();

		// Display a warning
		_RPTF0(_CRT_WARN,"CSerial::SetEventChar - Unable to obtain DCB information\n");
		return m_lLastError;
	}

	// Set the new event character
	dcb.EvtChar = char(bEventChar);

	// Adjust the event mask, to make sure the event will be received
	if (fAdjustMask)
	{
		// Enable 'receive event character' event.  Note that this
		// will generate an EEventNone if there is an asynchronous
		// WaitCommEvent pending.
		SetMask(GetEventMask() | EEventRcvEv);
	}

	// Set the new DCB structure
	if (!::SetCommState(m_hFile,&dcb))
	{
		// Obtain the error code
		m_lLastError = ::GetLastError();

		// Display a warning
		_RPTF0(_CRT_WARN,"CSerial::SetEventChar - Unable to set DCB information\n");
		return m_lLastError;
	}

	// Return successful
	return m_lLastError;
}



LONG CSerial::SetupHandshaking (EHandshake eHandshake)
{
	// Reset error state
	m_lLastError = API_ERROR_SUCCESS;

	// Check if the device is open
	if (m_hFile == 0)
	{
		// Set the internal error code
		m_lLastError = API_ERROR_INVALID_HANDLE;

		// Issue an error and quit
		_RPTF0(_CRT_WARN,"CSerial::SetupHandshaking - Device is not opened\n");
		return m_lLastError;
	}

	// Obtain the DCB structure for the device
	CDCB dcb;
	if (!::GetCommState(m_hFile,&dcb))
	{
		// Obtain the error code
		m_lLastError = ::GetLastError();

		// Display a warning
		_RPTF0(_CRT_WARN,"CSerial::SetupHandshaking - Unable to obtain DCB information\n");
		return m_lLastError;
	}

	// Set the handshaking flags
	switch (eHandshake)
	{
	case EHandshakeOff:
		dcb.fOutxCtsFlow = false;					// Disable CTS monitoring
		dcb.fOutxDsrFlow = false;					// Disable DSR monitoring
		dcb.fDtrControl = DTR_CONTROL_DISABLE;		// Disable DTR monitoring
		dcb.fOutX = false;							// Disable XON/XOFF for transmission
		dcb.fInX = false;							// Disable XON/XOFF for receiving
		dcb.fRtsControl = RTS_CONTROL_DISABLE;		// Disable RTS (Ready To Send)
		break;

	case EHandshakeHardware:
		dcb.fOutxCtsFlow = true;					// Enable CTS monitoring
		dcb.fOutxDsrFlow = true;					// Enable DSR monitoring
		dcb.fDtrControl = DTR_CONTROL_HANDSHAKE;	// Enable DTR handshaking
		dcb.fOutX = false;							// Disable XON/XOFF for transmission
		dcb.fInX = false;							// Disable XON/XOFF for receiving
		dcb.fRtsControl = RTS_CONTROL_HANDSHAKE;	// Enable RTS handshaking
		break;

	case EHandshakeSoftware:
		dcb.fOutxCtsFlow = false;					// Disable CTS (Clear To Send)
		dcb.fOutxDsrFlow = false;					// Disable DSR (Data Set Ready)
		dcb.fDtrControl = DTR_CONTROL_DISABLE;		// Disable DTR (Data Terminal Ready)
		dcb.fOutX = true;							// Enable XON/XOFF for transmission
		dcb.fInX = true;							// Enable XON/XOFF for receiving
		dcb.fRtsControl = RTS_CONTROL_DISABLE;		// Disable RTS (Ready To Send)
		break;

	default:
		// This shouldn't be possible
		_ASSERTE(false);
		m_lLastError = E_INVALIDARG;
		return m_lLastError;
	}

	// Set the new DCB structure
	if (!::SetCommState(m_hFile,&dcb))
	{
		// Obtain the error code
		m_lLastError = ::GetLastError();

		// Display a warning
		_RPTF0(_CRT_WARN,"CSerial::SetupHandshaking - Unable to set DCB information\n");
		return m_lLastError;
	}

	// Return successful
	return m_lLastError;
}

CSerial::EDataBits CSerial::GetDataBits (void)
{
	// Reset error state
	m_lLastError = API_ERROR_SUCCESS;

	// Check if the device is open
	if (m_hFile == 0)
	{
		// Set the internal error code
		m_lLastError = API_ERROR_INVALID_HANDLE;

		// Issue an error and quit
		_RPTF0(_CRT_WARN,"CSerial::GetDataBits - Device is not opened\n");
		return EDataUnknown;
	}

	// Obtain the DCB structure for the device
	CDCB dcb;
	if (!::GetCommState(m_hFile,&dcb))
	{
		// Obtain the error code
		m_lLastError = ::GetLastError();

		// Display a warning
		_RPTF0(_CRT_WARN,"CSerial::GetDataBits - Unable to obtain DCB information\n");
		return EDataUnknown;
	}

	// Return the appropriate bytesize
	return EDataBits(dcb.ByteSize);
}

BYTE CSerial::GetEventChar (void)
{
	// Reset error state
	m_lLastError = API_ERROR_SUCCESS;

	// Check if the device is open
	if (m_hFile == 0)
	{
		// Set the internal error code
		m_lLastError = API_ERROR_INVALID_HANDLE;

		// Issue an error and quit
		_RPTF0(_CRT_WARN,"CSerial::GetEventChar - Device is not opened\n");
		return 0;
	}

	// Obtain the DCB structure for the device
	CDCB dcb;
	if (!::GetCommState(m_hFile,&dcb))
	{
		// Obtain the error code
		m_lLastError = ::GetLastError();

		// Display a warning
		_RPTF0(_CRT_WARN,"CSerial::GetEventChar - Unable to obtain DCB information\n");
		return 0;
	}

	// Set the new event character
	return BYTE(dcb.EvtChar);
}

CSerial::EParity CSerial::GetParity (void)
{
	// Reset error state
	m_lLastError = API_ERROR_SUCCESS;

	// Check if the device is open
	if (m_hFile == 0)
	{
		// Set the internal error code
		m_lLastError = API_ERROR_INVALID_HANDLE;

		// Issue an error and quit
		_RPTF0(_CRT_WARN,"CSerial::GetParity - Device is not opened\n");
		return EParUnknown;
	}

	// Obtain the DCB structure for the device
	CDCB dcb;
	if (!::GetCommState(m_hFile,&dcb))
	{
		// Obtain the error code
		m_lLastError = ::GetLastError();

		// Display a warning
		_RPTF0(_CRT_WARN,"CSerial::GetParity - Unable to obtain DCB information\n");
		return EParUnknown;
	}

	// Check if parity is used
	if (!dcb.fParity)
	{
		// No parity
		return EParNone;
	}

	// Return the appropriate parity setting
	return EParity(dcb.Parity);
}


bool CSerial::GetRLSD (void)
{
	// Reset error state
	m_lLastError = API_ERROR_SUCCESS;

	// Obtain the modem status
	DWORD dwModemStat = 0;
	if (!::GetCommModemStatus(m_hFile,&dwModemStat))
	{
		// Obtain the error code
		m_lLastError = ::GetLastError();

		// Display a warning
		_RPTF0(_CRT_WARN,"CSerial::GetRLSD - Unable to obtain the modem status");
		return false;
	}

	// Determine if RLSD is on
	return (dwModemStat & MS_RLSD_ON) != 0;
}


CSerial::EStopBits CSerial::GetStopBits (void)
{
	// Reset error state
	m_lLastError = API_ERROR_SUCCESS;

	// Check if the device is open
	if (m_hFile == 0)
	{
		// Set the internal error code
		m_lLastError = API_ERROR_INVALID_HANDLE;

		// Issue an error and quit
		_RPTF0(_CRT_WARN,"CSerial::GetStopBits - Device is not opened\n");
		return EStopUnknown;
	}

	// Obtain the DCB structure for the device
	CDCB dcb;
	if (!::GetCommState(m_hFile,&dcb))
	{
		// Obtain the error code
		m_lLastError = ::GetLastError();

		// Display a warning
		_RPTF0(_CRT_WARN,"CSerial::GetStopBits - Unable to obtain DCB information\n");
		return EStopUnknown;
	}

	// Return the appropriate stopbits
	return EStopBits(dcb.StopBits);
}

CSerial::EHandshake CSerial::GetHandshaking (void)
{
	// Reset error state
	m_lLastError = API_ERROR_SUCCESS;

	// Check if the device is open
	if (m_hFile == 0)
	{
		// Set the internal error code
		m_lLastError = API_ERROR_INVALID_HANDLE;

		// Issue an error and quit
		_RPTF0(_CRT_WARN,"CSerial::GetHandshaking - Device is not opened\n");
		return EHandshakeUnknown;
	}

	// Obtain the DCB structure for the device
	CDCB dcb;
	if (!::GetCommState(m_hFile,&dcb))
	{
		// Obtain the error code
		m_lLastError = ::GetLastError();

		// Display a warning
		_RPTF0(_CRT_WARN,"CSerial::GetHandshaking - Unable to obtain DCB information\n");
		return EHandshakeUnknown;
	}

	// Check if hardware handshaking is being used
	if ((dcb.fDtrControl == DTR_CONTROL_HANDSHAKE) && (dcb.fRtsControl == RTS_CONTROL_HANDSHAKE))
		return EHandshakeHardware;

	// Check if software handshaking is being used
	if (dcb.fOutX && dcb.fInX)
		return EHandshakeSoftware;

	// No handshaking is being used
	return EHandshakeOff;
}


#endif
