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

#ifndef SEARCHBAR_H
#define SEARCHBAR_H

#include <QPointer>
#include <QToolButton>
#include <QComboBox>
#include <QStandardItemModel>
#include "lineedit.h"

namespace fpad {

class ComboBox : public QComboBox
{
    Q_OBJECT
public:
    ComboBox (QWidget *parent = nullptr) :
        QComboBox (parent), hasPopup_ (false) {}
    ~ComboBox() {}

    bool hasPopup() const;
    void showPopup() override;
    void hidePopup() override;

signals:
    void moveInHistory (bool up);

protected:
    void keyPressEvent (QKeyEvent *event) override;

private:
    bool hasPopup_;
};

class SearchBar : public QFrame
{
    Q_OBJECT
public:
    SearchBar (QWidget *parent = nullptr,
               const QList<QKeySequence>& shortcuts = QList<QKeySequence>(),
               Qt::WindowFlags f = Qt::WindowFlags());

    void focusLineEdit();
    bool lineEditHasFocus() const;
    QString searchEntry() const;
    void clearSearchEntry();

    bool matchCase() const;
    bool matchRegex() const;

    bool hasPopup() const;

    void updateShortcuts (bool disable);

signals:
    void searchFlagChanged();
    void find (bool forward);

private:
    void searchStarted();
    void findForward();
    void findBackward();

    QPointer<LineEdit> lineEdit_;
    QPointer<ComboBox> combo_;
    QPointer<QToolButton> toolButton_nxt_;
    QPointer<QToolButton> toolButton_prv_;
    QPointer<QToolButton> button_case_;
    QPointer<QToolButton> button_regex_;
    QList<QKeySequence> shortcuts_;
    bool searchStarted_;
    QString searchText_;
};

}

#endif // SEARCHBAR_H
