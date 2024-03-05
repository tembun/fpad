 /*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2014-2019 <tsujan2000@gmail.com>
 *
 * FeatherPad is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * FeatherPad is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * @license GPL-3.0+ <https://spdx.org/licenses/GPL-3.0+.html>
 */

#include <QApplication>
#include <QTimer>
#include <QTextBlock>
#include <QDateTime>
#include <QPainter>
#include <QMenu>
#include <QDesktopServices>
#include <QProcess>
#include <QRegularExpression>
#include <QClipboard>
#include <QTextDocumentFragment>
#include "textedit.h"
#include "vscrollbar.h"

#define UPDATE_INTERVAL 50
#define SCROLL_FRAMES_PER_SEC 60
#define SCROLL_DURATION 300

namespace FeatherPad {

#if (QT_VERSION == QT_VERSION_CHECK(5,14,0))
static QColor overlayColor (const QColor& bgCol, const QColor& overlayCol)
{
    if (!overlayCol.isValid()) return QColor(0,0,0);
    if (!bgCol.isValid()) return overlayCol;

    qreal a1 = overlayCol.alphaF();
    if (a1 == 1.0) return overlayCol;
    qreal a0  = bgCol.alphaF();
    qreal a = (1.0 - a1) * a0 + a1;

    QColor res;
    res.setAlphaF(a);
    res.setRedF (((1.0 - a1) * a0 * bgCol.redF() + a1 * overlayCol.redF()) / a);
    res.setGreenF (((1.0 - a1) * a0 *bgCol.greenF() + a1 * overlayCol.greenF()) / a);
    res.setBlueF (((1.0 - a1) * a0 * bgCol.blueF() + a1 * overlayCol.blueF()) / a);

    return res;
}
#endif

TextEdit::TextEdit (QWidget *parent, int bgColorValue) : QPlainTextEdit (parent)
{
    prevAnchor_ = prevPos_ = -1;
    widestDigit_ = 0;
    autoIndentation_ = true;
    saveCursor_ = false;
    scrollTimer_ = nullptr;
    keepTxtCurHPos_ = false;
    txtCurHPos_ = -1;
    textTab_ = "    ";
    setMouseTracking (true);
    setCursorWidth( 13 );
    setStyleSheet ("QPlainTextEdit {"
                           "selection-background-color: rgb(160, 160, 160);"
                           "selection-color: black;}"
                           "QScrollBar {"
                           "	background:#000000}"
                           "QScrollBar::add-line{"
                           "	border:#000000}"
                           "QScrollBar::sub-line{"
                           "	border:#000000}"
                           "QScrollBar::add-page {"
                           "	background:#d8d8d8}"
                           "QScrollBar::sub-page{"
                           "	background:#d8d8d8}"
                           "QScrollBar::handle{"
                           "	min-height: 75px;"
                           "	border:1px solid #000000}"
	);
    QPalette p = palette();
    bgColorValue = qBound (0, bgColorValue, 255);
        viewport()->setStyleSheet (QString (".QWidget {"
                                            "color: black;"
                                            "background-color: rgb(%1, %1, %1);}")
                                   .arg (bgColorValue));
        QColor col = p.highlight().color();
        if (bgColorValue - qGray (col.rgb()) < 30 && col.hslSaturation() < 100)
        {
            setStyleSheet ("QPlainTextEdit {"
                           "selection-background-color: rgb(100, 100, 100);"
                           "selection-color: white;}");
        }
        else
        {
            col = p.color (QPalette::Inactive, QPalette::Highlight);
            if (bgColorValue - qGray (col.rgb()) < 30 && col.hslSaturation() < 100)
            {
                p.setColor (QPalette::Inactive, QPalette::Highlight, p.highlight().color());
                p.setColor (QPalette::Inactive, QPalette::HighlightedText, p.highlightedText().color());
                setPalette (p);
            }
        }
        separatorColor_ = Qt::black;
        separatorColor_.setAlpha (2 * qRound (static_cast<qreal>(bgColorValue) / 5) - 32);

#if (QT_VERSION == QT_VERSION_CHECK(5,14,0))
    separatorColor_ = overlayColor (QColor (bgColorValue, bgColorValue, bgColorValue), separatorColor_);
#endif

    resizeTimerId_ = 0;
    selectionTimerId_ = 0;
    size_ = 0;
    wordNumber_ = -1;
    encoding_= "UTF-8";
    uneditable_ = false;
    setFrameShape (QFrame::NoFrame);
    VScrollBar *vScrollBar = new VScrollBar;
    setVerticalScrollBar (vScrollBar);

#if (QT_VERSION == QT_VERSION_CHECK(5,14,0))
    HScrollBar *hScrollBar = new HScrollBar;
    setHorizontalScrollBar (hScrollBar);
#endif

    lineNumberArea_ = new LineNumberArea (this);
    lineNumberArea_->setToolTip (tr ("Double click to center current line"));
    lineNumberArea_->show();
    connect (this, &QPlainTextEdit::blockCountChanged, this, &TextEdit::updateLineNumberAreaWidth);
    connect (this, &QPlainTextEdit::updateRequest, this, &TextEdit::updateLineNumberArea);
    updateLineNumberAreaWidth (0);
    lineNumberArea_->installEventFilter (this);

    connect (this, &QPlainTextEdit::updateRequest, this, &TextEdit::onUpdateRequesting);
    connect (this, &QPlainTextEdit::cursorPositionChanged, [this] {
        if (!keepTxtCurHPos_)
            txtCurHPos_ = -1;
    });
    connect (this, &QPlainTextEdit::selectionChanged, this, &TextEdit::onSelectionChanged);

    setContextMenuPolicy (Qt::CustomContextMenu);
}
bool TextEdit::eventFilter (QObject *watched, QEvent *event)
{
    if (watched == lineNumberArea_ && event->type() == QEvent::Wheel)
    {
        if (QWheelEvent *we = static_cast<QWheelEvent*>(event))
        {
            wheelEvent (we);
            return false;
        }
    }
    return QPlainTextEdit::eventFilter (watched, event);
}

void TextEdit::setEditorFont (const QFont &f, bool setDefault)
{
    if (setDefault)
        font_ = f;
    setFont (f);
    viewport()->setFont (f);
    document()->setDefaultFont (f);
    QFontMetricsF metrics (f);
    QTextOption opt = document()->defaultTextOption();
#if (QT_VERSION >= QT_VERSION_CHECK(5,11,0))
    opt.setTabStopDistance (metrics.horizontalAdvance (textTab_));
#elif (QT_VERSION >= QT_VERSION_CHECK(5,10,0))
    opt.setTabStopDistance (metrics.width (textTab_));
#else
    opt.setTabStop (metrics.width (textTab_));
#endif
    document()->setDefaultTextOption (opt);
    QFont F(f);
    if (f.bold())
    {
        F.setBold (false);
        lineNumberArea_->setFont (F);
    }
    else
        lineNumberArea_->setFont (f);
    F.setBold (true);
    widestDigit_ = 0;
    int maxW = 0;
    for (int i = 0; i < 10; ++i)
    {
#if (QT_VERSION >= QT_VERSION_CHECK(5,11,0))
        int w = QFontMetrics (F).horizontalAdvance (QString::number (i));
#else
        int w = QFontMetrics (F).width (QString::number (i));
#endif
        if (w > maxW)
        {
            maxW = w;
            widestDigit_ = i;
        }
    }
}
TextEdit::~TextEdit()
{
    if (scrollTimer_)
    {
        disconnect (scrollTimer_, &QTimer::timeout, this, &TextEdit::scrollWithInertia);
        scrollTimer_->stop();
        delete scrollTimer_;
    }
    delete lineNumberArea_;
}
int TextEdit::lineNumberAreaWidth()
{
    QString digit = QString::number (widestDigit_);
    QString num = digit;
    int max = qMax (1, blockCount());
    while (max >= 10)
    {
        max /= 10;
        num += digit;
    }
    QFont f = font();
    f.setBold (true);
#if (QT_VERSION >= QT_VERSION_CHECK(5,11,0))
    return (6 + QFontMetrics (f).horizontalAdvance (num));
#else
    return (6 + QFontMetrics (f).width (num));
#endif
}

void TextEdit::updateLineNumberAreaWidth (int /* newBlockCount */)
{
    if (QApplication::layoutDirection() == Qt::RightToLeft)
        setViewportMargins (0, 0, lineNumberAreaWidth(), 0);
    else
        setViewportMargins (lineNumberAreaWidth(), 0, 0, 0);
}

void TextEdit::updateLineNumberArea (const QRect &rect, int dy)
{
    if (dy)
        lineNumberArea_->scroll (0, dy);
    else
    {
        if (lastCurrentLine_.isValid())
            lineNumberArea_->update (0, lastCurrentLine_.y(), lineNumberArea_->width(), lastCurrentLine_.height());
        QRect totalRect;
        QTextCursor cur = cursorForPosition (rect.center());
        if (rect.contains (cursorRect (cur).center()))
        {
            QRectF blockRect = blockBoundingGeometry (cur.block()).translated (contentOffset());
            totalRect = rect.united (blockRect.toRect());
        }
        else
            totalRect = rect;
        lineNumberArea_->update (0, totalRect.y(), lineNumberArea_->width(), totalRect.height());
    }

    if (rect.contains (viewport()->rect()))
        updateLineNumberAreaWidth (0);
}

QString TextEdit::computeIndentation (const QTextCursor &cur) const
{
    QTextCursor cusror = cur;
    if (cusror.hasSelection())
    {
        if (cusror.anchor() <= cusror.position())
            cusror.setPosition (cusror.anchor());
        else
            cusror.setPosition (cusror.position());
    }
    QTextCursor tmp = cusror;
    tmp.movePosition (QTextCursor::StartOfBlock);
    QString str;
    if (tmp.atBlockEnd())
        return str;
    int pos = tmp.position();
    tmp.setPosition (++pos, QTextCursor::KeepAnchor);
    QString selected;
    while (!tmp.atBlockEnd()
           && tmp <= cusror
           && ((selected = tmp.selectedText()) == " "
               || (selected = tmp.selectedText()) == "\t"))
    {
        str.append (selected);
        tmp.setPosition (pos);
        tmp.setPosition (++pos, QTextCursor::KeepAnchor);
    }
    if (tmp.atBlockEnd()
        && tmp <= cusror
        && ((selected = tmp.selectedText()) == " "
            || (selected = tmp.selectedText()) == "\t"))
    {
        str.append (selected);
    }
    return str;
}
QString TextEdit::remainingSpaces (const QString& spaceTab, const QTextCursor& cursor) const
{
    QTextCursor tmp = cursor;
    QString txt = cursor.block().text().left (cursor.positionInBlock());
    QFontMetricsF fm = QFontMetricsF (document()->defaultFont());
#if (QT_VERSION >= QT_VERSION_CHECK(5,11,0))
    qreal spaceL = fm.horizontalAdvance (" ");
#else
    qreal spaceL = fm.width (" ");
#endif
    int n = 0, i = 0;
    while ((i = txt.indexOf("\t", i)) != -1)
    {
        tmp.setPosition (tmp.block().position() + i);
        qreal x = static_cast<qreal>(cursorRect (tmp).right());
        tmp.setPosition (tmp.position() + 1);
        x = static_cast<qreal>(cursorRect (tmp).right()) - x;
        n += qMax (qRound (qAbs (x) / spaceL) - 1, 0);
        ++i;
    }
    n += txt.count();
    n = spaceTab.count() - n % spaceTab.count();
    QString res;
    for (int i = 0 ; i < n; ++i)
        res += " ";
    return res;
}
QTextCursor TextEdit::backTabCursor (const QTextCursor& cursor, bool twoSpace) const
{
    QTextCursor tmp = cursor;
    tmp.movePosition (QTextCursor::StartOfBlock);
    const QString blockText = cursor.block().text();
    int indx = 0;
    QRegularExpressionMatch match;
    if (blockText.indexOf (QRegularExpression ("^\\s+"), 0, &match) > -1)
        indx = match.capturedLength();
    else
        return tmp;
    int txtStart = cursor.block().position() + indx;

    QString txt = blockText.left (indx);
    QFontMetricsF fm = QFontMetricsF (document()->defaultFont());
#if (QT_VERSION >= QT_VERSION_CHECK(5,11,0))
    qreal spaceL = fm.horizontalAdvance (" ");
#else
    qreal spaceL = fm.width (" ");
#endif
    int n = 0, i = 0;
    while ((i = txt.indexOf("\t", i)) != -1)
    {
        tmp.setPosition (tmp.block().position() + i);
        qreal x = static_cast<qreal>(cursorRect (tmp).right());
        tmp.setPosition (tmp.position() + 1);
        x = static_cast<qreal>(cursorRect (tmp).right()) - x;
        n += qMax (qRound (qAbs (x) / spaceL) - 1, 0);
        ++i;
    }
    n += txt.count();
    n = n % textTab_.count();
    if (n == 0) n = textTab_.count();

    if (twoSpace) n = qMin (n, 2);

    tmp.setPosition (txtStart);
    QChar ch = blockText.at (indx - 1);
    if (ch == QChar (QChar::Space))
        tmp.setPosition (txtStart - n, QTextCursor::KeepAnchor);
    else
    {
        qreal x = static_cast<qreal>(cursorRect (tmp).right());
        tmp.setPosition (txtStart - 1, QTextCursor::KeepAnchor);
        x -= static_cast<qreal>(cursorRect (tmp).right());
        n -= qRound (qAbs (x) / spaceL);
        if (n < 0) n = 0;
        tmp.setPosition (tmp.position() - n, QTextCursor::KeepAnchor);
    }

    return tmp;
}

static inline bool isOnlySpaces (const QString &str)
{
    int i = 0;
    while (i < str.size())
    { // always skip the starting spaces
        QChar ch = str.at (i);
        if (ch == QChar (QChar::Space) || ch == QChar (QChar::Tabulation))
            ++i;
        else return false;
    }
    return true;
}

void TextEdit::sync_cursor()
{
	QTextCursor cursor = textCursor();
	int cursor_pos = cursor.position();
	
	int first_visible_line_start_pos = cursorForPosition( QPoint( 0 , 0 ) ).position();
      	
	QPoint bottom_left_point( 0 , viewport()->height() - 1 );
	int last_visible_line_start_pos = cursorForPosition( bottom_left_point).position();
      	
	if( cursor_pos < first_visible_line_start_pos )
	{
      	cursor.setPosition(first_visible_line_start_pos);
	}
	else if( cursor_pos >= last_visible_line_start_pos )
	{
      	cursor.setPosition( last_visible_line_start_pos - 1 );
      	cursor.movePosition( QTextCursor::StartOfLine );
	}
	
	setTextCursor( cursor );
}

void TextEdit::keyPressEvent (QKeyEvent *event)
{
    keepTxtCurHPos_ = false;

    /* workarounds for copy/cut/... -- see TextEdit::copy()/cut()/... */
    if (event == QKeySequence::Copy)
    {
        copy();
        event->accept();
        return;
    }
    if (event == QKeySequence::Cut)
    {
        if (!isReadOnly())
            cut();
        event->accept();
        return;
    }
    if (event == QKeySequence::Paste)
    {
        if (!isReadOnly())
            paste();
        event->accept();
        return;
    }
    if (event == QKeySequence::SelectAll)
    {
        selectAll();
        event->accept();
        return;
    }
    if (event == QKeySequence::Undo)
    {
        /* QWidgetTextControl::undo() callls ensureCursorVisible() even when there's nothing to undo.
           Users may press Ctrl+Z just to know whether a document is in its original state and
           a scroll jump can confuse them when there's nothing to undo. Also see "TextEdit::undo()". */
        if (!isReadOnly() && document()->isUndoAvailable())
            undo();
        event->accept();
        return;
    }
    if (event == QKeySequence::Redo)
    {
        /* QWidgetTextControl::redo() calls ensureCursorVisible() even when there's nothing to redo.
           That may cause a scroll jump, which can be confusing when nothing else has happened.
           Also see "TextEdit::redo()". */
        if (!isReadOnly() && document()->isRedoAvailable())
            redo();
        event->accept();
        return;
    }

    if (isReadOnly())
    {
        QPlainTextEdit::keyPressEvent (event);
        return;
    }

    if (event->key() == Qt::Key_Backspace)
    {
        keepTxtCurHPos_ = true;
        if (txtCurHPos_ < 0)
        {
            QTextCursor startCur = textCursor();
            startCur.movePosition (QTextCursor::StartOfLine);
            txtCurHPos_ = qAbs (cursorRect().left() - cursorRect (startCur).left()); // is negative for RTL
        }

    }
    else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
    {
        keepTxtCurHPos_ = true;
        if (txtCurHPos_ < 0)
        {
            QTextCursor startCur = textCursor();
            startCur.movePosition (QTextCursor::StartOfLine);
            txtCurHPos_ = qAbs (cursorRect().left() - cursorRect (startCur).left());
        }

        QTextCursor cur = textCursor();
        QString selTxt = cur.selectedText();
        QString prefix, indent;
        bool withShift (event->modifiers() & Qt::ShiftModifier);
        
        if( withShift )
        {
        	cur.clearSelection();
        	setTextCursor( cur );
        }
        
        if (autoIndentation_)
            indent = computeIndentation (cur);
        QTextCursor anchorCur = cur;
        anchorCur.setPosition (cur.anchor());

        if (withShift || autoIndentation_)
        {
            cur.beginEditBlock();
            cur.insertText (QChar (QChar::ParagraphSeparator));
            cur.insertText (indent);
            cur.endEditBlock();
            ensureCursorVisible();
            event->accept();
            return;
        }
    }
    else if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right)
    {
        QTextCursor cursor = textCursor();
    
        if( event->modifiers() == (Qt::ControlModifier | Qt::AltModifier) )
        {
        	
        	cursor.setPosition( cursor.position() , QTextCursor::KeepAnchor );
        	
        	if( event->key() == Qt::Key_Left )
        	{
        	
        		cursor.movePosition( QTextCursor::StartOfLine , QTextCursor::KeepAnchor );
        	
        	}
        	else {
        		
        		cursor.movePosition( QTextCursor::EndOfLine , QTextCursor::KeepAnchor );
        		
        	}
        	
        	setTextCursor( cursor );
        	
        }
        /* when text is selected, use arrow keys
           to go to the start or end of the selection */
        else if (event->modifiers() == Qt::NoModifier && cursor.hasSelection())
        {
            QString selTxt = cursor.selectedText();
            if (event->key() == Qt::Key_Left)
            {
                if (selTxt.isRightToLeft())
                    cursor.setPosition (cursor.selectionEnd());
                else
                    cursor.setPosition (cursor.selectionStart());
            }
            else
            {
                if (selTxt.isRightToLeft())
                    cursor.setPosition (cursor.selectionStart());
                else
                    cursor.setPosition (cursor.selectionEnd());
            }
            cursor.clearSelection();
            setTextCursor (cursor);
            event->accept();
            return;
        }
    }
    else if (event->key() == Qt::Key_Down || event->key() == Qt::Key_Up)
    {
        
        if (
        	( event->modifiers() == Qt::ControlModifier )
        	||
        	( event->modifiers() ==  Qt::AltModifier )
        )
        {
            if (QScrollBar* vbar = verticalScrollBar())
            {
                int step = (
                    event->modifiers() == Qt::AltModifier
                    ?
                    	4
                    :
                    	1
                );      
            
                vbar->setValue(vbar->value() + (event->key() == Qt::Key_Down ? step : -step ));
                
                sync_cursor();
                
                event->accept();
                return;
            }
        }
        else if (event->modifiers() == Qt::NoModifier
                || (!(event->modifiers() & Qt::AltModifier)
                    && ((event->modifiers() & Qt::ShiftModifier)
                        || (event->modifiers() & Qt::MetaModifier)
                        || (event->modifiers() & Qt::KeypadModifier))))
        {
            /* NOTE: This also includes a useful Qt feature with Down/Up after Backspace/Enter.
                     The feature was removed with Backspace due to a regression in Qt 5.14.1. */
            keepTxtCurHPos_ = true;
            QTextCursor cursor = textCursor();
            int hPos;
            if (txtCurHPos_ >= 0)
                hPos = txtCurHPos_;
            else
            {
                QTextCursor startCur = cursor;
                startCur.movePosition (QTextCursor::StartOfLine);
                hPos = qAbs (cursorRect().left() - cursorRect (startCur).left());
                txtCurHPos_ = hPos;
            }
            QTextCursor::MoveMode mode = ((event->modifiers() & Qt::ShiftModifier)
                                              ? QTextCursor::KeepAnchor
                                              : QTextCursor::MoveAnchor);
            if ((event->modifiers() & Qt::MetaModifier))
            { // try to restore the cursor pixel position between blocks
                cursor.movePosition (event->key() == Qt::Key_Down
                                         ? QTextCursor::EndOfBlock
                                         : QTextCursor::StartOfBlock,
                                     mode);
                if (cursor.movePosition (event->key() == Qt::Key_Down
                                             ? QTextCursor::NextBlock
                                             : QTextCursor::PreviousBlock,
                                         mode))
                {
                    setTextCursor (cursor); // WARNING: This is needed because of a Qt bug.
                    bool rtl (cursor.block().text().isRightToLeft());
                    QPoint cc = cursorRect (cursor).center();
                    cursor.setPosition (cursorForPosition (QPoint (cc.x() + (rtl ? -1 : 1) * hPos, cc.y())).position(), mode);
                }
            }
            else
            { // try to restore the cursor pixel position between lines
                cursor.movePosition (event->key() == Qt::Key_Down
                                         ? QTextCursor::EndOfLine
                                         : QTextCursor::StartOfLine,
                                     mode);
                if (cursor.movePosition (event->key() == Qt::Key_Down
                                             ? QTextCursor::NextCharacter
                                             : QTextCursor::PreviousCharacter,
                                         mode))
                { // next/previous line or block
                    cursor.movePosition (QTextCursor::StartOfLine, mode);
                    setTextCursor (cursor); // WARNING: This is needed because of a Qt bug.
                    bool rtl (cursor.block().text().isRightToLeft());
                    QPoint cc = cursorRect (cursor).center();
                    cursor.setPosition (cursorForPosition (QPoint (cc.x() + (rtl ? -1 : 1) * hPos, cc.y())).position(), mode);
                }
            }
            setTextCursor (cursor);
            ensureCursorVisible();
            event->accept();
            return;
        }
    }
    else if ( event->key() == Qt::Key_PageDown || event->key() == Qt::Key_PageUp )
    {
    	
      if ( QScrollBar* vbar = verticalScrollBar() )
      {
      	
      	if ( event->modifiers() == Qt::ControlModifier )
      	{
      		vbar->setValue( vbar->value() + ( event->key() == Qt::Key_PageDown ? 1 : -1 ) * vbar->pageStep() );
      		sync_cursor();
      	}
      	else if( event->modifiers() == Qt::AltModifier )
      	{
      		vbar->setValue( ( event->key() == Qt::Key_PageUp ? 0 : vbar->maximum() ) );
      		
      		QTextCursor cursor = textCursor();
      		
      		cursor.movePosition(
      			event->key() == Qt::Key_PageUp
      			?
      				QTextCursor::Start
      			:
      				QTextCursor::End
      		);
      		
      		setTextCursor( cursor );
      		
      	}
      	else
      	{
      		vbar->setValue( vbar->value() + ( event->key() == Qt::Key_PageDown ? 9 : -9 ) );
      		sync_cursor();
      	}
      	
            event->accept();
            return;
      	
      }
    	
    }
    else if (event->key() == Qt::Key_Tab)
    {
        QTextCursor cursor = textCursor();
        int newLines = cursor.selectedText().count (QChar (QChar::ParagraphSeparator));
        if (newLines > 0)
        {
            cursor.beginEditBlock();
            cursor.setPosition (qMin (cursor.anchor(), cursor.position()));
            cursor.movePosition (QTextCursor::StartOfBlock);
            for (int i = 0; i <= newLines; ++i)
            {
                int indx = 0;
                QRegularExpressionMatch match;
                if (cursor.block().text().indexOf (QRegularExpression ("^\\s+"), 0, &match) > -1)
                    indx = match.capturedLength();
                cursor.setPosition (cursor.block().position() + indx);
                if (event->modifiers() & Qt::ControlModifier)
                {
                    cursor.insertText (remainingSpaces (event->modifiers() & Qt::MetaModifier
                                                        ? "  " : textTab_, cursor));
                }
                else
                    cursor.insertText ("\t");
                if (!cursor.movePosition (QTextCursor::NextBlock))
                    break;
            }
            cursor.endEditBlock();
            ensureCursorVisible();
            event->accept();
            return;
        }
        else if (event->modifiers() & Qt::ControlModifier)
        {
            QTextCursor tmp (cursor);
            tmp.setPosition (qMin (tmp.anchor(), tmp.position()));
            cursor.insertText (remainingSpaces (event->modifiers() & Qt::MetaModifier
                                                ? "  " : textTab_, tmp));
            ensureCursorVisible();
            event->accept();
            return;
        }
    }
    else if (event->key() == Qt::Key_Backtab)
    {
        QTextCursor cursor = textCursor();
        int newLines = cursor.selectedText().count (QChar (QChar::ParagraphSeparator));
        cursor.setPosition (qMin (cursor.anchor(), cursor.position()));
        cursor.beginEditBlock();
        cursor.movePosition (QTextCursor::StartOfBlock);
        for (int i = 0; i <= newLines; ++i)
        {
            if (cursor.atBlockEnd())
            {
                if (!cursor.movePosition (QTextCursor::NextBlock))
                    break;
                continue;
            }
            cursor = backTabCursor (cursor, event->modifiers() & Qt::MetaModifier
                                            ? true : false);
            cursor.removeSelectedText();
            if (!cursor.movePosition (QTextCursor::NextBlock))
                break;
        }
        cursor.endEditBlock();
        ensureCursorVisible();
        event->accept();
        return;
    }
    else if (event->key() == Qt::Key_Insert)
    {
        if (event->modifiers() == Qt::NoModifier || event->modifiers() == Qt::KeypadModifier)
        {
            setOverwriteMode (!overwriteMode());
            if (!overwriteMode())
                update();
            event->accept();
            return;
        }
    }
    else if (event->key() == 0x200c)
    {
        insertPlainText (QChar (0x200C));
        event->accept();
        return;
    }
    else if (event->key() == Qt::Key_Home)
    {
        if (!(event->modifiers() & Qt::ControlModifier))
        {
            QTextCursor cur = textCursor();
            int p = cur.positionInBlock();
            int indx = 0;
            QRegularExpressionMatch match;
            if (cur.block().text().indexOf (QRegularExpression ("^\\s+"), 0, &match) > -1)
                indx = match.capturedLength();
            if (p > 0)
            {
                if (p <= indx) p = 0;
                else p = indx;
            }
            else p = indx;
            cur.setPosition (p + cur.block().position(),
                             event->modifiers() & Qt::ShiftModifier ? QTextCursor::KeepAnchor
                                                                    : QTextCursor::MoveAnchor);
            setTextCursor (cur);
            ensureCursorVisible();
            event->accept();
            return;
        }
    }
    QPlainTextEdit::keyPressEvent (event);
}
void TextEdit::copy()
{
    QTextCursor cursor = textCursor();
    if (cursor.hasSelection())
        QApplication::clipboard()->setText (cursor.selection().toPlainText());
}
void TextEdit::cut()
{
    QTextCursor cursor = textCursor();
    if (cursor.hasSelection())
    {
        keepTxtCurHPos_ = false;
        txtCurHPos_ = -1;
        QApplication::clipboard()->setText (cursor.selection().toPlainText());
        cursor.removeSelectedText();
    }
}
void TextEdit::undo()
{
    setGreenSel (QList<QTextEdit::ExtraSelection>());
    if (getSearchedText().isEmpty())
    {
        QList<QTextEdit::ExtraSelection> es;
        es.prepend (currentLineSelection());
        es.append (blueSel_);
        es.append (redSel_);
        setExtraSelections (es);
    }
    keepTxtCurHPos_ = false;
    txtCurHPos_ = -1;
    QPlainTextEdit::undo();
}
void TextEdit::redo()
{
    keepTxtCurHPos_ = false;
    txtCurHPos_ = -1;
    QPlainTextEdit::redo();
}
void TextEdit::paste()
{
    keepTxtCurHPos_ = false;
    QPlainTextEdit::paste();
}
void TextEdit::selectAll()
{
    keepTxtCurHPos_ = false;
    txtCurHPos_ = -1;
    QPlainTextEdit::selectAll();
}
void TextEdit::insertPlainText (const QString &text)
{
    keepTxtCurHPos_ = false;
    txtCurHPos_ = -1;
    QPlainTextEdit::insertPlainText (text);
}
void TextEdit::keyReleaseEvent (QKeyEvent *event)
{
    QPlainTextEdit::keyReleaseEvent (event);
}
void TextEdit::wheelEvent (QWheelEvent *event)
{
#if (QT_VERSION >= QT_VERSION_CHECK(5,12,0))
        bool horizontal (event->angleDelta().x() != 0);
#else
        bool horizontal (event->orientation() == Qt::Horizontal);
#endif
    if (event->modifiers() & Qt::ShiftModifier)
    { // line-by-line scrolling when Shift is pressed
        int delta = horizontal
                        ? event->angleDelta().x() : event->angleDelta().y();
#if (QT_VERSION >= QT_VERSION_CHECK(5,12,0))
#if (QT_VERSION >= QT_VERSION_CHECK(5,15,0))
        QWheelEvent e (event->position(),
                       event->globalPosition(),
#else
        QWheelEvent e (event->posF(),
                       event->globalPosF(),
#endif
                       event->pixelDelta(),
#if (QT_VERSION == QT_VERSION_CHECK(5,14,0))
                       horizontal
                           ? QPoint (delta / QApplication::wheelScrollLines(), 0)
                           : QPoint (0, delta / QApplication::wheelScrollLines()),
#else
                       QPoint (0, delta / QApplication::wheelScrollLines()),
#endif
                       event->buttons(),
                       Qt::NoModifier,
                       event->phase(),
                       false,
                       event->source());
#else
        QWheelEvent e (event->posF(),
                       event->globalPosF(),
                       delta / QApplication::wheelScrollLines(),
                       event->buttons(),
                       Qt::NoModifier,
                       Qt::Vertical);
#endif
        QCoreApplication::sendEvent (horizontal
                                         ? horizontalScrollBar()
                                         : verticalScrollBar(), &e);
        return;
    }
    QAbstractScrollArea::wheelEvent (event);
    updateMicroFocus();
}

void TextEdit::scrollWithInertia()
{
    if (!verticalScrollBar()) return;

    int totalDelta = 0;
    for (QList<scrollData>::iterator it = queuedScrollSteps_.begin(); it != queuedScrollSteps_.end(); ++it)
    {
        totalDelta += qRound (static_cast<qreal>(it->delta) / static_cast<qreal>(it->totalSteps));
        -- it->leftSteps;
    }
    while (!queuedScrollSteps_.empty())
    {
        int t = queuedScrollSteps_.begin()->totalSteps;
        int l = queuedScrollSteps_.begin()->leftSteps;
        if ((t > 10 && l <= 0)
            || (t > 5 && t <= 10 && l <= -3)
            || (t <= 5 && l <= -6))
        {
            queuedScrollSteps_.removeFirst();
        }
        else break;
    }

#if (QT_VERSION >= QT_VERSION_CHECK(5,12,0))
    QWheelEvent e (QPointF(),
                   QPointF(),
                   QPoint(),
                   QPoint (0, totalDelta),
                   Qt::NoButton,
                   Qt::NoModifier,
                   Qt::NoScrollPhase,
                   false);
#else
    QWheelEvent e (QPointF(),
                   QPointF(),
                   totalDelta,
                   Qt::NoButton,
                   Qt::NoModifier,
                   Qt::Vertical);
#endif
    QCoreApplication::sendEvent (verticalScrollBar(), &e);

    /* update text selection if the left mouse button is pressed (-> QPlainTextEdit::timerEvent) */
    if (QApplication::mouseButtons() & Qt::LeftButton)
    {
        const QPoint globalPos = QCursor::pos();
        QPoint pos = viewport()->mapFromGlobal (globalPos);
        QMouseEvent ev (QEvent::MouseMove, pos, viewport()->mapTo (viewport()->topLevelWidget(), pos), globalPos,
                        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        mouseMoveEvent (&ev);
    }

    if (queuedScrollSteps_.empty())
        scrollTimer_->stop();
}

void TextEdit::resizeEvent (QResizeEvent *event)
{
    QPlainTextEdit::resizeEvent (event);

    QRect cr = contentsRect();
    lineNumberArea_->setGeometry (QRect (QApplication::layoutDirection() == Qt::RightToLeft ? cr.width() - lineNumberAreaWidth() : cr.left(),
                                         cr.top(), lineNumberAreaWidth(), cr.height()));

    if (resizeTimerId_)
    {
        killTimer (resizeTimerId_);
        resizeTimerId_ = 0;
    }
    resizeTimerId_ = startTimer (UPDATE_INTERVAL);
}

void TextEdit::timerEvent (QTimerEvent *event)
{
    QPlainTextEdit::timerEvent (event);

    if (event->timerId() == resizeTimerId_)
    {
        killTimer (event->timerId());
        resizeTimerId_ = 0;
        emit resized();
    }
    else if (event->timerId() == selectionTimerId_)
    {
        killTimer (event->timerId());
    }
}
static void fillBackground (QPainter *p, const QRectF &rect, QBrush brush, const QRectF &gradientRect = QRectF())
{
    p->save();
    if (brush.style() >= Qt::LinearGradientPattern && brush.style() <= Qt::ConicalGradientPattern)
    {
        if (!gradientRect.isNull())
        {
            QTransform m = QTransform::fromTranslate (gradientRect.left(), gradientRect.top());
            m.scale (gradientRect.width(), gradientRect.height());
            brush.setTransform (m);
            const_cast<QGradient *>(brush.gradient())->setCoordinateMode (QGradient::LogicalMode);
        }
    }
    else
        p->setBrushOrigin (rect.topLeft());
    p->fillRect (rect, brush);
    p->restore();
}
void TextEdit::paintEvent (QPaintEvent *event)
{
    QPainter painter (viewport());
    Q_ASSERT (qobject_cast<QPlainTextDocumentLayout*>(document()->documentLayout()));

    QPointF offset (contentOffset());

    QRect er = event->rect();
    QRect viewportRect = viewport()->rect();
    qreal maximumWidth = document()->documentLayout()->documentSize().width();
    painter.setBrushOrigin (offset);

    int maxX = static_cast<int>(offset.x() + qMax (static_cast<qreal>(viewportRect.width()), maximumWidth)
                                - document()->documentMargin());
    er.setRight (qMin (er.right(), maxX));
    painter.setClipRect (er);

    bool editable = !isReadOnly();
    QAbstractTextDocumentLayout::PaintContext context = getPaintContext();
    QTextBlock block = firstVisibleBlock();
    while (block.isValid())
    {
        QRectF r = blockBoundingRect (block).translated (offset);
        QTextLayout *layout = block.layout();

        if (!block.isVisible())
        {
            offset.ry() += r.height();
            block = block.next();
            continue;
        }

        if (r.bottom() >= er.top() && r.top() <= er.bottom())
        {
            /* take care of RTL */
            bool rtl (block.text().isRightToLeft());
            QTextOption opt = document()->defaultTextOption();
            if (rtl)
            {
                if (lineWrapMode() == QPlainTextEdit::WidgetWidth)
                    opt.setAlignment (Qt::AlignRight); // doesn't work without wrapping
                opt.setTextDirection (Qt::RightToLeft);
                layout->setTextOption (opt);
            }

            QTextBlockFormat blockFormat = block.blockFormat();
            QBrush bg = blockFormat.background();
            if (bg != Qt::NoBrush)
            {
                QRectF contentsRect = r;
                contentsRect.setWidth (qMax (r.width(), maximumWidth));
                fillBackground (&painter, contentsRect, bg);
            }

            if (lineNumberArea_->isVisible() && (opt.flags() & QTextOption::ShowLineAndParagraphSeparators))
            {
                /* "QTextFormat::FullWidthSelection" isn't respected when new-lines are shown.
                   This is a workaround. */
                QRectF contentsRect = r;
                contentsRect.setWidth (qMax (r.width(), maximumWidth));
                if (contentsRect.contains (cursorRect().center()))
                {
                    contentsRect.setTop (cursorRect().top());
                    contentsRect.setBottom (cursorRect().bottom());
                    fillBackground (&painter, contentsRect, lineHColor_);
                }
            }

            QVector<QTextLayout::FormatRange> selections;
            int blpos = block.position();
            int bllen = block.length();
            for (int i = 0; i < context.selections.size(); ++i)
            {
                const QAbstractTextDocumentLayout::Selection &range = context.selections.at (i);
                const int selStart = range.cursor.selectionStart() - blpos;
                const int selEnd = range.cursor.selectionEnd() - blpos;
                if (selStart < bllen && selEnd > 0
                    && selEnd > selStart)
                {
                    QTextLayout::FormatRange o;
                    o.start = selStart;
                    o.length = selEnd - selStart;
                    o.format = range.format;
                    selections.append (o);
                }
                else if (!range.cursor.hasSelection() && range.format.hasProperty (QTextFormat::FullWidthSelection)
                         && block.contains(range.cursor.position()))
                {
                    QTextLayout::FormatRange o;
                    QTextLine l = layout->lineForTextPosition (range.cursor.position() - blpos);
                    o.start = l.textStart();
                    o.length = l.textLength();
                    if (o.start + o.length == bllen - 1)
                        ++o.length; // include newline
                    o.format = range.format;
                    selections.append (o);
                }
            }

            bool drawCursor ((editable || (textInteractionFlags() & Qt::TextSelectableByKeyboard))
                             && context.cursorPosition >= blpos
                             && context.cursorPosition < blpos + bllen);
            bool drawCursorAsBlock (drawCursor && overwriteMode());

            if (drawCursorAsBlock)
            {
                if (context.cursorPosition == blpos + bllen - 1)
                    drawCursorAsBlock = false;
                else
                {
                    QTextLayout::FormatRange o;
                    o.start = context.cursorPosition - blpos;
                    o.length = 1;
                    o.format.setForeground (Qt::black);
                    o.format.setBackground (Qt::white);
                    selections.append (o);
                }
            }

            if (!placeholderText().isEmpty() && document()->isEmpty())
            {
                painter.save();
                QColor col = palette().text().color();
                col.setAlpha (128);
                painter.setPen (col);
                const int margin = int(document()->documentMargin());
                painter.drawText (r.adjusted (margin, 0, 0, 0), Qt::AlignTop | Qt::TextWordWrap, placeholderText());
                painter.restore();
            }
            else
            {
                if (opt.flags() & QTextOption::ShowLineAndParagraphSeparators)
                {
                    painter.save();
                    painter.setPen (separatorColor_);
                }
                layout->draw (&painter, offset, selections, er);
                if (opt.flags() & QTextOption::ShowLineAndParagraphSeparators)
                    painter.restore();
            }

            if ((drawCursor && !drawCursorAsBlock)
                || (editable && context.cursorPosition < -1
                    && !layout->preeditAreaText().isEmpty()))
            {
                int cpos = context.cursorPosition;
                if (cpos < -1)
                    cpos = layout->preeditAreaPosition() - (cpos + 2);
                else
                    cpos -= blpos;
                layout->drawCursor (&painter, offset, cpos, cursorWidth());
            }
        }
        offset.ry() += r.height();
        if (offset.y() > viewportRect.height())
            break;
        block = block.next();
    }
    if (backgroundVisible() && !block.isValid() && offset.y() <= er.bottom()
        && (centerOnScroll() || verticalScrollBar()->maximum() == verticalScrollBar()->minimum()))
    {
        painter.fillRect (QRect (QPoint (static_cast<int>(er.left()), static_cast<int>(offset.y())), er.bottomRight()), palette().window());
    }
}

void TextEdit::lineNumberAreaPaintEvent (QPaintEvent *event)
{
    QPainter painter (lineNumberArea_);
    painter.fillRect (event->rect(), QColor ( 255 , 255 , 255 ));
    painter.setPen (Qt::black);
    int w = lineNumberArea_->width();
    int left = 3;
    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = static_cast<int>(blockBoundingGeometry (block).translated (contentOffset()).top());
    int bottom = top + static_cast<int>(blockBoundingRect (block).height());
    int h = fontMetrics().height();
    QFont bf = font();
    bf.setBold (true);
    while (block.isValid() && top <= event->rect().bottom())
    {
        if (block.isVisible() && bottom >= event->rect().top())
        {
            QString number = QString::number (blockNumber + 1);
            painter.drawText (left, top, w - 3, h,
                              Qt::AlignRight, number);
        }
        block = block.next();
        top = bottom;
        bottom = top + static_cast<int>(blockBoundingRect (block).height());
        ++blockNumber;
    }
}
void TextEdit::adjustScrollbars()
{
    QSize vSize = viewport()->size();
    QResizeEvent *_resizeEvent = new QResizeEvent (vSize, vSize);
    QCoreApplication::postEvent (viewport(), _resizeEvent);
}
void TextEdit::onUpdateRequesting (const QRect& /*rect*/, int dy)
{
    if (dy == 0) return;
    emit updateRect();
}
void TextEdit::onSelectionChanged()
{
    QTextCursor cur = textCursor();
    if (!cur.hasSelection())
    {
        prevAnchor_ = prevPos_ = -1;
    }
    else
    {
        prevAnchor_ = cur.anchor();
        prevPos_ = cur.position();
    }
    if (selectionTimerId_)
    {
        killTimer (selectionTimerId_);
        selectionTimerId_ = 0;
    }
    selectionTimerId_ = startTimer (UPDATE_INTERVAL);
}
void TextEdit::showEvent (QShowEvent *event)
{
    QPlainTextEdit::showEvent (event);
    emit updateRect();
}
QString TextEdit::getUrl (const int pos) const
{
    static const QRegularExpression urlPattern ("[A-Za-z0-9_\\-]+://((?!&quot;|&gt;|&lt;)[A-Za-z0-9_.+/\\?\\=~&%#,;!@\\*\'\\-:\\(\\)\\[\\]])+(?<!\\.|\\?|!|:|;|,|\\(|\\)|\\[|\\]|\')|([A-Za-z0-9_.\\-]+@[A-Za-z0-9_\\-]+\\.[A-Za-z0-9.]+)(?<!\\.)");

    QString url;
    QTextBlock block = document()->findBlock (pos);
    QString text = block.text();
    if (text.length() <= 10000)
    {
        int cursorIndex = pos - block.position();
        QRegularExpressionMatch match;
        int indx = text.lastIndexOf (urlPattern, cursorIndex, &match);
        if (indx > -1 && indx + match.capturedLength() > cursorIndex)
        {
            url = match.captured();
            if (!match.captured (2).isEmpty())
                url = "mailto:" + url;
        }
    }
    return url;
}

void TextEdit::mouseMoveEvent (QMouseEvent *event)
{
    if (event->buttons() == Qt::LeftButton
        && (event->globalPos() - selectionPressPoint_).manhattanLength() <= qApp->startDragDistance())
    {
        return;
    }

    QPlainTextEdit::mouseMoveEvent (event);

    if (!(qApp->keyboardModifiers() & Qt::ControlModifier))
    {
        viewport()->setCursor (Qt::IBeamCursor);
        return;
    }

    if (getUrl (cursorForPosition (event->pos()).position()).isEmpty())
        viewport()->setCursor (Qt::IBeamCursor);
    else
        viewport()->setCursor (Qt::PointingHandCursor);
}

void TextEdit::mousePressEvent (QMouseEvent *event)
{
    keepTxtCurHPos_ = false;
    txtCurHPos_ = -1;
    if (tripleClickTimer_.isValid())
    {
        if (!tripleClickTimer_.hasExpired (qApp->doubleClickInterval())
            && event->buttons() == Qt::LeftButton)
        {
            tripleClickTimer_.invalidate();
            QTextCursor txtCur = textCursor();
            const QString txt = txtCur.block().text();
            const int l = txt.length();
            if (l > 10000) return;
            txtCur.movePosition (QTextCursor::StartOfBlock);
            int i = 0;
            while (i < l && txt.at (i).isSpace())
                ++i;
            if (i < l)
            {
                txtCur.setPosition (txtCur.position() + i);
                int j = l;
                while (j > i && txt.at (j -  1).isSpace())
                    --j;
                txtCur.setPosition (txtCur.position() + j - i, QTextCursor::KeepAnchor);
            }
            else
                txtCur.setPosition (txtCur.position() + i, QTextCursor::KeepAnchor);
            setTextCursor (txtCur);
            return;
        }
        tripleClickTimer_.invalidate();
    }
    if (event->buttons() == Qt::LeftButton
        && qApp->keyboardModifiers() == Qt::NoModifier)
    {
        QTextCursor cur = cursorForPosition (event->pos());
        int pos = cur.position();
        QTextCursor txtCur = textCursor();
        int selStart = txtCur.selectionStart();
        int selEnd = txtCur.selectionEnd();
        if (pos == cur.anchor() && selStart != selEnd
            && pos >= qMin (selStart, selEnd) && pos <= qMax (selStart, selEnd))
        {
            selectionPressPoint_ = event->globalPos();
        }
        else
            selectionPressPoint_ = QPoint();
    }
    else
        selectionPressPoint_ = QPoint();

    QPlainTextEdit::mousePressEvent (event);
}

void TextEdit::mouseReleaseEvent (QMouseEvent *event)
{
    QPlainTextEdit::mouseReleaseEvent (event);
    QTextCursor cursor = textCursor();
    if (cursor.hasSelection())
    {
        QClipboard *cl = QApplication::clipboard();
        if (cl->supportsSelection())
            cl->setText (cursor.selection().toPlainText(), QClipboard::Selection);
    }

    return;
}

void TextEdit::mouseDoubleClickEvent (QMouseEvent *event)
{
    tripleClickTimer_.start();
    QPlainTextEdit::mouseDoubleClickEvent (event);
}

bool TextEdit::event (QEvent *event)
{
    if (((event->type() == QEvent::WindowDeactivate && hasFocus()) // another window is activated
            || event->type() == QEvent::FocusOut)) // another widget has been focused
    {
        viewport()->setCursor (Qt::IBeamCursor);
    }
    return QPlainTextEdit::event (event);
}
static bool findBackwardInBlock (const QTextBlock &block, const QString &str, int offset,
                                 QTextCursor &cursor, QTextDocument::FindFlags flags)
{
    Qt::CaseSensitivity cs = !(flags & QTextDocument::FindCaseSensitively)
                             ? Qt::CaseInsensitive : Qt::CaseSensitive;
    QString text = block.text();
    text.replace (QChar::Nbsp, QLatin1Char (' '));
    if (offset > 0 && offset == text.length())
        -- offset;
    int idx = -1;
    while (offset >= 0 && offset <= text.length())
    {
        idx = text.lastIndexOf (str, offset, cs);
        if (idx == -1)
            return false;
        if (flags & QTextDocument::FindWholeWords)
        {
            const int start = idx;
            const int end = start + str.length();
            if ((start != 0 && text.at (start - 1).isLetterOrNumber())
                || (end != text.length() && text.at (end).isLetterOrNumber()))
            {
                offset = idx - 1;
                idx = -1;
                continue;
            }
        }
        cursor.setPosition (block.position() + idx);
        cursor.setPosition (cursor.position() + str.length(), QTextCursor::KeepAnchor);
        return true;
    }
    return false;
}

static bool findBackward (const QTextDocument *txtdoc, const QString &str,
                          QTextCursor &cursor, QTextDocument::FindFlags flags)
{
    if (!str.isEmpty() && !cursor.isNull())
    {
        int pos = cursor.anchor()
                  - str.size();
        if (pos >= 0)
        {
            QTextBlock block = txtdoc->findBlock (pos);
            int blockOffset = pos - block.position();
            while (block.isValid())
            {
                if (findBackwardInBlock (block, str, blockOffset, cursor, flags))
                    return true;
                block = block.previous();
                blockOffset = block.length() - 1; // newline is included in QTextBlock::length()
            }
        }
    }
    cursor = QTextCursor();
    return false;
}
QTextCursor TextEdit::finding (const QString& str, const QTextCursor& start, QTextDocument::FindFlags flags,
                               bool isRegex, const int end) const
{
    if (str.isEmpty())
        return QTextCursor();

    QTextCursor res = start;
    if (isRegex)
    {
        QRegularExpression regexp (str, (flags & QTextDocument::FindCaseSensitively)
                                            ? QRegularExpression::NoPatternOption
                                            : QRegularExpression::CaseInsensitiveOption);
        if (!regexp.isValid())
            return QTextCursor();
        QTextCursor cursor = start;
        QRegularExpressionMatch match;
        if (!(flags & QTextDocument::FindBackward))
        {
            cursor.setPosition (qMax (cursor.anchor(), cursor.position()));
            while (!cursor.atEnd())
            {
                if (!cursor.atBlockEnd())
                {
                    if (end > 0 && cursor.anchor() > end)
                        break;
                    int indx = cursor.block().text().indexOf (regexp, cursor.positionInBlock(), &match);
                    if (indx > -1)
                    {
                        if (match.capturedLength() == 0)
                        {
                            cursor.setPosition (cursor.position() + 1);
                            continue;
                        }
                        if (end > 0 && indx + cursor.block().position() > end)
                            break;
                        res.setPosition (indx + cursor.block().position());
                        res.setPosition (res.position() + match.capturedLength(), QTextCursor::KeepAnchor);
                        return  res;
                    }
                }
                if (!cursor.movePosition (QTextCursor::NextBlock))
                    break;
            }
        }
        else
        {
            cursor.setPosition (cursor.anchor());
            while (true)
            {
                const int bp = cursor.block().position();
                int indx = cursor.block().text().lastIndexOf (regexp, cursor.position() - bp, &match);
                if (indx > -1)
                {
                    if (match.capturedLength() == 0
                        || bp + indx == start.anchor())
                    {
                        if (cursor.atBlockStart())
                        {
                            if (!cursor.movePosition (QTextCursor::PreviousBlock))
                                break;
                            cursor.movePosition (QTextCursor::EndOfBlock);
                        }
                        else
                            cursor.setPosition (cursor.position() - 1);
                        continue;
                    }
                    res.setPosition (indx + bp);
                    res.setPosition (res.position() + match.capturedLength(), QTextCursor::KeepAnchor);
                    return  res;
                }
                if (!cursor.movePosition (QTextCursor::PreviousBlock))
                    break;
                cursor.movePosition (QTextCursor::EndOfBlock);
            }
        }
        return QTextCursor();
    }
    else if (str.contains ('\n'))
    {
        QTextCursor cursor = start;
        QTextCursor found;
        QStringList sl = str.split ("\n");
        int i = 0;
        Qt::CaseSensitivity cs = !(flags & QTextDocument::FindCaseSensitively)
                                 ? Qt::CaseInsensitive : Qt::CaseSensitive;
        QString subStr;
        if (!(flags & QTextDocument::FindBackward))
        {
            while (i < sl.count())
            {
                if (i == 0)
                {
                    subStr = sl.at (0);
                    if (subStr.isEmpty())
                    {
                        cursor.movePosition (QTextCursor::EndOfBlock);
                        if (end > 0 && cursor.anchor() > end)
                            return QTextCursor();
                        res.setPosition (cursor.position());
                        if (!cursor.movePosition (QTextCursor::NextBlock))
                            return QTextCursor();
                        ++i;
                    }
                    else
                    {
                        if ((found = document()->find (subStr, cursor, flags)).isNull())
                            return QTextCursor();
                        if (end > 0 && found.anchor() > end)
                            return QTextCursor();
                        cursor.setPosition (found.position());
                        while (!cursor.atBlockEnd())
                        {
                            cursor.movePosition (QTextCursor::EndOfBlock);
                            cursor.setPosition (cursor.position() - subStr.length());
                            if ((found = document()->find (subStr, cursor, flags)).isNull())
                                return QTextCursor();
                            if (end > 0 && found.anchor() > end)
                                return QTextCursor();
                            cursor.setPosition (found.position());
                        }

                        res.setPosition (found.anchor());
                        if (!cursor.movePosition (QTextCursor::NextBlock))
                            return QTextCursor();
                        ++i;
                    }
                }
                else if (i != sl.count() - 1)
                {
                    if (QString::compare (cursor.block().text(), sl.at (i), cs) != 0)
                    {
                        cursor.setPosition (res.position());
                        if (!cursor.movePosition (QTextCursor::NextBlock))
                            return QTextCursor();
                        i = 0;
                        continue;
                    }

                    if (!cursor.movePosition (QTextCursor::NextBlock))
                        return QTextCursor();
                    ++i;
                }
                else
                {
                    subStr = sl.at (i);
                    if (subStr.isEmpty()) break;
                    if (!(flags & QTextDocument::FindWholeWords))
                    {
                        if (!cursor.block().text().startsWith (subStr, cs))
                        {
                            cursor.setPosition (res.position());
                            if (!cursor.movePosition (QTextCursor::NextBlock))
                                return QTextCursor();
                            i = 0;
                            continue;
                        }
                        cursor.setPosition (cursor.anchor() + subStr.count());
                        break;
                    }
                    else
                    {
                        if ((found = document()->find (subStr, cursor, flags)).isNull()
                            || found.anchor() != cursor.position())
                        {
                            cursor.setPosition (res.position());
                            if (!cursor.movePosition (QTextCursor::NextBlock))
                                return QTextCursor();
                            i = 0;
                            continue;
                        }
                        cursor.setPosition (found.position());
                        break;
                    }
                }
            }
            res.setPosition (cursor.position(), QTextCursor::KeepAnchor);
        }
        else
        {
            cursor.setPosition (cursor.anchor());
            int endPos = cursor.position();
            while (i < sl.count())
            {
                if (i == 0)
                {
                    subStr = sl.at (sl.count() - 1);
                    if (subStr.isEmpty())
                    {
                        cursor.movePosition (QTextCursor::StartOfBlock);
                        endPos = cursor.position();
                        if (!cursor.movePosition (QTextCursor::PreviousBlock))
                            return QTextCursor();
                        cursor.movePosition (QTextCursor::EndOfBlock);
                        ++i;
                    }
                    else
                    {
                        if (!findBackward (document(), subStr, cursor, flags))
                            return QTextCursor();
                        while (cursor.anchor() > cursor.block().position())
                        {
                            cursor.setPosition (cursor.block().position() + subStr.count());
                            if (!findBackward (document(), subStr, cursor, flags))
                                return QTextCursor();
                        }

                        endPos = cursor.position();
                        if (!cursor.movePosition (QTextCursor::PreviousBlock))
                            return QTextCursor();
                        cursor.movePosition (QTextCursor::EndOfBlock);
                        ++i;
                    }
                }
                else if (i != sl.count() - 1) // the middle strings
                {
                    if (QString::compare (cursor.block().text(), sl.at (sl.count() - i - 1), cs) != 0)
                    {
                        cursor.setPosition (endPos);
                        if (!cursor.movePosition (QTextCursor::PreviousBlock))
                            return QTextCursor();
                        cursor.movePosition (QTextCursor::EndOfBlock);
                        i = 0;
                        continue;
                    }

                    if (!cursor.movePosition (QTextCursor::PreviousBlock))
                        return QTextCursor();
                    cursor.movePosition (QTextCursor::EndOfBlock);
                    ++i;
                }
                else
                {
                    subStr = sl.at (0);
                    if (subStr.isEmpty()) break;
                    if (!(flags & QTextDocument::FindWholeWords))
                    {
                        if (!cursor.block().text().endsWith (subStr, cs))
                        {
                            cursor.setPosition (endPos);
                            if (!cursor.movePosition (QTextCursor::PreviousBlock))
                                return QTextCursor();
                            cursor.movePosition (QTextCursor::EndOfBlock);
                            i = 0;
                            continue;
                        }
                        cursor.setPosition (cursor.anchor() - subStr.count());
                        break;
                    }
                    else
                    {
                        found = cursor;
                        if (!findBackward (document(), subStr, found, flags)
                            || found.position() != cursor.position())
                        {
                            cursor.setPosition (endPos);
                            if (!cursor.movePosition (QTextCursor::PreviousBlock))
                                return QTextCursor();
                            cursor.movePosition (QTextCursor::EndOfBlock);
                            i = 0;
                            continue;
                        }
                        cursor.setPosition (found.anchor());
                        break;
                    }
                }
            }
            res.setPosition (cursor.anchor());
            res.setPosition (endPos, QTextCursor::KeepAnchor);
        }
    }
    else
    {
        if (!(flags & QTextDocument::FindBackward))
        {
            res = document()->find (str, start, flags);
            if (end > 0 && res.anchor() > end)
                return QTextCursor();
        }
        else
            findBackward (document(), str, res, flags);
    }

    return res;
}

}
