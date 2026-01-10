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

#ifndef FPWIN_H
#define FPWIN_H

#include <QListWidget>
#include <QEvent>
#include <QCollator>
#include "lineedit.h"
#include <QMainWindow>
#include <QActionGroup>
#include "textedit.h"
#include "tabpage.h"
#include "config.h"

namespace fpad {

namespace Ui {
class FPwin;
}

class BusyMaker : public QObject {
    Q_OBJECT

public:
    BusyMaker(){}
    ~BusyMaker(){}

public slots:
    void waiting();

private slots :
    void makeBusy();

signals:
    void finished();

private:
    static const int timeout = 1000;
};


// A fpad window.
class FPwin : public QMainWindow
{
    Q_OBJECT

public:
    explicit FPwin (QWidget *parent = nullptr, bool standalone = false);
    ~FPwin();
    
    bool isLoading() const {
        return (loadingProcesses_ > 0);
    }
    bool isReady() {
        if (loadingProcesses_ <= 0)
        {
            closeWarningBar();
            return true;
        }
        return false;
    }
    void showCrashWarning();
    void showRootWarning();
    void updateCustomizableShortcuts (bool disable = false);

    QHash<QAction*, QKeySequence> defaultShortcuts() const {
        return defaultShortcuts_;
    }

signals:
    void finishedLoading();

public slots:
    void newTabFromName (const QString& fileName,
                         int restoreCursor, /* ==  0 : Do not restore cursor.
                                               ==  1 : Restore cursor in a session file.
                                               == -1 : Restore cursor while opening last files with application startup.
                                               <  -1 : Move the cursor to the document end with command-line.
                                               >   1 : Move the cursor to the block restoreCursor-1 with command-line. */
                         int posInLine, // If restoreCursor > 1, this is the cursor position in the line.
                         bool multiple = false);
    void newTab();
    void enableSaving (bool modified);

private slots:
    void closeTab();
    void closeTabAtIndex (int index);
    void closeOtherTabs();
    void fileOpen();
    void reload();
    void enforceEncoding (QAction*);
    void cutText();
    void copyText();
    void pasteText();
    void deleteText();
    void selectAllText();
    void undoing();
    void redoing();
    void onTabChanged (int index);
    void tabSwitch (int index);
    void fontDialog();
    void find (bool forward);
    void hlight() const;
    void searchFlagChanged();
    void showHideSearch();
    void toggleWrapping();
    void toggleIndent();
    void replace();
    void replaceAll();
    void closeReplaceDock (bool visible);
    void replaceDock();
    void resizeDock (bool topLevel);
    void jumpTo();
    void setMax (const int max);
    void goTo();
    void asterisk (bool modified);
    void defaultSize();
    void focus_view_soft();
    void focus_view_hard();
    void nextTab();
    void previousTab();
    void editorContextMenu (const QPoint& p);
    void changeTab (QListWidgetItem *current, QListWidgetItem*);
    void prefDialog();
    void addText (const QString& text, const QString& fileName, const QString& charset,
                  bool enforceEncod, bool reload,
                  int restoreCursor, int posInLine,
                  bool uneditable,
                  bool multiple);
    void onOpeningHugeFiles();
    void onOpeninNonTextFiles();
    void onPermissionDenied();
    void onOpeningUneditable();
    void onOpeningNonexistent();

public:
    QWidget *dummyWidget;
    Ui::FPwin *ui;
    int already_opened_idx (const QString& fileName, bool& modified) const;
    void stealFocus();

private:
    enum DOCSTATE {
      SAVED,
      UNDECIDED,
      DISCARDED
    };

    TabPage *createEmptyTab(bool setCurrent);
    bool hasAnotherDialog();
    void deleteTabPage (int tabIndex, bool saveToList = false);
    void loadText (const QString& fileName, bool enforceEncod, bool reload,
                   int restoreCursor = 0, int posInLine = 0,
                   bool enforceUneditable = false, bool multiple = false);
    void setTitle (const QString& fileName, int tabIndex = -1);
    DOCSTATE savePrompt (int tabIndex, bool noToAll);
    bool saveFile ();
    void saveAllFiles (bool showWarning);
    void closeEvent (QCloseEvent *event);
    void apply_snippet(QString snip,int off_vert, int off_hor);
    bool closeTabs (int first, int last, bool saveFilesList = false);
    void changeEvent (QEvent *event);
    bool event (QEvent *event);
    QTextDocument::FindFlags getSearchFlags() const;
    void enableWidgets (bool enable) const;
    void updateShortcuts (bool disable, bool page = true);
    void encodingToCheck (const QString& encoding);
    const QString checkToEncoding() const;
    void applyConfigOnStarting();
    bool matchLeftParenthesis (QTextBlock currentBlock, int index, int numRightParentheses);
    bool matchRightParenthesis (QTextBlock currentBlock, int index, int numLeftParentheses);
    bool matchLeftBrace (QTextBlock currentBlock, int index, int numRightBraces);
    bool matchRightBrace (QTextBlock currentBlock, int index, int numLeftBraces);
    void createSelection (int pos);
    void removeGreenSel();
    void waitToMakeBusy();
    void unbusy();
    void showWarningBar (const QString& message, bool startupBar = false);
    void closeWarningBar (bool keepOnStartup = false);
    void disconnectLambda();
    QActionGroup *aGroup_;
    QString lastFile_;
    QHash<QString, QVariant> lastWinFilesCur_;
    QString txtReplace_;
    int rightClicked_;
    int loadingProcesses_;
    QPointer<QThread> busyThread_;
    QMetaObject::Connection lambdaConnection_;
    QHash<QListWidgetItem*, TabPage*> sideItems_;
    QHash<QString, QAction*> langs_;
    QHash<QAction*, QKeySequence> defaultShortcuts_;
    bool inactiveTabModified_;
    bool standalone_;
};

}

#endif // FPWIN_H
