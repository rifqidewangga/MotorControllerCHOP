//*****************************************************************************
// NAME
//		lnkAccessAPIwin32.h
//
// DESCRIPTION:
/**
	\file
	\brief Link Access API definitions and helper functions for the Windows
	platform.

	The items in this file should not be exposed to the end user.
**/
//
// CREATION DATE:
//		01/15/2004 17:43:23	- as ioCorePrivate.h
//		06/11/2009 10:18:00 - renamed lnkAccessAPIwin32.h
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
#ifndef __LNKACCESSAPIWIN32_H__
#define	__LNKACCESSAPIWIN32_H__


//*****************************************************************************
// NAME																          *
// 	lnkAccessAPIwin32.h headers

//																			  *
//*****************************************************************************



//*****************************************************************************
// NAME																          *
// 	lnkAccessAPIwin32.h constants
//

//																			  *
//*****************************************************************************


	// ---------------------------------
	// ActiveX Interface
	// ---------------------------------
	#ifdef USE_ACTIVEX
	typedef struct _activeXInfo
		nodebool NetChangeMarshalled;		// Set to use Net Change marshall 
		long ChangeRecurse;					// Change recursion detector 
		// ActiveX marshalling info
		DWORD mnCookie;						// Marshalling cookie to the VB apartment
		DWORD mnThreadID;					// Thread ID of registering thread
	} activeXInfo
	#endif



//*****************************************************************************
// NAME																          *
// 	lnkAccessAPIwin32.h function prototypes
//
//
#ifdef __cplusplus
extern "C" {
#endif
// Return the resource string for <resID> using VB friendly BSTRs
MN_EXPORT nodebool MN_DECL infcStringGetRes(
		nodestr *dest, 
		nodeushort resID);

	
//																			  *
//*****************************************************************************

#ifdef __cplusplus
}
#endif
	#ifdef USE_ACTIVEX
	// ---------------------------------
	// LEGACY ACTIVEX INTERFACE. 
	// ---------------------------------
	// Change the marshalling of the network changes through the NetAdapter
	void MN_DECL infcMarshallNetChange(
				netaddr cNum,
				nodebool newState);

		#if __cplusplus 
			#ifndef _MNCLASS_DEFED_
				namespace mn1 {
					class _NetAdapterObj;
				};
			#endif
			// TekNetLib2 implementation
			void MN_DECL infcSetNetAdapterRef(netaddr cNum, mn1::_NetAdapterObj **theNet);
		#endif
	#endif

#endif
//============================================================================= 
//	END OF FILE lnkAccessAPIwin32.h
//=============================================================================
