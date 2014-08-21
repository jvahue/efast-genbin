#ifndef ALT_STDTYPES_H
#define ALT_STDTYPES_H

/******************************************************************************
            Copyright (C) 2012-2014 Pratt & Whitney Engine Services, Inc.
               All Rights Reserved. Proprietary and Confidential.

    File:         alt_stdtypes.h

    Description:  This file provides the standard data types for all PWES
    Software systems.

    VERSION
    $Revision: 8 $  $Date: 7/14/14 2:37p $

******************************************************************************/

/*****************************************************************************/
/* Compiler Specific Includes                                                */
/*****************************************************************************/
#include <limits.h>

/*****************************************************************************/
/* Software Specific Includes                                                */
/*****************************************************************************/

/******************************************************************************
                                 Package Defines
******************************************************************************/
#undef FALSE
#undef TRUE
#define FALSE 0
#define TRUE  1


#ifndef ULONG_MAX
  #define SHRT_MIN      (-32767-1)        /* minimum signed   short value */
  #define SHRT_MAX        32767           /* maximum signed   short value */
  #define LONG_MIN      (-2147483647L-1)  /* minimum signed   long value */
  #define LONG_MAX        2147483647L     /* maximum signed   long value */
  #define UCHAR_MAX       255             /* maximum unsigned char value */
  #define USHRT_MAX       65535U          /* maximum unsigned short value */
  #define ULONG_MAX       4294967295UL    /* maximum unsigned long value */
#endif

#define UINT8_MAX          UCHAR_MAX
#define INT8_MIN           SCHAR_MIN
#define INT8_MAX           SCHAR_MAX
#define UINT16_MAX         USHRT_MAX
#define INT16_MIN          SHRT_MIN
#define INT16_MAX          SHRT_MAX
#define UINT32_MAX         ULONG_MAX
#define INT32_MIN          LONG_MIN
#define INT32_MAX          LONG_MAX

/******************************************************************************
                                Package Typedefs
******************************************************************************/

/******************************************************************************
                                   Numeric typedefs

  The following standard types are allowed.

  BOOLEAN : 0 .. 1, FALSE, TRUE

  INT8    : -128 .. 127
  INT16   : -32768 .. 32767
  INT32   : -2147483648 .. 2147483647

  UINT8   : 0 .. 255
  UINT16  : 0 .. 65535
  UINT32  : 0 .. 4294967296

  FLOAT32 : 24 bit mantissa, 8 bit exponent  (six significant digits)
  FLOAT64 : 53 bit mantissa, 10 bit exponent (15  significant digits)

  If the compiler does not support the type it should not be defined thus allowing compiler
  to identify where changes to the code are required.

  For a specific embedded program the else portion of the conditional compilation should be
  defined, this allows projects source code to be run/debugged on a PC and the target if
  needed.

******************************************************************************/
typedef char               CHAR;
typedef unsigned char      BYTE;
typedef unsigned char      UINT8;
typedef char               SINT8;
typedef BYTE               BOOLEAN;
typedef BYTE               BOOL;

typedef short int          INT16;
typedef unsigned short int UINT16;
typedef signed short       SINT16;

typedef long               INT32;
typedef unsigned long      UINT32;
typedef signed long        SINT32;
typedef unsigned long      DWORD;

typedef float              FLOAT32;
typedef double             FLOAT64;


//#define strtok_r(a,b,c) strtok(a,b)
#define snprintf      _snprintf
//#define strcasecmp    strcmpi
//#define strtod        g4eStrtod

typedef signed char   INT8;


#include "alt_Time.h"

/******************************************************************************
                                 Package Exports
******************************************************************************/
#undef EXPORT

#if defined ( ALT_STDTYPES_BODY )
  #define EXPORT
#else
  #define EXPORT extern
#endif

/******************************************************************************
                             Package Exports Variables
******************************************************************************/

/******************************************************************************
                             Package Exports Functions
******************************************************************************/

#endif // ALT_STDTYPES_H

