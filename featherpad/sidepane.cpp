/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2017-2020 <tsujan2000@gmail.com>
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
#include <QGridLayout>
#include <QToolTip>
#include "sidepane.h"

namespace FeatherPad {

bool ListWidgetItem::operator<(const QListWidgetItem &other) const {
    /* treat dot as a separator for a natural sorting */
    const QString txt1 = text();
    const QString txt2 = other.text();
    int start1 = 0, start2 = 0;
    for (;;)
    {
        int end1 = txt1.indexOf (QLatin1Char ('.'), start1);
        int end2 = txt2.indexOf (QLatin1Char ('.'), start2);
        int comp = collator_.compare (txt1.mid(start1, end1 - start1),
                                      txt2.mid(start2, end2 - start2));
        if (comp != 0)
            return comp < 0;
        if (end1 == -1 || end2 == -1)
            return end1 < end2;
        start1 = end1 + 1;
        start2 = end2 + 1;
    }
}
/*************************/
// See "qabstractitemview.cpp".
QItemSelectionModel::SelectionFlags ListWidget::selectionCommand (const QModelIndex &index, const QEvent *event) const
{
    Qt::KeyboardModifiers keyModifiers = Qt::NoModifier;
    if (event)
    {
        switch (event->type()) {
            case QEvent::MouseButtonDblClick:
            case QEvent::MouseButtonPress:
            case QEvent::MouseButtonRelease:
            case QEvent::MouseMove:
            case QEvent::KeyPress:
            case QEvent::KeyRelease:
                keyModifiers = (static_cast<const QInputEvent*>(event))->modifiers();
                break;
            default:
                keyModifiers = QApplication::keyboardModifiers();
        }
    }
    if (selectionMode() == QAbstractItemView::SingleSelection && (keyModifiers & Qt::ControlModifier) && selectionModel()->isSelected (index))
        return QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows;
    else
        return QListWidget::selectionCommand (index, event);
}
/*************************/
void ListWidget::mousePressEvent (QMouseEvent *event)
{
    if (selectionMode() == QAbstractItemView::SingleSelection)
    {
        if (event->button() == Qt::MidButton)
        {
            QModelIndex index = indexAt (event->pos());
            if (QListWidgetItem *item = itemFromIndex (index)) // index is checked for its validity in QListWidget::itemFromIndex()
                emit closeItem (item);
            else
                emit closeSidePane();
            return;
        }
        else if (event->button() == Qt::RightButton)
            return;
    }
    QListWidget::mousePressEvent (event);
}
/*************************/
void ListWidget::mouseMoveEvent (QMouseEvent *event)
{
    QListWidget::mouseMoveEvent (event);
    if (event->button() == Qt::NoButton && !(event->buttons() & Qt::LeftButton))
    {
        if (QListWidgetItem *item = itemAt (event->pos()))
            QToolTip::showText (event->globalPos(), item->toolTip());
        else
            QToolTip::hideText();
    }
}
/*************************/
QListWidgetItem *ListWidget::getItemFromIndex (const QModelIndex &index) const
{
    return itemFromIndex (index);
}
/*************************/
void ListWidget::rowsInserted (const QModelIndex &parent, int start, int end)
{
    emit rowsAreInserted (start, end);
    QListView::rowsInserted (parent, start, end);
}
/*************************/
SidePane::SidePane (QWidget *parent)
    : QWidget (parent)
{
    filterTimer_ = nullptr;

    QGridLayout *mainGrid = new QGridLayout;
    mainGrid->setVerticalSpacing (4);
    mainGrid->setContentsMargins (0, 0, 0, 0);
    lw_ = new ListWidget (this);
    lw_->setSortingEnabled (true);
    lw_->setSelectionMode (QAbstractItemView::SingleSelection);
    lw_->setContextMenuPolicy (Qt::CustomContextMenu);
    mainGrid->addWidget (lw_, 0, 0);
    le_ = new LineEdit (this);
    le_->setPlaceholderText (tr ("Filter..."));
    mainGrid->addWidget (le_, 1, 0);
    setLayout (mainGrid);

    connect (le_, &QLineEdit::textChanged, this, &SidePane::filter);
    connect (lw_, &ListWidget::rowsAreInserted, this, &SidePane::onRowsInserted);
    connect (lw_, &ListWidget::itemChanged, [this] (QListWidgetItem *item) {
        if (item->text().contains (le_->text(), Qt::CaseInsensitive))
            item->setHidden (false);
        else
            item->setHidden (true);
    });
}
/*************************/
SidePane::~SidePane()
{
    if (filterTimer_)
    {
        disconnect (filterTimer_, &QTimer::timeout, this, &SidePane::reallyApplyFilter);
        filterTimer_->stop();
        delete filterTimer_;
    }
}
/*************************/
void SidePane::filter (const QString&/*text*/)
{
    if (!filterTimer_)
    {
        filterTimer_ = new QTimer();
        filterTimer_->setSingleShot (true);
        connect (filterTimer_, &QTimer::timeout, this, &SidePane::reallyApplyFilter);
    }
    filterTimer_->start (200);
}
/*************************/
// Simply hide items that don't contain the filter string.
// Clearing items isn't an option because they are tracked
// for their correspondence with tab pages.
void SidePane::reallyApplyFilter()
{
    for (int i = 0; i < lw_->count(); ++i)
    {
        QListWidgetItem *wi = lw_->item (i);
        if (wi->text().contains (le_->text(), Qt::CaseInsensitive))
            wi->setHidden (false);
        else
            wi->setHidden (true);
    }
    lw_->scrollToItem (lw_->currentItem());
}
/*************************/
void SidePane::onRowsInserted (int start, int end)
{
    for (int i = start; i <= end; ++i)
    {
        if (lw_->item (i) && !lw_->item (i)->text().contains (le_->text(), Qt::CaseInsensitive))
            lw_->item (i)->setHidden (true);
    }
}

}
