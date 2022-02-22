//*****************************************************************************
// NAME	  
//		tekThreadsWin32.cpp
//
// DESCRIPTION:
/**
		\file
		COM aware threading virtual base class.
**/
//
// CREATION DATE:
//		03/10/2004 14:08:01
// 
// COPYRIGHT NOTICE:
//		(C)Copyright 2004-2018  Teknic, Inc.  All rights reserved.
//
//		This copyright notice must be reproduced in any copy, modification, 
//		or portion thereof merged into another program. A copy of the 
//		copyright notice must be included in the object library of a user
//		program.
//																			  *
//*****************************************************************************



//*****************************************************************************
// NAME	  																	  *
// 	Thread.cpp headers
//
	#include "tekTypes.h"
	#include <process.h>
	#include <set>
	#include <list>
	#include <crtdbg.h>

#if defined(_MSC_VER)
	#pragma warning(push, 4)
	#pragma warning(disable: 4100)
#endif
	#include "tekThreads.h"

	//using namespace sigslot;
//																			  *
//*****************************************************************************




//*****************************************************************************
// NAME	  																	  *
// 	Thread.cpp function prototypes
//
//
extern "C" double __stdcall infcCoreTime(void);

//																			  *
//*****************************************************************************



//*****************************************************************************
// NAME	  																	  *
// 	Thread.cpp constants
//
//
const DWORD DLLtimeOut = 1500;

//#define TERM_PRIO (THREAD_PRIORITY_LOWEST)
#define TERM_PRIO (THREAD_PRIORITY_HIGHEST)
// - - - - - - - - - - - - - - - - - - -
// DEBUG TRACING
// - - - - - - - - - - - - - - - - - - -
#ifdef _DEBUG
#define TRACE_PRINT 0
#else
#define TRACE_PRINT 0
#endif
//																			  *
//*****************************************************************************



//*****************************************************************************
// NAME	  																	  *
// 	Thread.cpp static variables
//
// 

//																			  *
//*****************************************************************************


//*****************************************************************************
// NAME																		  *
//		CThread::ThreadEntry
//
// DESCRIPTION
//		Wait efficiently
//
// SYNOPSIS
void CThread::Sleep(Uint32 milliseconds) 
{
	::Sleep(milliseconds);
}
//																			  *
//*****************************************************************************


//*****************************************************************************
// NAME																		  *
//		CThread::ThreadEntry
// Static thread entry point
unsigned __stdcall CThread::ThreadEntry(void* pArgs)
{
	int rCode;

	CThread *thrd = static_cast<CThread *>(pArgs);
	// Init COM
	if (thrd->m_isCOM) {
		CoInitialize(NULL);
	}
	// Setup random number generator
	srand(1);				// reset random # gen
	srand(thrd->m_seed);	// set the seed
	// Fire off the derived Run function
	rCode = thrd->Run(thrd->m_context);
	// We are done now
	
	// Boost priority to lock out others until we really terminate
	// Not sure if this helps or not
	// SetThreadPriority(thrd->m_hThread, TERM_PRIO);
	// 8/30/16 DS couple of crashes where this ended up NULL
	assert(thrd);

	// DeInit COM before we say done
	if (thrd->m_isCOM) {
		CoUninitialize();
	}
	// Signal completion event
	if (thrd->m_lclOwnTerm)
		*thrd->m_pTermFlag = true;
#ifdef THREAD_SLOTS
	// Generate signal
	thrd->Shutdown.emit();
#endif

	#if TRACE_PRINT
	_RPT2(_CRT_WARN,"%.1f Thread 0x%X signaled and is exiting\n", 
					 infcCoreTime(), thrd->m_idThreadSaved);
	#endif
	thrd->m_TermEvent.SetEvent();
	// WARNING: do not touch thrd after this - it can be freed
	return rCode;
}
//																			  *
//*****************************************************************************

//*****************************************************************************
// NAME	  																	  *
//		CThread::CThread
//
// DESCRIPTION:
//		Construction/Destruction
//
// SYNOPSIS:
CThread::CThread()
{
	constructInit(FALSE);
}

CThread::CThread(nodebool isCOM)
{
	constructInit(isCOM);
}

CThread::CThread(void *context, int priority, nodebool isCOM)
{
	constructInit(isCOM);
	LaunchThread(context, priority);
}

CThread::~CThread()
{
	if (m_lclOwnTerm)
		*m_pTermFlag = true;
	if (m_hThread)	{
		try {
			TerminateAndWait();
			CloseHandle( m_hThread );
			#if TRACE_PRINT
				_RPT1(_CRT_WARN, "CThread destructor handle closed thrd=0x%X\n",
						m_idThreadSaved );
			#endif
		}
		catch(...) {
			_RPT0(_CRT_WARN, "CThread destructor failed\n");
		}
	}
	if (m_lclOwnTerm)
		delete m_pTermFlag;

	m_hThread = NULL;
}
//																			  *
//*****************************************************************************


//*****************************************************************************
// NAME	  																	  *
//		CThread::constructInit
//
// DESCRIPTION:
//		
//
// SYNOPSIS:
void CThread::constructInit(nodebool isCom)
{
	m_isCOM = isCom;
	m_pTermFlag = new nodebool;
	m_lclOwnTerm = true;
	*m_pTermFlag = false;
	m_hThread = NULL;
	m_DLLterm = false;
	m_idThread = m_idThreadSaved = 0;
	m_running = false;
}
//																			  *
//*****************************************************************************


//*****************************************************************************
// NAME	  																	  *
//		CThread::LaunchThread
//
// DESCRIPTION:
//		Launch the thread at <priority>.
//
// RETURNS:
//		HANDLE
//
// SYNOPSIS:
HANDLE CThread::LaunchThread(void *context, int priority)
{
	m_context = context;
	// Forget last cancel
	*m_pTermFlag = false;
	m_exitSection.Lock();
	m_running = true;
	// Thread start here
	if (priority == 0)  {
		m_hThread = 
			(void*)_beginthreadex( NULL, 0, ThreadEntry, 
												  (void*)this, 
												  0, 
												  &m_idThread );
			m_idThreadSaved = m_idThread;
	}
	else {
		m_hThread = 
			(void*)_beginthreadex( NULL, 0, ThreadEntry, 
												  (void*)this, 
												  CREATE_SUSPENDED, 
												  &m_idThread );
		m_idThreadSaved = m_idThread;
		if( SetThreadPriority( m_hThread, priority ) )
		{
			ResumeThread(m_hThread);
			m_exitSection.Unlock();
			return m_hThread;
		}
		else
		{
			throw GetLastError();
			m_exitSection.Unlock();
			return NULL;
		}
	}
	m_exitSection.Unlock();
	return m_hThread;
}
//																			  *
//*****************************************************************************


//*****************************************************************************
// NAME	  																	  *
//		CThread::SetTerminateFlag
//
// DESCRIPTION:
//		Set the terminate flag to another source
//
// SYNOPSIS:
void CThread::SetTerminateFlag(nodebool *flag)
{
	// Return the existing flag if we own it now
	if (m_lclOwnTerm) {
		delete m_pTermFlag;
		m_lclOwnTerm = false;			// Delegate delete
	}
	m_pTermFlag = flag;
}
//																			  *
//*****************************************************************************



//*****************************************************************************
// NAME	  																	  *
//		CThread::Terminate
//
// DESCRIPTION:
//		Initiate the terminate sequence.
//
// RETURNS:
//		Handle to wait on for on exit.
//
// SYNOPSIS:
HANDLE CThread::Terminate(void)
{
	// Thread still OK?
	if (m_hThread) {
		if (m_pTermFlag && !(*m_pTermFlag)) {
			if (m_lclOwnTerm)
				*m_pTermFlag = true;
			#ifdef THREAD_SLOTS
				ShuttingDown.emit();
			#endif
			// Start if we have not exited our run function 
			if (m_running)
				ResumeThread( m_hThread );
		}
	}
	// Insure we break through the parking event
	m_ThreadParkedEvent.SetEvent();
	return(m_hThread);
}
//																			  *
//*****************************************************************************


//*****************************************************************************
// NAME	  																	  *
//		CThread::TerminateAndWait
//
// DESCRIPTION:
//		Initiate thread termination and wait for termination to complete.
//
// SYNOPSIS:
void CThread::TerminateAndWait(void)
{
	Terminate();
	WaitForTerm();
}
//																			  *
//*****************************************************************************


//*****************************************************************************
// NAME	  																	  *
//		CThread::WaitForTerm
//
// DESCRIPTION:
//		Wait for the thread to terminate.
//
// RETURNS:
//		nodebool: TRUE if normal termination
//
// SYNOPSIS:
nodebool CThread::WaitForTerm(void)
{
	nodebool exitState = FALSE;
	// Lock state from thread
	m_exitSection.Lock();
	// Thread already dead!
	if (!m_running) {
		#if TRACE_PRINT
			_RPT1(_CRT_WARN, "CThread::WaitForTerm already died 0x%X\n", 
								m_idThreadSaved);
		#endif
		exitState = TRUE;
	}
	// The thread is still "running", wait for it to signal the
	// end of its control function.
	else if (m_DLLterm) {
		// Using DLL termination strategy to avoid deadlock
		#if TRACE_PRINT
			_RPT1(_CRT_WARN, "CThread::WaitForTerm waiting DLL thread 0x%X\n", 
								m_idThreadSaved);
		#endif
		BOOL waitOK = m_TermEvent.WaitFor(DLLtimeOut);
		#if TRACE_PRINT
			_RPT2(_CRT_WARN, "CThread::WaitForTerm waiting DLL thread 0x%X, OK=%d\n", 
							m_idThreadSaved, waitOK);
		#endif
		exitState = (nodebool)waitOK;
		// We get here because we are dead or hung, make sure atomic state 
		// reflects this.
		m_idThread = 0;
		m_running = false;
	}
	else {
		// Using non-Windows DLL kludge, wait for OS to signal end of thread
		#if TRACE_PRINT
			_RPT1(_CRT_WARN, "CThread::WaitForTerm waiting for thread 0x%X\n",
							 m_idThreadSaved);
		#endif
		DWORD wRes = WaitForSingleObject(m_hThread, DLLtimeOut);
		// If balking, kill it
		if (wRes != WAIT_OBJECT_0) {
			TerminateThread(m_hThread, 99999);
			#if TRACE_PRINT
				_RPT1(_CRT_WARN, "CThread::WaitForTerm 0x%X exit (forced!)\n", 
								 m_idThreadSaved);
			#endif
		}
		exitState = (nodebool)(wRes == WAIT_OBJECT_0);
		// We get here because we are dead or hung, make sure atomic state 
		// reflects this.
		m_idThread = 0;
		m_running = false;
	}
	// Unlock state from thread
	m_exitSection.Unlock();
	return(exitState);
}
//																			  *
//*****************************************************************************


//*****************************************************************************
//	NAME																	  *
//		CThread::CurrentThreadID
//
//	DESCRIPTION:
///		Returns the operatings system dependent identifier for the currently
///		running thread.
///
/// 	\param xxx description
///		\return description
/// 
/// 	Detailed description.
//
//	SYNOPSIS:
nodeulong CThread::CurrentThreadID()
{
	return(nodeulong(GetCurrentThreadId()));
}
nodeulong CThread::UIthreadID()
{
	return(nodeulong(GetCurrentThreadId()));
}
//																			  *
//*****************************************************************************




#if defined(_MSC_VER)
#pragma warning(pop)
#endif

//============================================================================= 
//	END OF FILE Thread.cpp
//=============================================================================
