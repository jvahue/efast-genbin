// GenBinCfg.cpp : Defines the entry point for the console application.
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
#include "EfastMgr.h"
#include "EventMgr.h"
#include "ParamMgr.h"
#include "ReportMgr.h"
#include "TimeHistoryMgr.h"
#include "TriggerMgr.h"

//#include "AdrfStub.h"
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
#define BIN_ID "BIN_"
#define START_A "Channel_A_Begin"
#define END_A   "Channel_A_End"
#define START_B "Channel_B_Begin"
#define END_B   "Channel_A_End"
#define BINCFG_END_ADJ  12  // Exclude the 3 CRCs at the End of CFGMGR_NVRAM
#define LINE_SIZE 512
#define CRC_BUFFER_SIZE (500*1024)

#define htonl(A) ((((UINT32)(A) & 0xff000000) >> 24) | \
                  (((UINT32)(A) & 0x00ff0000) >> 8) | \
                  (((UINT32)(A) & 0x0000ff00) << 8) | \
                  (((UINT32)(A) & 0x000000ff) << 24))
//-----------------------------------------------------------------------------
// Local Variables
//-----------------------------------------------------------------------------
static const CFGMGR_NVRAM DefaultNVCfg = {

    "EFAST DEFAULT",     // Id
    0,                   // VerId

    // Time History default data
    {TH_MGR_CFG_DEFAULT},

    // Report Manager default data
    {REPORTMGR_CFG_DEFAULT},

    // ADRF Report default data
    {ADRF_RPT_CFG_DEFAULT},
    {ADRF_TRIG_CFG_DEFAULT},
    {ADRF_TRIG_HIST_CFG_DEFAULT},

    // ACMF Report default data
    {ACMF_RPT_CFG_DEFAULT},
    {ACMF_TRIG_CFG_DEFAULT},
    {ACMF_TRIG_HIST_CFG_DEFAULT},

    // Event default data
    {EVENT_CFG_DEFAULT},

    // Event default data
    {CRUISE_CFG_DEFAULT},

    // Efast Mgr default data
    {EFAST_MGR_CFG_DEFAULT},

    // Cycle default data
    {CYCLE_CFG_DEFAULT},

    // ACMF Divergence Configuration
    {DIVERGE_CFG_DEFAULT},

    // Param default data - Leave blank here.  Update of this field is coded.
    // Trig default data - Leave blank here.  Update of this field is coded.
};

// create the default Cfg structure to be filled in by the ASCCI Cfg file
CFGMGR_NVRAM binFileCoreA;  // holds the binary cfg image for Channel A
CFGMGR_NVRAM binFileCoreB;  // holds the binary cfg image for Channel B
BOOL copyAtoB;              // should we copy chan A image to chan B image
BOOL processChannelA;       // Which buffer is actively being filled
BYTE crcData[CRC_BUFFER_SIZE]; // CRC calc buffer

//-----------------------------------------------------------------------------
// Local Functions
//-----------------------------------------------------------------------------
void Init(void);
BOOL ChannelLogic(CHAR* gseCmd);
BOOL ComputeCrc( CHAR* filename, UINT32* crc);
int ProcessCfgFile(CHAR* filename, CHAR* outName, int argc, UINT32 xmlCrc, UINT32 cfgCrc);

CFGMGR_NVRAM* CfgMgr_RuntimeConfigPtr(void);

////////////////////////////////////////////////////////////////////////////////////////////////////////
// Usage: GenBinCfg <xml> <asciiCfg> [<outputName>]
//
int _tmain(int argc, _TCHAR* argv[])
{
    UINT32 readCount;
    UINT32 xmlCrc;
    UINT32 binCrc;
    UINT32 cfgCrc;
    BOOL xmlCrcValid;
    BOOL cfgCrcValid;
    BOOL isXml;

    if ( argc == 3 || argc == 4)
    {
        Init();

        // assume we have a cfg file with only one channel's worth of data
        copyAtoB = TRUE;
        processChannelA = TRUE;

        // make sure param1 is an xml file, and check both files CRCs
        xmlCrcValid = FALSE;
        cfgCrcValid = FALSE;
        Supper(argv[1]);
        isXml = strstr(argv[1], ".XML") != NULL;
        if (isXml)
        {
            if( ComputeCrc( argv[1], &xmlCrc))
            {
                xmlCrcValid = TRUE;
                if (ComputeCrc(argv[2], &cfgCrc))
                {
                    cfgCrcValid = TRUE;
                }
            }
        }

        if (cfgCrcValid)
        {
            return ProcessCfgFile(argv[2], argv[1], argc, xmlCrc, cfgCrc);
        }
        else
        {
            if (!isXml)
            {
                printf("Usage Error: GenBinCfg <xml> <asciiCfg> [<outputName>]\n");
            }
            if (!xmlCrcValid)
            {
                printf("XML File CRC Error: %s\n", argv[1]);
            }
            if (!cfgCrcValid)
            {
                printf("CFG File CRC Error: %s\n", argv[2]);
            }
            return -2;
        }
    }
    else
    {
        printf("Usage Error: GenBinCfg <xml> <asciiCfg> [<outputName>]");
        return -1;
    }
}

//-----------------------------------------------------------------------------------------------
// Initialize the Bianry Cfg File Generator
void Init(void)
{
    memcpy(&binFileCoreA, &DefaultNVCfg, sizeof(binFileCoreA));
    memcpy(&binFileCoreB, &DefaultNVCfg, sizeof(binFileCoreB));

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
int ProcessCfgFile(CHAR* filename, CHAR* outName, int argc, UINT32 xmlCrc, UINT32 cfgCrc)
{
    FILE* fi;  // the input ASCII CFG/XML file
    FILE* fo;  // the output file
    BOOL isEmpty;
    BOOL isComment;
    BOOL isChanLogic;
    CHAR lineBuffer[LINE_SIZE];
    CHAR cmdResponse[LINE_SIZE];
    USER_MSG_RETCODE errCode;

    // open/process the cfg file (ascii)
    fi = fopen(filename, "r");
    if (fi != NULL)
    {
        while ( !feof(fi) )
        {
            if ( fgets (lineBuffer , LINE_SIZE , fi) == NULL )
                break;
            // --- do cmd line filtering ---
            // Remove comments, start with "//"
            StripString(lineBuffer);

            isEmpty = strlen(lineBuffer) == 0;
            isComment = lineBuffer[0] == '/' && lineBuffer[1] == '/';
            isChanLogic = ChannelLogic(lineBuffer);

            if (!isEmpty && !isComment && !isChanLogic)
            {
                User_ExecuteCmdMsg(lineBuffer, 0, 0, &errCode, cmdResponse, LINE_SIZE);
            }
            insertAt = 0;
        }
        fclose (fi);

        // Fill in the CRCs
        binFileCoreA.checksumXML = xmlCrc;
        binFileCoreA.checksumCombined = cfgCrc;

        // Set CRC for default cfg
        CRC32((const void*)&binFileCoreA,
            (sizeof(binFileCoreA)-BINCFG_END_ADJ),
            &binFileCoreA.checksum,
            CRC_FUNC_SINGLE);

        if (copyAtoB)
        {
            memcpy(&binFileCoreB, &binFileCoreA, sizeof(binFileCoreB));
        }

        // Write out each of the channels config.bin files
        fo = fopen("configA.bin", "wb");
        fwrite(&binFileCoreA, sizeof(binFileCoreA), 1, fo);
        fclose(fo);

        fo = fopen("configB.bin", "wb");
        fwrite(&binFileCoreB, sizeof(binFileCoreB), 1, fo);
        fclose(fo);

        // write the binary file out - did they give us a name?
        if (argc == 4)
        {
            strcpy(lineBuffer, outName);
        }
        else
        {
            sprintf(lineBuffer, "%s.bcf", filename);
        }

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

            return 0;
        }
        else
        {
            printf("Failed to open file for write: %s", lineBuffer);
            return -101;
        }
    }
    else
    {
        printf("Failed to open file for read: %s", filename);
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
BOOL ComputeCrc( CHAR* filename, UINT32* crc)
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
                    crcRemaining = fileSize - 4;
                    readCount = fread(crcData, 1, CRC_BUFFER_SIZE, f);
                    if (readCount == fileSize)
                    {
                        CRC32(crcData, crcRemaining, crc, CRC_FUNC_SINGLE);
                    }

                    memcpy(&fileCrc, &crcData[crcRemaining], 4);
                }
                else
                {
                    // this will take us several reads
                    crcRemaining = fileSize - 4;
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

                    fread(&fileCrc, 1, 4, f);
                }
            }
        }

        result = (htonl(fileCrc) == *crc);
        fclose(f);
    }

    return result;
}