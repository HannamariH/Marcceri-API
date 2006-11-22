/*
 *  USEMARCON Software - Command Line version
 *  Copyright The British Library, The USEMarcon Consortium, 1995-2000
 *  Adapted by Crossnet Systems Limited - British Library Contract No. BSDS 851
 *
 *  Adapted by ATP Library Systems Ltd, Finland, 2002-2003
 *
 *  File:  tagnoind.cpp
 *
 *  implements a class to manage MARC tags that do not have indicators
 *
 *  NOTE:  IN NO WAY WHATSOEVER SHOULD THIS FILE BE USED IN THE EARLIER
 *         VERSIONS OF USEMARCON SOFTWARE.
 *
 */

#include "tagnoind.h"

///////////////////////////////////////////////////////////////////////////////
//
// TTagNoInd
//
///////////////////////////////////////////////////////////////////////////////
TTagNoInd::TTagNoInd(void)
{
    *itsTag     = 0;
    itsNext     = NULL;
}

///////////////////////////////////////////////////////////////////////////////
//
// ~TTagNoInd
//
///////////////////////////////////////////////////////////////////////////////
TTagNoInd::~TTagNoInd()
{
}

