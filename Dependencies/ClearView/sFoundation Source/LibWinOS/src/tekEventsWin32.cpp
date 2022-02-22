//*****************************************************************************
// $Archive: /ClearPath SC/User Driver/win/src/tekEventsWin32.cpp $
// $Date: 01/24/2017 11:17 $
// $Workfile: tekEventsWin32.cpp $
//
/// \file	
/// \brief C++ API: xx
//
// NAME
//		tekEventsWin32.cpp
//
// DESCRIPTION:
///		Windows implementations of operation system events, mutexes
///		and other common synchronization mechanisms.
//
// CREATION DATE:
//		02/08/2010 11:08:15
// 
// COPYRIGHT NOTICE:
//		(C)Copyright 2010-2018  Teknic, Inc.  All rights reserved.
//
//		This copyright notice must be reproduced in any copy, modification, 
//		or portion thereof merged into another program. A copy of the 
//		copyright notice must be included in the object library of a user
//		program.
//																			  *
//*****************************************************************************



//*****************************************************************************
// NAME																          *
// 	tekEventsWin32.cpp headers
//
	#if (defined(_WIN32)||defined(_WIN64))
		#include <windows.h>
		#include <winbase.h>
	#endif
	#include "tekTypes.h"
	#include "tekEventsWin32.h"
	#include <assert.h>
//																			  *
//*****************************************************************************




//*****************************************************************************
// NAME																	      *
// 	tekEventsWin32.cpp constants
//
//

//																			  *
//*****************************************************************************



//*****************************************************************************
//	NAME																	  *
//		class CCMTSyncObject implementation
//
//	DESCRIPTION:
///		
///
/// 	\param xxx description
///		\return description
/// 
/// 	Detailed description.
//
//	SYNOPSIS:
CCMTSyncObject::~CCMTSyncObject()
{
	if (hObject != NULL)
		::CloseHandle(hObject);
	hObject = NULL;
}


bool CCMTSyncObject::Lock(
	Uint32 dwTimeout)
{
	if (::WaitForSingleObject(hObject, dwTimeout) == WAIT_OBJECT_0)
		return TRUE;
	return FALSE;
}
//																			  *
//*****************************************************************************


//*****************************************************************************
//	NAME																	  *
//		class CCMTMultiLock	implementation
//
//	DESCRIPTION:
///		
///
/// 	\param xxx description
///		\return description
/// 
/// 	Detailed description.
//
//	SYNOPSIS:
Uint32 CCMTMultiLock::Lock(
	Uint32 dwTimeOut,
	bool bWaitForAll,
	Uint32 dwWakeMask)
{
	Uint32 dwResult;
	if (dwWakeMask == 0)
		dwResult = ::WaitForMultipleObjects(m_dwCount,
						m_pHandleArray, bWaitForAll, dwTimeOut);
	else
		dwResult = ::MsgWaitForMultipleObjects(m_dwCount,
						m_pHandleArray, bWaitForAll, dwTimeOut, dwWakeMask);

	if (dwResult >= WAIT_OBJECT_0 && dwResult < (WAIT_OBJECT_0 + m_dwCount)) {
		if (bWaitForAll){
			for (Uint32 i = 0; i < m_dwCount; i++)
				m_bLockedArray[i] = TRUE;
		}
		else {
			assert((dwResult - WAIT_OBJECT_0) >= 0);
			m_bLockedArray[dwResult - WAIT_OBJECT_0] = TRUE;
		}
	}
	return dwResult;
}
//																			  *
//*****************************************************************************



//*****************************************************************************
//	NAME																	  *
//		class CCSemaphore
//
//	DESCRIPTION:
///		Semaphore implementation.
///
/// 	\param xxx description
///		\return description
/// 
/// 	Detailed description.
//
//	SYNOPSIS:
CCSemaphore::CCSemaphore(
	          long lInitialCount,
			  long lMaxCount,
	          LPSECURITY_ATTRIBUTES lpsaAttributes)
		: CCMTSyncObject()
{
  assert(lMaxCount > 0);
  assert(lInitialCount <= lMaxCount);

  hObject = ::CreateSemaphoreA(lpsaAttributes, lInitialCount, 
							  lMaxCount, 
							  NULL);
  if (!hObject && (GetLastError() == ERROR_ALREADY_EXISTS))  
    hObject = ::OpenSemaphoreA(MUTEX_ALL_ACCESS,TRUE,
							  NULL);  
  assert(hObject != NULL);
}


bool CCSemaphore::Unlock(long lCount, long *lPrevCount)
{
  	return ::ReleaseSemaphore(hObject, lCount, lPrevCount)!=0;
}

//																			  *
//*****************************************************************************


//*****************************************************************************
//	NAME																	  *
//		class CCMutex Implementation
//
//	DESCRIPTION:
///		
///
/// 	\param xxx description
///		\return description
/// 
/// 	Detailed description.
//
//	SYNOPSIS:
CCMutex::CCMutex(
	      bool bInitiallyOwn, 
	      LPSECURITY_ATTRIBUTES lpsaAttribute)
		  :	CCMTSyncObject()
{
      	
	hObject = ::CreateMutexA(lpsaAttribute, bInitiallyOwn, 
							NULL);
	if (!hObject && (GetLastError() == ERROR_ALREADY_EXISTS))  
	  	hObject = ::OpenMutexA(MUTEX_ALL_ACCESS,TRUE,
	  						NULL);  
	assert(hObject != NULL);
}

bool CCMutex::Unlock()
{
  return ::ReleaseMutex(hObject)!=0;
}
//																			  *
//*****************************************************************************


//*****************************************************************************
//	NAME																	  *
//		class CCEvent
//
//	DESCRIPTION:
///		Thread
///
/// 	\param xxx description
///		\return description
/// 
/// 	Detailed description.
//
//	SYNOPSIS:
CCEvent::CCEvent(
	      bool bInitiallyOwn, 
		  bool bManualReset,
	      LPSECURITY_ATTRIBUTES lpsaAttribute)
		  :	CCMTSyncObject()
{
	hObject = ::CreateEventA(lpsaAttribute, bManualReset,
	                      bInitiallyOwn, 
	                      NULL);
	if (!hObject && (GetLastError() == ERROR_ALREADY_EXISTS))  
	  hObject = ::OpenEventA(MUTEX_ALL_ACCESS,TRUE,
						  NULL);
	assert(hObject != NULL);
}

// Signal and leave signalled our event      
bool CCEvent::SetEvent()
{ 
	return(::SetEvent(hObject)!=0);
}

// Un-signal and block waiters
bool CCEvent::ResetEvent()
{ 
	return(::ResetEvent(hObject)!=0);
}

// Cause the calling thread to block until "signalled" or TimeOut
// occurs. Returns TRUE if wait signalled normally.
bool CCEvent::WaitFor(unsigned TimeOut)
{
	return(WaitForSingleObject(hObject, TimeOut) == WAIT_OBJECT_0);
}
//																			  *
//*****************************************************************************




//============================================================================= 
//	END OF FILE tekEventsWin32.cpp
//=============================================================================
