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

#ifndef SIDEPANE_H
#define SIDEPANE_H

#include <QEvent>
#include <QTimer>
#include <QCollator>
#include <QListWidget>
#include "lineedit.h"

namespace FeatherPad {

/* For having control over sorting. */
class ListWidgetItem : public QListWidgetItem
{
public:
    ListWidgetItem (const QIcon &icon, const QString &text, QListWidget *parent = nullptr, int type = QListWidgetItem::Type) :
        QListWidgetItem (icon, text, parent, type) {
        collator_.setNumericMode (true);
    }
    ListWidgetItem (const QString &text, QListWidget *parent = nullptr, int type = QListWidgetItem::Type) :
        QListWidgetItem (text, parent, type) {
        collator_.setNumericMode (true);
    }

    bool operator<(const QListWidgetItem &other) const override;

private:
    QCollator collator_;
};

/* In the single-selection mode, we don't want Ctrl + left click to
   deselect an item or an item to be selected with the middle click. */
class ListWidget : public QListWidget
{
    Q_OBJECT
public:
    ListWidget (QWidget *parent = nullptr) : QListWidget (parent) {
        setMouseTracking (true); // for instant tooltips
    }

    QListWidgetItem *getItemFromIndex (const QModelIndex &index) const;

signals:
    void closeItem (QListWidgetItem *item);
    void closeSidePane();
    void rowsAreInserted (int start, int end);

protected:
    QItemSelectionModel::SelectionFlags selectionCommand (const QModelIndex &index, const QEvent *event = nullptr) const override;
    void mousePressEvent (QMouseEvent *event) override;
    void mouseMoveEvent (QMouseEvent *event) override; // for instant tooltips

protected slots:
    void rowsInserted (const QModelIndex &parent, int start, int end) override;
};

class SidePane : public QWidget
{
    Q_OBJECT
public:
    SidePane (QWidget *parent = nullptr);
    ~SidePane();

    ListWidget* listWidget() const {
        return lw_;
    }

private slots:
    void filter (const QString&);
    void reallyApplyFilter();
    void onRowsInserted (int start, int end);

private:
    ListWidget *lw_;
    LineEdit *le_;
    QTimer *filterTimer_;
};

}

#endif // SIDEPANE_H
