/*------------------------------------------------------------*/
/* файл — nmstddlg.cpp */
/*                                                            */
/* Определяет имена сериализуемых классов: */
/* TFileInputLine, TSortedListBox, TFileInfoPane. */
/*------------------------------------------------------------*/
/*
 *      Turbo Vision - Version 2.0
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 */

#if !defined(NO_STREAMABLE)
#define Uses_TFileInputLine
#define Uses_TSortedListBox
#define Uses_TFileInfoPane
#include <tvision/tv.h>

const char * const _NEAR TFileInputLine::name = "TFileInputLine";
const char * const _NEAR TSortedListBox::name = "TSortedListBox";
const char * const _NEAR TFileInfoPane::name = "TFileInfoPane";
#endif

