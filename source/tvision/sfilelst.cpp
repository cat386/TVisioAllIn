/*------------------------------------------------------------*/
/* файл — sfilelst.cpp */
/*                                                            */
/* Объект регистрации класса TFileList. */
/*------------------------------------------------------------*/
/*
 *      Turbo Vision - Version 2.0
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 */

#if !defined(NO_STREAMABLE)
#define Uses_TFileList
#define Uses_TStreamableClass
#include <tvision/tv.h>
__link( RSortedListBox )            // На случай переноса объекта ниже в другой файл.
                                    //

TStreamableClass RFileList( TFileList::name,
                            TFileList::build,
                            __DELTA(TFileList)
                          );

/* Регистрация TSortedListBox. */
__link( RListBox )

TStreamableClass RSortedListBox( TSortedListBox::name,
                                 TSortedListBox::build,
                                 __DELTA( TSortedListBox )
                               );
#endif

