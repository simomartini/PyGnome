
#ifndef __TriCurMover__
#define __TriCurMover__

#include "Earl.h"
#include "TypeDefs.h"
#include "TriCurMover_c.h"

#include "TCurrentMover.h"

class TriCurMover : virtual public TriCurMover_c,  public TCurrentMover
{
	public:
						TriCurMover (TMap *owner, char *name);
						~TriCurMover () { Dispose (); }
	virtual void		Dispose ();
	virtual OSErr		InitMover ();
	virtual ClassID 	GetClassID () { return TYPE_TRICURMOVER; }
	virtual Boolean	IAm(ClassID id) { if(id==TYPE_TRICURMOVER) return TRUE; return TCurrentMover::IAm(id); }
	virtual Boolean		IAmA3DMover(){return true;}
	void 					DisposeLoadedData(LoadedData * dataPtr);	
	void 					ClearLoadedData(LoadedData * dataPtr);
	virtual Boolean 	CheckInterval(long &timeDataInterval);
	virtual OSErr	 	SetInterval(char *errmsg);
	
	// I/O methods
	virtual OSErr 		Read (BFPB *bfpb); 	// read from current position
	virtual OSErr 		Write (BFPB *bfpb); // write to  current position
	
	virtual OSErr		TextRead(char *path, TMap **newMap);
	//OSErr 				ReadTriCurVertices(CHARH fileBufH,long *line,LongPointHdl *pointsH,char* errmsg,long numPoints);
	OSErr 				ReadTriCurVertices(CHARH fileBufH,long *line,LongPointHdl *pointsH,FLOATH *totalDepthH,char* errmsg,long numPoints);
	OSErr 				ReadTriCurDepths(CHARH fileBufH,long *line,LongPointHdl *pointsH,char* errmsg,long numPoints);
	OSErr 				ReadHeaderLine(char *s);
	OSErr 				ReadBaromodesInputValues(CHARH fileBufH,long *line,BaromodesParameters *inputValues,char* errmsg,short modelType);
	OSErr 				ReadTimeData(long index,VelocityFH *velocityH, char* errmsg); 
	OSErr 				ScanFileForTimes(char *path,PtCurTimeDataHdl *timeDataHdl,Boolean setStartTime);
	OSErr 				ReadCentroidDepths(CHARH fileBufH,long *line,long numTris,/*FLOATH *centroidDepthsH,*/char* errmsg);
	OSErr 				ReadSigmaLevels(CHARH fileBufH,long *line,FLOATH *sigmaLevelsH,long numLevels,char* errmsg);
	//virtual OSErr 		CheckAndScanFile(char *errmsg);
	//OSErr 				ReadInputFileNames(CHARH fileBufH, long *line, long numFiles, PtCurFileInfoH *inputFilesH);
	
	//OSErr 				ReadTopology(char* path, TMap **newMap);
	//OSErr 				ExportTopology(char* path);
	
	// list display methods
	virtual OSErr 		CheckAndPassOnMessage(TModelMessage *message);
	
	virtual void		Draw (Rect r, WorldRect view);
	virtual Boolean	DrawingDependsOnTime(void);
	void 					DrawContourScale(Rect r, WorldRect view);
	virtual long		GetListLength ();
	virtual ListItem 	GetNthListItem (long n, short indent, short *style, char *text);
	virtual Boolean 	ListClick (ListItem item, Boolean inBullet, Boolean doubleClick);
	virtual Boolean 	FunctionEnabled (ListItem item, short buttonID);
	//virtual OSErr 		AddItem (ListItem item);
	virtual OSErr 		SettingsItem (ListItem item);
	virtual OSErr 		DeleteItem (ListItem item);
	
	virtual OSErr 		SettingsDialog();
	OSErr 				InputValuesDialog();
	
	long 					GetNumTimesInFile();
	//long 					GetNumFiles();
	void 			SetInputValues(BaromodesParameters inputValues);

};

#endif //  __TriCurMover__

