/*------------------------------------------------------------------*/
/*                                                                  */
/* Демонстрация форм Turbo Vision. */
/*                                                                  */
/*------------------------------------------------------------------*/
/*                                                                  */
/* Эта программа использует GENPHONE.H и GENPARTS.H для создания */
/* файлов данных, используемых демонстрацией TVFORMS. Используйте */
/* GENFORMS.MAK для создания файлов данных примера TVFORMS. */
/*                                                                  */
/*------------------------------------------------------------------*/
/*
 *      Turbo Vision - Version 2.0
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 */

#define Uses_fpstream
#define Uses_TResourceFile
#define Uses_TScreen
#define Uses_TStreamableClass
#include <tvision/tv.h>
__link ( RResourceCollection )

#if defined( PHONENUM )
#include "genphone.h"
#elif defined( PARTS )
#include "genparts.h"
#else
#error Specify PHONENUM or PARTS as a conditional define, compile and then run.
#endif

#if !defined( __FORMS_H )
#include "forms.h"
#endif  // __FORMS_H

#if !defined( __STDLIB_H )
#include <stdlib.h>
#endif  // __STDLIB_H

int main(void)
{
    TSortedCollection *collection;
    int i;
    TForm *f;
    void *p;
    fpstream *s;
    TResourceFile* r;

    TScreen::clearOnSuspend=False;

    cout <<"Creating  " << rezFileName << "\n";

    // Создаём поток и ресурс.
    s = new fpstream (rezFileName, ios::out|ios::binary);
    r = new TResourceFile(s);

    // Форма.
    f = makeForm();
    r->put(f, "FormDialog");

    // Данные.
    collection = new TDataCollection((dataCount + 10), 5, sizeof(TDataRec),
                          dataKeyType);
    collection->duplicates = allowDuplicates;
    for(i = 0; i < dataCount; ++i)
        {
        p = malloc(sizeof(TDataRec));
        memset(p, 0 , sizeof(TDataRec));   // Байты выравнивания должны быть инициализированы.
        f->setData((void *)&data[i]);      // Переносим в объект.
        f->getData(p);                     // Переносим в кучу.
        collection->insert(p);             // Вставляем в отсортированном порядке.
        }
    r->put(collection, "FormData");

    // Готово.
    TObject::destroy(f);
    TObject::destroy((TCollection *)collection);
    TObject::destroy(r);
    return 0;
}
