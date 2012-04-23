#include "Cross.h"
#include "MapUtils.h"
#include "OUtils.h"
#include "EditWindsDialog.h"
#include "Uncertainty.h"

static TComponentMover	*sSharedDialogComponentMover = 0;
static TCATSMover		*sSharedOriginalPattern1 = 0;
static TCATSMover		*sSharedOriginalPattern2 = 0;

static CurrentUncertainyInfo sSharedComponentUncertainyInfo; // used to hold the uncertainty dialog box info in case of a user cancel

void DeleteIfNotOriginalPattern(TCATSMover * pattern)
{
	if (pattern == sSharedOriginalPattern1) return;
	if (pattern == sSharedOriginalPattern2) return;
	if (!pattern) return;
	
	pattern -> Dispose();
	delete pattern;
}

TComponentMover::TComponentMover (TMap *owner, char *name) : TCurrentMover (owner, name)
{
	pattern1 = nil;
	pattern2 = nil;
	bPat1Open = true;
	bPat2Open = true;
	timeFile = nil;
	
	memset(&refP,0,sizeof(refP));
	bRefPointOpen = false;
	pat1Angle = 0;
	pat2Angle = pat1Angle + 90;

	pat1Speed = 10;
	pat2Speed = 10;
	
	pat1SpeedUnits = kMetersPerSec;
	pat2SpeedUnits = kMetersPerSec;
	
	pat1ScaleToValue = .1;
	pat2ScaleToValue = .1;
	
	//scaleBy = WINDSTRESS;	// changed default 5/26/00
	scaleBy = NONE;	// default for dialog is WINDSTRESS, but for location files is WINDSPEED 5/29/00
					// both are set when they are first encountered
	memset(&fOptimize,0,sizeof(fOptimize));

	timeMoverCode = kLinkToNone;
	windMoverName [0] = 0;

	bUseAveragedWinds = false;
	bExtrapolateWinds = false;
	bUseMainDialogScaleFactor = false;
	fScaleFactorAveragedWinds = 1.;
	fPowerFactorAveragedWinds = 1.;
	fPastHoursToAverage = 24;
	fAveragedWindsHdl = 0;

	return;
}

void TComponentMover::Dispose ()
{
	if (pattern1) {
		pattern1 -> Dispose();
		delete pattern1;
		pattern1 = nil;
	}
	if (pattern2) {
		pattern2 -> Dispose();
		delete pattern2;
		pattern2 = nil;
	}
	if (timeFile)
	{
		timeFile -> Dispose ();
		delete timeFile;
		timeFile = nil;
	}
	if (fAveragedWindsHdl)
	{
		DisposeHandle((Handle)fAveragedWindsHdl);
		fAveragedWindsHdl = 0;
	}
}

OSErr TComponentMover::InitMover ()
{
	OSErr	err = noErr;
	TMap * owner = this -> GetMoverMap();
	err = TCurrentMover::InitMover ();
	if(owner) refP = WorldRectCenter(owner->GetMapBounds());
	return err;
}

OSErr TComponentMover::DeleteItem(ListItem item)
{
	if (item.index == I_COMPONENTNAME)
		return moverMap -> DropMover(this);
	
	return 0;
}


OSErr TComponentMover::AddUncertainty(long setIndex, long leIndex,VelocityRec *patVelocity,double timeStep)
{
	
	double u,v,lengthS,alpha,beta;
	LEUncertainRec unrec;
	
	OSErr err = 0;
	
	err = this -> UpdateUncertainty();
	if(err) return err;
	
	
	if(!fUncertaintyListH || !fLESetSizesH) 
		return 0; // this is our clue to not add uncertainty
	
	if(fUncertaintyListH && fLESetSizesH)
	{
		unrec=(*fUncertaintyListH)[(*fLESetSizesH)[setIndex]+leIndex];
		lengthS = sqrt(patVelocity->u*patVelocity->u + patVelocity->v * patVelocity->v);
	
		u = patVelocity->u;
		v = patVelocity->v;

		if(lengthS>1e-6) // so we don't divide by zero
		{	
			
			alpha = unrec.downStream;
			beta = unrec.crossStream;
		
			patVelocity->u = u*(1+alpha)+v*beta;
			patVelocity->v = v*(1+alpha)-u*beta;	
		}
	}
	else 
	{
		TechError("TComponentMover::AddUncertainty()", "fUncertaintyListH==nil", 0);
		patVelocity->u=patVelocity->v=0;
	}
	return err;
}


Boolean TComponentMover::FunctionEnabled(ListItem item, short buttonID)
{
	long i;
	switch (item.index) {
		case I_CATSNAME:
			switch (buttonID) {
				case ADDBUTTON: return FALSE;
				case DELETEBUTTON: return TRUE;
				case UPBUTTON:
				case DOWNBUTTON:
					if (!moverMap->moverList->IsItemInList((Ptr)&item.owner, &i)) return FALSE;
					switch (buttonID) {
						case UPBUTTON: return i > 0;
						case DOWNBUTTON: return i < (moverMap->moverList->GetItemCount() - 1);
					}
			}
			break;
	}
	
	if (buttonID == SETTINGSBUTTON) return TRUE;
	
//	return TCurrentMover::FunctionEnabled(item, buttonID);
	return FALSE;
}

OSErr TComponentMover::SettingsItem(ListItem item)
{
	switch (item.index) {
		default:
			return ComponentMoverSettingsDialog(this, this -> moverMap);
	}
	return 0;
}

Boolean TComponentMover::ListClick(ListItem item, Boolean inBullet, Boolean doubleClick)
{
	if (inBullet)
		switch (item.index) {
			case I_COMPONENTNAME: bOpen = !bOpen; return TRUE;
			case I_COMPONENTACTIVE:
			{
				bActive = !bActive;
				InvalMapDrawingRect();
				return TRUE;
			}
			
			case I_COMPONENT1NAME: bPat1Open = !bPat1Open; return TRUE;
			case I_COMPONENT2NAME: bPat2Open = !bPat2Open; return TRUE;
			case I_COMPONENTREFERENCE: bRefPointOpen = !bRefPointOpen; return TRUE;
			case I_COMPONENT1GRID: pattern1->bShowGrid = !pattern1->bShowGrid; 
				model->NewDirtNotification(DIRTY_MAPDRAWINGRECT); return TRUE;
			case I_COMPONENT2GRID: if (pattern2) pattern2->bShowGrid = !pattern2->bShowGrid; 
				model->NewDirtNotification(DIRTY_MAPDRAWINGRECT); return TRUE;
			case I_COMPONENT1ARROWS: pattern1->bShowArrows = !pattern1->bShowArrows; 
				model->NewDirtNotification(DIRTY_MAPDRAWINGRECT); return TRUE;
			case I_COMPONENT2ARROWS: if (pattern2) pattern2->bShowArrows = !pattern2->bShowArrows; 
				model->NewDirtNotification(DIRTY_MAPDRAWINGRECT); return TRUE;
		}

	if (doubleClick)
	{
		ComponentMoverSettingsDialog(this, this -> moverMap);
	}	

	return FALSE;
}

long TComponentMover::GetListLength()
{
	long	listLength = 0;
	
	listLength = 1;
	
	if (bOpen)
	{
		listLength += 1;		// add one for active box at top
		listLength +=1;			// add one for scale type
		listLength += 1;		// add one for reference position
		if (bRefPointOpen)
			listLength += 2;
		listLength += 2;		// pattern 1 & 2 names
		if (bPat1Open) listLength += 4;
		if (bPat2Open && pattern2) listLength += 4; // JLM 8/31/99 only show the items if pattern 2 is selected
	}
	
	return listLength;
}

ListItem TComponentMover::GetNthListItem (long n, short indent, short *style, char *text)
{
	ListItem item = { this, 0, indent, 0 };
	char *p, latS[20], longS[20], timeS[32],valStr[32],val2Str[32],unitStr[32];
	
	if (n == 0) {
		item.index = I_COMPONENTNAME;
		item.bullet = bOpen ? BULLET_OPENTRIANGLE : BULLET_CLOSEDTRIANGLE;
		sprintf(text, "Component");
		if(!bActive)*style = italic; // JLM 6/14/10
		
		return item;
	}
	
	item.indent++;
	
	if (bOpen)
	{

		if (--n == 0) {
			item.index = I_COMPONENTACTIVE;
			item.bullet = bActive ? BULLET_FILLEDBOX : BULLET_EMPTYBOX;
			strcpy(text, "Active");
			
			return item;
		}

		if (--n == 0) {
			item.indent--;
			item.index = I_COMPONENTSCALEBY;
			if (scaleBy==WINDSTRESS) strcpy(valStr,"Wind Stress");
			else if (scaleBy==WINDSPEED) strcpy(valStr,"Wind Speed");
			else strcpy(valStr,"error");
			sprintf(text, "Scale type: %s", valStr);

			return item;
		}

		if (--n == 0) {
			item.index = I_COMPONENTREFERENCE;
			item.bullet = bRefPointOpen ? BULLET_OPENTRIANGLE : BULLET_CLOSEDTRIANGLE;
			strcpy(text, "Reference Point");
			
			return item;
		}

		if (bRefPointOpen) {
			if (--n < 2) {
				item.indent++;
				item.index = (n == 0) ? I_COMPONENTLAT : I_COMPONENTLONG;
				//item.bullet = BULLET_DASH;
				WorldPointToStrings(refP, latS, longS);
				strcpy(text, (n == 0) ? latS : longS);
				
				return item;
			}
			
			--n;
		}
		
/////////////////////////// Start of first CATS mover items

		indent++;

		if (--n == 0) {
			item.index = I_COMPONENT1NAME;
			item.bullet = bPat1Open ? BULLET_OPENTRIANGLE : BULLET_CLOSEDTRIANGLE;
			sprintf(text, "Pattern1: %s",pattern1->className);
			
			return item;
		}

		if (bPat1Open)
		{
			item.indent++;

			if (--n == 0) {
				item.index = I_COMPONENT1GRID;
				item.bullet = pattern1 -> bShowGrid ? BULLET_FILLEDBOX : BULLET_EMPTYBOX;
				sprintf(text, "Show Grid");
				
				return item;
			}
			
			if (--n == 0) {
				item.index = I_COMPONENT1ARROWS;
				item.bullet = pattern1 -> bShowArrows ? BULLET_FILLEDBOX : BULLET_EMPTYBOX;
				StringWithoutTrailingZeros(valStr,pattern1->arrowScale,6);
				sprintf(text, "Show Velocities (@ 1 in = %s m/s)", valStr);
				
				return item;
			}
			
			if (--n == 0) {
				item.index = I_COMPONENT1DIRECTION;
				StringWithoutTrailingZeros(valStr,pat1Angle,1);
				sprintf(text, "Direction: %s degrees", valStr);
				
				return item;
			}

			if (--n == 0) {
				item.index = I_COMPONENT1SCALE;
				StringWithoutTrailingZeros(valStr, pat1Speed, 6);
				StringWithoutTrailingZeros(val2Str, pat1ScaleToValue, 6);
				ConvertToUnitsShort (pat1SpeedUnits, unitStr);
				sprintf(text, "For wind speed %s %s scale to %s m/s", valStr, unitStr, val2Str);

				return item;
			}

			item.indent--;
		}
		
/////////////////////////// Start of second CATS mover items

		if (--n == 0) {
			item.index = I_COMPONENT2NAME;
			item.bullet = bPat2Open ? BULLET_OPENTRIANGLE : BULLET_CLOSEDTRIANGLE; 
			if (pattern2) sprintf(text, "Pattern2: %s", pattern2->className);
			else sprintf(text, "Pattern2: %s", "Not Used");

			return item;
		}
		
		if (bPat2Open && pattern2) // JLM 8/31/99, in theory we won't get here if pattern2 is nil
		{
			item.indent++;

			if (--n == 0) {
				item.index = I_COMPONENT2GRID;
				item.bullet = pattern2 -> bShowGrid ? BULLET_FILLEDBOX : BULLET_EMPTYBOX;
				sprintf(text, "Show Grid");
				
				return item;
			}
			
			if (--n == 0) {
				item.index = I_COMPONENT2ARROWS;
				item.bullet = pattern2 -> bShowArrows ? BULLET_FILLEDBOX : BULLET_EMPTYBOX;
				StringWithoutTrailingZeros(valStr,pattern2->arrowScale, 6);
				sprintf(text, "Show Velocities (@ 1 in = %s m/s)", valStr);
			}

			if (--n == 0) {
				item.index = I_COMPONENT2DIRECTION;
				StringWithoutTrailingZeros(valStr,pat2Angle,1);
				sprintf(text, "Direction: %s degrees", valStr);

				return item;
			}

			if (--n == 0) {
				item.index = I_COMPONENT2SCALE;
				StringWithoutTrailingZeros(valStr, pat2Speed, 6);
				StringWithoutTrailingZeros(val2Str, pat2ScaleToValue, 6);
				ConvertToUnitsShort (pat2SpeedUnits, unitStr);
				sprintf(text, "For wind speed %s %s Scale to: %s m/s", valStr, unitStr, val2Str);

				return item;
			}
		}
	}

	return item;
}

void TComponentMover::Draw(Rect r, WorldRect view)
{
	if (pattern1) pattern1 -> Draw(r,view);
	if (pattern2) pattern2 -> Draw(r,view);
}

void TComponentMover::ModelStepIsDone()
{
	memset(&fOptimize,0,sizeof(fOptimize));
}

OSErr TComponentMover::PrepareForModelStep()
{
	char errmsg[256];
	OSErr err = TCurrentMover::PrepareForModelStep(); // note: this calls UpdateUncertainty()

	errmsg[0]=0;

	err = SetOptimizeVariables (errmsg);

	// code goes here, jump to done?
	//if (err) goto done;

	this -> fOptimize.isOptimizedForStep = true;
	this -> fOptimize.isFirstStep = (model->GetModelTime() == model->GetStartTime());

	// code goes here, I think this is redundant
	if (this -> fOptimize.isFirstStep)
	{	
		if (bUseAveragedWinds)
		{
			if (fAveragedWindsHdl)
			{	// should dispose at end of run??
				DisposeHandle((Handle)fAveragedWindsHdl);
				fAveragedWindsHdl = 0;
			}
			err = CalculateAveragedWindsHdl(errmsg);
			//if (err) printError("There is a problem with the averaged winds. Please check your inputs.");
			if (err) {if (!errmsg[0]) strcpy(errmsg,"There is a problem with the averaged winds. Please check your inputs.");}
		}
	}
done:
	if (err)
	{
		if(!errmsg[0])
			strcpy(errmsg,"An error occurred in TComponentMover::PrepareForModelStep");
		printError(errmsg); 
		//printError("An error occurred in TComponentMover::PrepareForModelStep");
	}
	return err;
}

OSErr TComponentMover::CalculateAveragedWindsHdl(char *errmsg)
{
	OSErr err = 0;
	long i, j, numTimeSteps = (model -> GetEndTime () - model -> GetStartTime ()) / model -> GetTimeStep() + 1;
	VelocityRec value, avValue;
	TMover 		*mover;
	VelocityRec wVel = {0.,0.};
	Boolean		bFound = false;
	double pat1Theta = PI * -(0.5 + (pat1Angle / 180.0));
	double pat2Theta = PI * -(0.5 + (pat2Angle / 180.0));
	VelocityRec pat1ValRef = pattern1 -> GetPatValue (refP);
	double pat1ValRefLength = sqrt (pat1ValRef.u * pat1ValRef.u + pat1ValRef.v * pat1ValRef.v);

	strcpy(errmsg,"");
	
	// calculate handle size - number of time steps, end time - start time / time step + 1
	// for each time step, start at 24 hrs (or whatever) before and average wind at each step up to current
	// if no values that far back give an error
	// put time and value in the handle
	// if error delete

	// code goes here, might want to extend handle if model run time is changed, or recreate?
	// then should delete handle in case it still exists...
	if (fAveragedWindsHdl)
	{	// should dispose at end of run??
		DisposeHandle((Handle)fAveragedWindsHdl);
		fAveragedWindsHdl = 0;
	}
	fAveragedWindsHdl = (TimeValuePairH)_NewHandleClear(sizeof(TimeValuePair)*numTimeSteps);
	if (!fAveragedWindsHdl)
		{ TechError("TComponentMover::CalculateAveragedWindsHdl()", "_NewHandleClear()", 0); return -1; }
		
	// get the wind mover, if it's constant or nonexistent this is an error, should be checked in the dialog												  
	if (timeMoverCode == kLinkToWindMover)
	{
		long 	/*	i, j,*/ m, n;
	//	double 		length, theirLengthSq, myLengthSq, dotProduct;
	//	VelocityRec theirVelocity,myVelocity;
	//VelocityRec wVel = {0.,0.};
		TMap		*map;
	//	TMover 		*mover;
	//	Boolean		bFound = false;

		for (j = 0, m = model -> mapList -> GetItemCount() ; j < m && !bFound ; j++) 
		{
			model -> mapList -> GetListItem((Ptr)&map, j);
			
			for (i = 0, n = map -> moverList -> GetItemCount() ; i < n ; i++) 
			{
				map -> moverList -> GetListItem((Ptr)&mover, i);
				if (mover -> GetClassID() != TYPE_WINDMOVER) continue;
				if (!strcmp(mover -> className, windMoverName)) 
				{
					// JLM, note: we are implicitly matching by file name above
				//	((TWindMover*) mover) -> GetTimeValue (model -> modelTime, &wVel);
					bFound = true;
					break;
				}
			}
			
			if (!bFound)
			{
				map = model -> uMap;
				for (i = 0, n = map -> moverList -> GetItemCount() ; i < n ; i++) 
				{
					map -> moverList -> GetListItem((Ptr)&mover, i);
					if (mover -> GetClassID() != TYPE_WINDMOVER) continue;
					if (!strcmp(mover -> className, windMoverName)) 
					{
						// JLM, note: we are implicitly matching by file name above
						//((TWindMover*) mover) -> GetTimeValue (model -> modelTime, &wVel);
						bFound = true;
						break;
					}
				}
			}
		}
		
		if (!bFound)
		{
			strcpy(errmsg,"There is no wind time file associated with the component mover");
			//printError("There is no wind time file associated with the component mover");
			return -1;
			//print error, also check if it's a constant wind or times out of range
		}
		// alert code goes here if mover is not found
	}
	else
	{
			strcpy(errmsg,"There is no wind time file associated with the component mover");
		//	printError("There is no wind time file associated with the component mover");
			return -1;
	}

	// check wind time file exists for whole length of run, including the past averaging hours
	for (i = 0 ; i < numTimeSteps ; i++) 
	{
		long averageTimeSteps;
		double averageSpeed=0.,averageDir = 0;
		Seconds timeToGetAverageFor = model -> GetStartTime () + i * model -> GetTimeStep();
		Seconds startPastTime  = timeToGetAverageFor - fPastHoursToAverage * 3600;
		INDEXH(fAveragedWindsHdl, i).time = model -> GetStartTime () + i * model -> GetTimeStep();
		//averageTimeSteps = fPastHoursToAverage+1; // for now, will change to match model time steps...
		averageTimeSteps = fPastHoursToAverage; // for now, will change to match model time steps...
		// code goes here, may want to change to GetStartTime, GetEndTime, then check out of range
		if (i==0) 	err = dynamic_cast<TWindMover *>(mover)->CheckStartTime(startPastTime); //if (forTime < INDEXH(timeValues, 0).time) 
		if (err==-1) 
		{
			if (bExtrapolateWinds)
			{	// GetTimeValue() already extrapolates
				err = 0;
			}
			else
				{strcpy(errmsg,"There is not enough data in your wind file for the averaging"); goto done;}
			//printError("There is not enough data in your wind file for the averaging"); goto done;
		}
		if (err==-2) 
		{
			//strcpy(errmsg,"No point in averaging for constant wind."); goto done;
			fPastHoursToAverage=0; err=0;	// allow constant wind, only need one value though 
			//printError("No point in averaging for constant wind."); goto done;
		}
		//if (forTime > INDEXH(timeValues, n - 1).time) 

		if (fPastHoursToAverage==0) averageTimeSteps = 1;	// just use the straight wind
		for (j=0;j<averageTimeSteps;j++)
		{
			Seconds timeToAddToAverage = startPastTime + j*3600; // eventually this will be time step...
			double		windSpeedToScale, windDir,theta;
		// get the time file / wind mover value for this time
			// get the mover first then repeat using it for the times..., but make sure get time value gets a value...
			// check first value - 24, last value else will just use first/last value 
			// also check if it's not a time file...
			// make sure in the GetMove to GetTimeValue from the averaged handle
			// check here that time is in the handle...
			dynamic_cast<TWindMover *>(mover)-> GetTimeValue (timeToAddToAverage, &wVel);
			//windSpeedToScale = sqrt(wVel.u*wVel.u + wVel.v*wVel.v);
			// code goes here, take the component first, then average ?
			windSpeedToScale = wVel.u * cos (pat1Theta) + wVel.v * sin (pat1Theta);
			//averageSpeed += (windSpeedToScale) * fScaleFactorAveragedWinds / pat1ValRefLength; //windSpeedToScale; //?? need the dot product too
			//averageSpeed += (windSpeedToScale); //windSpeedToScale; //?? need the dot product too - seems this was done twice?
			//windDir = fmod(atan2(wVel.u,wVel.v)*180/PI+360,360); // floating point remainder
			windDir = atan2(wVel.u,wVel.v); // 
			//windDir = fmod(-180,360); // not sure what above does...
			//theta = fmod(theta+180,360); // rotate the vector cause wind is FROM this direction
			//r=sqrt(u*u+v*v);
		//	windDir = 0;
			averageSpeed = averageSpeed + windSpeedToScale; // need to divide by averageTimeSteps
			averageDir = averageDir + windDir;
			//averageDir = averageDir + windSpeedToScale; // need to divide by averageTimeSteps
			// if add up wind dir make sure it's -180 to 180 - not necessary
		}
		averageSpeed = averageSpeed / averageTimeSteps;
		// apply power and scale - is this the right order?
		if (averageSpeed<0) averageSpeed = -1. * pow(abs(averageSpeed),fPowerFactorAveragedWinds);
		else
		/*if (fPowerFactorAveragedWinds!=1.)*/  averageSpeed = pow(averageSpeed,fPowerFactorAveragedWinds); 
		//for now apply the scale factor in SetOptimizeVariables()
		//averageSpeed = averageSpeed*fScaleFactorAveragedWinds; 
		// code goes here bUseMainDialogScaleFactor = true do this way leave out fSFAW, = false just use fSFAW
		averageDir = averageDir / averageTimeSteps;
		//avValue.u = averageSpeed*sin(averageDir/*+PI*/);	// not sure about the pi
		//avValue.v = averageSpeed*cos(averageDir/*+PI*/);
		avValue.u = averageSpeed;	// not sure about the pi
		avValue.v = 0;	// not sure about the pi
		INDEXH(fAveragedWindsHdl, i).value = avValue;// translate back to u,v

	}
done:
	if (err)
	{
		if (fAveragedWindsHdl)
		{	// should dispose at end of run??
			DisposeHandle((Handle)fAveragedWindsHdl);
			fAveragedWindsHdl = 0;
		}
	}
	return err;
}
///////////////////////////////////////////////////////////////////////////
OSErr TComponentMover::CheckAndPassOnMessage(TModelMessage *message)
{	// JLM
	char ourName[kMaxNameLen];
	long lVal;
	OSErr err = 0;
	
	// see if the message is of concern to us
	this->GetClassName(ourName);
	
	if(message->IsMessage(M_CREATEMOVER,ourName)) 
	{
		char moverName[kMaxNameLen];
		char typeName[64];
		char path[256];
		TCATSMover *mover = nil;
		Boolean unrecognizedType = false;
		Boolean needToInitMover = true;
		message->GetParameterString("NAME",moverName,kMaxNameLen);
		message->GetParameterString("TYPE",typeName,64);
		message->GetParameterString("PATH",path,256);
		ResolvePath(path);

		err = message->GetParameterAsLong("PATTERN",&lVal);
		if(!err) 
		{ 
			if(!strcmpnocase(typeName,"Cats")) 
			{	// the cats mover needs a file and so is a special case
				mover = CreateAndInitCatsCurrentsMover (this->moverMap,false,path,moverName);
			}
			////////////// 
			if(mover) 
			{
				mover -> refP = this -> refP;	// need to set mover's refP so it is drawn correctly, 3/28/00
				switch(lVal)
				{
					case 1: this -> pattern1 = mover; break;
					case 2: this -> pattern2 = mover; break;
					default: printError("pattern value out of range in TComponentMover"); break;
				}
			}
		}
		model->NewDirtNotification();// tell model about dirt
	}
	else if(message->IsMessage(M_SETFIELD,ourName))
	{
		double val;
		char str[256];
		OSErr err = 0;

		////////////////
		err = message->GetParameterAsDouble("pat1Angle",&val);
		if(!err) this->pat1Angle = val; 
		err = message->GetParameterAsDouble("pat2Angle",&val);
		if(!err) { this->pat2Angle = val;} 
		////////////////
		err = message->GetParameterAsDouble("pat1Speed",&val);
		if(!err) this->pat1Speed = val; 
		err = message->GetParameterAsDouble("pat2Speed",&val);
		if(!err) { this->pat2Speed = val;} 
		////////////////
		err = message->GetParameterAsDouble("pat1ScaleToValue",&val);
		if(!err) { this->pat1ScaleToValue = val;} 
		err = message->GetParameterAsDouble("pat2ScaleToValue",&val);
		if(!err) { this->pat2ScaleToValue = val;} 
		/////////////
		message->GetParameterString("pat1SpeedUnits",str,256);
		if(str[0]) { this->pat1SpeedUnits = StrToSpeedUnits(str);} 
		message->GetParameterString("pat2SpeedUnits",str,256);
		if(str[0]) { this->pat2SpeedUnits = StrToSpeedUnits(str);} 
		/////////////
		message->GetParameterString("scaleBy",str,256); 
		if(str[0]) 
		{	
			if(!strcmpnocase(str,"windspeed")) this->scaleBy = WINDSPEED; 
			if(!strcmpnocase(str,"windstress")) this->scaleBy = WINDSTRESS; 
		}
		if (this -> scaleBy == NONE)
			this -> scaleBy = WINDSPEED;	// default for old location files with no scaleBy message

		/////////////
		/////////////
		message->GetParameterString("refP",str,256);
		if(str[0]) 
		{
			WorldPoint p;
			char dirLat = 0, dirLong = 0;
			double degreesLat =0, degreesLong = 0;
			long numscanned = sscanf(str,lfFix("%lf %c %lf %c"),&degreesLong,&dirLong,&degreesLat,&dirLat);
			if(dirLong == 'e') dirLong = 'E';
			else if(dirLong == 'w') dirLong = 'W';
			if(dirLat == 'n') dirLat = 'N';
			else if(dirLat == 'S') dirLat = 'S';

			if(numscanned == 4 
				&& (dirLong == 'E' || dirLong == 'W') 
				&& (dirLat == 'N' || dirLat == 'S') 
				)
			{
				DoublesToWorldPoint(degreesLat, degreesLong, dirLat, dirLong, &p);
				this->refP = p;
				if (this -> pattern2)	this -> pattern2 -> refP = p;	// need to set mover's refP so it is drawn correctly, 3/28/00
				if (this -> pattern1)	this -> pattern1 -> refP = p;	// need to set mover's refP so it is drawn correctly, 3/28/00
			}
		}
		////////////////
		message->GetParameterString("scaleType",str,256);
		if(!strcmpnocase(str,"WINDMOVER")) 
		{
			TWindMover *wMover = model->GetWindMover(false); // look for mover but do not create one
			this -> timeMoverCode = kLinkToWindMover;
			if (wMover) strcpy(this -> windMoverName, wMover -> className);
			else windMoverName[0] = 0;
		}
		////////////////
			
		model->NewDirtNotification();// tell model about dirt
	}
	
	/////////////////////////////////////////////////
	// sub-guys need us to pass this message 
	/////////////////////////////////////////////////
	if(this->timeFile) err = this->timeFile->CheckAndPassOnMessage(message);
	if(this->pattern1) err = this->pattern1->CheckAndPassOnMessage(message);
	if(this->pattern2) err = this->pattern2->CheckAndPassOnMessage(message);

	/////////////////////////////////////////////////
	//  pass on this message to our base class
	/////////////////////////////////////////////////
	return TCurrentMover::CheckAndPassOnMessage(message);
}

OSErr TComponentMover::GetAveragedWindValue(Seconds time, VelocityRec *avValue)
{
	long index, numValuesInHdl;
	VelocityRec avWindValue = {0.,0.};
	Seconds avTime;

	*avValue = avWindValue;

	index = (long)((time - model->GetStartTime())/model->GetTimeStep());
	numValuesInHdl = _GetHandleSize((Handle)fAveragedWindsHdl)/sizeof(**fAveragedWindsHdl);
	if (index<0 || index >= numValuesInHdl) {return -1;}	// may want to recalculate
	avTime = INDEXH(fAveragedWindsHdl, index).time;
	if (avTime != time) return -1;
	*avValue = INDEXH(fAveragedWindsHdl, index).value;// translate back to u,v
	return noErr;
}

OSErr TComponentMover::SetOptimizeVariables (char *errmsg)
{
	VelocityRec	vVel, hVel, wVel;
	OSErr		err = noErr;
	Boolean 	useEddyUncertainty = false;	
	VelocityRec	ref1Wind, ref2Wind,pat1ValRef, pat2ValRef;
	double		dot1,dot2;
	double		pat1SpeedMPS,pat2SpeedMPS,pat1Theta,pat2Theta;
	double		pat1ValRefLength,pat2ValRefLength;
	double		windSpeedToScale;

	strcpy(errmsg,"");
	
	wVel.u = wVel.v = 0;

	// get the time file / wind mover value for this time

	if (timeMoverCode == kLinkToWindMover)
	{
		long 		i, j, m, n;
		double 		length, theirLengthSq, myLengthSq, dotProduct;
		VelocityRec theirVelocity,myVelocity;
		TMap		*map;
		TMover 		*mover;
		Boolean		bFound = false;

		if (bUseAveragedWinds)
		{
			// routine to get the time value from the averaged winds handle
			if (fAveragedWindsHdl)
			{	// 
				//get averaged value from the handle, figure out the time step index
			//	long index = (long) ((model -> GetModelTime() - model->GetStartTime()) / model->GetTimeStep())
			//	wvel = INDEXH(fAveragedWindsHdl,index); // check index is in range
				err = GetAveragedWindValue(model->GetModelTime(), &wVel);
				if (err) 
				{
					err = CalculateAveragedWindsHdl(errmsg);
					if (err) 
					{
						if (!errmsg[0]) {strcpy(errmsg,"There is a problem with the averaged winds. Please check your inputs.");} return -1;
						//printError("There is a problem with the averaged winds. Please check your inputs."); return -1;
					}
					else 
					{
						err = GetAveragedWindValue(model->GetModelTime(), &wVel);
						if (err) 
						{
							if (!errmsg[0]) {strcpy(errmsg,"There is a problem with the averaged winds. Please check your inputs.");} return -1;
							//printError("There is a problem with the averaged winds. Please check your inputs."); return -1;
						}
					}
				}
			}
			else
			{
				// try to calculate
			//	printError("No averaged winds handle"); // hmm, can't return an error...
				err = CalculateAveragedWindsHdl(errmsg);
				if (err) 
				{
					//printError("There is a problem with the averaged winds. Please check your inputs."); return -1;
					if (!errmsg[0]) {strcpy(errmsg,"There is a problem with the averaged winds. Please check your inputs.");} return -1;
				}
				else 
				{
					err = GetAveragedWindValue(model->GetModelTime(), &wVel);
					if (err)
					{ 
						if (!errmsg[0]) {strcpy(errmsg,"There is a problem with the averaged winds. Please check your inputs.");} return -1;
						//printError("There is a problem with the averaged winds. Please check your inputs."); return -1;
					}
				}
			}
			//return;
		}
		else
		{
			for (j = 0, m = model -> mapList -> GetItemCount() ; j < m && !bFound ; j++) {
				model -> mapList -> GetListItem((Ptr)&map, j);
				
				for (i = 0, n = map -> moverList -> GetItemCount() ; i < n ; i++) {
					map -> moverList -> GetListItem((Ptr)&mover, i);
					if (mover -> GetClassID() != TYPE_WINDMOVER) continue;
					if (!strcmp(mover -> className, windMoverName)) {
						// JLM, note: we are implicitly matching by file name above
						dynamic_cast<TWindMover *>(mover)-> GetTimeValue (model -> modelTime, &wVel);
						bFound = true;
						break;
					}
				}
				
				if (!bFound)
				{
					map = model -> uMap;
					for (i = 0, n = map -> moverList -> GetItemCount() ; i < n ; i++) {
						map -> moverList -> GetListItem((Ptr)&mover, i);
						if (mover -> GetClassID() != TYPE_WINDMOVER) continue;
						if (!strcmp(mover -> className, windMoverName)) {
							// JLM, note: we are implicitly matching by file name above
							dynamic_cast<TWindMover *>(mover)-> GetTimeValue (model -> modelTime, &wVel);
							bFound = true;
							break;
						}
					}
				}
			}
		}
		// alert code goes here if mover is not found
	}
	
	// code goes here, option for averaged winds to set a scale or use the main dialog scale, would pat1ValScale/pat2ValScale just be averaged wind value? 
	
	//if (bUseAveragedWinds && bUseMainDialogScaleFactor)
	windSpeedToScale = sqrt(wVel.u*wVel.u + wVel.v*wVel.v);
	
	pat1SpeedMPS = pat1Speed * speedconversion (pat1SpeedUnits);
	pat2SpeedMPS = pat2Speed * speedconversion (pat2SpeedUnits);

	pat1Theta = PI * -(0.5 + (pat1Angle / 180.0));
	pat2Theta = PI * -(0.5 + (pat2Angle / 180.0));
	
	ref1Wind.u = cos (pat1Theta) * pat1SpeedMPS;
	ref1Wind.v = sin (pat1Theta) * pat1SpeedMPS;
	ref2Wind.u = cos (pat2Theta) * pat2SpeedMPS;
	ref2Wind.v = sin (pat2Theta) * pat2SpeedMPS;
	
	dot1 = wVel.u*ref1Wind.u + wVel.v*ref1Wind.v;
	dot2 = wVel.u*ref2Wind.u + wVel.v*ref2Wind.v;
	
	pat1ValRef = pattern1 -> GetPatValue (refP);
	pat1ValRefLength = sqrt (pat1ValRef.u * pat1ValRef.u + pat1ValRef.v * pat1ValRef.v);

	// code goes here, some restriction on when WINDSTRESS scaling can be used?
	if (!bUseAveragedWinds)	// usemaindialogscalefactor different...
	{	
		if (scaleBy == WINDSPEED)
			fOptimize.pat1ValScale = dot1 * pat1ScaleToValue / (pat1SpeedMPS * pat1SpeedMPS * pat1ValRefLength);
		else // scaleBy == WINDSTRESS
			fOptimize.pat1ValScale = dot1 * pat1ScaleToValue * windSpeedToScale / (pat1SpeedMPS * pat1SpeedMPS * pat1SpeedMPS * pat1ValRefLength);
	}
	else
		// I think the value needs to be normalized, then match Matt's windage with a scale factor - 3,4 or whatever windage is (not .03)
		// at this point the normalization means the same scale factor is used (I think Matt is dividing by ref val rather than multiplying...)
		//fOptimize.pat1ValScale = (wVel.u * cos (pat1Theta) + wVel.v * sin (pat1Theta)) * fScaleFactorAveragedWinds; //windSpeedToScale; //?? need the dot product too
{		//fOptimize.pat1ValScale = (wVel.u * cos (pat1Theta) + wVel.v * sin (pat1Theta)) * fScaleFactorAveragedWinds / pat1ValRefLength; //windSpeedToScale; //?? need the dot product too

		if (bUseMainDialogScaleFactor)	// wind speed/ wind stress issue?
			//fOptimize.pat1ValScale = wVel.u * pat1ScaleToValue / (pat1SpeedMPS * pat1SpeedMPS * pat1ValRefLength); //stored component in average handle
			fOptimize.pat1ValScale = wVel.u * pat1ScaleToValue / (pat1SpeedMPS * pat1ValRefLength); //stored component in average handle, don't multiply by pat1SpeedMPS in Average Wind calc
		else
			//fOptimize.pat1ValScale = wVel.u * fScaleFactorAveragedWinds / pat1ValRefLength; //stored component in average handle
			fOptimize.pat1ValScale = wVel.u * fScaleFactorAveragedWinds; // Bushy says just multiply by the factor, don't use any reference point stuff
			//fOptimize.pat1ValScale = wVel.u * fScaleFactorAveragedWinds * pat1SpeedMPS / pat1ValRefLength; //stored component in average handle
}
	if (pattern2) 
	{
		pat2ValRef = pattern2 -> GetPatValue (refP);
		pat2ValRefLength = sqrt (pat2ValRef.u * pat2ValRef.u + pat2ValRef.v * pat2ValRef.v);
		if (!bUseAveragedWinds || (bUseAveragedWinds && bUseMainDialogScaleFactor))	// averaged winds shouldn't be an issue here
		{
			if (scaleBy == WINDSPEED)
				fOptimize.pat2ValScale = dot2 * pat2ScaleToValue / (pat2SpeedMPS * pat2SpeedMPS * pat2ValRefLength);
			else // scaleBy == WINDSTRESS
				fOptimize.pat2ValScale = dot2 * pat2ScaleToValue * windSpeedToScale / (pat2SpeedMPS * pat2SpeedMPS * pat2SpeedMPS * pat2ValRefLength);
		}
		else	// assuming there is no pat2 for averaged winds option...
			fOptimize.pat2ValScale = (wVel.u * cos (pat2Theta) + wVel.v * sin (pat2Theta)) * fScaleFactorAveragedWinds; //windSpeedToScale; //?? need the dot product too
		
	}
	else fOptimize.pat2ValScale = 0;

	return noErr;
}

WorldPoint3D TComponentMover::GetMove (Seconds timeStep,long setIndex,long leIndex,LERec *theLE,LETYPE leType)
{
	double 		dLat, dLong;
	WorldPoint3D	deltaPoint = {0,0,0.};
	WorldPoint refPoint = (*theLE).p;	
	VelocityRec	finalVel, pat1Val,pat2Val;
	OSErr err = 0;
	char errmsg[256];
	     
	pat1Val = pattern1 -> GetPatValue (refPoint);
	if (pattern2) pat2Val = pattern2 -> GetPatValue (refPoint);
	else {pat2Val.u = pat2Val.v = 0;}

	if (!fOptimize.isOptimizedForStep)
	{
		err = SetOptimizeVariables (errmsg);
		if (err) return deltaPoint;
	}


	finalVel.u = pat1Val.u * fOptimize.pat1ValScale + pat2Val.u * fOptimize.pat2ValScale;
	finalVel.v = pat1Val.v * fOptimize.pat1ValScale + pat2Val.v * fOptimize.pat2ValScale;
		
	if(leType == UNCERTAINTY_LE)
	{
		AddUncertainty(setIndex,leIndex,&finalVel,timeStep);
	}

	dLong = ((finalVel.u / METERSPERDEGREELAT) * timeStep) / LongToLatRatio3 (refPoint.pLat);
	dLat =   (finalVel.v / METERSPERDEGREELAT) * timeStep;

	deltaPoint.p.pLong = dLong * 1000000;
	deltaPoint.p.pLat  = dLat  * 1000000;

	return deltaPoint;
}

Boolean TComponentMover::VelocityStrAtPoint(WorldPoint3D wp, char *diagnosticStr)
{
	char str1[32], str2[32], str3[32];
	double length1=0., length2=0., length3 = 0.;
	VelocityRec	pat1Val={0.,0.}, pat2Val={0.,0.}, finalVel;
	OSErr err = 0;
	char errmsg[256];

	if (this -> pattern1) {
		pat1Val = this -> pattern1 -> GetPatValue (wp.p);
		length1 = sqrt(pat1Val.u * pat1Val.u + pat1Val.v * pat1Val.v);
		StringWithoutTrailingZeros(str1,length1,6);
	}
	if (this -> pattern2) {
		pat2Val = this -> pattern2 -> GetPatValue (wp.p);
		length2 = sqrt(pat2Val.u * pat2Val.u + pat2Val.v * pat2Val.v);
		StringWithoutTrailingZeros(str2,length2,6);
	}
	if (!(this->fOptimize.isOptimizedForStep))
	{
		err = this->SetOptimizeVariables (errmsg);
		if (err) return false;
	}

	finalVel.u = pat1Val.u * this->fOptimize.pat1ValScale + pat2Val.u * this->fOptimize.pat2ValScale;
	finalVel.v = pat1Val.v * this->fOptimize.pat1ValScale + pat2Val.v * this->fOptimize.pat2ValScale;
	length3 = sqrt(finalVel.u * finalVel.u + finalVel.v * finalVel.v);

	StringWithoutTrailingZeros(str1,length1 * this->fOptimize.pat1ValScale,4);
	StringWithoutTrailingZeros(str2,length2 * this->fOptimize.pat2ValScale,4);
	StringWithoutTrailingZeros(str3,length3,4);
	sprintf(diagnosticStr, " [grid: %s, pat1: %s m/s, pat2: %s m/s, total: %s m/s]",
						   "component", str1, str2, str3);
	return true;
					
}

/////////////////////////////////////////////////

//static CurrentUncertainyInfo sInfo;
//static Boolean sDialogValuesChanged;
static long sTimeToAverage;
static double sScaleFactor;
static double sWindPowerFactor;
static Boolean sUseMainDialogScaleFactor;


short AveragedWindsClick(DialogPtr dialog, short itemNum, long lParam, VOIDPTR data)
{
#pragma unused (data)


	switch(itemNum)
	{
		case M20bOK:
		{	
			//Seconds startTime;
		//	double duration, downCur, upCur, leftCur, rightCur, eddyDiffusion; 
			// ignore certain fields that are not used by component movers, STH
			
			sTimeToAverage = EditText2Long(dialog, M20bAVERAGEHRS);
			sScaleFactor = EditText2Float(dialog, M20bSCALEFACTOR);
			sWindPowerFactor = EditText2Float(dialog, M20bWINDPOWER);
			sUseMainDialogScaleFactor = GetButton (dialog, M20bSCALEFACTORCHECKBOX);

			return M20bOK;
		}
			
		case M20bCANCEL:
			return M20bCANCEL;
			break;
			
		case M20bAVERAGEHRS:		
			CheckNumberTextItem(dialog, itemNum, FALSE); //  don't allow decimals
			break;

		case M20bSCALEFACTOR:		
			CheckNumberTextItemAllowingNegative(dialog, itemNum, TRUE); //  allow decimals
			break;
		case M20bWINDPOWER:		
			CheckNumberTextItem(dialog, itemNum, TRUE); //  allow decimals
			break;

		case M20bSCALEFACTORCHECKBOX:		
			ToggleButton(dialog, itemNum);
			ShowHideDialogItem(dialog, M20bSCALEFACTOR, GetButton (dialog, itemNum) == 0); 
			ShowHideDialogItem(dialog, M20bSCALEFACTORLABEL, GetButton (dialog, itemNum) == 0); 
			break;
	}
	 
	return 0;
}

OSErr AveragedWindsInit(DialogPtr dialog, VOIDPTR data)
{
	#pragma unused (data)
	Long2EditText(dialog,M20bAVERAGEHRS,sTimeToAverage);
	Float2EditText(dialog,M20bSCALEFACTOR,sScaleFactor,2);
	Float2EditText(dialog, M20bWINDPOWER, sWindPowerFactor, 2);

	// code goes here, need a field to track which scale factor to use
	SetButton (dialog, M20bSCALEFACTORCHECKBOX, sUseMainDialogScaleFactor);
	ShowHideDialogItem(dialog, M20bSCALEFACTOR, GetButton (dialog, M20bSCALEFACTORCHECKBOX) == 0); 
	ShowHideDialogItem(dialog, M20bSCALEFACTORLABEL, GetButton (dialog, M20bSCALEFACTORCHECKBOX) == 0); 
	MySelectDialogItemText(dialog, M20bAVERAGEHRS, 0, 255);

	return 0;
}

OSErr AveragedWindsParametersDialog(long *timeToAverage, double *scaleFactor, double *windPowerFactor, Boolean *useMainDialogScaleFactor, WindowPtr parentWindow/*, Boolean *uncertaintyChanged*/)
{
	short item;
	if(parentWindow == nil) parentWindow = mapWindow; // JLM 6/2/99, we need the parent on the IBM
//	if(info == nil) return -1;
	sTimeToAverage = *timeToAverage;
	sScaleFactor = *scaleFactor;
	sWindPowerFactor = *windPowerFactor;
	sUseMainDialogScaleFactor = *useMainDialogScaleFactor;
	//sDialogValuesChanged = *uncertaintyChanged;
	item = MyModalDialog(M20b, parentWindow, 0, AveragedWindsInit, AveragedWindsClick);
	if (item == M20bOK) {
	//	*info = sInfo;
	//	*uncertaintyChanged = sDialogValuesChanged;
	*timeToAverage = sTimeToAverage;
	*scaleFactor = sScaleFactor;
	*useMainDialogScaleFactor = sUseMainDialogScaleFactor;
	*windPowerFactor = sWindPowerFactor;

		if(parentWindow == mapWindow) {
			model->NewDirtNotification(); // when a dialog is the parent, we rely on that  dialog's to notify about Dirt 
			// that way we don't get the map redrawing behind the parent dialog on the IBM
		}
	}
	return item ==M20bOK? 0 : -1;
}

////////////////////////////////////
void ShowHideTComponentDialogItems(DialogPtr dialog)
{
	TComponentMover	*mover = sSharedDialogComponentMover;
	short moverCode = GetPopSelection(dialog, M20MOVERTYPES);
	Boolean showPat2Items = FALSE, showScaleByItems = FALSE;

	if (mover -> pattern2) 	// don't show pattern 2 items unless the pattern has been loaded
		showPat2Items = TRUE;
		
	if (moverCode == kLinkToWindMover) // don't show scaleBy popup unless a WindMover has been selected
		showScaleByItems = TRUE;


	ShowHideDialogItem(dialog, M20SCALEUSING, showScaleByItems); 
	ShowHideDialogItem(dialog, M20SCALEBYTYPES, showScaleByItems); 

//	ShowHideDialogItem(dialog, M20USEAVERAGEDWINDS, showScaleByItems); 
//	ShowHideDialogItem(dialog, M20SETAVERAGEDWINDSPARAMETERS, showScaleByItems); 

	ShowHideDialogItem(dialog, M20RELWINDDIRLABEL, showPat2Items); 
	ShowHideDialogItem(dialog, M20DIRPLUS90, showPat2Items); 
	ShowHideDialogItem(dialog, M20DIRMINUS90, showPat2Items); 
	ShowHideDialogItem(dialog, M20PAT2DIR, showPat2Items); 
	ShowHideDialogItem(dialog, M20PAT2SPEED, showPat2Items); 
	ShowHideDialogItem(dialog, M20PAT2SPEEDUNITS, showPat2Items); 
	ShowHideDialogItem(dialog, M20PAT2SCALE, showPat2Items);
	ShowHideDialogItem(dialog, M20EQUALS, showPat2Items);
	ShowHideDialogItem(dialog, M20PAT2DIRUNITS, showPat2Items);
	ShowHideDialogItem(dialog, M20PAT2FORWINDSPEED, showPat2Items); 
	ShowHideDialogItem(dialog, M20PAT2SCALETO, showPat2Items); 
	ShowHideDialogItem(dialog, M20PAT2SCALEUNITS, showPat2Items); 
	ShowHideDialogItem(dialog, M20UNSCALEDVAL2LABEL, showPat2Items); 
	ShowHideDialogItem(dialog, M20UNSCALEDVALUE2, showPat2Items); 
	ShowHideDialogItem(dialog, M20UNSCALEDVAL2UNITS, showPat2Items);
}

void ShowUnscaledComponentValues(DialogPtr dialog,double * pat1Val,double* pat2Val)
{
	double len = 0;
	WorldPoint p;
	VelocityRec velocity;
	OSErr err = 0;
	char str1[32]="*",str2[32] = "*";
	TComponentMover	*mover = sSharedDialogComponentMover;
	
	if(pat1Val) *pat1Val = 0;
	if(pat2Val) *pat2Val = 0;
	
	err = EditTexts2LL(dialog, M20LATDEGREES, &p, FALSE);
	if (!err && mover) {
		if (mover -> pattern1) {
			velocity = mover -> pattern1 -> GetPatValue (p);
			len = sqrt(velocity.u * velocity.u + velocity.v * velocity.v);
			if(pat1Val) *pat1Val = len;
			StringWithoutTrailingZeros(str1,len,6);
		}
		if (mover -> pattern2) {
			velocity = mover -> pattern2 -> GetPatValue (p);
			len = sqrt(velocity.u * velocity.u + velocity.v * velocity.v);
			if(pat2Val) *pat2Val = len;
			StringWithoutTrailingZeros(str2,len,6);
		}
	}

	mysetitext(dialog, M20UNSCALEDVALUE1, str1);
	mysetitext(dialog, M20UNSCALEDVALUE2, str2);

}

static OSErr EditTextToDirection(DialogPtr dialog, short item,double *angle)
{
	char s[30];
	long n=10,i=0;
	OSErr err = noErr;
	
	mygetitext(dialog,item,s,n);
	StrToUpper(s);
	RemoveTrailingSpaces(s);
	//if(isdigit(s[0]))
	if (s[0]>='0' && s[0]<='9')	// for code warrior
	{
		*angle = EditText2Float(dialog,item); // in degrees 
		if(*angle > 360)
		{
			printError("Your direction value cannot exceed 360.");	
			MySelectDialogItemText (dialog, item, 0, 255);
			err = -2;
		}
	}
	else
	{
		if(strcmp(s,NORTH) == 0){i=0;
		}
		else if(strcmp(s,NNE)==0){i=1;
		}
		else if(strcmp(s,NE)==0){i=2;
		}
		else if(strcmp(s,ENE)==0){i=3;
		}
		else if(strcmp(s,EAST)==0){i=4;
		}
		else if(strcmp(s,ESE)==0){i=5;
		}
		else if(strcmp(s,SE)==0){i=6;
		}
		else if(strcmp(s,SSE)==0){i=7;
		}
		else if(strcmp(s,SOUTH)==0){i=8;
		}
		else if(strcmp(s,SSW)==0){i=9;
		}
		else if(strcmp(s,SW)==0){i=10;
		}
		else if(strcmp(s,WSW)==0){i=11;
		}
		else if(strcmp(s,WEST)==0){i=12;
		}
		else if(strcmp(s,WNW)==0){i=13;
		}
		else if(strcmp(s,NW)==0){i=14;
		}
		else if(strcmp(s,NNW)==0){i=15;
		}
		else{
			SysBeep(5);
			err=-1;
		}
		*angle= i*22.5;	// in degrees
	}
	return err;
}

short ComponentDlgClick(DialogPtr dialog, short itemNum, long lParam, VOIDPTR data)
{
	Boolean			bChanged;
	short 			item;
	WorldPoint 		p;
	TOSSMTimeValue *newTimeFile;
	OSErr 			err = 0;
	long 			units;
	double	pat1Dir, pat2Dir;
	long moverCode;
	double unscaled1,unscaled2;
	TComponentMover	*mover = sSharedDialogComponentMover;
	#define MIN_UNSCALED_REF_LENGTH 1.0E-5 // it's way too small
	double pat1Angle;

	StandardLLClick(dialog, itemNum, M20LATDEGREES, M20DEGREES, &p, &bChanged);
	ShowUnscaledComponentValues(dialog,&unscaled1,&unscaled2);

	switch (itemNum) {

		case M20OK:
		{
			if (!mover -> pattern1)
			{
				printError("Pattern #1 has not been selected.");
				return 0;
			}
			
			err = EditTexts2LL(dialog, M20LATDEGREES, &p, TRUE);
			if(err) return 0;

			// check that we can scale at the given ref point
			if(fabs(unscaled1) < MIN_UNSCALED_REF_LENGTH) {
				printError("The unscaled value for pattern 1 is too small at the chosen reference point.");
				return 0;
			}
			if (mover -> pattern2 && fabs(unscaled2) < MIN_UNSCALED_REF_LENGTH)
			{
				printError("The unscaled value for pattern 2 is too small at the chosen reference point.");
				return 0;
			}
			
			moverCode = GetPopSelection (dialog, M20MOVERTYPES);
			if (moverCode == kLinkToNone)
				printError("This mover will remain inactive until either a wind mover or time file is specified.");
			
			// code goes here, put in an error if user selected averaged winds and there are no winds
			err = EditTextToDirection(dialog, M20PAT1DIR,&pat1Angle);	// allow degrees or text 5/26/00
			if(err) break;

			// point of no return
			////////////////////////////////////////////////////////////
			mover -> refP = p;
			
			if (mover -> pattern1) mover -> pattern1 -> refP = p;	// need to set pattern's refP so it is drawn correctly, 3/28/00
			if (mover -> pattern2) mover -> pattern2 -> refP = p;	// need to set pattern's refP so it is drawn correctly, 3/28/00

			// delete original patterns if they are no longer the pattern of choice
			if (sSharedOriginalPattern1 && sSharedOriginalPattern1 != mover -> pattern1) {
				sSharedOriginalPattern1 -> Dispose();
				delete sSharedOriginalPattern1;
				sSharedOriginalPattern1 = nil;
			}
			if (sSharedOriginalPattern2 && sSharedOriginalPattern2 != mover -> pattern2) {
				sSharedOriginalPattern2 -> Dispose();
				delete sSharedOriginalPattern2;
				sSharedOriginalPattern2 = nil;
			}
			
			mover -> timeMoverCode = moverCode;

			//mover -> pat1Angle = EditText2Float(dialog, M20PAT1DIR);
			mover -> pat1Angle = pat1Angle;	// allow degrees or text 5/26/00
			mover -> pat2Angle = EditText2Float(dialog, M20PAT2DIR);

			mover -> pat1Speed = EditText2Long(dialog, M20PAT1SPEED);
			mover -> pat2Speed = EditText2Long(dialog, M20PAT2SPEED);
			
			mover -> pat1ScaleToValue = EditText2Float(dialog, M20PAT1SCALE);
			mover -> pat2ScaleToValue = EditText2Float(dialog, M20PAT2SCALE);
			
			mover -> pat1SpeedUnits = GetPopSelection (dialog, M20PAT1SPEEDUNITS);
			mover -> pat2SpeedUnits = GetPopSelection (dialog, M20PAT2SPEEDUNITS);
			
			mover -> scaleBy = GetPopSelection (dialog, M20SCALEBYTYPES);
			
			mygetitext (dialog, M20MOVERNAME, mover -> windMoverName, 63);
			
			//mover -> SetCurrentUncertaintyInfo(sSharedComponentUncertainyInfo);
			if (!mover->CurrentUncertaintySame(sSharedComponentUncertainyInfo))
			{
				mover -> SetCurrentUncertaintyInfo(sSharedComponentUncertainyInfo);
				mover->UpdateUncertaintyValues(model->GetModelTime()-model->GetStartTime());
			}
			// code goes here, set the averaged winds parameters...
			mover->bUseAveragedWinds = GetButton(dialog,M20USEAVERAGEDWINDS);
			mover->bExtrapolateWinds = GetButton(dialog,M20EXTRAPOLATEWINDSCHECKBOX);
			mover->fPastHoursToAverage = sSharedDialogComponentMover->fPastHoursToAverage;
			mover->fScaleFactorAveragedWinds = sSharedDialogComponentMover->fScaleFactorAveragedWinds;
			mover->bUseMainDialogScaleFactor = sSharedDialogComponentMover->bUseMainDialogScaleFactor;
			mover->fPowerFactorAveragedWinds = sSharedDialogComponentMover->fPowerFactorAveragedWinds;
			return M20OK;
		}
		break;

		case M20CANCEL:
			// delete any new patterns, restore original patterns
			DeleteIfNotOriginalPattern(mover -> pattern1);
			DeleteIfNotOriginalPattern(mover -> pattern2);
			mover -> pattern1 = sSharedOriginalPattern1;
			mover -> pattern2 = sSharedOriginalPattern2;

			return M20CANCEL;
		break;
		
		
		case M20PAT1SELECT:
			DeleteIfNotOriginalPattern(mover -> pattern1);
			mover -> pattern1 = CreateAndInitCatsCurrentsMover (mover -> moverMap,true,0,0);
			////
			if (mover -> pattern1)
			{
				mysetitext(dialog, M20PAT1NAME, mover -> pattern1 -> className);
			}
			else
				err = memFullErr;
		break;

		case M20PAT2SELECT:
			DeleteIfNotOriginalPattern(mover -> pattern2);
			mover -> pattern2 = CreateAndInitCatsCurrentsMover (mover -> moverMap,true,0,0);
			if (mover -> pattern2)
			{
				mysetitext(dialog, M20PAT2NAME, mover -> pattern2 -> className);
				ShowHideTComponentDialogItems(dialog);	// have pattern 2 so show rest of dialog
			}
			else
				err = memFullErr;
		break;

		case M20DIRPLUS90:
		{
			SetButton(dialog, M20DIRPLUS90, true);
			SetButton(dialog, M20DIRMINUS90, false);
						
			//pat1Dir = EditText2Float(dialog, M20PAT1DIR);
			err = EditTextToDirection(dialog, M20PAT1DIR, &pat1Dir); // allow degrees or text 5/26/00
			if(err) break;

			pat2Dir = pat1Dir + 90;
			
			Float2EditText (dialog, M20PAT2DIR, pat2Dir>360 ? pat2Dir-360 : pat2Dir,1);
		}
		break;
		
		case M20DIRMINUS90:
		{
			SetButton(dialog, M20DIRMINUS90, true);
			SetButton(dialog, M20DIRPLUS90, false);
		
			//pat1Dir = EditText2Float(dialog, M20PAT1DIR);
			err = EditTextToDirection(dialog, M20PAT1DIR, &pat1Dir); // allow degrees or text 5/26/00
			if(err) break;
			
			pat2Dir = pat1Dir - 90;

			Float2EditText (dialog, M20PAT2DIR, pat2Dir<0 ? pat2Dir+360 : pat2Dir,1);
		}
		break;

		case M20PAT1SPEED:
		case M20PAT2SPEED:
			CheckNumberTextItem(dialog, itemNum, TRUE); //  allow decimals
			break;
		
		case M20PAT1SCALE:
		case M20PAT2SCALE:
			CheckNumberTextItemAllowingNegative(dialog, itemNum, TRUE); //  allow decimals
			break;

		//case M20PAT1DIR: is listed below
		case M20PAT2DIR:
			//CheckNumberTextItem(dialog, itemNum, TRUE); //  allow decimals
			CheckDirectionTextItem(dialog, itemNum);	// allow degrees or text 5/26/00
			break;

		case M20PAT1DIR:
		{
			//CheckNumberTextItem(dialog, itemNum, TRUE); //  allow decimals
			//pat1Dir = EditText2Float(dialog, M20PAT1DIR);
			CheckDirectionTextItem(dialog, itemNum);
			err = EditTextToDirection(dialog, M20PAT1DIR,&pat1Dir); // allow degrees or text 5/26/00
			if(err) break;
			if (GetButton (dialog, M20DIRPLUS90))
			{
				pat2Dir = pat1Dir + 90;
				if (pat2Dir > 360) pat2Dir -= 360;
			}
			else
			{
				pat2Dir = pat1Dir - 90;
				if (pat2Dir < 0) pat2Dir += 360;
			}
			Float2EditText (dialog, M20PAT2DIR, pat2Dir,1);
		}
		break;

		case M20DEGREES:
		case M20DEGMIN:
		case M20DMS:
			err = EditTexts2LL(dialog, M20LATDEGREES, &p,TRUE);
			if(err) break;
			err = EditTexts2LL(dialog, M20LATDEGREES, &p,TRUE);
			if(err) break;
			if (itemNum == M20DEGREES) settings.latLongFormat = DEGREES;
			if (itemNum == M20DEGMIN) settings.latLongFormat = DEGMIN;
			if (itemNum == M20DMS) settings.latLongFormat = DMS;
			SwitchLLFormatHelper(dialog, M20LATDEGREES, M20DEGREES,true);
			LL2EditTexts(dialog, M20LATDEGREES, &p);
			break;

		case M20PAT1SPEEDUNITS: PopClick(dialog, itemNum, &units); break;
		case M20PAT2SPEEDUNITS: PopClick(dialog, itemNum, &units); break;

		case M20SCALEBYTYPES: PopClick(dialog, itemNum, &units); break;
		
		case M20MOVERTYPES:
		{	
			// code goes here, might want a show hide with UseAveragedWinds...
			long moverCodeBeforePopClick = GetPopSelection (dialog, M20MOVERTYPES);
			long moverCodeAfterPopClick;
			char blankStr[32];
			strcpy(blankStr,"");
			PopClick(dialog, itemNum, &units);
			moverCodeAfterPopClick = GetPopSelection (dialog, M20MOVERTYPES);
			switch (moverCodeAfterPopClick)
			{
				case kLinkToNone:		// none
					//mysetitext(dialog, M20MOVERNAME, "");
					mysetitext(dialog, M20MOVERNAME, blankStr);
				break;

				case kLinkToTimeFile: 	// time file
					//mover->timeMoverCode = kLinkToTimeFile;
					// code goes here, how do they pick the file
					//mysetitext(dialog, M20MOVERNAME, "");
					mysetitext(dialog, M20MOVERNAME, blankStr);
				break;

				case kLinkToWindMover: 	// wind mover
				{
					char classNameOfSelectedGrid[32];
					
					classNameOfSelectedGrid [0] = 0;
					
					ActivateParentDialog(FALSE);
					//mygetitext(dialog, M16SCALEGRIDNAME, classNameOfSelectedGrid, 31); // 4/28/00 what does this do?
					mygetitext(dialog, M20MOVERNAME, classNameOfSelectedGrid, 31);
					item = ChooseWindMoverDialog(classNameOfSelectedGrid);
					ActivateParentDialog(TRUE);
					if (item == M20OK)
					{
						mysetitext(dialog, M20MOVERNAME, classNameOfSelectedGrid);
					}
					else {
						// user canceled from the dialog, that means return to previous state
						SetPopSelection(dialog, M20MOVERTYPES, moverCodeBeforePopClick);
						PopDraw(dialog,M20MOVERTYPES);
					}
				}
				break;
			}
			ShowHideTComponentDialogItems(dialog);	
		}
		break;
		
		case M20UNCERTAINTY:
		{
			Boolean userCanceledOrErr, uncertaintyValuesChanged=false;
			CurrentUncertainyInfo info  = sSharedComponentUncertainyInfo;
			userCanceledOrErr = CurrentUncertaintyDialog(&info,GetDialogWindow(dialog),&uncertaintyValuesChanged);
			if(!userCanceledOrErr) 
			{
				if (uncertaintyValuesChanged)
				{
					sSharedComponentUncertainyInfo = info;
				}
			}
			break;
		}
		break;

		case M20USEAVERAGEDWINDS:
			ToggleButton(dialog, itemNum);
			ShowHideDialogItem(dialog, M20SETAVERAGEDWINDSPARAMETERS, GetButton (dialog, itemNum)); 
			break;

		case M20EXTRAPOLATEWINDSCHECKBOX:
			ToggleButton(dialog, itemNum);
			ShowHideDialogItem(dialog, M20EXTRAPOLATEWINDSCHECKBOX, GetButton (dialog, itemNum)); 
			break;

		case M20SETAVERAGEDWINDSPARAMETERS:
		{
			Boolean userCanceledOrErr/*, uncertaintyValuesChanged=false*/;
			// code goes here, need another set of parameters in case user cancels...
			//CurrentUncertainyInfo info  = sSharedComponentUncertainyInfo;
			long hoursToAverage = sSharedDialogComponentMover->fPastHoursToAverage; 
			Boolean useMainDialogScaleFactor = sSharedDialogComponentMover->bUseMainDialogScaleFactor;
			double scaleFactor = sSharedDialogComponentMover->fScaleFactorAveragedWinds;
			double windPowerFactor = sSharedDialogComponentMover->fPowerFactorAveragedWinds;
			userCanceledOrErr = AveragedWindsParametersDialog(/*&info,*/&hoursToAverage, &scaleFactor, &windPowerFactor, &useMainDialogScaleFactor, GetDialogWindow(dialog)/*,&uncertaintyValuesChanged*/);
			if(!userCanceledOrErr) 
			{
			/*	if (uncertaintyValuesChanged)
				{
					sSharedComponentUncertainyInfo = info;
				}*/
				sSharedDialogComponentMover->fPastHoursToAverage = hoursToAverage;
				sSharedDialogComponentMover->fScaleFactorAveragedWinds = scaleFactor;
				sSharedDialogComponentMover->bUseMainDialogScaleFactor = useMainDialogScaleFactor;
				sSharedDialogComponentMover->fPowerFactorAveragedWinds = windPowerFactor;

			}
			break;
		}
		break;
	}

	return 0;
}

///////////////////////////////////////////////////////////////////////////

static PopInfoRec componentIntDlgPopTable[] = {
		{ M20, nil, M20PAT1SPEEDUNITS, 0, pSPEEDUNITS,  0, 1, FALSE, nil },
		{ M20, nil, M20PAT2SPEEDUNITS, 0, pSPEEDUNITS2, 0, 1, FALSE, nil },
		{ M20, nil, M20LATDIR, 0, pNORTHSOUTH1, 0, 1, FALSE, nil },
		{ M20, nil, M20LONGDIR, 0, pEASTWEST1, 0, 1, FALSE, nil },
		{ M20, nil, M20MOVERTYPES, 0, pMOVERTYPES2, 0, 1, FALSE, nil },
		{ M20, nil, M20SCALEBYTYPES, 0, pSCALEBYTYPES, 0, 1, FALSE, nil }
	};

OSErr ComponentDlgInit(DialogPtr dialog, VOIDPTR data)
{
	char 			s[256];
	TComponentMover	*mover;
	char blankStr[32];
	strcpy(blankStr,"");
	
	mover = sSharedDialogComponentMover;
	if (!mover) return -1;
	sSharedOriginalPattern1 = mover -> pattern1;
	sSharedOriginalPattern2 = mover -> pattern2;
	sSharedComponentUncertainyInfo = mover -> GetCurrentUncertaintyInfo();
	
	RegisterPopTable (componentIntDlgPopTable, sizeof (componentIntDlgPopTable) / sizeof (PopInfoRec));
	RegisterPopUpDialog (M20, dialog);

	SetDialogItemHandle(dialog, M20HILITEDEFAULT, (Handle)FrameDefault);
	SetDialogItemHandle(dialog, M20FROST1, (Handle)FrameEmbossed);
	SetDialogItemHandle(dialog, M20FROST2, (Handle)FrameEmbossed);
	SetDialogItemHandle(dialog, M20FROST3, (Handle)FrameEmbossed);
	SetDialogItemHandle(dialog, M20FROST4, (Handle)FrameEmbossed);

	LL2EditTexts (dialog, M20LATDEGREES, &mover -> refP);

	SwitchLLFormatHelper(dialog, M20LATDEGREES, M20DEGREES,true);
	
	// restricted to 0 to 360, so not always +/- 90  6/18/01
	SetButton(dialog, M20DIRPLUS90,  mover -> pat2Angle == mover -> pat1Angle + 90 || mover -> pat2Angle == mover -> pat1Angle - 270);
	SetButton(dialog, M20DIRMINUS90, mover -> pat2Angle == mover -> pat1Angle - 90 || mover -> pat2Angle == mover -> pat1Angle + 270);
	
	Float2EditText(dialog, M20PAT1DIR, mover -> pat1Angle,1);
	Float2EditText(dialog, M20PAT2DIR, mover -> pat2Angle,1);

	Long2EditText(dialog, M20PAT1SPEED, mover -> pat1Speed);
	Long2EditText(dialog, M20PAT2SPEED, mover -> pat2Speed);
	
	Float2EditText(dialog, M20PAT1SCALE, mover -> pat1ScaleToValue, 3);
	Float2EditText(dialog, M20PAT2SCALE, mover -> pat2ScaleToValue, 3);
	
	if (mover -> pattern1) mysetitext(dialog, M20PAT1NAME, mover -> pattern1 -> className);
	//else mysetitext(dialog, M20PAT1NAME, "");
	else mysetitext(dialog, M20PAT1NAME, blankStr);
	if (mover -> pattern2) mysetitext(dialog, M20PAT2NAME, mover -> pattern2 -> className);
	//else mysetitext(dialog, M20PAT2NAME, "");
	else mysetitext(dialog, M20PAT2NAME, blankStr);

	mysetitext(dialog, M20MOVERNAME, mover -> windMoverName);
	SetPopSelection (dialog, M20MOVERTYPES, mover ->timeMoverCode);
	
	if (mover -> scaleBy == NONE) 
		mover -> scaleBy = WINDSTRESS;	// default for a new component mover
	SetPopSelection (dialog, M20SCALEBYTYPES, mover -> scaleBy);
	
	SetPopSelection (dialog, M20PAT1SPEEDUNITS, mover -> pat1SpeedUnits);
	SetPopSelection (dialog, M20PAT2SPEEDUNITS, mover -> pat2SpeedUnits);

	ShowUnscaledComponentValues(dialog,nil,nil);
	ShowHideTComponentDialogItems(dialog);

	SetButton (dialog, M20EXTRAPOLATEWINDSCHECKBOX, mover->bExtrapolateWinds);
	SetButton (dialog, M20USEAVERAGEDWINDS, mover->bUseAveragedWinds);
	ShowHideDialogItem(dialog, M20SETAVERAGEDWINDSPARAMETERS, GetButton (dialog, M20USEAVERAGEDWINDS)); 

	return 0;
}

OSErr ComponentMoverSettingsDialog(TComponentMover *newMover, TMap *owner)
{
	short item;

	if (!newMover)return -1;
	
	sSharedDialogComponentMover = newMover;
	item = MyModalDialog(M20, mapWindow, 0, ComponentDlgInit, ComponentDlgClick);
	sSharedDialogComponentMover = 0;
	sSharedOriginalPattern1 = 0;
	sSharedOriginalPattern2 = 0;

	if(M20OK == item)	model->NewDirtNotification();// tell model about dirt
	return M20OK == item ? 0 : -1;
}

//#define TComponent_FileVersion 1
//#define TComponent_FileVersion 2	// HABS fields
#define TComponent_FileVersion 3	// extrapolate winds
OSErr TComponentMover::Write(BFPB *bfpb)
{
	long version = TComponent_FileVersion, numWinds;
	ClassID id = GetClassID ();
	OSErr err = 0;
	char c;
	
	if (err = TCurrentMover::Write(bfpb)) return err;
	
	// save class fields

	StartReadWriteSequence("TComponentMover::Write()");
	if (err = WriteMacValue(bfpb, id)) return err;
	if (err = WriteMacValue(bfpb, version)) return err;
	
	// first save both patterns
	if (err = pattern1->Write(bfpb)) return err;
	
	c = pattern2 ? TRUE : FALSE;
	if (err = WriteMacValue(bfpb, c)) return err;
	if (pattern2) if (err = pattern2->Write(bfpb)) return err;
	
	if (err = WriteMacValue(bfpb, bPat1Open)) return err;
	if (err = WriteMacValue(bfpb, bPat2Open)) return err;

	if (err = WriteMacValue(bfpb, refP.pLat)) return err;
	if (err = WriteMacValue(bfpb, refP.pLong)) return err;
	if (err = WriteMacValue(bfpb, bRefPointOpen)) return err;
	
	if (err = WriteMacValue(bfpb, pat1Angle)) return err;
	if (err = WriteMacValue(bfpb, pat2Angle)) return err;
	
	if (err = WriteMacValue(bfpb, pat1Speed)) return err;
	if (err = WriteMacValue(bfpb, pat2Speed)) return err;
	if (err = WriteMacValue(bfpb, pat1SpeedUnits)) return err;
	if (err = WriteMacValue(bfpb, pat2SpeedUnits)) return err;
	
	if (err = WriteMacValue(bfpb, pat1ScaleToValue)) return err;
	if (err = WriteMacValue(bfpb, pat2ScaleToValue)) return err;
	if (err = WriteMacValue(bfpb, scaleBy)) return err;
	if (err = WriteMacValue(bfpb, timeMoverCode)) return err;

	// save the name of wind / time mover or file
	if (err = WriteMacValue (bfpb, windMoverName, sizeof(windMoverName))) return err;
	
	//code goes here, averaged winds parameters
	if (err = WriteMacValue(bfpb, bUseAveragedWinds)) return err;
	if (err = WriteMacValue(bfpb, fScaleFactorAveragedWinds)) return err;
	if (err = WriteMacValue(bfpb, bUseMainDialogScaleFactor)) return err;
	if (err = WriteMacValue(bfpb, fPowerFactorAveragedWinds)) return err;
	if (err = WriteMacValue(bfpb, fPastHoursToAverage)) return err;
	/*if (bUseAveragedWinds)
	{
		if (err = WriteMacValue(bfpb, numWinds)) return err;
		for (i==0;i<numWinds;i++)
		{
			if (err = WriteMacValue(bfpb, INDEXH(fAveragedWindsHdl,i))) return err;
		}
	}*/

	if (err = WriteMacValue(bfpb, bExtrapolateWinds)) return err;

	return 0;
}

OSErr TComponentMover::Read(BFPB *bfpb)
{
	long version, numWinds;
	OSErr err = 0;
	ClassID id;
	char c, componentName[kMaxNameLen];
	VelocityRec avValue;
	
	if (err = TCurrentMover::Read(bfpb)) return err;
	
	GetClassName(componentName);
	if (componentName[0]==0)
		SetClassName("Component");
	//GetClassName(componentName);
	StartReadWriteSequence("TComponentMover::Write()");
	if (err = ReadMacValue(bfpb, &id)) return err;
	if (id != GetClassID ()) { TechError("TComponentMover::Read()", "id != GetClassID", 0); return -1; }
	if (err = ReadMacValue(bfpb, &version)) return err;
	if (version > TComponent_FileVersion) { printSaveFileVersionError(); return -1; }

	// create, init, and load the first pattern
	pattern1 = new TCATSMover(moverMap, "");
	if (pattern1)
	{
		((TMover*)pattern1) -> InitMover ();
		pattern1 -> Read (bfpb);
	}
	else
		{ TechError("TComponentMover::Read()", "pattern1 == nil", 0); return -1; }
		
	if (err = ReadMacValue(bfpb, &c)) return err;
	if (c) {
		// create, init, and load the second pattern
		pattern2 = new TCATSMover(moverMap, "");
		if (pattern2)
		{
			((TMover*)pattern2) -> InitMover ();
			pattern2 -> Read (bfpb);
		}
		else
			{ TechError("TComponentMover::Read()", "pattern2 == nil", 0); return -1; }
	}

	if (err = ReadMacValue(bfpb, &bPat1Open)) return err;
	if (err = ReadMacValue(bfpb, &bPat2Open)) return err;

	if (err = ReadMacValue(bfpb, &refP.pLat)) return err;
	if (err = ReadMacValue(bfpb, &refP.pLong)) return err;
	if (err = ReadMacValue(bfpb, &bRefPointOpen)) return err;
	
	if (err = ReadMacValue(bfpb, &pat1Angle)) return err;
	if (err = ReadMacValue(bfpb, &pat2Angle)) return err;
	
	if (err = ReadMacValue(bfpb, &pat1Speed)) return err;
	if (err = ReadMacValue(bfpb, &pat2Speed)) return err;
	if (err = ReadMacValue(bfpb, &pat1SpeedUnits)) return err;
	if (err = ReadMacValue(bfpb, &pat2SpeedUnits)) return err;
	
	if (err = ReadMacValue(bfpb, &pat1ScaleToValue)) return err;
	if (err = ReadMacValue(bfpb, &pat2ScaleToValue)) return err;
	if (err = ReadMacValue(bfpb, &scaleBy)) return err;
	if (err = ReadMacValue(bfpb, &timeMoverCode)) return err;

	// read the name of wind / time mover or file
	if (err = ReadMacValue (bfpb, windMoverName, sizeof(windMoverName))) return err;
	
	// code goes here, averaged winds parameters
	if (version>1)
	{
		if (err = ReadMacValue(bfpb, &bUseAveragedWinds)) return err;
		if (err = ReadMacValue(bfpb, &fScaleFactorAveragedWinds)) return err;
		if (err = ReadMacValue(bfpb, &bUseMainDialogScaleFactor)) return err;
		if (err = ReadMacValue(bfpb, &fPowerFactorAveragedWinds)) return err;
		if (err = ReadMacValue(bfpb, &fPastHoursToAverage)) return err;
		/*if (bUseAveragedWinds)
		{
			if (err = ReadMacValue(bfpb, &numWinds)) return err;
			if (numWinds>0)
			{
				fAveragedWindsHdl = (TimeValuePairH)_NewHandleClear(sizeof(TimeValuePair)*numWinds);
				if (!fAveragedWindsHdl)
					{ TechError("TComponentMover::Read()", "_NewHandleClear()", 0); return -1; }
			
				for (i==0;i<numWinds;i++)
				{
					if (err = ReadMacValue(bfpb, &avWind)) 
					{
						if (fAveragedWindsHdl)
						{
							DisposeHandle((Handle)fAveragedWindsHdl);
							fAveragedWindsHdl = 0;
							return err;
						}
					}
					INDEXH(fAveragedWindsHdl,i) = avWind;
				}
			}
		}*/
	}
	
	if (version>2)
	{
		if (err = ReadMacValue(bfpb, &bExtrapolateWinds)) return err;
	}
	return 0;
}

CurrentUncertainyInfo TComponentMover::GetCurrentUncertaintyInfo ()
{
	CurrentUncertainyInfo	info;

	memset(&info,0,sizeof(info));
	info.setEddyValues = FALSE;
	info.fUncertainStartTime	= this -> fUncertainStartTime;
	info.fDuration				= this -> fDuration;
	info.fEddyDiffusion			= 0;		
	info.fEddyV0				= 0;		
	info.fDownCurUncertainty	= this -> fDownCurUncertainty;	
	info.fUpCurUncertainty		= this -> fUpCurUncertainty;	
	info.fRightCurUncertainty	= this -> fRightCurUncertainty;	
	info.fLeftCurUncertainty	= this -> fLeftCurUncertainty;	

	return info;
}

void TComponentMover::SetCurrentUncertaintyInfo (CurrentUncertainyInfo info)
{
	
	this -> fUncertainStartTime		= info.fUncertainStartTime;
	this -> fDuration 				= info.fDuration;
	this -> fDownCurUncertainty 	= info.fDownCurUncertainty;	
	this -> fUpCurUncertainty 		= info.fUpCurUncertainty;	
	this -> fRightCurUncertainty 	= info.fRightCurUncertainty;	
	this -> fLeftCurUncertainty 	= info.fLeftCurUncertainty;	

	return;
}

Boolean TComponentMover::CurrentUncertaintySame (CurrentUncertainyInfo info)
{
	if (this -> fUncertainStartTime	== info.fUncertainStartTime 
	&&	this -> fDuration 				== info.fDuration
	&&	this -> fDownCurUncertainty 	== info.fDownCurUncertainty	
	&&	this -> fUpCurUncertainty 		== info.fUpCurUncertainty	
	&&	this -> fRightCurUncertainty 	== info.fRightCurUncertainty	
	&&	this -> fLeftCurUncertainty 	== info.fLeftCurUncertainty	)
	return true;
	else return false;
}