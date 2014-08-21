#ifndef AdrfStub_h
#define AdrfStub_h

// enums ---
typedef enum
{
    STREAM_IDLE,            // no processing
    STREAM_GETDATA,         // get stream data
    STREAM_FORMAT,          // format for output
    STREAM_CHECKSUM,        // compute checksum
    STREAM_OUT              // output stream
} STREAM_STATE;


// structures ---
typedef struct
{
    // config info
    REPORT_STREAM   streamType;                 // stream output type
    REPORT_RATE     streamRate;                 // stream output rate
    BOOLEAN         bRecordFile;                // record report file
    // control info
    STREAM_STATE    streamState;                // current stream state
    BOOLEAN         bStreamActive;              // stream output Active
    INT16           reportId;                   // report ID number (-1 is no ID for display)
    UINT16          streamTimerSet;             // timer set value
    UINT16          streamTimer;                // stream output timer
    BOOLEAN         bStreamCmdFromGSE;          // current stream active cmd from GSE
} ADRF_STREAM, *ADRF_STREAM_PTR;

#endif