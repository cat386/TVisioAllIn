/*------------------------------------------------------------*/
/* файл — nmview.cpp */
/*                                                            */
/* Определяет имя сериализуемого класса TView. */
/*------------------------------------------------------------*/
/*
 *      Turbo Vision - Version 2.0
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 */

#if !defined(NO_STREAMABLE)
#define Uses_TView
#include <tvision/tv.h>

const char * const _NEAR TView::name = "TView";
#endif
