/*------------------------------------------------------------*/
/* файл — tstrview.cpp */
/*                                                            */
/* функции:  */
/* Дружественные функции TStringView. */
/*------------------------------------------------------------*/

#include <tvision/tv.h>

#include <iostream.h>

ostream _FAR & _Cdecl operator<<(ostream _FAR &os, TStringView s)
{
    return os.write(s.data(), s.size());
}
