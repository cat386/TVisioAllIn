/*------------------------------------------------------------*/
/* файл — sfilcoll.cpp */
/*                                                            */
/* Объект регистрации класса TFileCollection. */
/*------------------------------------------------------------*/
/*
 *      Turbo Vision - Version 2.0
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 */

#define Uses_TFileCollection
#define Uses_TStreamableClass
#include <tvision/tv.h>

TStreamableClass RFileCollection( TFileCollection::name,
                                  TFileCollection::build,
                                  __DELTA(TFileCollection)
                                );

