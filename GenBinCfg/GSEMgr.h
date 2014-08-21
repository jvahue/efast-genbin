#ifndef GSEMgr_h
#define GSEMgr_h

#include "alt_stdTypes.h"

UINT32 insertAt;
CHAR outputBuffer[4096];


BOOLEAN GSEMgr_Write(const CHAR* pBuf, UINT32 dataSize, BOOLEAN bEnd,
                     UINT32 mbox, UINT32 mboxPort);



#endif