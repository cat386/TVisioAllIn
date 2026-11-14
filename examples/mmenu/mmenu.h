/*
 * MMENU.H. Модуль содержит определения классов для MMENU.CPP. TMultiMenu
 * — производный от TMenuBar класс с поддержкой динамического
 * переключения меню. TMenuItem& operator+(TMenuItem& one, TMenuItem&
 * two) связывает два объекта TMenuItem. This code was written by Borland
 * Technical Support. It is provided as is with no warranties expressed
 * or implied.
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 */

/*
 * Класс TMultiMenu — строка меню с динамическим переключением через
 * систему сообщений. При создании конструктор получает массив TMenu или
 * TSubMenu. Во время работы любое из этих меню можно выбрать
 * широковещательным сообщением. Параметры message(TView *receiver,
 * ushort what, ushort command, void *infoPtr): receiver — указатель на
 * строку меню или её владельца; системная строка меню доступна через
 * статическую переменную TProgram::menubar. what = evBroadcast. command
 * = cmMMChangeMenu выбирает меню, номер которого передан через infoPtr
 * как беззнаковое целое. Значение infoPtr зависит от команды;
 * подробности приведены в её описании. Пример:
 * message(TProgram::menubar, evBroadcast, cmMMChangeMenu, (void *)3)
 * выбирает третье системное меню. Если меню меньше трёх, сообщение
 * игнорируется. Конструкторы TMultiMenu похожи на конструкторы TMenuBar,
 * но принимают массив указателей TMenu или объектов TSubMenu вместо
 * одного указателя или ссылки. В варианте TMenu *[] последний элемент
 * массива должен быть NULL, если используется значение по умолчанию
 * третьего аргумента — числа меню. В конце файла определён перегруженный
 * оператор связывания двух TMenuItem, упрощающий чтение примера в
 * тестовом модуле.
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 */

#if !defined( __MMENU_H )
#define __MMENU_H

class TMultiMenu : public TMenuBar
{

public:

    TMultiMenu( const TRect& bounds, TMenu *aMenu[], int nMenus = 0 );
    TMultiMenu( const TRect& bounds, TSubMenu aMenu[], int nMenus );
    ~TMultiMenu();

    virtual void handleEvent( TEvent& event );

protected:

    TMenu **mList;
    int numMenus;

};

const unsigned cmMMChangeMenu = 0x1600;

#endif // __MMENU_H
