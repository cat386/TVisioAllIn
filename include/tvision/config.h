/* ------------------------------------------------------------------------*/
/*                                                                         */
/*   CONFIG.H                                                              */
/*                                                                         */
/* Прочие общесистемные параметры конфигурации. */
/* ТОЛЬКО ДЛЯ ВНУТРЕННЕГО ИСПОЛЬЗОВАНИЯ. */
/*                                                                         */
/* ------------------------------------------------------------------------*/
/*
 *      Turbo Vision - Version 2.0
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 */

#if !defined( __CONFIG_H )
#define __CONFIG_H

#if !defined( __LIMITS_H )
#include <limits.h>
#endif  // __LIMITS_H

const int eventQSize = 16;
const int maxCollectionSize = (int)(( (unsigned long) UINT_MAX - 16)/sizeof( void * ));

#if !defined( __FLAT__ )
const int maxViewWidth = 132;
#endif

const int maxFindStrLen    = 80;
const int maxReplaceStrLen = 80;

const int minPasteEventCount = 3;

#if !defined( __BORLANDC__ )
const int maxCharSize = 4; // Один символ в UTF-8 занимает до четырёх байт.
#else
const int maxCharSize = 1; // Все символы кодируются одним байтом.
#endif

#endif  // __CONFIG_H
