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

#ifndef FILEDIALOG_H
#define FILEDIALOG_H

#include <QFileDialog>
#include <QTreeView>
#include <QTimer>
#include <QShortcut>

namespace FeatherPad {

static bool showHidden = false;

class FileDialog : public QFileDialog {
    Q_OBJECT
public:
    FileDialog (QWidget *parent) : QFileDialog (parent) {
        tView = nullptr;
        p = parent;
        setWindowModality (Qt::WindowModal);
        setViewMode (QFileDialog::Detail);
    }

    ~FileDialog() {
        showHidden = (filter() & QDir::Hidden);
    }

protected:
    void showEvent(QShowEvent * event) {
        QFileDialog::showEvent (event);
    }

private slots:
    void scrollToSelection() {
        if (tView)
        {
            QModelIndexList iList = tView->selectionModel()->selectedIndexes();
            if (!iList.isEmpty())
                tView->scrollTo (iList.at (0), QAbstractItemView::PositionAtCenter);
        }
    }

    void center() {
        move (p->x() + p->width()/2 - width()/2,
              p->y() + p->height()/2 - height()/ 2);
    }

    void toggleHidden() {
        showHidden = !(filter() & QDir::Hidden);
        if (showHidden)
            setFilter (filter() | QDir::Hidden);
        else
            setFilter (filter() & ~QDir::Hidden);
    }

private:
    QTreeView *tView;
    QWidget *p;
};

}

#endif // FILEDIALOG_H
