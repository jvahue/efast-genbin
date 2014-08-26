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
BOOL ChannelLogic(CHAR* gseCmd);
BOOL ComputeCrc( CHAR* filename, UINT32* crc, UINT32 exclude);
void ProcessXmlFile(CHAR* filename);
int ProcessCfgFile(CHAR* filename, CHAR* outName, int argc, UINT32 xmlCrc, UINT32 cfgCrc);
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

static const XML_MAP xmlMap[XML_TAGS] = {
    {"<Aircraft_Type>",    "EFAST.CFG.AC.TYPE"},
    {"<Fleet_Ident>",      "EFAST.CFG.AC.FLEETIDENT"},
    {"<Aircraft_Number>",  "EFAST.CFG.AC.NUMBER"},
    {"<Tail_Number>",      "EFAST.CFG.AC.TAILNUMBER"},
    {"<Aircraft_Style>",   "EFAST.CFG.AC.STYLE"},
    {"<Airline_Operator>", "EFAST.CFG.AC.OPERATOR"},
    {"<Aircraft_Owner>",   "EFAST.CFG.AC.OWNER"},
    {"<Validate_Data>",    "EFAST.CFG.AC.VALIDATE"},
};

///////////////////////////////////////////////////////////////////////////////////////////////
// Usage: GenBinCfg <xml> <asciiCfg> [<outputName>]
//
int _tmain(int argc, _TCHAR* argv[])
{
    UINT32 result;
    UINT32 readCount;
    UINT32 xmlCrc;
    UINT32 binCrc;
    UINT32 cfgCrc;
    BOOL xmlCrcValid;
    BOOL cfgCrcValid;
    BOOL isXml;

    if ( argc == 3 || argc == 4)
    {
        ShowOffsets();
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
            if( ComputeCrc( argv[1], &xmlCrc, 4))
            {
                xmlCrcValid = TRUE;

                // send in any cfg info from the xml file
                ProcessXmlFile(argv[1]);

                if (ComputeCrc(argv[2], &cfgCrc, 4))
                {
                    cfgCrcValid = TRUE;
                }
            }
        }

        if (cfgCrcValid)
        {
            result = ProcessCfgFile(argv[2], argv[1], argc, xmlCrc, cfgCrc);

            if (result == 0)
            {
                printf("\nA Binary Cfg File has been generated.\n");
                printf("If this is a new ADRF code release please confirm the following\n");
                printf("items have not been altered as the conversion program has its\n");
                printf("own implementation of them to facilitate the conversion:\n");
                printf("  1. DefaultNVCfg - the definition is unchanged\n");
                printf("  2. ParamMgr_ResetDefaultCfg - no code changes to this function\n");
                printf("  3. AcmfRpt_ResetDefaultCfg - no code changes to this function\n");
                printf("  4. AdrfRpt_ResetDefaultCfg - no code changes to this function\n");
            }
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

//---------------------------------------------------------------------------------------------
void ProcessXmlFile(CHAR* filename)
{
    int insert;
    int x;
    FILE* fi;  // the input ASCII CFG/XML file
    CHAR* p;
    CHAR value[LINE_SIZE];
    CHAR cmdLine[LINE_SIZE];
    CHAR cmdResponse[LINE_SIZE];
    USER_MSG_RETCODE errCode;

    fi = fopen(filename, "r");
    if (fi != NULL)
    {
        if ( fread(crcData, 1, CRC_BUFFER_SIZE, fi) != NULL )
        {
            for (x = 0; x < XML_TAGS; ++x)
            {
                p = strstr(crcData, xmlMap[x].xmlTag);
                if (p != NULL)
                {
                    // we found a tag of interest - get its value
                    p += strlen(xmlMap[x].xmlTag);
                    insert = 0;
                    memset(value, 0, sizeof(value));
                    while (*p != '<' && insert < LINE_SIZE)
                    {
                        value[insert] = *p;
                        p++;
                        insert += 1;
                    }
                    
                    if (*p == '<')
                    {
                        sprintf(cmdLine, "%s=%s", xmlMap[x].cmd, value);
                        User_ExecuteCmdMsg(cmdLine, 0, 0, &errCode, cmdResponse, LINE_SIZE);
                        outputBuffer[0] = '\0';
                    }
                }
            }
        }
        fclose(fi);
    }
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
    UINT32 binFileCrc;
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
                outputBuffer[0] = '\0';
            }
            insertAt = 0;
        }
        fclose (fi);

        // Set CRC for default cfg
        CRC32((const void*)&binFileCoreA,
            (sizeof(binFileCoreA)-BINCFG_END_ADJ),
            &binFileCoreA.checksum,
            CRC_FUNC_SINGLE);

        // Fill in the CRCs & correct endianess
        binFileCoreA.checksum = hton4(binFileCoreA.checksum);
        binFileCoreA.checksumXML = hton4(xmlCrc);
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
            binFileCoreB.checksumXML = hton4(xmlCrc);
            binFileCoreB.checksumCombined = hton4(cfgCrc);
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
                printf("Failed to append CRC to binary file: %s", lineBuffer);
                return -102;
            }

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

//----------------
FLOAT32 ReverseFloat(FLOAT32 A)
{
    FLOAT32 fTemp = A;
    UINT32* temp = (UINT32*)&fTemp;

    *temp = hton4(*temp);

    return fTemp;
}