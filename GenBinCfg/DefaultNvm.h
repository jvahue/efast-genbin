#ifndef DefaultNvm_h
#define DefaultNvm_h

static const PARAM_CFG paramDefault = 
{
    PARAM_MGR_CFG_DEFAULT
};

static const TRIG_CFG trigDefault = 
{
    TRIG_MGR_CFG_DEFAULT
};

static const CFGMGR_NVRAM DefaultNVCfg = {

    "EFAST DEFAULT",     // Id
    hton2(0),            // VerId

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


#endif