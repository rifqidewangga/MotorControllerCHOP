//*****************************************************************************
// DESCRIPTION:
/**
	\file
	Thin layer to create WIN32 synchronization objects.

**/
//
// CREATION DATE:
//		05/14/2004 13:03:56
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
#ifndef __TEKEVENTS_H__
#define	__TEKEVENTS_H__



//*****************************************************************************
// NAME																          *
// 	tekEventsWin32.h headers
//
	#include <string.h>
	#include <assert.h>
	#include "tekTypes.h"
	// Include windows specific items
	#if (defined(_WIN32)||defined(_WIN64))
	#include <windows.h>
	#include <winbase.h>
	#endif
	
//																			  *
//*****************************************************************************





//*****************************************************************************
// NAME																          *
// 	tekEventsWin32.h constants
//
//

#ifndef NULL
#define NULL 0
#endif

// Define a the "infinite" timeout, this is currently set to Win32 
#if (defined(_WIN32)||defined(_WIN64))
#define SYNC_INFINITE  INFINITE
#else
#define SYNC_INFINITE  0xFFFFFFFF
#define LPSECURITY_ATTRIBUTES void *
typedef void *HANDLE;
#endif


// Forward references
class CCMTLock;
class CCMTMultiLock;
class CCMTSyncObject;
class CCSemaphore;
class CCMutex;
class CCEvent;
class CCCriticalSection;
//																			  *
//*****************************************************************************



//*****************************************************************************
//	NAME																	  *
//		class CCMTSyncObject
//
//	DESCRIPTION:
///		Multi-threaded synchronization object pure virtual base class.
///
/// 	Detailed description.
//
//	SYNOPSIS:
class CCMTSyncObject
{
friend class CCLock;
friend class CCMultiLock;
protected:
  	HANDLE hObject;
public:
	
	CCMTSyncObject() 
	{
		hObject = 0;           	
	}
	virtual ~CCMTSyncObject();
	
	bool isOK() {
		return(hObject!=0);
	}

	HANDLE GetHandle()
	{
		return hObject;
	}
		
	virtual  bool   Lock(Uint32 dwTimeout = SYNC_INFINITE);
	virtual  bool   Unlock() = 0;  
	virtual  bool   Unlock(int32 lCount, int32 *lpPrevCount=NULL)		
	{
		return Unlock();
	}

};
//																			  *
//*****************************************************************************


//---------------------------------------------------------------
class CCMTLock
{
public:
  	CCMTLock(CCMTSyncObject *pArgObj,
  			 Uint32 UnlockCount=1,
  			 Uint32 dwTimeout=SYNC_INFINITE)
	{
		pObj = pArgObj;
	  	assert(pObj);
	  	pObj->Lock(dwTimeout);
	}
 	~CCMTLock()
	{
	  int32 prevCnt;
	  pObj->Unlock(UnlockCount,&prevCnt);
	}
protected:
  	Uint32 UnlockCount;
  	CCMTSyncObject *pObj;
};
//																			  *
//*****************************************************************************


//*****************************************************************************
//	NAME																	  *
//		class CCSemaphore
//
//	DESCRIPTION:
///		The classic resource counting mutual exclusion device.  A request
///		to Lock is blocked via the scheduler until some other thread 
///		Unlocks the resource. Optionally multiple "counts" can be made 
///		available if required.
///
/// 	Detailed description.
//
//	SYNOPSIS:
class CCSemaphore : public CCMTSyncObject
{
public:
  	CCSemaphore(long lInitialCount = 1,
			  long lMaxCount = 1,
	          LPSECURITY_ATTRIBUTES lpsaAttributes = NULL);
	          
	// Release one item
  	virtual bool Unlock()
	{
	  	return Unlock(1, NULL);
	}
	
	// Release multiple items and return the count that was left.
  	virtual bool Unlock(long lCount, long *lPrevCount = NULL);
};
//																			  *
//*****************************************************************************


//*****************************************************************************
//	NAME																	  *
//		class CCMutex
//
//	DESCRIPTION:
///		A scheduler based critical section mutual exclusion object.
///
/// 	Detailed description.
//
//	SYNOPSIS:
class CCMutex : public CCMTSyncObject
{
public:
  	CCMutex(bool bInitiallyOwn = false, 
	      LPSECURITY_ATTRIBUTES lpsaAttribute = NULL);
	      
  	bool Unlock();
};
//																			  *
//*****************************************************************************


//*****************************************************************************
//	NAME																	  *
//		class CCEvent
//
//	DESCRIPTION:
///		A scheduler based blocking event. Multiple threads can wait for the
///		underlying event to "signal" via SetEvent to restart
///		their execution. There is an optional time-out for the wait.
///
/// 	Detailed description.
//
//	SYNOPSIS:
class CCEvent : public CCMTSyncObject
{
public:
//	unsigned context;					// Some arbitrary context
	
	CCEvent(bool bInitiallyOwn = false, 
		  bool bManualReset = true,
	      LPSECURITY_ATTRIBUTES lpsaAttribute = NULL); 
	      
	// Signal and leave signalled our event      
	bool SetEvent();
	
	// Un-signal and block waiters
	bool ResetEvent();
	
	// Cause the calling thread to block until "signalled" or TimeOut
	// occurs. Returns TRUE if wait was signalled normally.
	bool WaitFor(unsigned TimeOut=SYNC_INFINITE);
	
	bool Unlock()
	{
	  	return true;
	}
	HANDLE GetHandle()
	{
		return hObject;
	}
};
//																			  *
//*****************************************************************************


//*****************************************************************************
//	NAME																	  *
//		class CCCriticalSection
//
//	DESCRIPTION:
///		Create a critical section lockout.
///
/// 	\param xxx description
///		\return description
/// 
/// 	Detailed description.
//
//	SYNOPSIS:
#if (defined(_WIN32)||defined(_WIN64))
class CCCriticalSection : public CCMTSyncObject
{
public:
  	CCCriticalSection()
	{ 
	  ::InitializeCriticalSection(&m_sect); 
	}
	
  	~CCCriticalSection()
	{ 
	  ::DeleteCriticalSection(&m_sect); 
	}

	CRITICAL_SECTION *GetHandle()
	{
	  return &m_sect;
	}
	bool Unlock()
	{ 
	  	::LeaveCriticalSection(&m_sect); 
	  	return true; 
	}
	bool Lock(Uint32 dwTimeout=SYNC_INFINITE)
	{
	  	::EnterCriticalSection(&m_sect); 
	  	return true; 
	}
protected:
  CRITICAL_SECTION m_sect;
};
#else
#error "CCCriticalSection TODO"
#endif
//																			  *
//*****************************************************************************


//---------------------------------------------------------------
class CCMTMultiLock
{
protected:
	Uint32            m_dwUnlockCount;
	Uint32            m_dwCount;
	HANDLE           m_hPreallocated[8]; 
	bool             m_bPreallocated[8];
	CCEvent * const * 
	               m_ppObjectArray;
	HANDLE         * m_pHandleArray;    
	bool           * m_bLockedArray;

public:
	CCMTMultiLock(CCEvent *pObjects[], Uint32 dwCount,
		          Uint32 UnlockCount=1, bool bWaitForAll=true,
				  Uint32 dwTimeout=SYNC_INFINITE, Uint32 dwWakeMask=0)
	{
		m_dwUnlockCount = UnlockCount;			  	
		assert(dwCount > 0 && dwCount <= MAXIMUM_WAIT_OBJECTS);
		assert(pObjects != NULL);

		m_ppObjectArray = pObjects;
		m_dwCount = dwCount;

		// as an optimization, skip alloacating array if
		// we can use a small, predeallocated bunch of handles
		if (m_dwCount > (sizeof(m_hPreallocated)/sizeof(m_hPreallocated[0]))) {
			m_pHandleArray = new HANDLE[m_dwCount];
			m_bLockedArray = new bool[m_dwCount];
		}
		else {
			m_pHandleArray = m_hPreallocated;
			m_bLockedArray = m_bPreallocated;
		}

		// get list of handles from array of objects passed
		for (Uint32 i = 0; i <m_dwCount; i++) {
			assert(pObjects[i]);
			// can't wait for critical sections
			assert(pObjects[i]->GetHandle()!=0);

			m_pHandleArray[i] = pObjects[i]->GetHandle();
			m_bLockedArray[i] = false;
		}
		////Lock(dwTimeout,bWaitForAll,dwWakeMask);
	}

	~CCMTMultiLock()
	{
		for (Uint32 i=0; i < m_dwCount; i++)  
			if (m_bLockedArray[i])
		  		m_ppObjectArray[i]->SetEvent();

		if (m_pHandleArray != m_hPreallocated) {
			delete[] m_bLockedArray;
			delete[] m_pHandleArray;
		}
	}
public:
	Uint32 Lock(Uint32 dwTimeOut=SYNC_INFINITE,
		     bool bWaitForAll=true,Uint32 dwWakeMask=0);

};
//																			  *
//*****************************************************************************

//*****************************************************************************
//	NAME																	  *
//		class CCatomicUpdate
//
//	DESCRIPTION:
/**
	Atomic increment and decrement / test object.
**/
//	SYNOPSIS:
class CCatomicUpdate
{
private:
	LONG m_value;
public:
	CCatomicUpdate() {
		m_value = 0;
	}

	LONG Incr() {
		return InterlockedIncrement(&m_value);
	}

	LONG Decr() {
		return InterlockedDecrement(&m_value);
	}
};
//																			  *
//*****************************************************************************



#endif
//============================================================================= 
//	END OF FILE tekEventsWin32.h
//=============================================================================
