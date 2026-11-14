/*-----------------------------------------------------------*/
/* файл — outline.cpp */
/*                                                           */
/* функции:  */
/* Методы TOutline */
/*-----------------------------------------------------------*/
/*
 *      Turbo Vision - Version 2.0
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 */

#define cpOutlineViewer "\x6\x7\x3\x8"
#define Uses_TOutlineViewer
#define Uses_TOutline
#define Uses_TEvent
#define Uses_TDrawBuffer
#define Uses_TKeys
#define Uses_ipstream
#define Uses_opstream
#include <tvision/tv.h>
#include <string.h>

TOutlineViewer::TOutlineViewer(const TRect& bounds, TScrollBar* aHScrollBar,
  TScrollBar* aVScrollBar) noexcept : TScroller(bounds, aHScrollBar, aVScrollBar)
{
  growMode = gfGrowHiX + gfGrowHiY;
  foc = 0;
}

// Внутренний вызов, проверяющий допустимость и видимость фокуса.

void TOutlineViewer::adjustFocus(int newFocus) noexcept
{
  if (newFocus < 0)
   newFocus = 0;
  else if (newFocus >= limit.y)
   newFocus = limit.y - 1;
  if (foc != newFocus)
   focused(newFocus);
  if (newFocus < delta.y)
    scrollTo(delta.x, newFocus);
  else if ((newFocus - size.y) >= delta.y)
    scrollTo(delta.x, newFocus - size.y + 1);
}

static thread_local TDrawBuffer *pdBuf;
static thread_local int auxPos;

// Вызывается для отрисовки дерева.

Boolean drawTree( TOutlineViewer *beingDrawn, TNode* cur, int level,
                  int position, long lines, ushort flags )
{
  TDrawBuffer &dBuf = *pdBuf;
  TAttrPair color;
  int x;

  if (position >= beingDrawn->delta.y)
  {
      if (position >= beingDrawn->delta.y + beingDrawn->size.y)
        return True;

      if ((position == beingDrawn->foc) && ((beingDrawn->state & sfFocused)!=0))
        color = beingDrawn->getColor(0x0202);
      else if (beingDrawn->isSelected(position))
        color = beingDrawn->getColor(0x0303);
      else
        color = beingDrawn->getColor(0x0401);
      dBuf.moveChar(0, ' ', color, beingDrawn->size.x);
      {
        TStringView graph = beingDrawn->getGraph(level, lines, flags);
        x = strwidth(graph) - beingDrawn->delta.x;
        if (x > 0)
          dBuf.moveStr(0, graph, color, (ushort) -1U, beingDrawn->delta.x);
        delete[] (char *) graph.data();
      }
      {
        TStringView text = beingDrawn->getText(cur);
        TColorAttr c = (flags & ovExpanded) ? color : (color >> 8);
        dBuf.moveStr(max(0, x), text, c, (ushort) -1U, max(0, -x));
      }
      beingDrawn->writeLine(0, position-beingDrawn->delta.y,
                               beingDrawn->size.x, 1, dBuf);
      auxPos = position;
  }

  return False;
}

void TOutlineViewer::draw()
{
  TAttrPair nrmColor = getColor(0x0401);
  TDrawBuffer dBuf;
  pdBuf = &dBuf;
  auxPos = -1;
  firstThat(drawTree);
  dBuf.moveChar(0, ' ', nrmColor, size.x);
  writeLine(0, auxPos + 1, size.x, size.y - (auxPos - delta.y), dBuf);
}

// ExpandAll раскрывает текущий узел и всех его потомков.

void TOutlineViewer::expandAll(TNode* node)
{
  int i, n;

  if (hasChildren(node))
  {
    adjust(node, True);
    n = getNumChildren(node) - 1;
    for (i=0; i <= n; i++)
      expandAll(getChild(node, i));
  }
}

/*
 * Создаёт графическую строку для getGraph. Level — уровень узла. Lines —
 * битовая маска уровней с маркером продолжения, обычно вертикальной
 * линией: бит 3 означает продолжение третьего уровня ниже текущего узла.
 * Flags задаёт оформление конца строки; см. ovXXX. LevWidth — отступ
 * уровня, endWidth — длина конечной части. Графика состоит из маркеров
 * уровней и конечной части узла. Маркеры Level Mark разделяются Level
 * Filler и определяются Lines. Конец состоит из End First, затем
 * endWidth-4 символов End Filler, затем End Child и символа
 * сворачивания/раскрытия. При endWidth = 2 используются End First и
 * символ сворачивания/раскрытия, при 1 — только последний. Выбор
 * символов определяется Flags. Массив Chars: 1 — Level Filler, обычно
 * пробел между маркерами; 2 — Level Mark, обычно вертикальная черта
 * активного уровня; 3 — End First для не последнего потомка, обычно
 * Т-образная фигура; 4 — End First последнего потомка, обычно угол; 5 —
 * End Filler, обычно горизонтальная линия; 6 — End Child, обычно не
 * используется, но при endWidth > LevWidth накладывается на маркеры
 * следующего уровня и обычно имеет Т-образный вид; 7 — символ свёрнутого
 * узла, обычно '+', если есть скрытые потомки; 8 — символ раскрытого
 * узла, обычно прямая линия, если потомки видны.
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
 *
 *
 */

char* TOutlineViewer::createGraph(int level, long lines, ushort flags,
 int levWidth, int endWidth,  const char* chars) noexcept
{
  static const int
    FillerOrBar      = 0,
    YorL          = 2,
    StraightOrTee = 4,
    retracted      = 6;

  char* graph = new char[level*levWidth+endWidth+1];
  char* p;

  Boolean expanded = Boolean((flags & ovExpanded) != 0);
  Boolean children = Boolean((flags & ovChildren) != 0);
  Boolean last       = Boolean((flags & ovLast)       != 0);

  for (p=graph; level > 0; level--, lines >>= 1)
  {
    *p++ = (lines & 1) ? chars[FillerOrBar+1]:chars[FillerOrBar];
    memset(p, chars[FillerOrBar], levWidth-1);
    p += levWidth-1;
  }

  if (--endWidth > 0)
  {
    *p++ = last ? chars[YorL+1]:chars[YorL];
    if (--endWidth > 0)
    {
        if (--endWidth > 0)
        {
                memset(p, chars[StraightOrTee], endWidth);
                p += endWidth;
        }
        *p++ = children ? chars[StraightOrTee+1]:chars[StraightOrTee];
    }
    *p++ = expanded ? chars[retracted+1]:chars[retracted];
  }
  *p = 0;

  return graph;
}

static Boolean visitNoArg( TOutlineViewer* outLine, TNode* cur, int level,
                           int position, long lines, ushort flags, void* arg )
{
  return (**(TOutlineVisitorNoArg*) arg)( outLine, cur, level, position,
                                          lines, flags );
}

/*
 * FirstThat обходит узлы до возврата true локальной функцией. Её
 * объявление имеет вид: function MyIter(Cur: Pointer; Level, Position:
 * Integer; Lines: LongInt; Flags: Word); far; Cur — указатель
 * проверяемого узла. Level — его глубина от нуля, пригодная для
 * getGraph/createGraph. Position — позиция узла в порядке отображения,
 * пригодная для Focused/Selected; если узел виден, Position - Delta.Y
 * задаёт его строку в представлении. Lines — биты активных уровней для
 * getGraph/createGraph, определяющие нужные линии. Flags — флаги
 * оформления ovXXXX для getGraph/createGraph.
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
TNode* TOutlineViewer::firstThat(TOutlineVisitor test, void* arg) noexcept
{
  return iterate(test, arg, True);
}

TNode* TOutlineViewer::firstThat(TOutlineVisitorNoArg test) noexcept
{
  return iterate(visitNoArg, &test, True);
}

// Вызывается при получении узлом фокуса.

void TOutlineViewer::focused(int i)
{
  foc = i;
}


/*
 * Внутренняя функция фактического обхода данных для FirstThat и ForEach;
 * подробности в FirstThat. }
 */


static TNode* traverseTree( TOutlineViewer* outLine,
                            TOutlineVisitor action, void *arg,
                            int& position, Boolean& checkResult, TNode* cur,
                            int level, long lines, Boolean lastChild )
{

  Boolean result;
  TNode* ret;
  ushort flags;
  Boolean children;

  if (cur == 0)
        return 0;

  children = outLine->hasChildren(cur);

  flags = 0;
  if (lastChild)
        flags |= ovLast;

  if (children && outLine->isExpanded(cur))
        flags |=  ovChildren;

  if (! children || outLine->isExpanded(cur))
        flags |= ovExpanded;

  position++;

  result = (*action)(outLine, cur, level, position, lines, flags, arg);
  if (checkResult && result)
        return cur;

  if (children && outLine->isExpanded(cur))
  {
    int childLines = lines;
    if (!lastChild)
        childLines |=  1 << level;
    TNode *child = outLine->getChild(cur, 0);
    do
    {
      TNode *next = outLine->getNext(child);
      ret = traverseTree( outLine, action, arg, position, checkResult,
                          child, level + 1, childLines, Boolean(!next) );
      child = next;
      if (ret)
        return ret;
    } while (child);
  }
  if (cur == outLine->getRoot())
  {
    TNode *next = cur;
    while ((next = outLine->getNext(next)) != 0)
    {
      ret = traverseTree( outLine, action, arg, position, checkResult,
                          next, level, lines, Boolean(!outLine->getNext(next)) );
      if (ret)
        return ret;
    }
  }
  return 0;
}


TNode* TOutlineViewer::iterate( TOutlineVisitor action, void *arg,
                                Boolean checkResult ) noexcept
{
  int position = -1;
  TNode *root = getRoot();
  if (root)
    return traverseTree( this, action, arg, position, checkResult,
                         root, 0, 0, Boolean(getNext(root) == 0) );
  return 0;
}


// Обходит все узлы; подробности в FirstThat.

TNode* TOutlineViewer::forEach(TOutlineVisitor action, void *arg) noexcept
{
  return iterate(action, arg, False);
}

TNode* TOutlineViewer::forEach(TOutlineVisitorNoArg action) noexcept
{
  return iterate(visitNoArg, &action, False);
}

// Возвращает палитру дерева.

TPalette& TOutlineViewer::getPalette() const
{
        static TPalette p(cpOutlineViewer, sizeof(cpOutlineViewer));
        return p;
}

/*
 * Возвращает графические символы перед текстом GetText. Можно
 * переопределить для изменения вида дерева. По умолчанию вызывает
 * createGraph со стандартными параметрами.
 *
 */

char* TOutlineViewer::getGraph(int level, long lines, ushort flags)
{
  static const int levelWidth = 3,    endWidth   = levelWidth;
  static const char* graphChars = "\x20\xB3\xC3\xC0\xC4\xC4+\xC4";

  return createGraph(level, lines, flags, levelWidth, endWidth, graphChars);
}

static Boolean isNode( TOutlineViewer* , TNode*, int,
                       int position, long, ushort )
{
    return Boolean(auxPos == position);
}


// Возвращает указатель на узел, отображаемый в строке i.

TNode* TOutlineViewer::getNode(int i)
{
  auxPos = i;

  return firstThat(isNode);
}

/*
 * Проверяет выделение узла. По умолчанию true для узла в фокусе, то есть
 * одиночного выделения. Можно переопределить для множественного. }
 *
 */
Boolean TOutlineViewer::isSelected(int i)
{
  return (foc == i) ? True:False;
}

static thread_local long focLines;
static thread_local ushort focFlags;
static thread_local int focLevel;

static Boolean isFocused( TOutlineViewer* focusCheck, TNode*, int level,
                          int position, long lines, ushort flags )
{
      if (position == focusCheck->foc)
      {
        focLevel = level;
        focLines = lines;
        focFlags = flags;

        return True;
      }
      else
        return False;
}

// Вызывается для обработки события.

void TOutlineViewer::handleEvent(TEvent& event)
{
  const int mouseAutoToSkip = 3;

  TPoint mouse;
  TNode* cur;
  int newFocus = 0;
  int count;
  char* graph;
  uchar dragged;

  TScroller::handleEvent(event);
  switch (event.what)
  {
  case evMouseDown:
        count = 0;
        dragged = 0;
        do {
          if (dragged < 2)
                dragged++;
          mouse = makeLocal(event.mouse.where);
          if (mouseInView(event.mouse.where))
          {
            int i = delta.y + mouse.y;
            newFocus = i < limit.y ? i : foc;
          }
          else
          {
            if (event.what == evMouseAuto)
                count++;
            if (count == mouseAutoToSkip)
            {
              count = 0;
              if (mouse.y < 0)
                newFocus--;
              if (mouse.y >= size.y)
                newFocus++;
            }
          }
          if (foc != newFocus)
          {
            adjustFocus(newFocus);
            drawView();
          }
        } while ( mouseEvent(event, evMouseMove + evMouseAuto) );

        if (event.mouse.eventFlags & meDoubleClick)
                selected(foc);
        else
        {
          if (dragged < 2 && (cur = firstThat(isFocused)) != 0)
          {
            graph = getGraph(focLevel,focLines,focFlags);
            if (mouse.x < strwidth(graph) )
            {
              adjust(cur, Boolean( !isExpanded(cur) ));
              update();
              drawView();
            }
            delete[] graph;
          }
        }

        break;

    case evKeyboard:

        newFocus = foc;
        switch (ctrlToArrow(event.keyDown.keyCode))
        {
        case kbUp:
        case kbLeft:
                newFocus--;
                break;
        case kbDown:
        case kbRight:
                newFocus++;
                break;
        case kbPgDn:
                newFocus += size.y - 1;
                break;
        case kbPgUp:
                newFocus -= size.y - 1;
                break;
        case kbHome:
                newFocus = delta.y;
                break;
        case kbEnd:
                newFocus = delta.y + size.y - 1;
                break;
        case kbCtrlPgUp:
                newFocus = 0;
                break;
        case kbCtrlPgDn:
                newFocus = limit.y - 1;
                break;
        case kbCtrlEnter:
        case kbEnter:
                selected(newFocus);
                break;
        default:
                uchar code = event.keyDown.charScan.charCode;
          switch ( code )
          {
          case '-':
          case '+':
                if ((cur = getNode(newFocus)) != 0)
                  adjust(cur, code == '+' ? True:False);
                break;
          case '*':
                if ((cur = getNode(newFocus)) != 0)
                  expandAll(cur);
                break;
          default:
                return;
          }
          update();
        }
        clearEvent(event);
        adjustFocus(newFocus);
        drawView();
  };
}

/*
 * Вызывается при выборе узла пользователем с клавиатуры или мышью. }
 *
 */
void TOutlineViewer::selected(int)
{
}

// Перерисовывает дерево при изменении состояния sfFocus.

void TOutlineViewer::setState(ushort aState, Boolean enable)
{
  TScroller::setState(aState, enable);
  if ( (aState & sfFocused) != 0)
        drawView();
}

static thread_local int updateCount;
static thread_local int updateMaxX;

static Boolean countNode( TOutlineViewer* beingCounted, TNode* p, int level,
                          int, long lines,  ushort flags )
{
    int len;
    char *graph;

    updateCount++;
    graph = beingCounted->getGraph(level, lines, flags);
    len = strwidth(beingCounted->getText(p)) + strwidth(graph);
    if (updateMaxX < len)
      updateMaxX = len;
    delete[] graph;
    return False;
}

/*
 * Обновляет границы просмотра дерева при любом изменении данных, в том
 * числе при инициализации базовых классов. TOutlineViewer предполагает
 * пустое дерево. Если при инициализации появились узлы, нужно вызвать
 * Update. При изменении отображаемых данных во время работы требуются
 * Update и DrawView. }
 *
 *
 */
void TOutlineViewer::update() noexcept
{
  updateCount = 0;
  updateMaxX = 0;
  firstThat(countNode);
  setLimit(updateMaxX, updateCount);
  adjustFocus(foc);
}

void TOutlineViewer::disposeNode(TNode* node) noexcept
{
  if (node)
  {
      if (node->childList)
        disposeNode(node->childList);
      if (node->next)
        disposeNode(node->next);
      delete node;
  }
}

#if !defined(NO_STREAMABLE)

void* TOutlineViewer::read(ipstream& ip)
{
    TScroller::read(ip);
    ip >> foc;
    return this;
}

void TOutlineViewer::write(opstream& op)
{
    TScroller::write(op);
    op << foc;
}

#endif

// TOutline

TOutline::TOutline(const TRect& bounds, TScrollBar* aHScrollBar,
                        TScrollBar* aVScrollBar,  TNode* aRoot) noexcept :
                TOutlineViewer(bounds,    aHScrollBar, aVScrollBar)
{
  root = aRoot;
  update();
}

TOutline::~TOutline()
{
  disposeNode(root);
}

void TOutline::adjust(TNode* node, Boolean expand)
{
  node->expanded = expand;
}

TNode* TOutline::getRoot()
{
  return root;
}

int TOutline::getNumChildren(TNode* node)
{
  int i;
  TNode* p;

  p = node->childList;
  i = 0;
  while (p)
  {
    i++;
    p = p->next;
  }
  return i;
}

TNode* TOutline::getNext(TNode* node)
{
  return node->next;
}

TNode* TOutline::getChild(TNode* node, int i)
{
  TNode *p;

  p = node->childList;
  while ((i != 0) && (p != 0))
  {
    i--;
    p = p->next;
  }
  return p;
}

const char* TOutline::getText(TNode* node)
{
  return node->text;
}

Boolean TOutline::isExpanded(TNode* node)
{
  return node->expanded;
}

Boolean TOutline::hasChildren(TNode* node)
{
  return Boolean(node->childList != 0);
}

#if !defined(NO_STREAMABLE)

TNode* TOutline::readNode(ipstream& ip)
{
    int nChildren;
    uchar more;
    uchar expand;

    TNode* node = new TNode((char*)0);

    ip >> more;
    ip >> expand;
    ip >> nChildren;
    node->text = ip.readString();
    node->expanded = Boolean(expand);

    if (nChildren)
        node->childList = readNode(ip);
    else
        node->childList = 0;

    if (more)
        node->next = readNode(ip);
    else
        node->next = 0;

    return node;
}

void TOutline::writeNode(TNode* node, opstream& op)
{
    uchar more = (node->next != 0) ? 1 : 0;
    uchar expand = (node->expanded) ? 1 : 0;

    op << more;
    op << expand;
    op << getNumChildren(node);
    op.writeString(node->text);

    if ( node->childList != 0 )
        writeNode(node->childList, op);

    if (node->next != 0)
        writeNode(node->next, op);
}


void* TOutline::read(ipstream& ip)
{
    TOutlineViewer::read(ip);

    root = readNode(ip);

    return this;
}

void TOutline::write(opstream& op)
{
    TOutlineViewer::write(op);

    writeNode(root, op);
}

TStreamable* TOutline::build()
{
    return new TOutline( streamableInit );
}

#endif
