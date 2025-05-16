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

#ifndef TABBAR_H
#define TABBAR_H

#include <QTabBar>

namespace fpad {
class TabBar : public QTabBar
{
    Q_OBJECT

public:
    TabBar (QWidget *parent = nullptr);
    void finishMouseMoveEvent();
    void releaseMouse();
    void hideSingle (bool hide) {
        hideSingle_ = hide;
    }
    void noTabDND() {
        noTabDND_ = true;
    }

signals:
    void tabDetached();
    void addEmptyTab();
    void hideTabBar();

protected:
    void mousePressEvent (QMouseEvent *event) override;
    void mouseReleaseEvent (QMouseEvent *event) override;
    void mouseMoveEvent (QMouseEvent *event) override;
    bool event (QEvent *event) override;
    void wheelEvent (QWheelEvent *event) override;
    QSize tabSizeHint (int index) const override;
    QSize minimumTabSizeHint (int index) const override;

private:
    QPoint dragStartPosition_;
    bool dragStarted_;
    bool hideSingle_;
    bool noTabDND_;
};

}

#endif // TABBAR_H
