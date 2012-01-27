/*
 *  ClassID_c.h
 *  gnome
 *
 *  Created by Generic Programmer on 10/18/11.
 *  Copyright 2011 __MyCompanyName__. All rights reserved.
 *
 */

#ifndef __ClassID_c__
#define __ClassID_c__

#include "Earl.h"
#include "TypeDefs.h"
#include "ClassID_b.h"

class ClassID_c : virtual public ClassID_b {

public:
	virtual ClassID 	GetClassID 	() { return TYPE_UNDENTIFIED; }
	virtual Boolean		IAm(ClassID id) { return FALSE; }
	void				GetClassName (char* theName) { strcpy (theName, className); }	// sohail
	void				SetClassName (char* name);
	virtual void		Dispose 	() { return; }
	virtual Boolean		IsDirty  	() { return bDirty;  }
	virtual Boolean		IsOpen   	() { return bOpen;   }
	virtual Boolean		IsActive 	() { return bActive; }
	virtual void		SetDirty  (Boolean bNewDirty)  { bDirty  = bNewDirty; }
	virtual void		SetOpen   (Boolean bNewOpen)   { bOpen   = bNewOpen;  }
	virtual void		SetActive (Boolean bNewActive) { bActive = bNewActive;}
	
};


#endif
