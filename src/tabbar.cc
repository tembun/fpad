/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2014-2019 <tsujan2000@gmail.com>
 *
 * fpad is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * fpad is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * @license GPL-3.0+ <https://spdx.org/licenses/GPL-3.0+.html>
 */

#include <QPointer>
#include <QMouseEvent>
#include <QDrag>
#include <QMimeData>
#include <QIcon>
#include <QApplication>
#include <QToolTip>
#include "tabbar.h"

namespace fpad {

TabBar::TabBar (QWidget *parent)
    : QTabBar (parent)
{
    setMouseTracking (true);
    setElideMode (Qt::ElideMiddle);
    dragStarted_ = false;
    noTabDND_ = false;
}
void TabBar::mousePressEvent (QMouseEvent *event)
{
    QTabBar::mousePressEvent (event);
    if (event->button() == Qt::LeftButton) {
        if (tabAt (event->pos()) > -1)
            dragStartPosition_ = event->pos();
        else if (event->type() == QEvent::MouseButtonDblClick && count() > 0)
            emit addEmptyTab();
    }
    dragStarted_ = false;
}
void TabBar::mouseMoveEvent (QMouseEvent *event)
{
    if (!dragStartPosition_.isNull()
        && (event->pos() - dragStartPosition_).manhattanLength() >= QApplication::startDragDistance())
    {
      dragStarted_ = true;
    }

    if (!noTabDND_
        && (event->buttons() & Qt::LeftButton)
        && dragStarted_
        && !window()->geometry().contains (event->globalPos()))
    {
        int index = currentIndex();
        if (index == -1)
        {
            event->accept();
            return;
        }
        QPointer<QDrag> drag = new QDrag (this);
        QMimeData *mimeData = new QMimeData;
        QByteArray array = (QString::number(window()->winId()) + "+" + QString::number(index)).toUtf8();
        mimeData->setData ("application/fpad-tab", array);
        drag->setMimeData (mimeData);
        int N = count();
        Qt::DropAction dragged = drag->exec (Qt::MoveAction);
        if (dragged != Qt::MoveAction)
        {
            if (N > 1)
                emit tabDetached();
            else
                finishMouseMoveEvent();
        }
        else
        {
            if (count() == N)
                releaseMouse();
        }
        event->accept();
        drag->deleteLater();
    }
    else
    {
        QTabBar::mouseMoveEvent (event);
    }
}
bool TabBar::event (QEvent *event)
{
#ifndef QT_NO_TOOLTIP
    if (event->type() == QEvent::ToolTip)
        return QWidget::event (event);
    else
       return QTabBar::event (event);
#else
    return QTabBar::event (event);
#endif
}
void TabBar::finishMouseMoveEvent()
{
    QMouseEvent finishingEvent (QEvent::MouseMove, QPoint(), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    mouseMoveEvent (&finishingEvent);
}
void TabBar::releaseMouse()
{
    QMouseEvent releasingEvent (QEvent::MouseButtonRelease, QPoint(), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    mouseReleaseEvent (&releasingEvent);
}

}
