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

#ifndef PREF_H
#define PREF_H

#include <QDialog>
#include <QStyledItemDelegate>
#include <QTableWidgetItem>
#include <QKeySequenceEdit>
#include "config.h"

namespace FeatherPad {

class FPKeySequenceEdit : public QKeySequenceEdit
{
    Q_OBJECT

public:
    FPKeySequenceEdit (QWidget *parent = nullptr);

protected:
    virtual void keyPressEvent (QKeyEvent *event);
};

class Delegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    Delegate (QObject *parent = nullptr);

    virtual QWidget* createEditor (QWidget *parent,
                                   const QStyleOptionViewItem&,
                                   const QModelIndex&) const;

protected:
    virtual bool eventFilter (QObject *object, QEvent *event);
};

namespace Ui {
class PrefDialog;
}

class PrefDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PrefDialog (QWidget *parent = nullptr);
    ~PrefDialog();

private slots:
    void onClosing();
    void prefSize (int checked);
    void prefStartSize (int value);
    void prefPos (int checked);
    void prefMaxSHSize (int value);
    void prefSplitterPos (int checked);
    void showWhatsThis();
    void prefShortcuts();
    void restoreDefaultShortcuts();
    void onShortcutChange (QTableWidgetItem *item);
    void prefSaveUnmodified();
    void prefTextTabSize (int value);
    void prefTextTab();

private:
    void closeEvent (QCloseEvent *event);
    void showPrompt (const QString& str = QString(), bool temporary = false);

    Ui::PrefDialog *ui;
    QWidget * parent_;
    bool saveUnmodified_;
    int textTabSize_;
    QHash<QString, QString> shortcuts_, newShortcuts_;
    QString prevtMsg_;
    QTimer *promptTimer_;
};

}

#endif // PREF_H
