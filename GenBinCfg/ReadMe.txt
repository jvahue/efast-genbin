========================================================================
    CONSOLE APPLICATION : GenBinCfg Project Overview
========================================================================
To build the GenBinCfg project after a code change

Define:
--------
GEN_CFG_BIN - done the the VS solution

Prebuild:
----------
Rename adrf/code/alt_stdtypes.h -> adrf/code/alt_stdtypesx.h

*** HOLD OFF ON THIS ***
Python scripts: (at least I'd write Python code for this)
----------------
Copy the following structures from the current target code file <.c> to
an adrf stub file called <AdrfStub.h>

Enums:
STREAM_STATE

Strucutres:
ADRF_STREAM
