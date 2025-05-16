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

#include "lineedit.h"
#include <QToolButton>

namespace FeatherPad {

LineEdit::LineEdit (QWidget *parent)
    : QLineEdit (parent)
{
    setClearButtonEnabled (true);
    QList<QToolButton*> list = findChildren<QToolButton*>();
    if (list.isEmpty()) return;
    QToolButton *clearButton = list.at (0);
    if (clearButton)
    {
        clearButton->setToolTip (tr ("Clear text (Ctrl+K)"));
        connect (clearButton, &QAbstractButton::clicked, this, &LineEdit::returnPressed);
    }
}

void LineEdit::keyPressEvent (QKeyEvent *event)
{
    /* because of a bug in Qt5, the non-breaking space (ZWNJ) isn't inserted with SHIFT+SPACE */
    if (event->key() == 0x200c)
    {
        insert (QChar (0x200C));
        event->accept();
        return;
    }
    if (event->modifiers() == Qt::ControlModifier)
    {
        if (event->key() == Qt::Key_Up || event->key() == Qt::Key_Down)
        {
            emit showComboPopup();
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_K)
        {
            clear();
            returnPressed();
        }
    }
    
    if(
  	( event->key() == Qt::Key_Return )
  	&&
  	( event -> modifiers() == Qt :: ShiftModifier )
    )
    {
    	
	emit shift_enter_pressed();
	
	event->accept();
	return;
	
    }
    
    QLineEdit::keyPressEvent (event);
}

void LineEdit::focusInEvent (QFocusEvent * ev)
{
    /* first do what QLineEdit does */
    QLineEdit::focusInEvent (ev);
    emit receivedFocus();
}

}
