/* ------------------------------------------------------------------------*/
/*                                                                         */
/*   TOBJSTRM.H                                                            */
/*                                                                         */
/* Определяет классы TStreamable, TStreamableClass, pstream, */
/* ipstream, opstream, iopstream, ifpstream, ofpstream и fpstream. */
/*                                                                         */
/* ------------------------------------------------------------------------*/
/*
 *      Turbo Vision - Version 2.0
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 */

typedef unsigned P_id_type;

/* ------------------------------------------------------------------------*/
/*                                                                         */
/* класс TStreamable */
/*                                                                         */
/* Базовый класс всех сохраняемых объектов. Предоставляет */
/* три метода: streamableName(), read() и write(), которые */
/* должны переопределяться в каждом производном классе. */
/*                                                                         */
/* ------------------------------------------------------------------------*/

#ifdef __DLL__
#define _FAR   far
#else
#define _FAR
#endif

#if defined( __BORLANDC__ )
#pragma warn -nst
#pragma option -Vo-
#endif
#if defined( __BCOPT__ ) && !defined (__FLAT__)
#pragma option -po-
#endif

#if !defined( __fLink_def )
#define __fLink_def

struct fLink
{
    fLink _NEAR *f;
    class TStreamableClass _NEAR *t;
    static class TStreamableClass * volatile forceLink;
};

#ifndef __COUNTER__

#define __link( s )             \
  extern TStreamableClass s;    \
  static fLink force ## s =     \
    { (fLink _NEAR *)&force ## s, (fLink::forceLink = &s, (TStreamableClass _NEAR *)&s) };

#else

// Используем макрос __COUNTER__, чтобы двойное подключение одного
// объекта не вызывало ошибку компиляции.

#define __link_declare( s, n )  \
  extern TStreamableClass s;    \
  static void * const force ## s ## n = ((void) force ## s ## n, fLink::forceLink = &s, nullptr);

#define __link_expand( s, ... ) __link_declare( s, __VA_ARGS__ )
#define __link( s ) __link_expand( s, __COUNTER__ )

#endif // __COUNTER__

#endif // __fLink_def

#if defined( Uses_TStreamable ) && !defined( __TStreamable )
#define __TStreamable

class TStreamable
{

    friend class pstream;
    friend class opstream;
    friend class ipstream;

private:

    virtual const char *streamableName() const = 0;

protected:

    virtual void *read( ipstream& ) = 0;
    virtual void write( opstream& ) = 0;

};

#endif  // Uses_TStreamable

/* ------------------------------------------------------------------------*/
/*                                                                         */
/* класс TStreamableClass */
/*                                                                         */
/* Используется внутри TStreamableTypes и pstream. */
/*                                                                         */
/* ------------------------------------------------------------------------*/

#if defined( Uses_TStreamableClass ) && !defined( __TStreamableClass )
#define __TStreamableClass

#include <tvision/compat/borland/dos.h>
#include <limits.h>
#include <stddef.h>

const P_id_type P_id_notFound = UINT_MAX;

typedef TStreamable *(*BUILDER)();

// Теперь ipstream вычисляет это во время выполнения.
#define __DELTA( d ) 0

class TStreamableClass
{

    friend TStreamableTypes;
    friend opstream;
    friend ipstream;

public:

    TStreamableClass( const char *n, BUILDER b, int /* Не используется. */ = 0 ) noexcept;

private:

    const char *name;
    BUILDER build;

};

#endif  // Uses_TStreamableClass

/* ------------------------------------------------------------------------*/
/*                                                                         */
/* класс TStreamableTypes */
/*                                                                         */
/* Поддерживает базу всех зарегистрированных типов приложения. */
/* Используется opstream и ipstream для поиска функций чтения и */
/* записи объектов. */
/*                                                                         */
/* ------------------------------------------------------------------------*/

#if defined( Uses_TStreamableTypes ) && !defined( __TStreamableTypes )
#define __TStreamableTypes

class TStreamableTypes : private TNSSortedCollection
{

public:

    TStreamableTypes() noexcept;
    ~TStreamableTypes();

    void registerType( const TStreamableClass * );
    const TStreamableClass *lookup( const char * );

private:

    virtual void *keyOf( void * );
    int compare( void *, void * );

};

#endif  // Uses_TStreamableTypes

/* ------------------------------------------------------------------------*/
/*                                                                         */
/* класс TPWrittenObjects */
/*                                                                         */
/* Поддерживает базу всех объектов, записанных в */
/* текущий объектный поток. */
/*                                                                         */
/* Используется opstream при записи указателя в поток, чтобы определить, */
/* был ли уже записан объект, на который он указывает. */
/*                                                                         */
/* ------------------------------------------------------------------------*/

#if defined( Uses_TPWrittenObjects ) && !defined( __TPWrittenObjects )
#define __TPWrittenObjects

class TPWrittenObjects : public TNSSortedCollection
{

    friend opstream;

public:

    void removeAll() { curId = 0; TNSSortedCollection::freeAll(); }

private:

    TPWrittenObjects() noexcept;
    ~TPWrittenObjects();

    void registerObject( const void *adr );
    P_id_type find( const void *adr );

    void *keyOf( void * );
    int compare( void *, void * );

    P_id_type curId;

};

/* ------------------------------------------------------------------------*/
/*                                                                         */
/* класс TPWObj */
/*                                                                         */
/* Используется внутри TPWrittenObjects. */
/*                                                                         */
/* ------------------------------------------------------------------------*/

class TPWObj
{

    friend TPWrittenObjects;

private:

    TPWObj( const void *adr, P_id_type id ) noexcept;

    const void *address;
    P_id_type ident;

};

#endif  // Uses_TPWrittenObjects

/* ------------------------------------------------------------------------*/
/*                                                                         */
/* класс TPReadObjects */
/*                                                                         */
/* Поддерживает базу всех объектов, прочитанных из */
/* текущего потока сохраняемых объектов. */
/*                                                                         */
/* Используется ipstream при чтении указателя из потока, чтобы определить */
/* адрес соответствующего объекта. */
/*                                                                         */
/* ------------------------------------------------------------------------*/

#if defined( Uses_TPReadObjects ) && !defined( __TPReadObjects )
#define __TPReadObjects

class TPReadObjects : public TNSCollection
{

    friend ipstream;

public:

    void removeAll() { curId = 0; TNSCollection::removeAll(); }

private:

    TPReadObjects() noexcept;
    ~TPReadObjects();

    void registerObject( const void *adr );
    const void *find( P_id_type id );

    P_id_type curId;

};

#endif  // Uses_TPReadObjects

/* ------------------------------------------------------------------------*/
/*                                                                         */
/* класс pstream */
/*                                                                         */
/* Базовый класс для работы с сохраняемыми объектами. */
/*                                                                         */
/* ------------------------------------------------------------------------*/

#if defined( Uses_pstream ) && !defined( __pstream )
#define __pstream

#if defined( __BORLANDC__ )
#pragma option -Vo-
#endif
#if defined( __BCOPT__ ) && !defined (__FLAT__)
#pragma option -po-
#endif

class _FAR TStreamableTypes;

class pstream
{

    friend TStreamableTypes;

public:

    enum StreamableError { peNotRegistered, peInvalidType };
    enum PointerTypes { ptNull, ptIndexed, ptObject };

    _Cdecl pstream( streambuf _FAR * ) noexcept;
    virtual _Cdecl ~pstream();

    typedef int openmode;
    typedef int seekdir;

    int _Cdecl rdstate() const noexcept;
    int _Cdecl eof() const noexcept;
    int _Cdecl fail() const noexcept;
    int _Cdecl bad() const noexcept;
    int _Cdecl good() const noexcept;
    void _Cdecl clear( int = 0 ) noexcept;
    _Cdecl operator void *() const noexcept;
    int _Cdecl operator ! () const noexcept;

    streambuf _FAR * _Cdecl rdbuf() const noexcept;

    static void initTypes() noexcept;
    static void registerType( TStreamableClass *ts ) noexcept;

protected:

    _Cdecl pstream() noexcept;

    void _Cdecl error( StreamableError ) noexcept;
    void _Cdecl error( StreamableError, const TStreamable& ) noexcept;

    streambuf _FAR *bp;
    int state;

    void _Cdecl init( streambuf _FAR * ) noexcept;
    void _Cdecl setstate( int ) noexcept;

    static TStreamableTypes * _NEAR types;

};

#endif  // Uses_pstream

/* ------------------------------------------------------------------------*/
/*                                                                         */
/* класс ipstream */
/*                                                                         */
/* Базовый класс для чтения сохраняемых объектов. */
/*                                                                         */
/* ------------------------------------------------------------------------*/

#if defined( Uses_ipstream ) && !defined( __ipstream )
#define __ipstream

#if defined( __BORLANDC__ )
#pragma option -Vo-
#endif
#if defined( __BCOPT__ ) && !defined (__FLAT__)
#pragma option -po-
#endif

class _FAR TStreamableClass;

class ipstream : virtual public pstream
{

public:

    _Cdecl ipstream( streambuf _FAR * ) noexcept;
    _Cdecl ~ipstream();

    streampos _Cdecl tellg();
    ipstream& _Cdecl seekg( streampos );
    ipstream& _Cdecl seekg( streamoff, pstream::seekdir );

    uchar _Cdecl readByte();
    void _Cdecl readBytes( void _FAR *, size_t );
    ushort _Cdecl readWord();
    char _FAR * _Cdecl readString();
    char _FAR * _Cdecl readString( char _FAR *, unsigned );

    friend ipstream& _Cdecl operator >> ( ipstream&, TStreamable& );
    friend ipstream& _Cdecl operator >> ( ipstream&, void _FAR *& );

protected:

    _Cdecl ipstream() noexcept;

    const TStreamableClass _FAR * _Cdecl readPrefix();
    void _FAR * _Cdecl readData( const TStreamableClass _FAR *,
                                        TStreamable _FAR * );
    void _Cdecl readSuffix();

    const void _FAR * _Cdecl find( P_id_type );
    void _Cdecl registerObject( const void _FAR *adr );

private:

    TPReadObjects objs;

};

#endif  // Uses_ipstream

/* ------------------------------------------------------------------------*/
/*                                                                         */
/* класс opstream */
/*                                                                         */
/* Базовый класс для записи сохраняемых объектов. */
/*                                                                         */
/* ------------------------------------------------------------------------*/

#if defined( Uses_opstream ) && !defined( __opstream )
#define __opstream

#if defined( __BORLANDC__ )
#pragma option -Vo-
#endif
#if defined( __BCOPT__ ) && !defined (__FLAT__)
#pragma option -po-
#endif


class _FAR TStreamableClass;

class opstream : virtual public pstream
{

public:

    _Cdecl opstream( streambuf _FAR * ) noexcept;
    _Cdecl ~opstream();

    streampos _Cdecl tellp();
    opstream& _Cdecl seekp( streampos );
    opstream& _Cdecl seekp( streamoff, pstream::seekdir );
    opstream& _Cdecl flush();

    void _Cdecl writeByte( uchar );
    void _Cdecl writeBytes( const void _FAR *, size_t );
    void _Cdecl writeWord( ushort );
    void _Cdecl writeString( const char _FAR * );
    void _Cdecl writeString( TStringView );

    friend opstream& _Cdecl operator << ( opstream&, TStreamable& );
    friend opstream& _Cdecl operator << ( opstream&, TStreamable _FAR * );

protected:

    _Cdecl opstream() noexcept;

    void _Cdecl writePrefix( const TStreamable& );
    void _Cdecl writeData( TStreamable& );
    void _Cdecl writeSuffix( const TStreamable& );

    P_id_type _Cdecl find( const void _FAR *adr );
    void _Cdecl registerObject( const void _FAR *adr );

private:

    TPWrittenObjects *objs;

};

#endif  // Uses_opstream

/* ------------------------------------------------------------------------*/
/*                                                                         */
/* класс iopstream */
/*                                                                         */
/* Базовый класс для чтения и записи сохраняемых объектов. */
/*                                                                         */
/* ------------------------------------------------------------------------*/

#if defined( Uses_iopstream ) && !defined( __iopstream )
#define __iopstream

#if defined( __BORLANDC__ )
#pragma option -Vo-
#endif
#if defined( __BCOPT__ ) && !defined (__FLAT__)
#pragma option -po-
#endif

class iopstream : public ipstream, public opstream
{

public:

    _Cdecl iopstream( streambuf _FAR * ) noexcept;
    _Cdecl ~iopstream();

protected:

    _Cdecl iopstream() noexcept;

};

#endif  // Uses_iopstream

/* ------------------------------------------------------------------------*/
/*                                                                         */
/* класс fpbase */
/*                                                                         */
/* Базовый класс для работы с сохраняемыми объектами в файловых потоках. */
/*                                                                         */
/* ------------------------------------------------------------------------*/

#if defined( Uses_fpbase ) && !defined( __fpbase )
#define __fpbase

#if !defined( __FSTREAM_H )
#include <tvision/compat/borland/fstream.h>
#endif  // __FSTREAM_H

#if defined( __BORLANDC__ )
#pragma option -Vo-
#endif
#if defined( __BCOPT__ ) && !defined (__FLAT__)
#pragma option -po-
#endif

class fpbase : virtual public pstream
{

public:

    _Cdecl fpbase() noexcept;
    _Cdecl fpbase( const char _FAR *, pstream::openmode);
    _Cdecl ~fpbase();

    void _Cdecl open( const char _FAR *, pstream::openmode);
    void _Cdecl close();
    filebuf _FAR * _Cdecl rdbuf() noexcept;

private:

    filebuf buf;

};

#endif  // Uses_fpbase

/* ------------------------------------------------------------------------*/
/*                                                                         */
/* класс ifpstream */
/*                                                                         */
/* Базовый класс для чтения сохраняемых объектов из файловых потоков. */
/*                                                                         */
/* ------------------------------------------------------------------------*/

#if defined( Uses_ifpstream ) && !defined( __ifpstream )
#define __ifpstream

#if defined( __BORLANDC__ )
#pragma option -Vo-
#endif
#if defined( __BCOPT__ ) && !defined (__FLAT__)
#pragma option -po-
#endif

class ifpstream : public fpbase, public ipstream
{

public:

    _Cdecl ifpstream() noexcept;
    _Cdecl ifpstream( const char _FAR *,
                      pstream::openmode = ios::in
                    );
    _Cdecl ~ifpstream();

    filebuf _FAR * _Cdecl rdbuf() noexcept;
    void _Cdecl open( const char _FAR *,
                      pstream::openmode = ios::in
                    );

};

#endif  // Uses_ifpstream

/* ------------------------------------------------------------------------*/
/*                                                                         */
/* класс ofpstream */
/*                                                                         */
/* Базовый класс для записи сохраняемых объектов в файловые потоки. */
/*                                                                         */
/* ------------------------------------------------------------------------*/

#if defined( Uses_ofpstream ) && !defined( __ofpstream )
#define __ofpstream

#if defined( __BORLANDC__ )
#pragma option -Vo-
#endif
#if defined( __BCOPT__ ) && !defined (__FLAT__)
#pragma option -po-
#endif


class ofpstream : public fpbase, public opstream
{

public:

    _Cdecl ofpstream() noexcept;
    _Cdecl ofpstream( const char _FAR *,
                      pstream::openmode = ios::out
                    );
    _Cdecl ~ofpstream();

    filebuf _FAR * _Cdecl rdbuf() noexcept;
    void _Cdecl open( const char _FAR *,
                      pstream::openmode = ios::out
                    );

};

#endif  // Uses_ofpstream

/* ------------------------------------------------------------------------*/
/*                                                                         */
/* класс fpstream */
/*                                                                         */
/* Базовый класс для чтения и записи сохраняемых объектов в */
/* двунаправленных файловых потоках. */
/*                                                                         */
/* ------------------------------------------------------------------------*/

#if defined( Uses_fpstream ) && !defined( __fpstream )
#define __fpstream

#if defined( __BORLANDC__ )
#pragma option -Vo-
#endif
#if defined( __BCOPT__ ) && !defined (__FLAT__)
#pragma option -po-
#endif

class fpstream : public fpbase, public iopstream
{

public:

    _Cdecl fpstream() noexcept;
    _Cdecl fpstream( const char _FAR *, pstream::openmode);
    _Cdecl ~fpstream();

    filebuf _FAR * _Cdecl rdbuf() noexcept;
    void _Cdecl open( const char _FAR *, pstream::openmode);

};


#endif  // Uses_fpstream

#if defined( __BORLANDC__ )
#pragma option -Vo.
#endif
#if defined( __BCOPT__ ) && !defined (__FLAT__)
#pragma option -po.
#endif
