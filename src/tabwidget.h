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

#ifndef TABWIDGET_H
#define TABWIDGET_H

#include <QTabWidget>
#include "tabbar.h"

namespace fpad {

class TabWidget : public QTabWidget
{
    Q_OBJECT

public:
    TabWidget (QWidget *parent = nullptr);
    ~TabWidget();
    /* make it public */
    TabBar* tabBar() const {return tb_;}
    void removeTab (int index);

    void noTabDND() {
        tb_->noTabDND();
    }

signals:
    void currentTabChanged (int curIndx);

private slots:
    void tabSwitch (int index);

private:
    TabBar *tb_;
    int curIndx_;
    /* This is the list of activated tabs, in the order of activation,
       and is used for finding the last active tab: */
    QList<QWidget*> activatedTabs_;
};

}

#endif // TABWIDGET_H
