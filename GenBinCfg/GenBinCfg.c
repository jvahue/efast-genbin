// GenBinCfg.cpp : Defines the entry point for the console application.
//
// Build Process: 
// Make sure you have access to the ADRF code and you have specified the code directory as an 
// include directory.  Additionally you may have to add the following files if the relative code 
// path to the ADRF has changed.  This build assumes the following directory structure:
// 
// dev [or some root directory name]
// + GenBinCfg
//   + GenBinCfg <-- standard Visual Studio directory structure
// + target
//   + adrf
//     + code <-- location of all adrf code files
//
// Assuming this directory structure exists do the following prior to building GenBinCfg.exe:
// 1. Go to the target/adrf/code and rename alt_stdtypes.h -> alt_stdtypesx.h
// 2. Perform the build in Visual Studio as usual <Release> build
// 3. Go to the target/adrf/code and rename alt_stdtypesx.h -> alt_stdtypes.h
//
// Dependencies on ADRF Code that will require a recompile of this code
// 1. Obviously any change to the CFGMGR_NVRAM structure 
// 2. DefaultNVCfg - the definition is unchanged
// 3. ParamMgr_ResetDefaultCfg - no code changes to this function
// 4. AcmfRpt_ResetDefaultCfg - no code changes to this function
// 5. AdrfRpt_ResetDefaultCfg - no code changes to this function
// 
#include <stdio.h>
#include "stdafx.h"

//-----------------------------------------------------------------------------
// Software Specific Includes
//-----------------------------------------------------------------------------
#include "alt_stdtypes.h"
#include "CommonDef.h"

#include "ConfigMgr.h"
#include "User.h"
#include "Utility.h"

#include "AcmfReport.h"
#include "AdrfReport.h"
#include "CruiseMgr.h"
#include "Cycle.h"
#include "DivergenceMgr.h"
#include "efast.h"
#include "EfastMgr.h"
#include "EventMgr.h"
#include "ParamMgr.h"
#include "ReportMgr.h"
#include "TimeHistoryMgr.h"
#include "TriggerMgr.h"


#include "GSEMgr.h"

//-----------------------------------------------------------------------------------
// These need to be above the User Tables as these are defined in the .c files that
// the various User Files are included into.
// acmf
ACMF_CFG        m_acmfCfg[MAX_ACMF_REPORT];     // ACMF report configuration
ACMF_CTL        m_acmfCtl[MAX_ACMF_REPORT];     // ACMF report control
ACMF_RUN_STATUS m_acmfStatus;                   // report status and statistics
RPT_RUN_CTL     m_acmfRunCtl[MAX_ACMF_REPORT];  // ACMF report run control
// adrf
ADRF_CFG        m_adrfCfg[MAX_ADRF_REPORT];   // ADRF report configuration
ADRF_CTL        m_adrfCtl[MAX_ADRF_REPORT];   // ADRF report control
//
// --- Include the User Tables ---
#define ACMF_REPORT_BODY
#include "AcmfReportUserTables.c"
#define ADRF_REPORT_BODY
#include "AdrfReportUserTables.c"
#define CRUISE_MGR_BODY
#include "CruiseMgrUserTables.c"
#define CYCLE_BODY
#include "CycleUserTables.c"
#define DIVERGENCE_MGR_BODY
#include "DivergenceMgrUserTables.c"
#define EFAST_MGR_BODY
#include "EfastMgrUserTables.c"
#define EVENT_MGR_BODY
#include "EventMgrUserTables.c"
#define PARAM_MGR_BODY
#include "ParamMgrUserTables.c"
#define REPORT_MGR_BODY
#include "ReportMgrUserTables.c"
#define TIMEHISTORY_MGR_BODY
#include "TimeHistoryMgrUserTables.c"
#define TRIGGER_MGR_BODY
#include "TriggerMgrUserTables.c"

//-----------------------------------------------------------------------------
// Defines
//-----------------------------------------------------------------------------
#define GBC_VERSION "v0.1.0"

#define BIN_ID "BIN_"
#define START_A "Channel_A_Begin"
#define END_A   "Channel_A_End"
#define START_B "Channel_B_Begin"
#define END_B   "Channel_B_End"
#define BINCFG_END_ADJ  12  // Exclude the 3 CRCs at the End of CFGMGR_NVRAM
#define LINE_SIZE 512
#define CRC_BUFFER_SIZE (500*1024)
#define XML_TAGS 8

//-----------------------------------------------------------------------------
// typedefs
//-----------------------------------------------------------------------------
typedef struct {
    char xmlTag[64];
    char cmd[64];
} XML_MAP;

//-----------------------------------------------------------------------------
// Local Functions
//-----------------------------------------------------------------------------
void Init(void);
int AsciiFileHasCrc(CHAR* filename);
BOOL ChannelLogic(CHAR* gseCmd);
BOOL ComputeCrc( CHAR* filename, UINT32* crc, UINT32 exclude);
int ProcessCfgFile(CHAR* filename, UINT32 cfgCrc, BOOL hasWrapper);
void ParamMgr_ResetDefaultCfg ( PARAM_CFG_PTR pParamCfg );
void TriggerMgr_ResetDefaultCfg ( TRIG_CFG_PTR pTrigCfg );
void AcmfRpt_ResetDefaultCfg(ACMF_CFG_PTR pParamCfg);
void AdrfRpt_ResetDefaultCfg(ADRF_CFG_PTR pParamCfg);
void ShowOffsets(void);

CFGMGR_NVRAM* CfgMgr_RuntimeConfigPtr(void);

//-----------------------------------------------------------------------------
// Local Variables
//-----------------------------------------------------------------------------
// create the default Cfg structure to be filled in by the ASCCI Cfg file
CFGMGR_NVRAM binFileCoreA;  // holds the binary cfg image for Channel A
CFGMGR_NVRAM binFileCoreB;  // holds the binary cfg image for Channel B
BOOL copyAtoB;              // should we copy chan A image to chan B image
BOOL processChannelA;       // Which buffer is actively being filled
BYTE crcData[CRC_BUFFER_SIZE]; // CRC calc buffer

// Do this here as for some reason the Default NVM structure confuses WholeTomato
#include "DefaultNvm.h"

///////////////////////////////////////////////////////////////////////////////////////////////
// Usage: GenBinCfg <xml> <asciiCfg> [<outputName>]
//
int _tmain(int argc, _TCHAR* argv[])
{
    int    progStatus;    // 1 valid File, 0 invalid CRC, -1 does not exist
    BOOL   hasWrapper;
    UINT32 result;
    UINT32 readCount;
    UINT32 binCrc;
    UINT32 cfgCrc;

    if ( argc == 2)
    {
        printf("\nGenerate Binary Cfg File %s\n\n", GBC_VERSION);
#ifdef _DEBUG
        ShowOffsets();
#endif
        Init();

        // assume we have a cfg file with only one channel's worth of data
        copyAtoB = TRUE;
        processChannelA = TRUE;

        progStatus = AsciiFileHasCrc(argv[1]);
        if (progStatus == 0)
        {
            ComputeCrc(argv[1], &cfgCrc, 0);
            progStatus = 1;
            hasWrapper = FALSE;
        }
        else if (progStatus == 1)
        {
            hasWrapper = TRUE;
            if (ComputeCrc(argv[1], &cfgCrc, 4))
            {
                progStatus = 1;
            }
            else
            {
                progStatus = 0;
            }
        }

        if (progStatus == 1)
        {
            result = ProcessCfgFile(argv[1], cfgCrc, hasWrapper);

            if (result == 0)
            {
                printf("\nA Binary Cfg File has been generated called:\n");
                printf("  <%s.bcf>.\n", argv[1]);
                printf("\nThis Binary Cfg File is only compatible with [v%s] ADRF code.\n\n", 
                    PRODUCT_VER_VALUE);
                printf("If this Binary Cfg file is to be loaded into a different ADRF code\n");
                printf("release please confirm the following items have not been altered as\n");
                printf("the conversion program has its own implementation of these functions\n");
                printf("and data to facilitate the conversion:\n\n");
                printf("  1. Any change to CFGMGR_NVRAM structure or DefaultNVCfg content\n");
                printf("  2. ParamMgr_ResetDefaultCfg - no code changes to this function\n");
                printf("  3. AcmfRpt_ResetDefaultCfg - no code changes to this function\n");
                printf("  4. AdrfRpt_ResetDefaultCfg - no code changes to this function\n");
            }
            else
            {
                printf("\n*** FAILED CONVERSION ***\n");
                progStatus = -3;
            }
        }
        else if (progStatus == 0)
        {

            printf("\nCFG File CRC Error: <%s>\n", argv[1]);
            progStatus = -2;
        }
    }
    else
    {
        printf("\nUsage Error: GenBinCfg <asciiCfg>");
        printf("Specify the name of the ASCII cfg file to convert to binary");
        progStatus = -1;
    }

#if _DEBUG
    printf("Hit any key to end ... exit status(%d)", progStatus);
    getchar();
#endif
    return progStatus;
}

//--------------------------------------------------------------------------------------------------
// Determine if the ASCII file is wrapped with TEXT..CRC
// return 1 when wrapped with TEXT..CRC, 
//        0 if not wrapped 
//       -1 on file open error
//
int AsciiFileHasCrc( CHAR* filename )
{
    int status = 0;
    char isText[4];
    FILE* f;

    f = fopen(filename, "rb");
    if (f != NULL)
    {
        fread(isText, 1, 4, f);
        fclose(f);

        Supper(isText);
        if (strncmp(isText, "TEXT", 4) == 0)
        {
            status = 1;
        }
        else
        {
            status = 0;
        }
    }
    else
    {
        printf("\n*** Failed to open file for read:\n<%s>\n", filename);
        status = -1;
    }

    return status;
}

//--------------------------------------------------------------------------------------------------
// Initialize the Binary Cfg File Generator
void Init(void)
{
    // clear the entire CFG area
    memset(&binFileCoreA, 0, sizeof(binFileCoreA));
    memset(&binFileCoreB, 0, sizeof(binFileCoreB));

    memcpy(&binFileCoreA, &DefaultNVCfg, sizeof(binFileCoreA));
    memcpy(&binFileCoreB, &DefaultNVCfg, sizeof(binFileCoreB));

    // Finalize A Initialization
    AdrfRpt_ResetDefaultCfg(binFileCoreA.adrfCfg);
    AcmfRpt_ResetDefaultCfg(binFileCoreA.acmfCfg);
    ParamMgr_ResetDefaultCfg ( &binFileCoreA.Param_Cfgs[0] );
    TriggerMgr_ResetDefaultCfg ( &binFileCoreA.Trig_Cfgs[0] );

    // Finalize B Initialization
    AdrfRpt_ResetDefaultCfg(binFileCoreB.adrfCfg);
    AcmfRpt_ResetDefaultCfg(binFileCoreB.acmfCfg);
    ParamMgr_ResetDefaultCfg ( &binFileCoreB.Param_Cfgs[0] );
    TriggerMgr_ResetDefaultCfg ( &binFileCoreB.Trig_Cfgs[0] );

    // Initialize the User Manager
    User_Init();

    // Add the user tables
    User_AddRootCmd(&AcmfReportRootTblPtr);
    User_AddRootCmd(&AdrfReportRootTblPtr);
    User_AddRootCmd(&cruiseRootTblPtr);
    User_AddRootCmd(&rootCycleMsg);
    User_AddRootCmd(&divergeRootTblPtr);
    User_AddRootCmd(&efastMgrRootTblPtr);
    User_AddRootCmd(&eventRootTblPtr);
    User_AddRootCmd(&paramRootTblPtr);
    User_AddRootCmd(&reportRootTblPtr);
    User_AddRootCmd(&timehistoryRootTblPtr);
    User_AddRootCmd(&trigRootTblPtr);
}

//-----------------------------------------------------------------------------------------------
// Process Cfg File
int ProcessCfgFile(CHAR* filename, UINT32 cfgCrc, BOOL hasWrapper)
{
    FILE* fi;  // the input ASCII CFG/XML file
    FILE* fo;  // the output file

    int cfgErrorCode;
    BOOL isEmpty;
    BOOL isComment;
    BOOL isChanLogic;
    CHAR lineBuffer[LINE_SIZE];
    CHAR saveLineBuffer[LINE_SIZE];
    CHAR cmdResponse[LINE_SIZE];
    UINT32 fileAt;
    UINT32 fileSize;
    UINT32 lineNumber;
    UINT32 binFileCrc;
    USER_MSG_RETCODE errCode;

    // open/process the cfg file (ASCII)
    fi = fopen(filename, "r");
    if (fseek(fi, 0, SEEK_END) == 0)
    {
        fileSize = ftell(fi);
        fclose(fi);
        fi = fopen(filename, "r");

        if (hasWrapper)
        {
            fileSize -= 4;
            fseek(fi, 4, SEEK_CUR);
        }
    }

    cfgErrorCode = 0;
    if (fi != NULL)
    {
        lineNumber = 0;
        while ( ftell(fi) != fileSize)
        {
            if ( fgets (lineBuffer , LINE_SIZE , fi) == NULL )
                break;
            lineNumber += 1;
            fileAt = ftell(fi);
            // --- do cmd line filtering ---
            // Remove comments, start with "//"
            strcpy(saveLineBuffer, lineBuffer);
            StripString(lineBuffer);

            isEmpty = strlen(lineBuffer) == 0;
            isComment = lineBuffer[0] == '/' && lineBuffer[1] == '/';
            isChanLogic = ChannelLogic(lineBuffer);

#ifdef _DEBUG
            if (lineNumber == 8848)
            {
                lineNumber = lineNumber;
            }
#endif
            if (!isEmpty && !isComment && !isChanLogic)
            {
                User_ExecuteCmdMsg(lineBuffer, 0, 0, &errCode, cmdResponse, LINE_SIZE);
                if (errCode != USER_MSG_OK)
                {
                    printf("\n*** Config File error on line: %d\n", lineNumber);
                    printf("Line: %s", saveLineBuffer);
                    printf("%s", outputBuffer);
                    cfgErrorCode += 1;                                     
                }
                outputBuffer[0] = '\0';
            }
            insertAt = 0;
        }
#ifdef _DEBUG
        printf("File size: %d at %d\n", fileSize, ftell(fi));
#endif
        fclose (fi);

        if (cfgErrorCode == 0)
        {
            // Set CRC for default cfg
            CRC32((const void*)&binFileCoreA,
                (sizeof(binFileCoreA)-BINCFG_END_ADJ),
                &binFileCoreA.checksum,
                CRC_FUNC_SINGLE);

            // Fill in the CRCs & correct endianess
            binFileCoreA.checksum = hton4(binFileCoreA.checksum);
            binFileCoreA.checksumXML = hton4(0);
            binFileCoreA.checksumCombined = hton4(cfgCrc);

            if (copyAtoB)
            {
                memcpy(&binFileCoreB, &binFileCoreA, sizeof(binFileCoreB));
            }
            else
            {
                // Set CRC for default cfg
                CRC32((const void*)&binFileCoreB,
                    (sizeof(binFileCoreB)-BINCFG_END_ADJ),
                    &binFileCoreB.checksum,
                    CRC_FUNC_SINGLE);

                // Fill in the CRCs & correct endianess
                binFileCoreB.checksum = hton4(binFileCoreB.checksum);
                binFileCoreB.checksumXML = hton4(0);
                binFileCoreB.checksumCombined = hton4(cfgCrc);
            }

            // Write out each of the channels config.bin files
            fo = fopen("configA.bin", "wb");
            fwrite(&binFileCoreA, sizeof(binFileCoreA), 1, fo);
            fclose(fo);

            fo = fopen("configB.bin", "wb");
            fwrite(&binFileCoreB, sizeof(binFileCoreB), 1, fo);
            fclose(fo);

            // write the binary file out
            sprintf(lineBuffer, "%s.bcf", filename);

            fo = fopen(lineBuffer, "wb");
            if (fo != NULL)
            {
                // BIN_ Identifier
                fwrite(BIN_ID, 1, strlen(BIN_ID), fo);

                // Channel A
                fwrite(START_A, 1, strlen(START_A), fo);
                fwrite(&binFileCoreA, sizeof(binFileCoreA), 1, fo);
                fwrite(END_A, 1, strlen(END_A), fo);

                // Channel B
                fwrite(START_B, 1, strlen(START_B), fo);
                fwrite(&binFileCoreB, sizeof(binFileCoreB), 1, fo);
                fwrite(END_B, 1, strlen(END_B), fo);

                fclose(fo);

                // now compute the CRC for the entire file and append it.
                ComputeCrc(lineBuffer, &binFileCrc, 0);
                fo = fopen(lineBuffer, "ab");
                if (fo != NULL)
                {
                    binFileCrc = hton4(binFileCrc);
                    fwrite(&binFileCrc, 1, sizeof(binFileCrc), fo);
                    fclose(fo);
                }
                else
                {
                    printf("\nFailed to append CRC to binary file: %s", lineBuffer);
                    return -103;
                }

                return 0;
            }
            else
            {
                printf("\nFailed to open file for write: %s", lineBuffer);
                return -102;
            }
        }
        else
        {
            printf("\n*** %d error%s occurred during the conversion.\n", 
                cfgErrorCode, cfgErrorCode > 1 ? "s" : "");
            printf("No binary configuration file was generated.\n");
            return -101;
        }
    }
    else
    {
        // we should never get here because this is caught in AsciiFileHasCrc
        printf("\nFailed to open file for read: %s", filename);
        return -100;
    }
}

//-----------------------------------------------------------------------------------------------
// Which binary channel image buffer is active
CFGMGR_NVRAM* CfgMgr_RuntimeConfigPtr(void)
{
    if (processChannelA)
    {
        return &binFileCoreA;
    }
    else
    {
        return &binFileCoreB;
    }
}

//-----------------------------------------------------------------------------------------------
// Decide if the file has multiple channel's defined, and that it fully implemented the
// start/end commands for all channels, otherwise reject the file.
BOOL ChannelLogic(CHAR* gseCmd)
{
    if (strcasecmp(gseCmd, START_A) == 0)
    {
        processChannelA = TRUE;
        return TRUE;
    }
    else if (strcasecmp(gseCmd, END_A) == 0)
    {
        return TRUE;
    }
    else if (strcasecmp(gseCmd, START_B) == 0)
    {
        processChannelA = FALSE;
        copyAtoB = FALSE;
        return TRUE;
    }
    else if (strcasecmp(gseCmd, END_B) == 0)
    {
        return TRUE;
    }

    return FALSE;
}

//-----------------------------------------------------------------------------------------------
// Compute the CRC for a file and verify it matches the last four bytes in the file.  Read in
// binary to ensure no CR/LF translation occurs.
// Return True if the computed CRC matches the file CRC, otherwise False.
//
BOOL ComputeCrc(CHAR* filename, UINT32* crc, UINT32 exclude)
{
    UINT32 crcRemaining;  // number of bytes remaining in the CRC
    UINT32 readCount;
    UINT32 fileSize;
    UINT32 fileCrc;
    BOOL result = FALSE;
    CRC_FUNC mode = CRC_FUNC_START;
    FILE* f = fopen(filename, "rb");

    if (f != NULL)
    {
        // determine the file size
        if (fseek(f, 0, SEEK_END) == 0)
        {
            fileSize = ftell(f);
            if (fseek(f, 0, SEEK_SET) == 0)
            {
                if ( fileSize < CRC_BUFFER_SIZE)
                {
                    crcRemaining = fileSize - exclude;
                    readCount = fread(crcData, 1, CRC_BUFFER_SIZE, f);
                    if (readCount == fileSize)
                    {
                        CRC32(crcData, crcRemaining, crc, CRC_FUNC_SINGLE);
                    }

                    if (exclude > 0)
                    {
                        memcpy(&fileCrc, &crcData[crcRemaining], exclude);
                    }
                    else
                    {
                        fileCrc = hton4(*crc);
                    }
                }
                else
                {
                    // this will take us several reads
                    crcRemaining = fileSize - exclude;
                    readCount = fread(crcData, 1, CRC_BUFFER_SIZE, f);
                    crcRemaining -= readCount;

                    CRC32(crcData, CRC_BUFFER_SIZE, crc, CRC_FUNC_START);
                    while (crcRemaining > 0)
                    {
                        readCount = fread(crcData, 1, min(crcRemaining, CRC_BUFFER_SIZE), f);
                        crcRemaining -= readCount;
                        if (crcRemaining == 0)
                        {
                            CRC32(crcData, readCount, crc, CRC_FUNC_END);
                        }
                        else
                        {
                            CRC32(crcData, readCount, crc, CRC_FUNC_NEXT);
                        }
                    }

                    if (exclude > 0)
                    {
                        fread(&fileCrc, 1, exclude, f);
                    }
                    else
                    {
                        fileCrc = hton4(*crc);
                    }
                }
            }
        }

        result = (hton4(fileCrc) == *crc);
        fclose(f);
    }

    return result;
}

//---------------------------------------------------------------------------------------------
void ParamMgr_ResetDefaultCfg ( PARAM_CFG_PTR pParamCfg )
{
    UINT16 i;

    for (i = 0; i < MAX_PARAMS; i++)
    {
        //*pParamCfg = (PARAM_CFG){PARAM_MGR_CFG_DEFAULT};
        memcpy(pParamCfg, &paramDefault, sizeof(paramDefault));
        pParamCfg->dbID = hton4(i);
        pParamCfg++;
    }
}

//---------------------------------------------------------------------------------------------
void TriggerMgr_ResetDefaultCfg ( TRIG_CFG_PTR pTrigCfg )
{
    UINT16 i;

    for (i = 0; i < MAX_TRIGGERS; i++)
    {
        //*pTrigCfg = (TRIG_CFG) {TRIG_MGR_CFG_DEFAULT};
        memcpy (pTrigCfg, &trigDefault, sizeof(trigDefault));
        pTrigCfg++;
    }
}

//---------------------------------------------------------------------------------------------
void AcmfRpt_ResetDefaultCfg(ACMF_CFG_PTR pParamCfg)
{
    UINT16 i;

    for (i = 0; i < MAX_ACMF_REPORT; ++i)
    {
        pParamCfg->userRptNum = hton2(i);
        pParamCfg++;
    }
}

//---------------------------------------------------------------------------------------------
void AdrfRpt_ResetDefaultCfg(ADRF_CFG_PTR pParamCfg)
{
    UINT16 i;

    for (i = 0; i < MAX_ADRF_REPORT; ++i)
    {
        pParamCfg->userRptNum = hton2(i);
        pParamCfg++;
    }
}

//---------------------------------------------------------------------------------------------
void ShowOffsets(void)
{
#define offsetof(a,b) ((int)(&(((a*)(0))->b)))

    printf("TH_CFG              : 0x%08x\n",   offsetof(CFGMGR_NVRAM, TH_Cfg));
    printf("REPORT_CFG          : 0x%08x\n",   offsetof(CFGMGR_NVRAM, reportCfg));
    printf("ADRF_CFGS           : 0x%08x\n",   offsetof(CFGMGR_NVRAM, adrfCfg));
    printf("ADRF_TRIG_CFG       : 0x%08x\n",   offsetof(CFGMGR_NVRAM, adrfTrigCfg));
    printf("ADRF_TRIG_HIST_CFG  : 0x%08x\n",   offsetof(CFGMGR_NVRAM, adrfTrigHistCfg));
    printf("ACMF_CFGS           : 0x%08x\n",   offsetof(CFGMGR_NVRAM, acmfCfg));
    printf("ACMF_TRIG_CFG       : 0x%08x\n",   offsetof(CFGMGR_NVRAM, acmfTrigCfg));
    printf("ACMF_TRIG_HIST_CFG  : 0x%08x\n",   offsetof(CFGMGR_NVRAM, acmfTrigHistCfg));
    printf("EVENT_CFGS          : 0x%08x\n",   offsetof(CFGMGR_NVRAM, eventCfg));
    printf("CRUISE_CFGS         : 0x%08x\n",   offsetof(CFGMGR_NVRAM, cruiseCfg));
    printf("EFAST_MGR_CFG       : 0x%08x\n",   offsetof(CFGMGR_NVRAM, efastMgrCfg));
    printf("CYCLE_CFGS          : 0x%08x\n",   offsetof(CFGMGR_NVRAM, Cycle_Cfgs));
    printf("DIVERGE_CFGS        : 0x%08x\n",   offsetof(CFGMGR_NVRAM, divergeCfg));
    printf("PARAM_CFGS          : 0x%08x\n",   offsetof(CFGMGR_NVRAM, Param_Cfgs));
    printf("TRIG_CFGS           : 0x%08x\n",   offsetof(CFGMGR_NVRAM, Trig_Cfgs));
}

//-----------------------------------
FLOAT32 ReverseFloat(FLOAT32 A)
{
    FLOAT32 fTemp = A;
    UINT32* temp = (UINT32*)&fTemp;

    *temp = hton4(*temp);

    return fTemp;
}
