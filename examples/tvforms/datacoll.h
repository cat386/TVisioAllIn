/*-------------------------------------------------------*/
/*                                                       */
/* Демонстрация форм Turbo Vision. */
/*                                                       */
/* Datacoll.h — заголовочный файл для Datacoll.cpp */
/* Вспомогательный заголовочный файл примера TVFORMS. */
/*-------------------------------------------------------*/
/*
 *      Turbo Vision - Version 2.0
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 */

#if !defined( __DATACOLL_H )
#define __DATACOLL_H

#if defined( __GNUC__ )
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverloaded-virtual"
#endif

#define Uses_TStringCollection
#define Uses_TStreamable
#include <tvision/tv.h>

enum KeyTypes {stringKey, longIntKey};

// ВНИМАНИЕ: коллекция допускает только тривиальные типы. Для элементов
// используются malloc/free, чтобы при сборке Borland C++ обходить
// резервный пул памяти.

class TDataCollection : public TStringCollection
{

public:

    TDataCollection( short, short, int , KeyTypes );
    virtual int compare( void *, void * );
    virtual void error( int code );
    virtual void freeItem( void * );

protected:

    TDataCollection( StreamableInit) : TStringCollection( streamableInit ) {};
    virtual void write( opstream& );
    virtual void *read( ipstream& );

private:

    virtual const char *streamableName() const
        { return name; }
    virtual void *readItem( ipstream& );
    virtual void writeItem( void *, opstream& );

public:

    static const char * const name;
    unsigned int itemSize;
    KeyTypes  keyType;
    int status;
    static TStreamable *build();

};

inline ipstream& operator >> ( ipstream& is, TDataCollection& cl )
    { return is >> (TStreamable&)cl; }
inline ipstream& operator >> ( ipstream& is, TDataCollection*& cl )
    { return is >> (void *&)cl; }
inline opstream& operator << ( opstream& os, TDataCollection& cl )
    { return os << (TStreamable&)cl; }
inline opstream& operator << ( opstream& os, TDataCollection* cl )
    { return os << (TStreamable *)cl; }

#if defined( __GNUC__ )
#pragma GCC diagnostic pop
#endif

#endif  // __DATACOLL_H
