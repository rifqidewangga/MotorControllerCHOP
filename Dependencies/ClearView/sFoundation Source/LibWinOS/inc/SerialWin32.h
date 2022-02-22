//	Serial.h - Definition of the CSerial class
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


#include <tekEventsWin32.h>

#ifndef __SERIAL_H
#define __SERIAL_H


/////////////////////////////////////////////////////////////////////
// The SERIAL_DEFAULT_OVERLAPPED defines if the default open mode uses
// overlapped I/O. When overlapped I/O is available (normal Win32
// platforms) it uses overlapped I/O. Windows CE doesn't allow the use
// of overlapped I/O, so it is disabled there by default.

#ifndef SERIAL_DEFAULT_OVERLAPPED
#ifdef __cplusplus

#ifndef SERIAL_NO_OVERLAPPED
#define SERIAL_DEFAULT_OVERLAPPED	true
#else
#define SERIAL_DEFAULT_OVERLAPPED	false
#endif
#endif
// #define SERIAL_FULL_API				// Define for full API


//////////////////////////////////////////////////////////////////////
//
// CSerial - Win32 wrapper for serial communications
//
// Serial communication often causes a lot of problems. This class
// tries to supply an easy to use interface to deal with serial
// devices.
//
// The class is actually pretty ease to use. You only need to open
// the COM-port, where you need to specify the basic serial
// communication parameters. You can also choose to setup handshaking
// and read timeout behaviour.
//
// The following serial classes are available:
//
// CSerial      - Serial communication support.
// CSerialEx    - Serial communication with listener thread for events
// CSerialSync  - Serial communication with synchronized event handler
// CSerialWnd   - Asynchronous serial support, which uses the Win32
//                message queue for event notification.
// CSerialMFC   - Preferred class to use in MFC-based GUI windows.
// 
//
// Pros:
// -----
//	- Easy to use (hides a lot of nasty Win32 stuff)
//	- Fully ANSI and Unicode aware
//
// Cons:
// -----
//  - Little less flexibility then native Win32 API, however you can
//    use this API at the same time for features which are missing
//    from this class.
//  - Incompatible with Windows 95 or Windows NT v3.51 (or earlier),
//    because CancelIo isn't support on these platforms. Define the
//	  SERIAL_NO_CANCELIO macro for support of these platforms as
//	  well. When this macro is defined, then only time-out values of
//	  0 or INFINITE are valid.
//
//
// Copyright (C) 1999-2003 Ramon de Klein
//                         (Ramon.de.Klein@ict.nl)

class CSerial
{
// Class enumerations
public:
	// These are return codes from serial API. They generally map
	// into the host OS's error code list except for the items
	// at the end of the list.
	typedef enum
	{
		API_ERROR_SUCCESS = ERROR_SUCCESS,
		// Between here and	API_ERROR_TIMEOUT are
		// mapped to the Windows error codes.
		API_ERROR_TIMEOUT = 0x84010001,
		API_ERROR_PORT_SETUP,
		API_ERROR_INVALID_HANDLE,
		API_ERROR_INVALID_ARG,
		API_ERROR_PORT_UNAVAILABLE,
		API_ERROR_UNKNOWN
	}
	SERAPI_ERR;
	// Communication events (bit encoded)
	typedef enum
	{
		EEventUnknown  	   = -1,			// Unknown event
		EEventNone  	   = 0,				// Event trigged without cause
		EEventBreak 	   = EV_BREAK,		// A break was detected on input
		EEventCTS   	   = EV_CTS,		// The CTS signal changed state
		EEventDSR   	   = EV_DSR,		// The DSR signal changed state
		EEventError 	   = EV_ERR,		// A line-status error occurred
		EEventRing  	   = EV_RING,		// A ring indicator was detected
		EEventRLSD  	   = EV_RLSD,		// The RLSD signal changed state
		EEventRecv  	   = EV_RXCHAR,		// Data is received on input
		EEventRcvEv 	   = EV_RXFLAG,		// Event character was received on input
		EEventSend		   = EV_TXEMPTY,	// Last character on output was sent
		EEventPrinterError = EV_PERR,		// Printer error occured
		EEventRx80Full	   = EV_RX80FULL,	// Receive buffer is 80 percent full
		EEventProviderEvt1 = EV_EVENT1,		// Provider specific event 1
		EEventProviderEvt2 = EV_EVENT2,		// Provider specific event 2
	} 
	EEvent;

	// Data bits (5-8)
	typedef enum
	{
		EDataUnknown = -1,			// Unknown
		EData5       =  5,			// 5 bits per byte
		EData6       =  6,			// 6 bits per byte
		EData7       =  7,			// 7 bits per byte
		EData8       =  8			// 8 bits per byte (default)
	}
	EDataBits;

	// Parity scheme
	typedef enum
	{
		EParUnknown = -1,			// Unknown
		EParNone    = NOPARITY,		// No parity (default)
		EParOdd     = ODDPARITY,	// Odd parity
		EParEven    = EVENPARITY,	// Even parity
		EParMark    = MARKPARITY,	// Mark parity
		EParSpace   = SPACEPARITY	// Space parity
	}
	EParity;

	// Stop bits
	typedef enum
	{
		EStopUnknown = -1,			// Unknown
		EStop1       = ONESTOPBIT,	// 1 stopbit (default)
		EStop2       = TWOSTOPBITS	// 2 stopbits
	} 
	EStopBits;

	// return constant for unknown baud rate
	static const nodeulong EBaudUnknown = 0;

	// Handshaking
	typedef enum
	{
		EHandshakeUnknown		= -1,	// Unknown
		EHandshakeOff			=  0,	// No handshaking
		EHandshakeHardware		=  1,	// Hardware handshaking (RTS/CTS)
		EHandshakeSoftware		=  2	// Software handshaking (XON/XOFF)
	} 
	EHandshake;

	// DTR
	typedef enum
	{
		EDTRClear		= 0,	// Clear - (Volt DTR = -5)
		EDTRSet			= true,	// Set - (Volt DTR = 5)
	} 
	EDTR;

	// RTS
	typedef enum
	{
		ERTSClear		= 0,	// Clear - (Volt RTS = -5)
		ERTSSet			= true,	// Set - (Volt RTS = 5)
	} 
	ERTS;

	// Timeout settings
	typedef enum
	{
		EReadTimeoutUnknown		= -1,	// Unknown
		EReadTimeoutNonblocking	=  0,	// Always return immediately
		EReadTimeoutBlocking	=  1	// Block until everything is retrieved
	}
	EReadTimeout;

	// Communication errors	(bit encoded)
	typedef enum
	{
		EErrorUnknown = 0,			// Unknown
		EErrorBreak   = CE_BREAK,	// Break condition detected
		EErrorFrame   = CE_FRAME,	// Framing error
		EErrorIOE     = CE_IOE,		// I/O device error
		EErrorMode    = CE_MODE,	// Unsupported mode
		EErrorOverrun = CE_OVERRUN,	// Character buffer overrun, next byte is lost
		EErrorRxOver  = CE_RXOVER,	// Input buffer overflow, byte lost
		EErrorParity  = CE_RXPARITY,// Input parity error
		EErrorTxFull  = CE_TXFULL	// Output buffer full
	}
	EError;

	// Port availability
	typedef enum
	{
		EPortUnknownError = -1,		// Unknown state
		EPortAvailable    =  0,		// Port is available
		EPortNotAvailable =  1,		// Port is not present
		EPortInUse        =  2		// Port is in use
	} 
	EPort;

// Construction
public:
	CSerial();
	virtual ~CSerial();

// Operations
public:
	// Check if particular COM-port is available (static method).
	static EPort CheckPort (LPCTSTR lpszDevice);

	// Open the serial communications for a particular COM port. You
	// need to use the full devicename (i.e. "COM1") to open the port.
	// It's possible to specify the size of the input/output queues.
	virtual SERAPI_ERR Open (LPCTSTR lpszDevice, DWORD dwInQueue = 0, DWORD dwOutQueue = 0, bool fOverlapped = SERIAL_DEFAULT_OVERLAPPED);
	virtual SERAPI_ERR Open (DWORD portNumber, DWORD dwInQueue = 0, DWORD dwOutQueue = 0, bool fOverlapped = SERIAL_DEFAULT_OVERLAPPED);

	// Close the serial port.
	virtual SERAPI_ERR Close (void);

	// Setup the communication settings such as baudrate, databits,
	// parity and stopbits. The default settings are applied when the
	// device has been opened. Call this function if these settings do
	// not apply for your application. If you prefer to use integers
	// instead of the enumerated types then just cast the integer to
	// the required type. So the following two initializations are
	// equivalent:
	//
	//   Setup(9600, EDataBits(8),EParity(NOPARITY),EStopBits(ONESTOPBIT))
	//
	// In the latter case, the types are not validated. So make sure
	// that you specify the appropriate values.
	virtual SERAPI_ERR Setup (nodeulong = 9600,
						EDataBits eDataBits = EData8,
						EParity   eParity   = EParNone,
						EStopBits eStopBits = EStop1,
						EDTR eDTRBit = EDTRClear,
						ERTS eRTSBit = ERTSClear);

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// Communications Event Interface. Provides signals upon various 
	// normal serial communications events such as break condition detect.
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
protected:
	CCEvent m_commEvent;
public:
	// Set the event mask, which indicates what events should be
	// monitored. The WaitEvent method can only monitor events that
	// have been enabled. The default setting only monitors the
	// error events and data events. An application may choose to
	// monitor CTS. DSR, RLSD, etc as well.
	virtual SERAPI_ERR SetEventMask (DWORD dwMask = EEventBreak|EEventError|EEventRecv|EEventCTS);

	// The WaitEvent method waits for one of the events that are
	// enabled (see SetMask).
	virtual SERAPI_ERR CommEventWaitInitiate();
	// This is the communications notification event, must outlive the thread
	bool WaitForCommEvent(unsigned int timeOutMS) {
		return m_commEvent.WaitFor(timeOutMS);
	}
	void ForceCommEvent() {
		m_commEvent.SetEvent();
	}
	// Determine what caused the event to trigger
	EEvent GetEventType (void);
	

	// Obtain communication settings
	virtual nodeulong  GetBaudrate    (void);
	virtual DWORD      GetEventMask   (void);

	// Write data to the serial port. Note that we are only able to
	// send ANSI strings, because it probably doesn't make sense to
	// transmit Unicode strings to an application.
	virtual SERAPI_ERR Write (const void* pData, size_t iLen, DWORD* pdwWritten = 0, DWORD dwTimeout = INFINITE);
	virtual SERAPI_ERR Write (LPCSTR pString, DWORD* pdwWritten = 0, DWORD dwTimeout = INFINITE);

	// Read data from the serial port. 
	virtual SERAPI_ERR Read (void* pData, size_t iLen, DWORD* pdwRead = 0, DWORD dwTimeout = INFINITE);

	// Send a break
	virtual SERAPI_ERR Break (DWORD breakDurationMs);


	// Obtain the error
	EError GetError (void);

	// Obtain the COMM and event handle
	HANDLE GetCommHandle (void)		{ return m_hFile; }

	// Check if com-port is opened
	bool IsOpen (void) const		{ return (m_hFile != 0); }


	// Obtain CTS/DSR/RING/RLSD settings
	bool GetDTR (void);
	void SetDTR(bool EDTRBit);
	bool GetRTS (void);
	void SetRTS(bool ERTSBit);
	bool GetCTS (void);
	bool GetDSR (void);
	

	#if (defined(_WIN32)||defined(_WIN64))
	// These structures must exist after the read thread dies to provide context for
	// CancelIo
	OVERLAPPED m_waitCommOverlapper;
	
	OVERLAPPED m_rdOverlapper;
	CCEvent m_rdEvent;
	#endif
	
protected:
	// Internal helper class which wraps DCB structure
	class CDCB : public DCB
	{
	public:
		CDCB() { DCBlength = sizeof(DCB); }
	};

// Attributes
protected:
	SERAPI_ERR	m_lLastError;	// Last serial error
	SERAPI_ERR	m_lLastRdError;	// Last serial read error
	SERAPI_ERR	m_lLastWrError;	// Last serial write error
	CCCriticalSection ReadLock;				// Internal structure lock
public:
	// Obtain last error status
	SERAPI_ERR GetLastError (void) const	{ return m_lLastError; }
	// Obtain last error status
	SERAPI_ERR GetLastWrError (void) const	{ return m_lLastWrError; }
	// Obtain last error status
	SERAPI_ERR GetLastRdError (void) const	{ return m_lLastRdError; }

protected:
	HANDLE	m_hFile;			// File handle
	EEvent	m_eEvent;			// Event type
	DWORD	m_dwEventMask;		// Event mask
	EDTR	m_DTRBit;			// Dtr Bit
	ERTS	m_RTSBit;			// Rts Bit

protected:
	// Check the requirements
	void CheckRequirements (LPOVERLAPPED lpOverlapped, DWORD dwTimeout) const;

	// Purge all buffers
	SERAPI_ERR Purge (void);
	// CancelIo wrapper (for Win95 compatibility)
	SERAPI_ERR CancelCommIo (void);
	// Read operations can be blocking or non-blocking. You can use
	// this method to setup wether to use blocking or non-blocking
	// reads. Non-blocking reads is the default, which is required
	// for most applications.
	//
	// 1) Blocking reads, which will cause the 'Read' method to block
	//    until the requested number of bytes have been read. This is
	//    useful if you know how many data you will receive.
	// 2) Non-blocking reads, which will read as many bytes into your
	//    buffer and returns almost immediately. This is often the
	//    preferred setting.
	virtual LONG SetupReadTimeouts (EReadTimeout eReadTimeout);
};

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	//						FULL API IMPLEMENTATION							  //
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	#ifdef SERIAL_FULL_API
	virtual BYTE       GetEventChar   (void);
	// Setup the handshaking protocol. There are three forms of
	// handshaking:
	//
	// 1) No handshaking, so data is always send even if the receiver
	//    cannot handle the data anymore. This can lead to data loss,
	//    when the sender is able to transmit data faster then the
	//    receiver can handle.
	// 2) Hardware handshaking, where the RTS/CTS lines are used to
	//    indicate if data can be sent. This mode requires that both
	//    ports and the cable support hardware handshaking. Hardware
	//    handshaking is the most reliable and efficient form of
	//    handshaking available, but is hardware dependant.
	// 3) Software handshaking, where the XON/XOFF characters are used
	//    to throttle the data. A major drawback of this method is that
	//    these characters cannot be used for data anymore.
	virtual LONG SetupHandshaking (EHandshake eHandshake);
	// Set/clear the event character. When this byte is being received
	// on the serial port then the EEventRcvEv event is signalled,
	// when the mask has been set appropriately. If the fAdjustMask flag
	// has been set, then the event mask is automatically adjusted.
	virtual LONG SetEventChar (BYTE bEventChar, bool fAdjustMask = true);

	// Obtain communication settings
	virtual EDataBits  GetDataBits    (void);
	virtual EParity    GetParity      (void);
	virtual EStopBits  GetStopBits    (void);
	virtual EHandshake GetHandshaking (void);
	// Obtain CTS/DSR/RING/RLSD settings
	bool GetDSR (void);
	bool GetRing (void);
	bool GetRLSD (void);
	#endif
#endif	// Cplusplus

#endif	// __SERIAL_H

