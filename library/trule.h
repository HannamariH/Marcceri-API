/*
 *  USEMARCON Software - Command Line version
 *
 *  File:  trule.h
 *
 *
 *  NOTE:  IN NO WAY WHATSOEVER SHOULD THIS FILE BE USED IN THE EARLIER
 *         VERSIONS OF USEMARCON SOFTWARE.
 *

CLASS
    TRule

    implements a class to manage Rules

OVERVIEW TEXT
    USEMARCON Software - Command Line version
    Copyright The British Library, The USEMarcon Consortium, 1995-2000
    Adapted by Crossnet Systems Limited - British Library Contract No. BSDS 851
    Adapted by ATP Library Systems Ltd, Finland, 2002-2003
    Adapted by The National Library of Finland, 2004-2006

AUTHOR
    Crossnet Systems Limited
    ATP Library Systems Limited
    Helsinki University Library, the National Library of Finland

*/
#ifndef _TRULE_H_
#define _TRULE_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "tcd.h"
#include "umrecord.h"

class TRule
{
public:
    TRule           (TCD *LastInputCD, TCD *LastOutputCD, TError *ErrorHandler);
    TRule           (TRule *aRule);
    virtual ~TRule  ();

    int   FromString      (char *aString);
    int   ToString        (char *aString);
    void  ResetLib        (void)              { itsLib.freestr(); };

    inline TCD   *GetInputCD     (void)              { return itsInputCD; };
    inline TCD   *GetOutputCD    (void)              { return itsOutputCD; };
    inline char  *GetLib         (void)              { return itsLib.str(); };
    inline TRule *GetNextRule    (void)              { return itsNextRule; };
    inline TRule *GetPreviousRule(void)              { return itsPreviousRule; };

    int   SetInputCD      (TCD* aCD)          { itsInputCD=aCD; return 0; };
    int   SetOutputCD     (TCD *aCD)          { itsOutputCD=aCD; return 0; };
    int   SetLib          (const char *aLib);
    int   SetNextRule     (TRule *NextRule)   { itsNextRule=NextRule; return 0; };
    int   SetPreviousRule (TRule *PreviousRule)   { itsPreviousRule=PreviousRule; return 0; };

protected:
    TCD           *itsInputCD;
    TCD           *itsOutputCD;
    TCD           *itsLastInputCD;
    TCD           *itsLastOutputCD;
    typestr       itsLib;
    TRule         *itsNextRule;
    TRule         *itsPreviousRule;

    TError        *itsErrorHandler;
};

#endif
