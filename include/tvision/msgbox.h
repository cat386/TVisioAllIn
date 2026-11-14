/* ------------------------------------------------------------------------*/
/*                                                                         */
/*   MSGBOX.H                                                              */
/*                                                                         */
/* Определяет функции messageBox(), messageBoxRect(), */
/* inputBox() и inputBoxRect(). */
/*                                                                         */
/* ------------------------------------------------------------------------*/
/*
 *      Turbo Vision - Version 2.0
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 */

#if defined( Uses_MsgBox ) && !defined( __MsgBox )
#define __MsgBox

#if defined( __BORLANDC__ )
#pragma option -Vo-
#endif
#if defined( __BCOPT__ ) && !defined (__FLAT__)
#pragma option -po-
#endif

class _FAR TRect;

ushort messageBox( TStringView msg, ushort aOptions ) noexcept;
ushort messageBox( unsigned aOptions, const char *msg, ... ) noexcept;

ushort messageBoxRect( const TRect &r, TStringView msg, ushort aOptions ) noexcept;
ushort messageBoxRect( const TRect &r, ushort aOptions, const char *msg, ... ) noexcept;

ushort inputBox( TStringView Title, TStringView aLabel, char *s, uchar limit ) noexcept;

ushort inputBoxRect( const TRect &bounds, TStringView title,
                     TStringView aLabel, char *s, uchar limit ) noexcept;

const int

// Виды окон сообщений.

    mfWarning      = 0x0000,       // Показать предупреждение.
    mfError        = 0x0001,       // Показать ошибку.
    mfInformation  = 0x0002,       // Показать информацию.
    mfConfirmation = 0x0003,       // Показать запрос подтверждения.

// Флаги кнопок окна сообщения.

    mfYesButton    = 0x0100,       // Добавить в диалог кнопку Yes.
    mfNoButton     = 0x0200,       // Добавить в диалог кнопку No.
    mfOKButton     = 0x0400,       // Добавить в диалог кнопку OK.
    mfCancelButton = 0x0800,       // Добавить в диалог кнопку Cancel.

    mfYesNoCancel  = mfYesButton | mfNoButton | mfCancelButton,
                                    // Стандартный диалог Yes, No, Cancel.
    mfOKCancel     = mfOKButton | mfCancelButton;
                                    // Стандартный диалог OK, Cancel.

class MsgBoxText
{

public:

    static const char * _NEAR yesText;
    static const char * _NEAR noText;
    static const char * _NEAR okText;
    static const char * _NEAR cancelText;
    static const char * _NEAR warningText;
    static const char * _NEAR errorText;
    static const char * _NEAR informationText;
    static const char * _NEAR confirmText;
};

#if defined( __BORLANDC__ )
#pragma option -Vo.
#endif
#if defined( __BCOPT__ ) && !defined (__FLAT__)
#pragma option -po.
#endif

#endif  // Uses_MsgBox
