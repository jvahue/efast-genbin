#include "alt_stdtypes.h"
#include "GSEMgr.h"

BOOLEAN GSEMgr_Write(const CHAR* pBuf, UINT32 dataSize, BOOLEAN bEnd,
                     UINT32 mbox, UINT32 mboxPort)
{
    sprintf(&outputBuffer[insertAt], "%s", pBuf);
    insertAt += strlen(pBuf);
    return TRUE;
}
