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
#define START_A "Start Channel A"
#define END_A   "End Channel A"
#define START_B "Start Channel B"
#define END_B   "End Channel B"


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
BOOL channelLogicValid;

BOOL ChannelLogic(CHAR* gseCmd);
CFGMGR_NVRAM* CfgMgr_RuntimeConfigPtr(void);

////////////////////////////////////////////////////////////////////////////////////////////////////////
// Usage: GenBinCfg <xml> <asciiCfg> [<outputName>]
//
int _tmain(int argc, _TCHAR* argv[])
{
#define LINE_SIZE 512

    FILE* fi;  // the input ASCII CFG/XML file
    FILE* fo;  // the output file
    UINT32 xmlCrc;
    UINT32 cfgCrc;
    UINT32 localCrc;
    BOOL isXml;
    BOOL isEmpty;
    BOOL isComment;
    BOOL isChanLogic;
    CHAR gseCmd[LINE_SIZE];
    CHAR cmdResponse[LINE_SIZE];
    USER_MSG_RETCODE errCode;

    if ( argc == 3 || argc == 4)
    {
        // assume we have a cfg file with only one channel's worth of data
        copyAtoB = TRUE;
        processChannelA = TRUE;
        channelLogicValid = TRUE;
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

        // open/process the xml file
        isXml = strstr(argv[1], ".xml") != NULL;

        // open/process the cfg file (ascii)
        fi = fopen(argv[2], "r");
        if (fi != NULL)
        {
            while ( !feof(fi) )
            {
                if ( fgets (gseCmd , LINE_SIZE , fi) == NULL )
                    break;
                // --- do cmd line filtering ---
                // Remove comments, start with "//"
                StripString(gseCmd);

                isEmpty = strlen(gseCmd) == 0;
                isComment = gseCmd[0] == '/' && gseCmd[1] == '/';
                isChanLogic = ChannelLogic(gseCmd);

                if (!isEmpty && !isComment && !isChanLogic)
                {
                    User_ExecuteCmdMsg(gseCmd, 0, 0, &errCode, cmdResponse, LINE_SIZE);
                }
                insertAt = 0;
            }
            fclose (fi);
        }

        // write the binary file out - did they give us a name?
        if (argc == 4)
        {
            strcpy(gseCmd, argv[3]);
        }
        else
        {
            sprintf(gseCmd, "%s.bcf", argv[2]);
        }

        fo = fopen(gseCmd, "wb");
        if (fo != NULL)
        {
            fwrite(BIN_ID, 1, strlen(BIN_ID), fo);
            fwrite(START_A, 1, strlen(START_A), fo);
            fwrite(&binFileCoreA, sizeof(binFileCoreA), 1, fo);
            if (!copyAtoB)
            {
                fwrite(START_A, 1, strlen(START_A), fo);
            }

            fclose(fo);

            return 0;
        }
        else
        {
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
    Supper(gseCmd);
    if (strcasecmp(gseCmd, "START_CHANNEL A") == 0)
    {

    }
    return FALSE;
}
