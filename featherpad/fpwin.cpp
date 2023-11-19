/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2014-2020 <tsujan2000@gmail.com>
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

#include "singleton.h"
#include "ui_fp.h"
#include "ui_about.h"
#include "encoding.h"
#include "filedialog.h"
#include "messagebox.h"
#include "pref.h"
#include "session.h"
#include "fontDialog.h"
#include "loading.h"
#include "warningbar.h"
#include "svgicons.h"

#include <QPrintDialog>
#include <QToolTip>
#include <QWindow>
#include <QScrollBar>
#include <QWidgetAction>
#include <fstream> // std::ofstream
#include <QPrinter>
#include <QClipboard>
#include <QProcess>
#include <QTextDocumentWriter>
#include <QTextCodec>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDesktopServices>
#include <QPushButton>

#ifdef HAS_X11
#include "x11.h"
#endif

namespace FeatherPad {

void BusyMaker::waiting() {
    QTimer::singleShot (timeout, this, &BusyMaker::makeBusy);
}

void BusyMaker::makeBusy() {
    if (QGuiApplication::overrideCursor() == nullptr)
        QGuiApplication::setOverrideCursor (QCursor (Qt::WaitCursor));
    emit finished();
}


FPwin::FPwin (QWidget *parent, bool standalone):QMainWindow (parent), dummyWidget (nullptr), ui (new Ui::FPwin)
{
    ui->setupUi (this);

    standalone_ = standalone;

    loadingProcesses_ = 0;
    rightClicked_ = -1;
    busyThread_ = nullptr;

    autoSaver_ = nullptr;
    autoSaverRemainingTime_ = -1;
    inactiveTabModified_ = false;

    /* "Jump to" bar */
    ui->spinBox->hide();
    ui->label->hide();
    ui->checkBox->hide();

    /* status bar */
    QLabel *statusLabel = new QLabel();
    statusLabel->setObjectName ("statusLabel");
    statusLabel->setIndent (2);
    statusLabel->setMinimumWidth (100);
    statusLabel->setTextInteractionFlags (Qt::TextSelectableByMouse);
    QToolButton *wordButton = new QToolButton();
    wordButton->setObjectName ("wordButton");
    wordButton->setFocusPolicy (Qt::NoFocus);
    wordButton->setAutoRaise (true);
    wordButton->setToolButtonStyle (Qt::ToolButtonIconOnly);
    wordButton->setIconSize (QSize (16, 16));
    wordButton->setIcon (symbolicIcon::icon (":icons/view-refresh.svg"));
    wordButton->setToolTip ("<p style='white-space:pre'>"
                            + tr ("Calculate number of words\n(For huge texts, this may be CPU-intensive.)")
                            + "</p>");
    connect (wordButton, &QAbstractButton::clicked, [=]{updateWordInfo();});
    ui->statusBar->addWidget (statusLabel);
    ui->statusBar->addWidget (wordButton);

    /* text unlocking */
    ui->actionEdit->setVisible (false);

    ui->actionRun->setVisible (false);

    /* replace dock */
    QWidget::setTabOrder (ui->lineEditFind, ui->lineEditReplace);
    QWidget::setTabOrder (ui->lineEditReplace, ui->toolButtonNext);
    /* tooltips are set here for easier translation */
    ui->toolButtonNext->setToolTip (tr ("Next") + " (" + QKeySequence (Qt::Key_F8).toString (QKeySequence::NativeText) + ")");
    ui->toolButtonPrv->setToolTip (tr ("Previous") + " (" + QKeySequence (Qt::Key_F9).toString (QKeySequence::NativeText) + ")");
    ui->toolButtonAll->setToolTip (tr ("Replace all") + " (" + QKeySequence (Qt::Key_F10).toString (QKeySequence::NativeText) + ")");
    ui->dockReplace->setVisible (false);
    if (QApplication::layoutDirection() == Qt::RightToLeft)
    {
        ui->actionRightTab->setShortcut (QKeySequence (Qt::ALT + Qt::Key_Left));
        ui->actionLeftTab->setShortcut (QKeySequence (Qt::ALT + Qt::Key_Right));
    }
    static const QStringList excluded = {"actionCut", "actionCopy", "actionPaste", "actionSelectAll"};
    const auto allMenus = ui->menuBar->findChildren<QMenu*>();
    for (const auto &thisMenu : allMenus)
    {
        const auto menuActions = thisMenu->actions();
        for (const auto &menuAction : menuActions)
        {
            QKeySequence seq = menuAction->shortcut();
            if (!seq.isEmpty() && !excluded.contains (menuAction->objectName()))
                defaultShortcuts_.insert (menuAction, seq);
        }
    }
    defaultShortcuts_.insert (ui->actionSaveAllFiles, QKeySequence());
    defaultShortcuts_.insert (ui->actionFont, QKeySequence());
    applyConfigOnStarting();
    QWidget* spacer = new QWidget();
    spacer->setSizePolicy (QSizePolicy::Expanding, QSizePolicy::Preferred);
    ui->mainToolBar->insertWidget (ui->actionMenu, spacer);
    QMenu *menu = new QMenu (ui->mainToolBar);
    menu->addMenu (ui->menuFile);
    menu->addMenu (ui->menuEdit);
    menu->addMenu (ui->menuOptions);
    menu->addMenu (ui->menuSearch);
    ui->actionMenu->setMenu(menu);
    QList<QToolButton*> tbList = ui->mainToolBar->findChildren<QToolButton*>();
    if (!tbList.isEmpty())
        tbList.at (tbList.count() - 1)->setPopupMode (QToolButton::InstantPopup);

    newTab();

    aGroup_ = new QActionGroup (this);
    ui->actionUTF_8->setActionGroup (aGroup_);
    ui->actionUTF_16->setActionGroup (aGroup_);
    ui->actionWindows_Arabic->setActionGroup (aGroup_);
    ui->actionISO_8859_1->setActionGroup (aGroup_);
    ui->actionISO_8859_15->setActionGroup (aGroup_);
    ui->actionWindows_1252->setActionGroup (aGroup_);
    ui->actionCyrillic_CP1251->setActionGroup (aGroup_);
    ui->actionCyrillic_KOI8_U->setActionGroup (aGroup_);
    ui->actionCyrillic_ISO_8859_5->setActionGroup (aGroup_);
    ui->actionChinese_BIG5->setActionGroup (aGroup_);
    ui->actionChinese_GB18030->setActionGroup (aGroup_);
    ui->actionJapanese_ISO_2022_JP->setActionGroup (aGroup_);
    ui->actionJapanese_ISO_2022_JP_2->setActionGroup (aGroup_);
    ui->actionJapanese_ISO_2022_KR->setActionGroup (aGroup_);
    ui->actionJapanese_CP932->setActionGroup (aGroup_);
    ui->actionJapanese_EUC_JP->setActionGroup (aGroup_);
    ui->actionKorean_CP949->setActionGroup (aGroup_);
    ui->actionKorean_CP1361->setActionGroup (aGroup_);
    ui->actionKorean_EUC_KR->setActionGroup (aGroup_);
    ui->actionOther->setActionGroup (aGroup_);

    ui->actionUTF_8->setChecked (true);
    ui->actionOther->setDisabled (true);

    if (standalone_
        /* since Wayland has a serious issue related to QDrag that interferes with
           dropping tabs outside all windows, we don't enable tab DND without X11 */
        || !static_cast<FPsingleton*>(qApp)->isX11())
    {
        ui->tabWidget->noTabDND();
    }
    connect (ui->actionNew, &QAction::triggered, this, &FPwin::newTab);
    connect (ui->tabWidget->tabBar(), &TabBar::addEmptyTab, this, &FPwin::newTab);
    connect (ui->actionRightTab, &QAction::triggered, this, &FPwin::nextTab);
    connect (ui->actionLeftTab, &QAction::triggered, this, &FPwin::previousTab);
    connect (ui->actionClose, &QAction::triggered, this, &FPwin::closeTab);
    connect (ui->tabWidget, &QTabWidget::tabCloseRequested, this, &FPwin::closeTabAtIndex);
    connect (ui->actionOpen, &QAction::triggered, this, &FPwin::fileOpen);
    connect (ui->actionReload, &QAction::triggered, this, &FPwin::reload);
    connect (aGroup_, &QActionGroup::triggered, this, &FPwin::enforceEncoding);
    connect (ui->actionSave, &QAction::triggered, [=]{saveFile (false);});
    connect (ui->actionSaveAs, &QAction::triggered, this, [=]{saveFile (false);});
    connect (ui->actionSaveAllFiles, &QAction::triggered, this, [=]{saveAllFiles (true);});
    connect (ui->actionCut, &QAction::triggered, this, &FPwin::cutText);
    connect (ui->actionCopy, &QAction::triggered, this, &FPwin::copyText);
    connect (ui->actionPaste, &QAction::triggered, this, &FPwin::pasteText);
    connect (ui->actionDelete, &QAction::triggered, this, &FPwin::deleteText);
    connect (ui->actionSelectAll, &QAction::triggered, this, &FPwin::selectAllText);
    connect (ui->actionEdit, &QAction::triggered, this, &FPwin::makeEditable);
    connect (ui->actionRun, &QAction::triggered, this, &FPwin::executeProcess);
    connect (ui->actionUndo, &QAction::triggered, this, &FPwin::undoing);
    connect (ui->actionRedo, &QAction::triggered, this, &FPwin::redoing);
    connect (ui->tabWidget, &QTabWidget::currentChanged, this, &FPwin::onTabChanged);
    connect (ui->tabWidget, &TabWidget::currentTabChanged, this, &FPwin::tabSwitch);
    ui->tabWidget->tabBar()->setContextMenuPolicy (Qt::CustomContextMenu);
    connect (ui->tabWidget->tabBar(), &QWidget::customContextMenuRequested, this, &FPwin::tabContextMenu);
    connect (ui->actionCopyName, &QAction::triggered, this, &FPwin::copyTabFileName);
    connect (ui->actionCopyPath, &QAction::triggered, this, &FPwin::copyTabFilePath);
    connect (ui->actionCloseAll, &QAction::triggered, this, &FPwin::closeAllTabs);
    connect (ui->actionCloseRight, &QAction::triggered, this, &FPwin::closeNextTabs);
    connect (ui->actionCloseLeft, &QAction::triggered, this, &FPwin::closePreviousTabs);
    connect (ui->actionCloseOther, &QAction::triggered, this, &FPwin::closeOtherTabs);
    connect (ui->actionFont, &QAction::triggered, this, &FPwin::fontDialog);
    connect (ui->actionFind, &QAction::triggered, this, &FPwin::showHideSearch);
    connect (ui->actionJump, &QAction::triggered, this, &FPwin::jumpTo);
    connect (ui->spinBox, &QAbstractSpinBox::editingFinished, this, &FPwin::goTo);
    connect (ui->actionLineNumbers, &QAction::toggled, this, &FPwin::showLN);
    connect (ui->actionWrap, &QAction::triggered, this, &FPwin::toggleWrapping);
    connect (ui->actionSyntax, &QAction::triggered, this, &FPwin::toggleSyntaxHighlighting);
    connect (ui->actionIndent, &QAction::triggered, this, &FPwin::toggleIndent);
    connect (ui->actionPreferences, &QAction::triggered, this, &FPwin::prefDialog);
    connect (ui->actionReplace, &QAction::triggered, this, &FPwin::replaceDock);
    connect (ui->toolButtonNext, &QAbstractButton::clicked, this, &FPwin::replace);
    connect (ui->toolButtonPrv, &QAbstractButton::clicked, this, &FPwin::replace);
    connect (ui->toolButtonAll, &QAbstractButton::clicked, this, &FPwin::replaceAll);
    connect (ui->dockReplace, &QDockWidget::visibilityChanged, this, &FPwin::closeReplaceDock);
    connect (ui->dockReplace, &QDockWidget::topLevelChanged, this, &FPwin::resizeDock);
    connect (ui->actionDoc, &QAction::triggered, this, &FPwin::docProp);
    connect (ui->actionPrint, &QAction::triggered, this, &FPwin::filePrint);
    connect (this, &FPwin::finishedLoading, [this]{});
    /***************************************************************************
     *****     KDE (KAcceleratorManager) has a nasty "feature" that        *****
     *****   "smartly" gives mnemonics to tab and tool button texts so     *****
     *****   that, sometimes, the same mnemonics are disabled in the GUI   *****
     *****     and, as a result, their corresponding action shortcuts      *****
     *****     become disabled too. As a workaround, we don't set text     *****
     *****     for tool buttons on the search bar and replacement dock.    *****
     ***** The toolbar buttons and menu items aren't affected by this bug. *****
     ***************************************************************************/
    ui->toolButtonNext->setShortcut (QKeySequence (Qt::Key_F8));
    ui->toolButtonPrv->setShortcut (QKeySequence (Qt::Key_F9));
    ui->toolButtonAll->setShortcut (QKeySequence (Qt::Key_F10));

    QShortcut *fullscreen = new QShortcut (QKeySequence (Qt::Key_F11), this);
    QShortcut *defaultsize = new QShortcut (QKeySequence (Qt::CTRL + Qt::SHIFT + Qt::Key_W), this);
    connect (fullscreen, &QShortcut::activated, [this] {setWindowState (windowState() ^ Qt::WindowFullScreen);});
    connect (defaultsize, &QShortcut::activated, this, &FPwin::defaultSize);

    QShortcut *focus_view_hard = new QShortcut (QKeySequence (Qt::Key_Escape), this);
    connect (focus_view_hard, &QShortcut::activated, this, &FPwin::focus_view_hard);
    
    QShortcut *focus_view_soft = (
    	new QShortcut( QKeySequence( Qt::ALT | Qt::Key_2 ) , this )
    );
    connect( focus_view_soft , &QShortcut::activated , this, &FPwin::focus_view_soft );
    QShortcut *kill = new QShortcut (QKeySequence (Qt::CTRL + Qt::ALT + Qt::Key_E), this);
    connect (kill, &QShortcut::activated, this, &FPwin::exitProcess);
    dummyWidget = new QWidget();
    setAcceptDrops (true);
    setAttribute (Qt::WA_AlwaysShowToolTips);
    setAttribute (Qt::WA_DeleteOnClose, false); // we delete windows in singleton
}
FPwin::~FPwin()
{
    startAutoSaving (false);
    delete dummyWidget; dummyWidget = nullptr;
    delete aGroup_; aGroup_ = nullptr;
    delete ui; ui = nullptr;
}
void FPwin::closeEvent (QCloseEvent *event)
{
    bool keep = closeTabs (-1, -1, true);
    if (keep)
    {
        event->ignore();
        lastWinFilesCur_.clear(); // just a precaution; it's done at closeTabs()
    }
    else
    {
        FPsingleton *singleton = static_cast<FPsingleton*>(qApp);
        Config& config = singleton->getConfig();
        if (config.getRemSize() && windowState() == Qt::WindowNoState)
            config.setWinSize (size());
        if (config.getRemPos())
            config.setWinPos (pos());
        config.setLastFileCursorPos (lastWinFilesCur_);
        singleton->removeWin (this);
        event->accept();
    }
}
void FPwin::applyConfigOnStarting()
{
    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();

    if (config.getRemSize())
    {
        resize (config.getWinSize());
        if (config.getIsMaxed())
            setWindowState (Qt::WindowMaximized);
        if (config.getIsFull() && config.getIsMaxed())
            setWindowState (windowState() ^ Qt::WindowFullScreen);
        else if (config.getIsFull())
            setWindowState (Qt::WindowFullScreen);
    }
    else
    {
        QSize startSize = config.getStartSize();
        if (startSize.isEmpty())
        {
            startSize = QSize (700, 500);
            config.setStartSize (startSize);
        }
        resize (startSize);
    }

    if (config.getRemPos())
        move (config.getWinPos());

    ui->mainToolBar->setVisible (!config.getNoToolbar());
    ui->menuBar->setVisible (!config.getNoMenubar());
    ui->menuBar->actions().at( 1 )->setVisible( false );
    ui->menuBar->actions().at( 3 )->setVisible( false );
    ui->actionMenu->setVisible (config.getNoMenubar());
    ui->actionDoc->setVisible (!config.getShowStatusbar());
    ui->actionWrap->setChecked (config.getWrapByDefault());
    ui->actionIndent->setChecked (config.getIndentByDefault());
    ui->actionLineNumbers->setChecked (config.getLineByDefault());
    ui->actionLineNumbers->setDisabled (config.getLineByDefault());
    ui->actionSyntax->setChecked (config.getSyntaxByDefault());
    if (!config.getShowStatusbar())
        ui->statusBar->hide();
    else
    {
        if (config.getShowCursorPos())
            addCursorPosLabel();
    }
    if (config.getShowLangSelector() && config.getSyntaxByDefault())
        addRemoveLangBtn (true);
    if (config.getTabPosition() != 0)
    {
        ui->tabWidget->setTabPosition (static_cast<QTabWidget::TabPosition>(config.getTabPosition()));
    }
        ui->tabWidget->tabBar()->hideSingle (config.getHideSingleTab());
    int recentNumber = config.getCurRecentFilesNumber();
    {
        QAction* recentAction = nullptr;
        for (int i = 0; i < recentNumber; ++i)
        {
            recentAction = new QAction (this);
            recentAction->setVisible (false);
            connect (recentAction, &QAction::triggered, this, &FPwin::newTabFromRecent);
        }
    }

    ui->actionSave->setEnabled (config.getSaveUnmodified());

    ui->actionNew->setIcon (symbolicIcon::icon (":icons/document-new.svg"));
    ui->actionOpen->setIcon (symbolicIcon::icon (":icons/document-open.svg"));
    ui->actionSave->setIcon (symbolicIcon::icon (":icons/document-save.svg"));
    ui->actionSaveAs->setIcon (symbolicIcon::icon (":icons/document-save-as.svg"));
    ui->actionSaveAllFiles->setIcon (symbolicIcon::icon (":icons/document-save-all.svg"));
    ui->actionPrint->setIcon (symbolicIcon::icon (":icons/document-print.svg"));
    ui->actionDoc->setIcon (symbolicIcon::icon (":icons/document-properties.svg"));
    ui->actionUndo->setIcon (symbolicIcon::icon (":icons/edit-undo.svg"));
    ui->actionRedo->setIcon (symbolicIcon::icon (":icons/edit-redo.svg"));
    ui->actionCut->setIcon (symbolicIcon::icon (":icons/edit-cut.svg"));
    ui->actionCopy->setIcon (symbolicIcon::icon (":icons/edit-copy.svg"));
    ui->actionPaste->setIcon (symbolicIcon::icon (":icons/edit-paste.svg"));
    ui->actionDelete->setIcon (symbolicIcon::icon (":icons/edit-delete.svg"));
    ui->actionSelectAll->setIcon (symbolicIcon::icon (":icons/edit-select-all.svg"));
    ui->actionReload->setIcon (symbolicIcon::icon (":icons/view-refresh.svg"));
    ui->actionClose->setIcon (symbolicIcon::icon (":icons/window-close.svg"));
    ui->actionQuit->setIcon (symbolicIcon::icon (":icons/application-exit.svg"));
    ui->actionFont->setIcon (symbolicIcon::icon (":icons/preferences-desktop-font.svg"));
    ui->actionPreferences->setIcon (symbolicIcon::icon (":icons/preferences-system.svg"));
    ui->actionEdit->setIcon (symbolicIcon::icon (":icons/document-edit.svg"));
    ui->actionRun->setIcon (symbolicIcon::icon (":icons/system-run.svg"));
    ui->actionCopyName->setIcon (symbolicIcon::icon (":icons/edit-copy.svg"));
    ui->actionCopyPath->setIcon (symbolicIcon::icon (":icons/edit-copy.svg"));

    ui->actionCloseOther->setIcon (symbolicIcon::icon (":icons/tab-close-other.svg"));
    ui->actionMenu->setIcon (symbolicIcon::icon (":icons/application-menu.svg"));

    ui->toolButtonNext->setIcon (symbolicIcon::icon (":icons/go-down.svg"));
    ui->toolButtonPrv->setIcon (symbolicIcon::icon (":icons/go-up.svg"));
    ui->toolButtonAll->setIcon (symbolicIcon::icon (":icons/arrow-down-double.svg"));

    if (QApplication::layoutDirection() == Qt::RightToLeft)
    {
        ui->actionCloseRight->setIcon (symbolicIcon::icon (":icons/go-previous.svg"));
        ui->actionCloseLeft->setIcon (symbolicIcon::icon (":icons/go-next.svg"));
        ui->actionRightTab->setIcon (symbolicIcon::icon (":icons/go-previous.svg"));
        ui->actionLeftTab->setIcon (symbolicIcon::icon (":icons/go-next.svg"));
    }
    else
    {
        ui->actionCloseRight->setIcon (symbolicIcon::icon (":icons/go-next.svg"));
        ui->actionCloseLeft->setIcon (symbolicIcon::icon (":icons/go-previous.svg"));
        ui->actionRightTab->setIcon (symbolicIcon::icon (":icons/go-next.svg"));
        ui->actionLeftTab->setIcon (symbolicIcon::icon (":icons/go-previous.svg"));
    }

    QIcon icn = QIcon::fromTheme ("featherpad");
    if (icn.isNull())
        icn = QIcon (":icons/featherpad.svg");
    setWindowIcon (icn);

    if (!config.hasReservedShortcuts())
    { // the reserved shortcuts list could also be in "singleton.cpp"
        QStringList reserved;
                    /* QPLainTextEdit */
        reserved << QKeySequence (Qt::CTRL + Qt::SHIFT + Qt::Key_Z).toString() << QKeySequence (Qt::CTRL + Qt::Key_Z).toString() << QKeySequence (Qt::CTRL + Qt::Key_X).toString() << QKeySequence (Qt::CTRL + Qt::Key_C).toString() << QKeySequence (Qt::CTRL + Qt::Key_V).toString() << QKeySequence (Qt::CTRL + Qt::Key_A).toString()
                 << QKeySequence (Qt::SHIFT + Qt::Key_Insert).toString() << QKeySequence (Qt::SHIFT + Qt::Key_Delete).toString() << QKeySequence (Qt::CTRL + Qt::Key_Insert).toString()
                 << QKeySequence (Qt::CTRL + Qt::Key_Left).toString() << QKeySequence (Qt::CTRL + Qt::Key_Right).toString() << QKeySequence (Qt::CTRL + Qt::Key_Up).toString() << QKeySequence (Qt::CTRL + Qt::Key_Down).toString()
                 << QKeySequence (Qt::CTRL + Qt::Key_Home).toString() << QKeySequence (Qt::CTRL + Qt::Key_End).toString()
                 << QKeySequence (Qt::CTRL + Qt::SHIFT + Qt::Key_Up).toString() << QKeySequence (Qt::CTRL + Qt::SHIFT + Qt::Key_Down).toString()
                 << QKeySequence (Qt::META + Qt::Key_Up).toString() << QKeySequence (Qt::META + Qt::Key_Down).toString() << QKeySequence (Qt::META + Qt::SHIFT + Qt::Key_Up).toString() << QKeySequence (Qt::META + Qt::SHIFT + Qt::Key_Down).toString()

                    /* search and replacement */
                 << QKeySequence (Qt::Key_F3).toString() << QKeySequence (Qt::Key_F4).toString() << QKeySequence (Qt::Key_F5).toString() << QKeySequence (Qt::Key_F6).toString() << QKeySequence (Qt::Key_F7).toString()
                 << QKeySequence (Qt::Key_F8).toString() << QKeySequence (Qt::Key_F9).toString() << QKeySequence (Qt::Key_F10).toString()
                 << QKeySequence (Qt::Key_F11).toString() << QKeySequence (Qt::CTRL + Qt::SHIFT + Qt::Key_W).toString()

                 << QKeySequence (Qt::CTRL + Qt::ALT  +Qt::Key_E).toString() // exiting a process
                 << QKeySequence (Qt::SHIFT + Qt::Key_Enter).toString() << QKeySequence (Qt::SHIFT + Qt::Key_Return).toString() << QKeySequence (Qt::CTRL + Qt::Key_Tab).toString() << QKeySequence (Qt::CTRL + Qt::META + Qt::Key_Tab).toString() // text tabulation
                 << QKeySequence (Qt::CTRL + Qt::SHIFT + Qt::Key_J).toString() // select text on jumping (not an action)
                 << QKeySequence (Qt::CTRL + Qt::Key_K).toString(); // used by LineEdit as well as QPlainTextEdit
        config.setReservedShortcuts (reserved);
        config.readShortcuts();
    }

    QHash<QString, QString> ca = config.customShortcutActions();
    QHash<QString, QString>::const_iterator it = ca.constBegin();
    while (it != ca.constEnd())
    { // NOTE: Custom shortcuts are saved in the PortableText format.
        if (QAction *action = findChild<QAction*>(it.key()))
            action->setShortcut (QKeySequence (it.value(), QKeySequence::PortableText));
        ++it;
    }

    if (config.getAutoSave())
        startAutoSaving (true, config.getAutoSaveInterval());
}
/*************************/
void FPwin::addCursorPosLabel()
{
    if (ui->statusBar->findChild<QLabel *>("posLabel"))
        return;
    QLabel *posLabel = new QLabel();
    posLabel->setObjectName ("posLabel");
    posLabel->setText ("<b>" + tr ("Position:") + "</b>");
    posLabel->setIndent (2);
    posLabel->setTextInteractionFlags (Qt::TextSelectableByMouse);
    ui->statusBar->addPermanentWidget (posLabel);
}
/*************************/
void FPwin::addRemoveLangBtn (bool add)
{
    static QStringList langList;
    if (langList.isEmpty())
    { // no "url" for the language button
        langList << "c" << "cmake" << "config" << "cpp" << "css"
                 << "dart" << "deb" << "desktop" << "diff" << "fountain"
                 << "html" << "javascript" << "log" << "lua" << "m3u"
                 << "markdown" << "makefile" << "perl" << "php" << "python"
                 << "qmake" << "qml" << "reST" << "ruby" << "scss"
                 << "sh" << "troff" << "theme" << "xml" << "yaml";
        langList.sort();
    }

    QToolButton *langButton = ui->statusBar->findChild<QToolButton *>("langButton");
    if (!add)
    {
        langs_.clear();
        if (langButton)
        {
            delete langButton; // deletes the menu and its actions
            langButton = nullptr;
        }

        for (int i = 0; i < ui->tabWidget->count(); ++i)
        {
            TextEdit *textEdit = qobject_cast<TabPage*>(ui->tabWidget->widget (i))->textEdit();
            if (!textEdit->getLang().isEmpty())
            {
                textEdit->setLang (QString()); // remove the enforced syntax
                if (ui->actionSyntax->isChecked())
                {
                    syntaxHighlighting (textEdit, false);
                    syntaxHighlighting (textEdit);
                }
            }
        }
    }
    else if (!langButton
             && langs_.isEmpty()) // not needed; we clear it on removing the button
    {
        QString normal = tr ("Normal");
        langButton = new QToolButton();
        langButton->setObjectName ("langButton");
        langButton->setFocusPolicy (Qt::NoFocus);
        langButton->setAutoRaise (true);
        langButton->setToolButtonStyle (Qt::ToolButtonTextOnly);
        langButton->setText (normal);
        langButton->setPopupMode (QToolButton::InstantPopup);

        /* a searchable menu */
        class Menu : public QMenu {
        public:
            Menu( QWidget *parent = nullptr) : QMenu (parent) {
                selectionTimer_ = nullptr;
            }
            ~Menu() {
                if (selectionTimer_) {
                    if (selectionTimer_->isActive()) selectionTimer_->stop();
                    delete selectionTimer_;
                }
            }
        protected:
            void keyPressEvent (QKeyEvent *e) override {
                if (selectionTimer_ == nullptr) {
                  selectionTimer_ = new QTimer();
                  connect (selectionTimer_, &QTimer::timeout, this, [this] {
                      if (txt_.isEmpty()) return;
                      const auto allActions = actions();
                      for (const auto &a : allActions) { // search in starting strings first
                          QString aTxt = a->text();
                          aTxt.remove ('&');
                          if (aTxt.startsWith (txt_, Qt::CaseInsensitive)) {
                              setActiveAction (a);
                              txt_.clear();
                              return;
                          }
                      }
                      for (const auto &a : allActions) { // now, search for containing strings
                          QString aTxt = a->text();
                          aTxt.remove ('&');
                          if (aTxt.contains (txt_, Qt::CaseInsensitive)) {
                              setActiveAction (a);
                              break;
                          }
                      }
                      txt_.clear();
                  });
                }
                selectionTimer_->start (400);
                txt_ += e->text().simplified();
                QMenu::keyPressEvent (e);
            }
        private:
            QTimer *selectionTimer_;
            QString txt_;
        };

        Menu *menu = new Menu (langButton);
        QActionGroup *aGroup = new QActionGroup (langButton);
        QAction *action;
        for (int i = 0; i < langList.count(); ++i)
        {
            QString lang = langList.at (i);
            action = menu->addAction (lang);
            action->setCheckable (true);
            action->setActionGroup (aGroup);
            langs_.insert (lang, action);
        }
        menu->addSeparator();
        action = menu->addAction (normal);
        action->setCheckable (true);
        action->setActionGroup (aGroup);
        langs_.insert (normal, action);
        langButton->setMenu (menu);
        ui->statusBar->insertPermanentWidget (2, langButton);
        connect (aGroup, &QActionGroup::triggered, this, &FPwin::enforceLang);
        if (TabPage *tabPage = qobject_cast< TabPage *>(ui->tabWidget->currentWidget()))
            updateLangBtn (tabPage->textEdit());
    }
}
bool FPwin::hasAnotherDialog()
{
    closeWarningBar();
    bool res = false;
    FPsingleton *singleton = static_cast<FPsingleton*>(qApp);
    for (int i = 0; i < singleton->Wins.count(); ++i)
    {
        FPwin *win = singleton->Wins.at (i);
        if (win != this)
        {
            QList<QDialog*> dialogs = win->findChildren<QDialog*>();
            for (int j = 0; j < dialogs.count(); ++j)
            {
                if (dialogs.at (j)->isModal())
                {
                    res = true;
                    break;
                }
            }
            if (res) break;
        }
    }
    if (res)
    {
        showWarningBar ("<center><b><big>" + tr ("Another FeatherPad window has a modal dialog!") + "</big></b></center>"
                        + "<center><i>" + tr ("Please attend to that window or just close its dialog!") + "</i></center>");
    }
    return res;
}
void FPwin::updateGUIForSingleTab (bool single)
{
    ui->actionRightTab->setEnabled (!single);
    ui->actionLeftTab->setEnabled (!single);
}
void FPwin::deleteTabPage (int tabIndex, bool saveToList, bool closeWithLastTab)
{
    TabPage *tabPage = qobject_cast<TabPage *>(ui->tabWidget->widget (tabIndex));
    if (tabPage == nullptr) return;
    TextEdit *textEdit = tabPage->textEdit();
    QString fileName = textEdit->getFileName();
    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
    if (!fileName.isEmpty())
    {
        if (textEdit->getSaveCursor())
            config.saveCursorPos (fileName, textEdit->textCursor().position());
        if (saveToList && config.getSaveLastFilesList() && QFile::exists (fileName))
            lastWinFilesCur_.insert (fileName, textEdit->textCursor().position());
    }
    disconnect (textEdit, &QPlainTextEdit::textChanged, this, &FPwin::hlight);
    disconnect (textEdit->document(), &QTextDocument::contentsChange, this, &FPwin::updateWordInfo);
    if (config.getSelectionHighlighting())
        disconnect (textEdit->document(), &QTextDocument::contentsChange, textEdit, &TextEdit::onContentsChange);
    syntaxHighlighting (textEdit, false);
    ui->tabWidget->removeTab (tabIndex);
    delete tabPage; tabPage = nullptr;
    if (closeWithLastTab && config.getCloseWithLastTab() && ui->tabWidget->count() == 0)
        close();
}
bool FPwin::closeTabs (int first, int last, bool saveFilesList)
{
    if (!isReady()) return true;
    pauseAutoSaving (true);
    TabPage *curPage = nullptr;
        int cur = ui->tabWidget->currentIndex();
        if (!(first < cur && (cur < last || last == -1)))
            curPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget());
    bool keep = false;
    int index, count;
    DOCSTATE state = SAVED;
    bool closing (saveFilesList);
    while (state == SAVED && ui->tabWidget->count() > 0)
    {
        waitToMakeBusy();

        if (last == 0) break;
        if (last < 0)
            index = ui->tabWidget->count() - 1;
        else
            index = last - 1;

        if (first >= index)
            break;
        int tabIndex = index;
        if (first == index - 1) // only one tab to be closed
            state = savePrompt (tabIndex, false);
        else
            state = savePrompt (tabIndex, true); // with a "No to all" button
        switch (state) {
        case SAVED: // close this tab and go to the next one on the left
            keep = false;
            if (lastWinFilesCur_.size() >= 50) // never remember more than 50 files
                saveFilesList = false;
            deleteTabPage (tabIndex, saveFilesList, !closing);

            if (last > -1) // also last > 0
                --last; // a left tab is removed

            /* final changes */
            count = ui->tabWidget->count();
            if (count == 0)
            {
                ui->actionReload->setDisabled (true);
                ui->actionSave->setDisabled (true);
                enableWidgets (false);
            }
            else if (count == 1)
                updateGUIForSingleTab (true);
            break;
        case UNDECIDED: // stop quitting (cancel or can't save)
            keep = true;
            lastWinFilesCur_.clear();
            break;
        case DISCARDED: // no to all: close all tabs (and quit)
            keep = false;
            while (index > first)
            {
                if (last == 0) break;
                if (lastWinFilesCur_.size() >= 50)
                    saveFilesList = false;
                deleteTabPage (tabIndex, saveFilesList, !closing);

                if (last < 0)
                    index = ui->tabWidget->count() - 1;
                else // if (last > 0)
                {
                    --last; // a left tab is removed
                    index = last - 1;
                }
                tabIndex = index;

                count = ui->tabWidget->count();
                if (count == 0)
                {
                    ui->actionReload->setDisabled (true);
                    ui->actionSave->setDisabled (true);
                    enableWidgets (false);
                }
                else if (count == 1)
                    updateGUIForSingleTab (true);
            }
            break;
        default:
            break;
        }
    }

    unbusy();
    if (!keep)
    {
        if (curPage)
            ui->tabWidget->setCurrentWidget (curPage);
    }

    pauseAutoSaving (false);

    return keep;
}
void FPwin::copyTabFileName()
{
    if (rightClicked_ < 0) return;
    TabPage *tabPage = nullptr;
    tabPage = qobject_cast<TabPage*>(ui->tabWidget->widget (rightClicked_));
    if (tabPage)
    {
        QString fname = tabPage->textEdit()->getFileName();
        QApplication::clipboard()->setText (fname.section ('/', -1));
    }
}
void FPwin::copyTabFilePath()
{
    if (rightClicked_ < 0) return;
    TabPage *tabPage = nullptr;
    tabPage = qobject_cast<TabPage*>(ui->tabWidget->widget (rightClicked_));
    if (tabPage)
    {
        QString str = tabPage->textEdit()->getFileName();
        if (!str.isEmpty())
            QApplication::clipboard()->setText (str);
    }
}
void FPwin::closeAllTabs()
{
    closeTabs (-1, -1);
}
/*************************/
void FPwin::closeNextTabs()
{
    closeTabs (rightClicked_, -1);
}
/*************************/
void FPwin::closePreviousTabs()
{
    closeTabs (-1, rightClicked_);
}
/*************************/
void FPwin::closeOtherTabs()
{
    if (!closeTabs (rightClicked_, -1))
        closeTabs (-1, rightClicked_);
}
/*************************/
void FPwin::dragEnterEvent (QDragEnterEvent *event)
{
    if (findChildren<QDialog *>().count() > 0)
        return;
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
    /* check if this comes from one of our windows (and not from a root instance, for example) */
    else if (event->mimeData()->hasFormat ("application/featherpad-tab")
             && event->source() != nullptr)
    {
        event->acceptProposedAction();
    }
}
/*************************/
void FPwin::dropEvent (QDropEvent *event)
{
    if (event->mimeData()->hasFormat ("application/featherpad-tab"))
        dropTab (QString::fromUtf8 (event->mimeData()->data ("application/featherpad-tab").constData()));
    else
    {
        const QList<QUrl> urlList = event->mimeData()->urls();
        bool multiple (urlList.count() > 1 || isLoading());
        for (const QUrl &url : urlList)
            newTabFromName (url.adjusted (QUrl::NormalizePathSegments) // KDE may give a double slash
                               .toLocalFile(),
                            0,
                            0,
                            multiple);
    }

    event->acceptProposedAction();
}
FPwin::DOCSTATE FPwin::savePrompt (int tabIndex, bool noToAll)
{
    DOCSTATE state = SAVED;
    TabPage *tabPage = qobject_cast<TabPage*>(ui->tabWidget->widget (tabIndex));
    if (tabPage == nullptr) return state;
    TextEdit *textEdit = tabPage->textEdit();
    QString fname = textEdit->getFileName();
    bool isRemoved (!fname.isEmpty() && !QFile::exists (fname));
    if (textEdit->document()->isModified() || isRemoved)
    {
        unbusy();
        if (hasAnotherDialog()) return UNDECIDED;

        if (tabIndex != ui->tabWidget->currentIndex())
        {
            ui->tabWidget->setCurrentIndex (tabIndex);
        }

        updateShortcuts (true);

        MessageBox msgBox (this);
        msgBox.setIcon (QMessageBox::Question);
        msgBox.setText ("<center><b><big>" + tr ("Save changes?") + "</big></b></center>");
        if (isRemoved)
            msgBox.setInformativeText ("<center><i>" + tr ("The file does not exist.") + "</i></center>");
        else
            msgBox.setInformativeText ("<center><i>" + tr ("The document has been modified.") + "</i></center>");
        if (noToAll && ui->tabWidget->count() > 1)
            msgBox.setStandardButtons (QMessageBox::Save
                                       | QMessageBox::Discard
                                       | QMessageBox::Cancel
                                       | QMessageBox::NoToAll);
        else
            msgBox.setStandardButtons (QMessageBox::Save
                                       | QMessageBox::Discard
                                       | QMessageBox::Cancel);
        msgBox.changeButtonText (QMessageBox::Save, tr ("&Save"));
        msgBox.changeButtonText (QMessageBox::Discard, tr ("&Discard changes"));
        msgBox.changeButtonText (QMessageBox::Cancel, tr ("&Cancel"));
        if (noToAll)
            msgBox.changeButtonText (QMessageBox::NoToAll, tr ("&No to all"));
        msgBox.setDefaultButton (QMessageBox::Save);
        msgBox.setWindowModality (Qt::WindowModal);
        /* enforce a central position */
        /*msgBox.show();
        msgBox.move (x() + width()/2 - msgBox.width()/2,
                     y() + height()/2 - msgBox.height()/ 2);*/
        switch (msgBox.exec()) {
        case QMessageBox::Save:
            if (!saveFile (true))
                state = UNDECIDED;
            break;
        case QMessageBox::Discard:
            break;
        case QMessageBox::Cancel:
            state = UNDECIDED;
            break;
        case QMessageBox::NoToAll:
            state = DISCARDED;
            break;
        default:
            state = UNDECIDED;
            break;
        }

        updateShortcuts (false);
    }
    return state;
}
/*************************/
// Enable or disable some widgets.
void FPwin::enableWidgets (bool enable) const
{
    if (!enable && ui->dockReplace->isVisible())
        ui->dockReplace->setVisible (false);
    if (!enable && ui->spinBox->isVisible())
    {
        ui->spinBox->setVisible (false);
        ui->label->setVisible (false);
        ui->checkBox->setVisible (false);
    }
    if ((!enable && ui->statusBar->isVisible())
        || (enable
            && static_cast<FPsingleton*>(qApp)->getConfig()
               .getShowStatusbar())) // starting from no tab
    {
        ui->statusBar->setVisible (enable);
    }

    ui->actionSelectAll->setEnabled (enable);
    ui->actionFind->setEnabled (enable);
    ui->actionJump->setEnabled (enable);
    ui->actionReplace->setEnabled (enable);
    ui->actionClose->setEnabled (enable);
    ui->actionSaveAs->setEnabled (enable);
    ui->actionSaveAllFiles->setEnabled (enable);
    ui->menuEncoding->setEnabled (enable);
    ui->actionFont->setEnabled (enable);
    ui->actionDoc->setEnabled (enable);
    ui->actionPrint->setEnabled (enable);
    if (!enable)
    {
        ui->actionUndo->setEnabled (false);
        ui->actionRedo->setEnabled (false);
        ui->actionEdit->setVisible (false);
        ui->actionRun->setVisible (false);
        ui->actionCut->setEnabled (false);
        ui->actionCopy->setEnabled (false);
        ui->actionPaste->setEnabled (false);
        ui->actionDelete->setEnabled (false);
    }
}
void FPwin::updateCustomizableShortcuts (bool disable)
{
    QHash<QAction*, QKeySequence>::const_iterator iter = defaultShortcuts_.constBegin();
    if (disable)
    { // remove shortcuts
        while (iter != defaultShortcuts_.constEnd())
        {
            iter.key()->setShortcut (QKeySequence());
            ++ iter;
        }
    }
    else
    { // restore shortcuts
        QHash<QString, QString> ca = static_cast<FPsingleton*>(qApp)->
                getConfig().customShortcutActions();
        QList<QString> cn = ca.keys();

        while (iter != defaultShortcuts_.constEnd())
        {
            const QString name = iter.key()->objectName();
            iter.key()->setShortcut (cn.contains (name)
                                     ? QKeySequence (ca.value (name), QKeySequence::PortableText)
                                     : iter.value());
            ++ iter;
        }
    }
}
/*************************/
// When a window-modal dialog is shown, Qt doesn't disable the main window shortcuts.
// This is definitely a bug in Qt. As a workaround, we use this function to disable
// all shortcuts on showing a dialog and to enable them again on hiding it.
// The searchbar shortcuts of the current tab page are handled separately.
//
// This function also updates shortcuts after they're customized in the Preferences dialog.
void FPwin::updateShortcuts (bool disable, bool page)
{
    if (disable)
    {
        ui->actionCut->setShortcut (QKeySequence());
        ui->actionCopy->setShortcut (QKeySequence());
        ui->actionPaste->setShortcut (QKeySequence());
        ui->actionSelectAll->setShortcut (QKeySequence());

        ui->toolButtonNext->setShortcut (QKeySequence());
        ui->toolButtonPrv->setShortcut (QKeySequence());
        ui->toolButtonAll->setShortcut (QKeySequence());
    }
    else
    {
        ui->actionCut->setShortcut (QKeySequence (Qt::CTRL+ Qt::Key_X));
        ui->actionCopy->setShortcut (QKeySequence (Qt::CTRL+ Qt::Key_C));
        ui->actionPaste->setShortcut (QKeySequence (Qt::CTRL+ Qt::Key_V));
        ui->actionSelectAll->setShortcut (QKeySequence (Qt::CTRL+ Qt::Key_A));

        ui->toolButtonNext->setShortcut (QKeySequence (Qt::Key_F8));
        ui->toolButtonPrv->setShortcut (QKeySequence (Qt::Key_F9));
        ui->toolButtonAll->setShortcut (QKeySequence (Qt::Key_F10));
    }
    updateCustomizableShortcuts (disable);

    if (page) // disable/enable searchbar shortcuts of the current page too
    {
        if (TabPage *tabPage = qobject_cast< TabPage *>(ui->tabWidget->currentWidget()))
            tabPage->updateShortcuts (disable);
    }
}
/*************************/
void FPwin::newTab()
{
    createEmptyTab (!isLoading());
}
/*************************/
TabPage* FPwin::createEmptyTab (bool setCurrent, bool allowNormalHighlighter)
{
    FPsingleton *singleton = static_cast<FPsingleton*>(qApp);
    Config config = singleton->getConfig();

    static const QList<QKeySequence> searchShortcuts = {QKeySequence (Qt::Key_F3), QKeySequence (Qt::Key_F4), QKeySequence (Qt::Key_F5), QKeySequence (Qt::Key_F6), QKeySequence (Qt::Key_F7)};
    TabPage *tabPage = new TabPage (config.getDarkColScheme() ? config.getDarkBgColorValue()
                                                              : config.getLightBgColorValue(),
                                    searchShortcuts,
                                    nullptr);
    tabPage->setSearchModel (singleton->searchModel());
    TextEdit *textEdit = tabPage->textEdit();
    connect (textEdit, &QWidget::customContextMenuRequested, this, &FPwin::editorContextMenu);
    textEdit->setSelectionHighlighting (config.getSelectionHighlighting());
    textEdit->setPastePaths (config.getPastePaths());
    textEdit->setAutoReplace (config.getAutoReplace());
    textEdit->setAutoBracket (config.getAutoBracket());
    textEdit->setTtextTab (config.getTextTabSize());
    textEdit->setCurLineHighlight (config.getCurLineHighlight());
    textEdit->setEditorFont (config.getFont());
    textEdit->setInertialScrolling (config.getInertialScrolling());
    textEdit->setDateFormat (config.getDateFormat());
    if (config.getThickCursor())
        textEdit->setThickCursor (true);

    if (allowNormalHighlighter && ui->actionSyntax->isChecked())
        syntaxHighlighting (textEdit); // the default (url) syntax highlighter

    int index = ui->tabWidget->currentIndex();
    if (index == -1) enableWidgets (true);

    /* hide the searchbar consistently */
    if ((index == -1 && config.getHideSearchbar())
        || (index > -1 && !qobject_cast< TabPage *>(ui->tabWidget->widget (index))->isSearchBarVisible()))
    {
        tabPage->setSearchBarVisible (false);
    }

    ui->tabWidget->insertTab (index + 1, tabPage, tr ("Untitled"));

    /* set all preliminary properties */
    if (index >= 0)
        updateGUIForSingleTab (false);
    ui->tabWidget->setTabToolTip (index + 1, tr ("Unsaved"));
    if (!ui->actionWrap->isChecked())
        textEdit->setLineWrapMode (QPlainTextEdit::NoWrap);
    if (!ui->actionIndent->isChecked())
        textEdit->setAutoIndentation (false);
    if (ui->actionLineNumbers->isChecked() || ui->spinBox->isVisible())
        textEdit->showLineNumbers (true);
    if (ui->spinBox->isVisible())
        connect (textEdit->document(), &QTextDocument::blockCountChanged, this, &FPwin::setMax);
    if (ui->statusBar->isVisible()
        || config.getShowStatusbar()) // when the main window is being created, isVisible() isn't set yet
    {
        int showCurPos = config.getShowCursorPos();
        if (setCurrent)
        {
            if (QToolButton *wordButton = ui->statusBar->findChild<QToolButton *>("wordButton"))
                wordButton->setVisible (false);
            QLabel *statusLabel = ui->statusBar->findChild<QLabel *>("statusLabel");
            statusLabel->setText ("<b>" + tr ("Encoding") + ":</b> <i>UTF-8</i>&nbsp;&nbsp;&nbsp;<b>"
                                        + tr ("Lines") + ":</b> <i>1</i>&nbsp;&nbsp;&nbsp;<b>"
                                        + tr ("Sel. Chars") + ":</b> <i>0</i>&nbsp;&nbsp;&nbsp;<b>"
                                        + tr ("Words") + ":</b> <i>0</i>");
            if (showCurPos)
                showCursorPos();
        }
        connect (textEdit, &QPlainTextEdit::blockCountChanged, this, &FPwin::statusMsgWithLineCount);
        connect (textEdit, &QPlainTextEdit::selectionChanged, this, &FPwin::statusMsg);
        if (showCurPos)
            connect (textEdit, &QPlainTextEdit::cursorPositionChanged, this, &FPwin::showCursorPos);
    }
    connect (textEdit->document(), &QTextDocument::undoAvailable, ui->actionUndo, &QAction::setEnabled);
    connect (textEdit->document(), &QTextDocument::redoAvailable, ui->actionRedo, &QAction::setEnabled);
    if (!config.getSaveUnmodified())
        connect (textEdit->document(), &QTextDocument::modificationChanged, this, &FPwin::enableSaving);
    connect (textEdit->document(), &QTextDocument::modificationChanged, this, &FPwin::asterisk);
    connect (textEdit, &QPlainTextEdit::copyAvailable, ui->actionCut, &QAction::setEnabled);
    connect (textEdit, &QPlainTextEdit::copyAvailable, ui->actionDelete, &QAction::setEnabled);
    connect (textEdit, &QPlainTextEdit::copyAvailable, ui->actionCopy, &QAction::setEnabled);

    connect (textEdit, &TextEdit::fileDropped, this, &FPwin::newTabFromName);

    connect (tabPage, &TabPage::find, this, &FPwin::find);
    connect (tabPage, &TabPage::searchFlagChanged, this, &FPwin::searchFlagChanged);
    if (setCurrent)
    {
        ui->tabWidget->setCurrentWidget (tabPage);
        textEdit->setFocus();
    }
#ifdef HAS_X11
    if (static_cast<FPsingleton*>(qApp)->isX11() && isWindowShaded (winId()))
        unshadeWindow (winId());
#endif
    if (setCurrent) stealFocus();

    return tabPage;
}
void FPwin::editorContextMenu (const QPoint& p)
{
    TextEdit *textEdit = qobject_cast<TextEdit*>(QObject::sender());
    if (textEdit == nullptr) return;
    if (!textEdit->textCursor().hasSelection())
        textEdit->setTextCursor (textEdit->cursorForPosition (p));

    QMenu *menu = textEdit->createStandardContextMenu (p);
    const QList<QAction*> actions = menu->actions();
    if (!actions.isEmpty())
    {
        for (QAction* const thisAction : actions)
        {
            /* remove the shortcut strings because shortcuts may change */
            QString txt = thisAction->text();
            if (!txt.isEmpty())
                txt = txt.split ('\t').first();
            if (!txt.isEmpty())
                thisAction->setText(txt);
            /* correct the slots of some actions */
            if (thisAction->objectName() == "edit-copy")
            {
                disconnect (thisAction, &QAction::triggered, nullptr, nullptr);
                connect (thisAction, &QAction::triggered, textEdit, &TextEdit::copy);
            }
            else if (thisAction->objectName() == "edit-cut")
            {
                disconnect (thisAction, &QAction::triggered, nullptr, nullptr);
                connect (thisAction, &QAction::triggered, textEdit, &TextEdit::cut);
            }
            else if (thisAction->objectName() == "edit-paste")
            {
                disconnect (thisAction, &QAction::triggered, nullptr, nullptr);
                connect (thisAction, &QAction::triggered, textEdit, &TextEdit::paste);
            }
            else if (thisAction->objectName() == "edit-undo")
            {
                disconnect (thisAction, &QAction::triggered, nullptr, nullptr);
                connect (thisAction, &QAction::triggered, textEdit, &TextEdit::undo);
            }
            else if (thisAction->objectName() == "edit-redo")
            {
                disconnect (thisAction, &QAction::triggered, nullptr, nullptr);
                connect (thisAction, &QAction::triggered, textEdit, &TextEdit::redo);
            }
            else if (thisAction->objectName() == "select-all")
            {
                disconnect (thisAction, &QAction::triggered, nullptr, nullptr);
                connect (thisAction, &QAction::triggered, textEdit, &TextEdit::selectAll);
            }
        }
        QString str = textEdit->getUrl (textEdit->textCursor().position());
        if (!str.isEmpty())
        {
            QAction *sep = menu->insertSeparator (actions.first());
            QAction *openLink = new QAction (tr ("Open Link"), menu);
            menu->insertAction (sep, openLink);
            connect (openLink, &QAction::triggered, [str] {
                QUrl url (str);
                if (url.isRelative())
                    url = QUrl::fromUserInput (str, "/");
                /* QDesktopServices::openUrl() may resort to "xdg-open", which isn't
                   the best choice. "gio" is always reliable, so we check it first. */
                if (!QProcess::startDetached ("gio", QStringList() << "open" << url.toString()))
                    QDesktopServices::openUrl (url);
            });
            if (str.startsWith ("mailto:")) // see getUrl()
                str.remove (0, 7);
            QAction *copyLink = new QAction (tr ("Copy Link"), menu);
            menu->insertAction (sep, copyLink);
            connect (copyLink, &QAction::triggered, [str] {
                QApplication::clipboard()->setText (str);
            });

        }
        menu->addSeparator();
    }
    menu->exec (textEdit->viewport()->mapToGlobal (p));
    delete menu;
}
void FPwin::reformat (TextEdit *textEdit)
{
    formatTextRect();
    if (!textEdit->getSearchedText().isEmpty())
        hlight();
    textEdit->selectionHlight();
}
void FPwin::defaultSize()
{
    QSize s = static_cast<FPsingleton*>(qApp)->getConfig().getStartSize();
    if (size() == s) return;
    if (isMaximized() || isFullScreen())
        showNormal();
    /*if (isMaximized() && isFullScreen())
        showMaximized();
    if (isMaximized())
        showNormal();*/
    /* instead of hiding, reparent with the dummy
       widget to guarantee resizing under all DEs */
    /*Qt::WindowFlags flags = windowFlags();
    setParent (dummyWidget, Qt::SubWindow);*/
    //hide();
    resize (s);
    /*if (parent() != nullptr)
        setParent (nullptr, flags);*/
    //QTimer::singleShot (0, this, &FPwin::show);
}
/*************************/
/*void FPwin::align()
{
    int index = ui->tabWidget->currentIndex();
    if (index == -1) return;

    TextEdit *textEdit = qobject_cast< TabPage *>(ui->tabWidget->widget (index))->textEdit();
    QTextOption opt = textEdit->document()->defaultTextOption();
    if (opt.alignment() == (Qt::AlignLeft))
    {
        opt = QTextOption (Qt::AlignRight);
        opt.setTextDirection (Qt::LayoutDirectionAuto);
        textEdit->document()->setDefaultTextOption (opt);
    }
    else if (opt.alignment() == (Qt::AlignRight))
    {
        opt = QTextOption (Qt::AlignLeft);
        opt.setTextDirection (Qt::LayoutDirectionAuto);
        textEdit->document()->setDefaultTextOption (opt);
    }
}*/

void FPwin::focus_view_soft()
{
	
    if (TabPage *tabPage = qobject_cast< TabPage *>(ui->tabWidget->currentWidget()))
    {
        if (!tabPage->hasPopup())
        {
        	
		TabPage *tabPage = qobject_cast< TabPage *>(ui->tabWidget->currentWidget());
		if (tabPage == nullptr) return;
        	
		tabPage->textEdit()->setFocus();
        	
        }
    }
};

void FPwin::focus_view_hard()
{
    if (TabPage *tabPage = qobject_cast< TabPage *>(ui->tabWidget->currentWidget()))
    {
        if (!tabPage->hasPopup())
        {
        	
		TabPage *tabPage = qobject_cast< TabPage *>(ui->tabWidget->currentWidget());
		if (tabPage == nullptr) return;
        	
        	/*********************/
        	
        	int count = ui->tabWidget->count();
    		for (int indx = 0; indx < count; ++indx)
    		{
        		TabPage *page = qobject_cast< TabPage *>(ui->tabWidget->widget (indx));
			TextEdit *textEdit = page->textEdit();
			textEdit->setSearchedText (QString());
			QList<QTextEdit::ExtraSelection> es;
			textEdit->setGreenSel (es); // not needed
			if (ui->actionLineNumbers->isChecked() || ui->spinBox->isVisible())
			es.prepend (textEdit->currentLineSelection());
			es.append (textEdit->getBlueSel());
			es.append (textEdit->getRedSel());
			textEdit->setExtraSelections (es);
			page->clearSearchEntry();
        		page->setSearchBarVisible (false);
    		}
        	
        	ui -> dockReplace -> setVisible(false);
        	
		ui -> spinBox -> setVisible(false);
		ui -> label -> setVisible(false);
		ui -> checkBox -> setVisible(false);
        	
        	/*********************/
        	
		tabPage->textEdit()->setFocus();
        	
        }
    }
};


void FPwin::executeProcess()
{
    QList<QDialog*> dialogs = findChildren<QDialog*>();
    for (int i = 0; i < dialogs.count(); ++i)
    {
        if (dialogs.at (i)->isModal())
            return; // shortcut may work when there's a modal dialog
    }
    closeWarningBar();

    Config config = static_cast<FPsingleton*>(qApp)->getConfig();
    if (!config.getExecuteScripts()) return;

    if (TabPage *tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget()))
    {
        if (tabPage->findChild<QProcess *>(QString(), Qt::FindDirectChildrenOnly))
        {
            showWarningBar ("<center><b><big>" + tr ("Another process is running in this tab!") + "</big></b></center>"
                            + "<center><i>" + tr ("Only one process is allowed per tab.") + "</i></center>");
            return;
        }

        QString fName = tabPage->textEdit()->getFileName();
        if (!isScriptLang (tabPage->textEdit()->getProg())  || !QFileInfo (fName).isExecutable())
        {
            ui->actionRun->setVisible (false);
            return;
        }

        QProcess *process = new QProcess (tabPage);
        process->setObjectName (fName); // to put it into the message dialog
        connect (process, &QProcess::readyReadStandardOutput,this, &FPwin::displayOutput);
        connect (process, &QProcess::readyReadStandardError,this, &FPwin::displayError);
        QString command = config.getExecuteCommand();
#if (QT_VERSION >= QT_VERSION_CHECK(5,15,0))
        /* Qt 5.15 has made things more complex and, at the same time, better:
           on the one hand, we should see if there is a command argument ; on the
           other hand, we don't need to worry about spaces and quotes in file names. */
        if (!command.isEmpty())
        {
            QStringList commandParts = command.split (QRegularExpression ("\\s+"), Qt::SkipEmptyParts);
            if (!commandParts.isEmpty())
            {
                command = commandParts.takeAt (0); // there may be arguments
                process->start (command, QStringList() << commandParts << fName);
            }
            else
                process->start (fName, QStringList());
        }
        else
            process->start (fName, QStringList());
#else
        if (!command.isEmpty())
            command +=  " ";
        fName.replace ("\"", "\"\"\""); // literal quotes in the command are shown by triple quotes
        process->start (command + "\"" + fName + "\"");
#endif
        /* old-fashioned: connect(process, static_cast<void(QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),... */
        connect (process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                 [=](int /*exitCode*/, QProcess::ExitStatus /*exitStatus*/) {process->deleteLater();});
    }
}
/*************************/
bool FPwin::isScriptLang (const QString& lang) const
{
    return (lang == "sh" || lang == "python"
            || lang == "ruby" || lang == "lua"
            || lang == "perl");
}
/*************************/
void FPwin::exitProcess()
{
    if (TabPage *tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget()))
    {
        if (QProcess *process = tabPage->findChild<QProcess *>(QString(), Qt::FindDirectChildrenOnly))
            process->kill();
    }
}
/*************************/
void FPwin::displayMessage (bool error)
{
    QProcess *process = static_cast<QProcess*>(QObject::sender());
    if (!process) return; // impossible
    QByteArray msg;
    if (error)
    {
        process->setReadChannel(QProcess::StandardError);
        msg = process->readAllStandardError();
    }
    else
    {
        process->setReadChannel(QProcess::StandardOutput);
        msg = process->readAllStandardOutput();
    }
    if (msg.isEmpty()) return;

    QPointer<QDialog> msgDlg = nullptr;
    QList<QDialog*> dialogs = findChildren<QDialog*>();
    for (int i = 0; i < dialogs.count(); ++i)
    {
        if (dialogs.at (i)->parent() == process->parent())
        {
            msgDlg = dialogs.at (i);
            break;
        }
    }
    if (msgDlg)
    { // append to the existing message
        if (QPlainTextEdit *tEdit = msgDlg->findChild<QPlainTextEdit*>())
        {
            tEdit->setPlainText (tEdit->toPlainText() + "\n" + msg.constData());
            QTextCursor cur = tEdit->textCursor();
            cur.movePosition (QTextCursor::End);
            tEdit->setTextCursor (cur);
        }
    }
    else
    {
        msgDlg = new QDialog (qobject_cast<QWidget*>(process->parent()));
        msgDlg->setWindowTitle (tr ("Script Output"));
        msgDlg->setSizeGripEnabled (true);
        QGridLayout *grid = new QGridLayout;
        QLabel *label = new QLabel (msgDlg);
        label->setText ("<center><b>" + tr ("Script File") + ": </b></center><i>" + process->objectName() + "</i>");
        label->setTextInteractionFlags (Qt::TextSelectableByMouse);
        label->setWordWrap (true);
        label->setMargin (5);
        grid->addWidget (label, 0, 0, 1, 2);
        QPlainTextEdit *tEdit = new QPlainTextEdit (msgDlg);
        tEdit->setTextInteractionFlags (Qt::TextSelectableByMouse);
        tEdit->ensureCursorVisible();
        grid->addWidget (tEdit, 1, 0, 1, 2);
        QPushButton *closeButton = new QPushButton (QIcon::fromTheme ("edit-delete"), tr ("Close"));
        connect (closeButton, &QAbstractButton::clicked, msgDlg, &QDialog::reject);
        grid->addWidget (closeButton, 2, 1, Qt::AlignRight);
        QPushButton *clearButton = new QPushButton (QIcon::fromTheme ("edit-clear"), tr ("Clear"));
        connect (clearButton, &QAbstractButton::clicked, tEdit, &QPlainTextEdit::clear);
        grid->addWidget (clearButton, 2, 0, Qt::AlignLeft);
        msgDlg->setLayout (grid);
        tEdit->setPlainText (msg.constData());
        QTextCursor cur = tEdit->textCursor();
        cur.movePosition (QTextCursor::End);
        tEdit->setTextCursor (cur);
        msgDlg->setAttribute (Qt::WA_DeleteOnClose);
        msgDlg->show();
        msgDlg->raise();
        msgDlg->activateWindow();
    }
}
void FPwin::displayOutput()
{
    displayMessage (false);
}
void FPwin::displayError()
{
    displayMessage (true);
}
void FPwin::closeTab()
{
    if (!isReady()) return;

    pauseAutoSaving (true);

    QListWidgetItem *curItem = nullptr;
    int index = -1;
	index = ui->tabWidget->currentIndex();
	if (index == -1)  // not needed
	{
	pauseAutoSaving (false);
	return;
	}

    if (savePrompt (index, false) != SAVED)
    {
        pauseAutoSaving (false);
        return;
    }

    deleteTabPage (index);
    int count = ui->tabWidget->count();
    if (count == 0)
    {
        ui->actionReload->setDisabled (true);
        ui->actionSave->setDisabled (true);
        enableWidgets (false);
    }
    else
    {
        if (count == 1)
            updateGUIForSingleTab (true);

        if (curItem)

        if (TabPage *tabPage = qobject_cast< TabPage *>(ui->tabWidget->currentWidget()))
            tabPage->textEdit()->setFocus();
    }

    pauseAutoSaving (false);
}
/*************************/
void FPwin::closeTabAtIndex (int index)
{
    pauseAutoSaving (true);

    TabPage *curPage = nullptr;
    if (index != ui->tabWidget->currentIndex())
        curPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget());
    if (savePrompt (index, false) != SAVED)
    {
        pauseAutoSaving (false);
        return;
    }
    closeWarningBar();

    deleteTabPage (index);
    int count = ui->tabWidget->count();
    if (count == 0)
    {
        ui->actionReload->setDisabled (true);
        ui->actionSave->setDisabled (true);
        enableWidgets (false);
    }
    else
    {
        if (count == 1)
            updateGUIForSingleTab (true);

        if (curPage) // restore the current page
            ui->tabWidget->setCurrentWidget (curPage);

        if (TabPage *tabPage = qobject_cast< TabPage *>(ui->tabWidget->currentWidget()))
            tabPage->textEdit()->setFocus();
    }

    pauseAutoSaving (false);
}
/*************************/
void FPwin::setTitle (const QString& fileName, int tabIndex)
{
    int index = tabIndex;
    if (index < 0)
        index = ui->tabWidget->currentIndex(); // is never -1

    bool isLink (false);
    QString shownName;
    if (fileName.isEmpty())
    {
        shownName = tr ("Untitled");
        if (tabIndex < 0)
            setWindowTitle (shownName);
    }
    else
    {
        QFileInfo fInfo (fileName);
        if (tabIndex < 0)
            setWindowTitle (fileName.contains ("/") ? fileName
                                                    : fInfo.absolutePath() + "/" + fileName);
        isLink = fInfo.isSymLink();
        shownName = fileName.section ('/', -1);
        shownName.replace ("\n", " "); // no multi-line tab text
    }
    shownName.replace ("&", "&&"); // single ampersand is for tab mnemonic
    shownName.replace ('\t', ' ');
    ui->tabWidget->setTabText (index, shownName);
    if (isLink)
        ui->tabWidget->setTabIcon (index, QIcon (":icons/link.svg"));
    else
        ui->tabWidget->setTabIcon (index, QIcon());
}
void FPwin::enableSaving (bool modified)
{
    if (!inactiveTabModified_)
        ui->actionSave->setEnabled (modified);
}
void FPwin::asterisk (bool modified)
{
    if (inactiveTabModified_) return;

    int index = ui->tabWidget->currentIndex();
    TabPage *tabPage = qobject_cast< TabPage *>(ui->tabWidget->widget (index));
    if (tabPage == nullptr) return;
    QString fname = tabPage->textEdit()->getFileName();
    QString shownName;
    if (fname.isEmpty())
    {
        shownName = tr ("Untitled");
        setWindowTitle ((modified ? "*" : QString()) + shownName);
    }
    else
    {
        shownName = fname.section ('/', -1);
        setWindowTitle ((modified ? "*" : QString())
                        + (fname.contains ("/") ? fname
                                                : QFileInfo (fname).absolutePath() + "/" + fname));
    }
    shownName.replace ("\n", " ");
    if (modified)
        shownName.prepend ("*");
    shownName.replace ("&", "&&");
    shownName.replace ('\t', ' ');
    ui->tabWidget->setTabText (index, shownName);
}
/*************************/
void FPwin::waitToMakeBusy()
{
    if (QGuiApplication::overrideCursor() != nullptr || busyThread_ != nullptr)
        return;
    busyThread_ = new QThread;
    BusyMaker *makeBusy = new BusyMaker();
    makeBusy->moveToThread (busyThread_);
    connect (busyThread_, &QThread::started, makeBusy, &BusyMaker::waiting);
    connect (makeBusy, &BusyMaker::finished, busyThread_, &QThread::quit);
    connect (busyThread_, &QThread::finished, makeBusy, &QObject::deleteLater);
    connect (busyThread_, &QThread::finished, busyThread_, &QObject::deleteLater);
    busyThread_->start();
}
/*************************/
void FPwin::unbusy()
{
    if (busyThread_ && !busyThread_->isFinished())
    {
        busyThread_->quit();
        busyThread_->wait();
    }
    if (QGuiApplication::overrideCursor() != nullptr)
        QGuiApplication::restoreOverrideCursor();
}
/*************************/
void FPwin::loadText (const QString& fileName, bool enforceEncod, bool reload,
                      int restoreCursor, int posInLine,
                      bool enforceUneditable, bool multiple)
{
    ++ loadingProcesses_;
    QString charset;
    if (enforceEncod)
        charset = checkToEncoding();
    Loading *thread = new Loading (fileName, charset, reload,
                                   restoreCursor, posInLine,
                                   enforceUneditable, multiple);
    thread->setSkipNonText (static_cast<FPsingleton*>(qApp)->getConfig().getSkipNonText());
    connect (thread, &Loading::completed, this, &FPwin::addText);
    connect (thread, &Loading::finished, thread, &QObject::deleteLater);
    thread->start();

    waitToMakeBusy();
    ui->tabWidget->tabBar()->lockTabs (true);
    updateShortcuts (true, false);
}
/*************************/
// When multiple files are being loaded, we don't change the current tab.
void FPwin::addText (const QString& text, const QString& fileName, const QString& charset,
                     bool enforceEncod, bool reload,
                     int restoreCursor, int posInLine,
                     bool uneditable,
                     bool multiple)
{
    if (fileName.isEmpty() || charset.isEmpty())
    {
        if (!fileName.isEmpty() && charset.isEmpty()) // means a very large file
            connect (this, &FPwin::finishedLoading, this, &FPwin::onOpeningHugeFiles, Qt::UniqueConnection);
        else if (fileName.isEmpty() && !charset.isEmpty()) // means a non-text file that shouldn't be opened
            connect (this, &FPwin::finishedLoading, this, &FPwin::onOpeninNonTextFiles, Qt::UniqueConnection);
        else
            connect (this, &FPwin::finishedLoading, this, &FPwin::onPermissionDenied, Qt::UniqueConnection);
        -- loadingProcesses_; // can never become negative
        if (!isLoading())
        {
            ui->tabWidget->tabBar()->lockTabs (false);
            updateShortcuts (false, false);
            closeWarningBar();
            emit finishedLoading();
            QTimer::singleShot (0, this, [this]() {unbusy();});
        }
        return;
    }

    if (enforceEncod || reload)
        multiple = false; // respect the logic

    /* only for the side-pane mode */
    static bool scrollToFirstItem (false);
    static TabPage *firstPage = nullptr;

    TextEdit *textEdit;
    TabPage *tabPage = nullptr;
    if (ui->tabWidget->currentIndex() == -1)
        tabPage = createEmptyTab (!multiple, false);
    else
        tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget());
    if (tabPage == nullptr) return;
    textEdit = tabPage->textEdit();

    bool openInCurrentTab (true);
    if (!reload
        && !enforceEncod
        && (!textEdit->document()->isEmpty()
            || textEdit->document()->isModified()
            || !textEdit->getFileName().isEmpty()))
    {
        tabPage = createEmptyTab (!multiple, false);
        textEdit = tabPage->textEdit();
        openInCurrentTab = false;
    }
    else
    {
#ifdef HAS_X11
        if (static_cast<FPsingleton*>(qApp)->isX11() && isWindowShaded (winId()))
            unshadeWindow (winId());
#endif
        stealFocus();
    }
    textEdit->setSaveCursor (restoreCursor == 1);

    textEdit->setLang (QString()); // remove the enforced syntax

    /* uninstall the syntax highlgihter to reinstall it below (when the text is reloaded,
       its encoding is enforced, or a new tab with normal as url was opened here) */
    if (textEdit->getHighlighter())
    {
        textEdit->setGreenSel (QList<QTextEdit::ExtraSelection>()); // they'll have no meaning later
        syntaxHighlighting (textEdit, false);
    }

    QFileInfo fInfo (fileName);

    if (scrollToFirstItem
        && (!firstPage
            || firstPage->textEdit()->getFileName().section ('/', -1).
               compare (fInfo.fileName(), Qt::CaseInsensitive) > 0))
    {
        firstPage = tabPage;
    }

    /* this workaround, for the RTL bug in QPlainTextEdit, isn't needed
       because a better workaround is included in textedit.cpp */
    /*QTextOption opt = textEdit->document()->defaultTextOption();
    if (text.isRightToLeft())
    {
        if (opt.alignment() == (Qt::AlignLeft))
        {
            opt = QTextOption (Qt::AlignRight);
            opt.setTextDirection (Qt::LayoutDirectionAuto);
            textEdit->document()->setDefaultTextOption (opt);
        }
    }
    else if (opt.alignment() == (Qt::AlignRight))
    {
        opt = QTextOption (Qt::AlignLeft);
        opt.setTextDirection (Qt::LayoutDirectionAuto);
        textEdit->document()->setDefaultTextOption (opt);
    }*/

    /* we want to restore the cursor later */
    int pos = 0, anchor = 0;
    int scrollbarValue = -1;
    if (reload)
    {
        textEdit->forgetTxtCurHPos();
        pos = textEdit->textCursor().position();
        anchor = textEdit->textCursor().anchor();
        if (QScrollBar *scrollbar = textEdit->verticalScrollBar())
        {
            if (scrollbar->isVisible())
                scrollbarValue = scrollbar->value();
        }
    }

    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();

    /* set the text */
    inactiveTabModified_ = true; // ignore QTextDocument::modificationChanged() temporarily
    textEdit->setPlainText (text); // undo/redo is reset
    inactiveTabModified_ = false;

    /* now, restore the cursor */
    if (reload)
    {
        QTextCursor cur = textEdit->textCursor();
        cur.movePosition (QTextCursor::End, QTextCursor::MoveAnchor);
        int curPos = cur.position();
        if (anchor <= curPos && pos <= curPos)
        {
            cur.setPosition (anchor);
            cur.setPosition (pos, QTextCursor::KeepAnchor);
        }
        textEdit->setTextCursor (cur);
    }
    else if (restoreCursor != 0)
    {
        if (restoreCursor == 1 || restoreCursor == -1) // restore cursor from settings
        {
            QHash<QString, QVariant> cursorPos = restoreCursor == 1 ? config.savedCursorPos()
                                                                    : config.getLastFilesCursorPos();
            if (cursorPos.contains (fileName))
            {
                QTextCursor cur = textEdit->textCursor();
                cur.movePosition (QTextCursor::End, QTextCursor::MoveAnchor);
                int pos = qMin (qMax (cursorPos.value (fileName, 0).toInt(), 0), cur.position());
                cur.setPosition (pos);
                QTimer::singleShot (0, textEdit, [textEdit, cur]() {
                    textEdit->setTextCursor (cur); // ensureCursorVisible() is called by this
                });
            }
        }
        else if (restoreCursor < -1) // doc end in commandline
        {
            QTextCursor cur = textEdit->textCursor();
            cur.movePosition (QTextCursor::End);
            QTimer::singleShot (0, textEdit, [textEdit, cur]() {
                textEdit->setTextCursor (cur);
            });
        }
        else
        {
            restoreCursor -= 2; // in Qt, blocks are started from 0
            if (restoreCursor < textEdit->document()->blockCount())
            {
                QTextBlock block = textEdit->document()->findBlockByNumber (restoreCursor);
                QTextCursor cur (block);
                QTextCursor tmp = cur;
                tmp.movePosition (QTextCursor::EndOfBlock);
                if (posInLine < 0 || posInLine >= tmp.positionInBlock())
                    cur = tmp;
                else
                    cur.setPosition (block.position() + posInLine);
                QTimer::singleShot (0, textEdit, [textEdit, cur]() {
                    textEdit->setTextCursor (cur);
                });
            }
            else
            {
                QTextCursor cur = textEdit->textCursor();
                cur.movePosition (QTextCursor::End);
                QTimer::singleShot (0, textEdit, [textEdit, cur]() {
                    textEdit->setTextCursor (cur);
                });
            }
        }
    }

    textEdit->setFileName (fileName);
    textEdit->setSize (fInfo.size());
    textEdit->setLastModified (fInfo.lastModified());
    lastFile_ = fileName;
    if (config.getRecentOpened())
        config.addRecentFile (lastFile_);
    textEdit->setEncoding (charset);
    textEdit->setWordNumber (-1);
    if (uneditable)
    {
        connect (this, &FPwin::finishedLoading, this, &FPwin::onOpeningUneditable, Qt::UniqueConnection);
        textEdit->makeUneditable (uneditable);
    }
    setProgLang (textEdit);
    if (ui->actionSyntax->isChecked())
        syntaxHighlighting (textEdit);
    setTitle (fileName, (multiple && !openInCurrentTab) ?
                        /* An Old comment not valid anymore: "The index may have changed because syntaxHighlighting()
                           waits for all events to be processed (but it won't change from here on)." */
                        ui->tabWidget->indexOf (tabPage) : -1);
    QString tip (fInfo.absolutePath());
    if (!tip.endsWith ("/")) tip += "/";
    QFontMetrics metrics (QToolTip::font());
    QString elidedTip = "<p style='white-space:pre'>"
#if (QT_VERSION >= QT_VERSION_CHECK(5,11,0))
                        + metrics.elidedText (tip, Qt::ElideMiddle, 200 * metrics.horizontalAdvance (' '))
#else
                        + metrics.elidedText (tip, Qt::ElideMiddle, 200 * metrics.width (' '))
#endif
                        + "</p>";
    ui->tabWidget->setTabToolTip (ui->tabWidget->indexOf (tabPage), elidedTip);
    if (!sideItems_.isEmpty())
    {
        if (QListWidgetItem *wi = sideItems_.key (tabPage))
            wi->setToolTip (elidedTip);
    }

    if (uneditable || alreadyOpen (tabPage))
    {
        textEdit->setReadOnly (true);
        if (!textEdit->hasDarkScheme())
        {
            if (uneditable) // as with Help
                textEdit->viewport()->setStyleSheet (".QWidget {"
                                                     "color: black;"
                                                     "background-color: rgb(225, 238, 255);}");
            else
                textEdit->viewport()->setStyleSheet (".QWidget {"
                                                     "color: black;"
                                                     "background-color: rgb(236, 236, 208);}");
        }
        else
        {
            if (uneditable)
                textEdit->viewport()->setStyleSheet (".QWidget {"
                                                     "color: white;"
                                                     "background-color: rgb(0, 60, 110);}");
            else
                textEdit->viewport()->setStyleSheet (".QWidget {"
                                                     "color: white;"
                                                     "background-color: rgb(60, 0, 0);}");
        }
        if (!multiple || openInCurrentTab)
        {
            if (!uneditable)
                ui->actionEdit->setVisible (true);
            else
                ui->actionSaveAs->setDisabled (true);
            ui->actionCut->setDisabled (true);
            ui->actionPaste->setDisabled (true);
            ui->actionDelete->setDisabled (true);
            if (config.getSaveUnmodified())
                ui->actionSave->setDisabled (true);
        }
        disconnect (textEdit, &QPlainTextEdit::copyAvailable, ui->actionCut, &QAction::setEnabled);
        disconnect (textEdit, &QPlainTextEdit::copyAvailable, ui->actionDelete, &QAction::setEnabled);
    }
    else if (textEdit->isReadOnly())
        QTimer::singleShot (0, this, &FPwin::makeEditable);

    if (!multiple || openInCurrentTab)
    {
        if (!fInfo.exists())
            connect (this, &FPwin::finishedLoading, this, &FPwin::onOpeningNonexistent, Qt::UniqueConnection);
        if (ui->statusBar->isVisible())
        {
            statusMsgWithLineCount (textEdit->document()->blockCount());
            if (QToolButton *wordButton = ui->statusBar->findChild<QToolButton *>("wordButton"))
                wordButton->setVisible (true);
            if (text.isEmpty())
                updateWordInfo();
        }
        if (config.getShowLangSelector() && config.getSyntaxByDefault())
            updateLangBtn (textEdit);
        encodingToCheck (charset);
        ui->actionReload->setEnabled (true);
        textEdit->setFocus();

        if (openInCurrentTab)
        {
            if (isScriptLang (textEdit->getProg()) && fInfo.isExecutable())
                ui->actionRun->setVisible (config.getExecuteScripts());
            else
                ui->actionRun->setVisible (false);
        }
    }

    -- loadingProcesses_;
    if (!isLoading())
    {
        ui->tabWidget->tabBar()->lockTabs (false);
        updateShortcuts (false, false);
        if (reload && scrollbarValue > -1)
        {
            lambdaConnection_ = QObject::connect (this, &FPwin::finishedLoading, textEdit,
                                                  [this, textEdit, scrollbarValue]() {
                if (QScrollBar *scrollbar = textEdit->verticalScrollBar())
                {
                    if (scrollbar->isVisible())
                        scrollbar->setValue (scrollbarValue);
                }
                disconnectLambda();
            });
        }
        scrollToFirstItem = false;
        firstPage = nullptr;
        closeWarningBar (true);
        emit finishedLoading();
        QTimer::singleShot (0, this, [this]() {unbusy();});
    }
}
void FPwin::disconnectLambda()
{
    QObject::disconnect (lambdaConnection_);
}
void FPwin::onOpeningHugeFiles()
{
    disconnect (this, &FPwin::finishedLoading, this, &FPwin::onOpeningHugeFiles);
    QTimer::singleShot (0, this, [=]() {
        showWarningBar ("<center><b><big>" + tr ("Huge file(s) not opened!") + "</big></b></center>\n"
                        + "<center>" + tr ("FeatherPad does not open files larger than 100 MiB.") + "</center>");
    });
}
/*************************/
void FPwin::onOpeninNonTextFiles()
{
    disconnect (this, &FPwin::finishedLoading, this, &FPwin::onOpeninNonTextFiles);
    QTimer::singleShot (0, this, [=]() {
        showWarningBar ("<center><b><big>" + tr ("Non-text file(s) not opened!") + "</big></b></center>\n"
                        + "<center><i>" + tr ("See Preferences → Files → Do not permit opening of non-text files") + "</i></center>");
    });
}
/*************************/
void FPwin::onPermissionDenied()
{
    disconnect (this, &FPwin::finishedLoading, this, &FPwin::onPermissionDenied);
    QTimer::singleShot (0, this, [=]() {
        showWarningBar ("<center><b><big>" + tr ("Some file(s) could not be opened!") + "</big></b></center>\n"
                        + "<center>" + tr ("You may not have the permission to read.") + "</center>");
    });
}
/*************************/
void FPwin::onOpeningUneditable()
{
    disconnect (this, &FPwin::finishedLoading, this, &FPwin::onOpeningUneditable);
    /* A timer is needed here because the scrollbar position is restored on reloading by a
       lambda connection. Timers are also used in similar places for the sake of certainty. */
    QTimer::singleShot (0, this, [=]() {
        showWarningBar ("<center><b><big>" + tr ("Uneditable file(s)!") + "</big></b></center>\n"
                        + "<center>" + tr ("Non-text files or files with huge lines cannot be edited.") + "</center>");
    });
}
/*************************/
void FPwin::onOpeningNonexistent()
{
    disconnect (this, &FPwin::finishedLoading, this, &FPwin::onOpeningNonexistent);
    QTimer::singleShot (0, this, [=]() {
        /* show the bar only if the current file doesn't exist at this very moment */
        if (TabPage *tabPage = qobject_cast< TabPage *>(ui->tabWidget->currentWidget()))
        {
            QString fname = tabPage->textEdit()->getFileName();
            if (!fname.isEmpty() && !QFile::exists (fname))
                showWarningBar ("<center><b><big>" + tr ("The file does not exist.") + "</big></b></center>");
        }
    });
}
/*************************/
void FPwin::showWarningBar (const QString& message, bool startupBar)
{
    /* don't show the warning bar when there's a modal dialog */
    QList<QDialog*> dialogs = findChildren<QDialog*>();
    for (int i = 0; i < dialogs.count(); ++i)
    {
        if (dialogs.at (i)->isModal())
            return;
    }
    /* don't close and show the same warning bar */
    if (WarningBar *prevBar = ui->tabWidget->findChild<WarningBar *>())
    {
        if (!prevBar->isClosing() && prevBar->getMessage() == message)
            return;
    }

    int vOffset = 0;
    TabPage *tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget());
    if (tabPage)
        vOffset = tabPage->height() - tabPage->textEdit()->height();
    WarningBar *bar = new WarningBar (message, vOffset, ui->tabWidget);
    if (startupBar)
        bar->setObjectName ("startupBar");
}
/*************************/
void FPwin::showCrashWarning()
{
    QTimer::singleShot (0, this, [=]() {
        showWarningBar ("<center><b><big>" + tr ("A previous crash detected!") + "</big></b></center>"
                        + "<center><i>" + tr ("Preferably, close all FeatherPad windows and start again!") + "</i></center>", true);
    });
}
void FPwin::showRootWarning()
{
    QTimer::singleShot (0, this, [=]() {
        showWarningBar ("<center><b><big>" + tr ("Root Instance") + "</big></b></center>", true);
    });
}
void FPwin::closeWarningBar (bool keepOnStartup)
{
    const QList<WarningBar*> warningBars = ui->tabWidget->findChildren<WarningBar*>();
    for (WarningBar *wb : warningBars)
    {
        if (!keepOnStartup || wb->objectName() != "startupBar")
            wb->closeBar();
    }
}
void FPwin::newTabFromName (const QString& fileName, int restoreCursor, int posInLine, bool multiple)
{
    if (!fileName.isEmpty())
        loadText (fileName, false, false,
                  restoreCursor, posInLine,
                  false, multiple);
}
void FPwin::newTabFromRecent()
{
    QAction *action = qobject_cast<QAction*>(QObject::sender());
    if (!action) return;
    loadText (action->data().toString(), false, false);
}
void FPwin::fileOpen()
{
    if (isLoading()) return;

    /* find a suitable directory */
    QString fname;
    if (TabPage *tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget()))
        fname = tabPage->textEdit()->getFileName();

    QString path;
    if (!fname.isEmpty())
    {
        if (QFile::exists (fname))
            path = fname;
        else
        {
            QDir dir = QFileInfo (fname).absoluteDir();
            if (!dir.exists())
                dir = QDir::home();
            path = dir.path();
        }
    }
    else
    {
        /* I like the last opened file to be remembered */
        fname = lastFile_;
        if (!fname.isEmpty())
        {
            QDir dir = QFileInfo (fname).absoluteDir();
            if (!dir.exists())
                dir = QDir::home();
            path = dir.path();
        }
        else
        {
            QDir dir = QDir::home();
            path = dir.path();
        }
    }

    if (hasAnotherDialog()) return;
    updateShortcuts (true);
    QString filter = tr ("All Files (*)");
    if (!fname.isEmpty()
        && QFileInfo (fname).fileName().contains ('.'))
    {
        /* if relevant, do filtering to make opening of similar files easier */
        filter = tr ("All Files (*);;.%1 Files (*.%1)").arg (fname.section ('.', -1, -1));
    }
    FileDialog dialog (this, static_cast<FPsingleton*>(qApp)->getConfig().getNativeDialog());
    dialog.setAcceptMode (QFileDialog::AcceptOpen);
    dialog.setWindowTitle (tr ("Open file..."));
    dialog.setFileMode (QFileDialog::ExistingFiles);
    dialog.setNameFilter (filter);
    /*dialog.setLabelText (QFileDialog::Accept, tr ("Open"));
    dialog.setLabelText (QFileDialog::Reject, tr ("Cancel"));*/
    if (QFileInfo (path).isDir())
        dialog.setDirectory (path);
    else
    {
        dialog.setDirectory (path.section ("/", 0, -2)); // it's a shame the KDE's file dialog is buggy and needs this
        dialog.selectFile (path);
        dialog.autoScroll();
    }
    if (dialog.exec())
    {
        const QStringList files = dialog.selectedFiles();
        bool multiple (files.count() > 1 || isLoading());
        for (const QString &file : files)
            newTabFromName (file, 0, 0, multiple);
    }
    updateShortcuts (false);
}
/*************************/
// Check if the file is already opened for editing somewhere else.
bool FPwin::alreadyOpen (TabPage *tabPage) const
{
    bool res = false;

    QString fileName = tabPage->textEdit()->getFileName();
    QFileInfo info (fileName);
    QString target = info.isSymLink() ? info.symLinkTarget() // consider symlinks too
                                      : fileName;
    FPsingleton *singleton = static_cast<FPsingleton*>(qApp);
    for (int i = 0; i < singleton->Wins.count(); ++i)
    {
        FPwin *thisOne = singleton->Wins.at (i);
        for (int j = 0; j < thisOne->ui->tabWidget->count(); ++j)
        {
            TabPage *thisTabPage = qobject_cast<TabPage*>(thisOne->ui->tabWidget->widget (j));
            if (thisOne == this && thisTabPage == tabPage)
                continue;
            TextEdit *thisTextEdit = thisTabPage->textEdit();
            if (thisTextEdit->isReadOnly())
                continue;
            QFileInfo thisInfo (thisTextEdit->getFileName());
            QString thisTarget = thisInfo.isSymLink() ? thisInfo.symLinkTarget()
                                                      : thisTextEdit->getFileName();
            if (thisTarget == target)
            {
                res = true;
                break;
            }
        }
        if (res) break;
    }
    return res;
}
/*************************/
void FPwin::enforceEncoding (QAction*)
{
    /* here, we don't need to check if some files are loading
       because encoding has no keyboard shortcut or tool button */

    int index = ui->tabWidget->currentIndex();
    TabPage *tabPage = qobject_cast< TabPage *>(ui->tabWidget->widget (index));
    if (tabPage == nullptr) return;

    TextEdit *textEdit = tabPage->textEdit();
    QString fname = textEdit->getFileName();
    if (!fname.isEmpty())
    {
        if (savePrompt (index, false) != SAVED)
        { // back to the previous encoding
            encodingToCheck (textEdit->getEncoding());
            return;
        }
        /* if the file is removed, close its tab to open a new one */
        if (!QFile::exists (fname))
            deleteTabPage (index, false, false);
        loadText (fname, true, true,
                  0, 0,
                  textEdit->isUneditable(), false);
    }
    else
    {
        /* just change the statusbar text; the doc
           might be saved later with the new encoding */
        textEdit->setEncoding (checkToEncoding());
        if (ui->statusBar->isVisible())
        {
            QLabel *statusLabel = ui->statusBar->findChild<QLabel *>("statusLabel");
            QString str = statusLabel->text();
            QString encodStr = tr ("Encoding");
            // the next info is about lines; there's no syntax info
            QString lineStr = "</i>&nbsp;&nbsp;&nbsp;<b>" + tr ("Lines");
            int i = str.indexOf (encodStr);
            int j = str.indexOf (lineStr);
            int offset = encodStr.count() + 9; // size of ":</b> <i>"
            str.replace (i + offset, j - i - offset, checkToEncoding());
            statusLabel->setText (str);
        }
    }
}
/*************************/
void FPwin::reload()
{
    if (isLoading()) return;

    int index = ui->tabWidget->currentIndex();
    TabPage *tabPage = qobject_cast< TabPage *>(ui->tabWidget->widget (index));
    if (tabPage == nullptr) return;

    if (savePrompt (index, false) != SAVED) return;

    TextEdit *textEdit = tabPage->textEdit();
    QString fname = textEdit->getFileName();
    /* if the file is removed, close its tab to open a new one */
    if (!QFile::exists (fname))
        deleteTabPage (index, false, false);
    if (!fname.isEmpty())
    {
        loadText (fname, false, true,
                  textEdit->getSaveCursor() ? 1 : 0);
    }
}
/*************************/
static inline int trailingSpaces (const QString &str)
{
    int i = 0;
    while (i < str.length())
    {
        if (!str.at (str.length() - 1 - i).isSpace())
            return i;
        ++i;
    }
    return i;
}
/*************************/
// This is for both "Save" and "Save As"
bool FPwin::saveFile (bool keepSyntax)
{
    if (!isReady()) return false;

    int index = ui->tabWidget->currentIndex();
    TabPage *tabPage = qobject_cast< TabPage *>(ui->tabWidget->widget (index));
    if (tabPage == nullptr) return false;

    TextEdit *textEdit = tabPage->textEdit();
    QString fname = textEdit->getFileName();
    QString filter = tr ("All Files (*)");
    if (fname.isEmpty())
        fname = lastFile_;
    else if (QFileInfo (fname).fileName().contains ('.'))
    {
        /* if relevant, do filtering to prevent disastrous overwritings */
        filter = tr (".%1 Files (*.%1);;All Files (*)").arg (fname.section ('.', -1, -1));
    }

    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();

    if (fname.isEmpty()
        || !QFile::exists (fname)
        || textEdit->getFileName().isEmpty())
    {
        bool restorable = false;
        if (fname.isEmpty())
        {
            QDir dir = QDir::home();
            fname = dir.filePath (tr ("Untitled"));
        }
        else if (!QFile::exists (fname))
        {
            QDir dir = QFileInfo (fname).absoluteDir();
            if (!dir.exists())
            {
                dir = QDir::home();
                if (textEdit->getFileName().isEmpty())
                    filter = tr ("All Files (*)");
            }
            /* if the removed file is opened in this tab and its
               containing folder still exists, it's restorable */
            else if (!textEdit->getFileName().isEmpty())
                restorable = true;

            /* add the file name */
            if (!textEdit->getFileName().isEmpty())
                fname = dir.filePath (QFileInfo (fname).fileName());
            else
                fname = dir.filePath (tr ("Untitled"));
        }
        else
            fname = QFileInfo (fname).absoluteDir().filePath (tr ("Untitled"));
        if (!restorable
            && QObject::sender() != ui->actionSaveAs
        )
        {
            if (hasAnotherDialog()) return false;
            updateShortcuts (true);
            FileDialog dialog (this, config.getNativeDialog());
            dialog.setAcceptMode (QFileDialog::AcceptSave);
            dialog.setWindowTitle (tr ("Save as..."));
            dialog.setFileMode (QFileDialog::AnyFile);
            dialog.setNameFilter (filter);
            dialog.setDirectory (fname.section ("/", 0, -2)); // workaround for KDE
            dialog.selectFile (fname);
            dialog.autoScroll();
            /*dialog.setLabelText (QFileDialog::Accept, tr ("Save"));
            dialog.setLabelText (QFileDialog::Reject, tr ("Cancel"));*/
            if (dialog.exec())
            {
                fname = dialog.selectedFiles().at (0);
                if (fname.isEmpty() || QFileInfo (fname).isDir())
                {
                    updateShortcuts (false);
                    return false;
                }
            }
            else
            {
                updateShortcuts (false);
                return false;
            }
            updateShortcuts (false);
        }
    }

    if (QObject::sender() == ui->actionSaveAs)
    {
        if (hasAnotherDialog()) return false;
        updateShortcuts (true);
        FileDialog dialog (this, config.getNativeDialog());
        dialog.setAcceptMode (QFileDialog::AcceptSave);
        dialog.setWindowTitle (tr ("Save as..."));
        dialog.setFileMode (QFileDialog::AnyFile);
        dialog.setNameFilter (filter);
        dialog.setDirectory (fname.section ("/", 0, -2));
        dialog.selectFile (fname);
        dialog.autoScroll();
        if (dialog.exec())
        {
            fname = dialog.selectedFiles().at (0);
            if (fname.isEmpty() || QFileInfo (fname).isDir())
            {
                updateShortcuts (false);
                return false;
            }
        }
        else
        {
            updateShortcuts (false);
            return false;
        }
        updateShortcuts (false);
    }
    if (config.getRemoveTrailingSpaces() && textEdit->getProg() != "diff")
    {
        waitToMakeBusy();
        QTextBlock block = textEdit->document()->firstBlock();
        QTextCursor tmpCur = textEdit->textCursor();
        tmpCur.beginEditBlock();
        while (block.isValid())
        {
            if (const int num = trailingSpaces (block.text()))
            {
                tmpCur.setPosition (block.position() + block.text().length());
                if (num > 1 && (textEdit->getProg() == "markdown" || textEdit->getProg() == "fountain")) // md sees two trailing spaces as a new line
                    tmpCur.movePosition (QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor, num - 2);
                else
                    tmpCur.movePosition (QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor, num);
                tmpCur.removeSelectedText();
            }
            block = block.next();
        }
        tmpCur.endEditBlock();
        unbusy();
    }
    if (config.getAppendEmptyLine()
        && !textEdit->document()->lastBlock().text().isEmpty())
    {
        QTextCursor tmpCur = textEdit->textCursor();
        tmpCur.beginEditBlock();
        tmpCur.movePosition (QTextCursor::End);
        tmpCur.insertBlock();
        tmpCur.endEditBlock();
    }
    QTextDocumentWriter writer (fname, "plaintext");
    bool success = false;
    if (!success)
        success = writer.write (textEdit->document());

    if (success)
    {
        QFileInfo fInfo (fname);

        textEdit->document()->setModified (false);
        textEdit->setFileName (fname);
        textEdit->setSize (fInfo.size());
        textEdit->setLastModified (fInfo.lastModified());
        ui->actionReload->setDisabled (false);
        setTitle (fname);
        QString tip (fInfo.absolutePath());
        if (!tip.endsWith ("/")) tip += "/";
        QFontMetrics metrics (QToolTip::font());
        QString elidedTip = "<p style='white-space:pre'>"
#if (QT_VERSION >= QT_VERSION_CHECK(5,11,0))
                            + metrics.elidedText (tip, Qt::ElideMiddle, 200 * metrics.horizontalAdvance (' '))
#else
                            + metrics.elidedText (tip, Qt::ElideMiddle, 200 * metrics.width (' '))
#endif
                            + "</p>";
        ui->tabWidget->setTabToolTip (index, elidedTip);
        if (!sideItems_.isEmpty())
        {
            if (QListWidgetItem *wi = sideItems_.key (tabPage))
                wi->setToolTip (elidedTip);
        }
        lastFile_ = fname;
        config.addRecentFile (lastFile_);
        if (!keepSyntax)
        {
            QString prevLan = textEdit->getProg();
            setProgLang (textEdit);
            if (prevLan != textEdit->getProg())
            {
                if (config.getShowLangSelector() && config.getSyntaxByDefault())
                {
                    if (textEdit->getLang() == textEdit->getProg())
                        textEdit->setLang (QString()); // not enforced because it's the real syntax
                    updateLangBtn (textEdit);
                }

                if (ui->statusBar->isVisible()
                    && textEdit->getWordNumber() != -1)
                {
                    disconnect (textEdit->document(), &QTextDocument::contentsChange, this, &FPwin::updateWordInfo);
                }

                if (textEdit->getLang().isEmpty())
                {
                    syntaxHighlighting (textEdit, false);
                    if (ui->actionSyntax->isChecked())
                        syntaxHighlighting (textEdit);
                }

                if (ui->statusBar->isVisible())
                {
                    QLabel *statusLabel = ui->statusBar->findChild<QLabel *>("statusLabel");
                    QString str = statusLabel->text();
                    QString syntaxStr = tr ("Syntax");
                    int i = str.indexOf (syntaxStr);
                    if (i == -1) // there was no real language before saving (prevLan was "url")
                    {
                        QString lineStr = "&nbsp;&nbsp;&nbsp;<b>" + tr ("Lines");
                        int j = str.indexOf (lineStr);
                        syntaxStr = "&nbsp;&nbsp;&nbsp;<b>" + tr ("Syntax") + QString (":</b> <i>%1</i>")
                                                                              .arg (textEdit->getProg());
                        str.insert (j, syntaxStr);
                    }
                    else
                    {
                        if (textEdit->getProg() == "url") // there's no real language after saving
                        {
                            syntaxStr = "&nbsp;&nbsp;&nbsp;<b>" + tr ("Syntax");
                            QString lineStr = "&nbsp;&nbsp;&nbsp;<b>" + tr ("Lines");
                            int j = str.indexOf (syntaxStr);
                            int k = str.indexOf (lineStr);
                            str.remove (j, k - j);
                        }
                        else // the language is changed by saving
                        {
                            QString lineStr = "</i>&nbsp;&nbsp;&nbsp;<b>" + tr ("Lines");
                            int j = str.indexOf (lineStr);
                            int offset = syntaxStr.count() + 9; // size of ":</b> <i>"
                            str.replace (i + offset, j - i - offset, textEdit->getProg());
                        }
                    }
                    statusLabel->setText (str);
                    if (textEdit->getWordNumber() != -1)
                        connect (textEdit->document(), &QTextDocument::contentsChange, this, &FPwin::updateWordInfo);
                }
            }
        }
    }
    else
    {
        QString str = writer.device()->errorString();
        showWarningBar ("<center><b><big>" + tr ("Cannot be saved!") + "</big></b></center>\n"
                        + "<center><i>" + QString ("<center><i>%1.</i></center>").arg (str) + "<i/></center>");
    }

    if (success && textEdit->isReadOnly() && !alreadyOpen (tabPage))
         QTimer::singleShot (0, this, &FPwin::makeEditable);

    return success;
}
void FPwin::cutText()
{
    if (TabPage *tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget()))
        tabPage->textEdit()->cut();
}
void FPwin::copyText()
{
    if (TabPage *tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget()))
        tabPage->textEdit()->copy();
}
void FPwin::pasteText()
{
    if (TabPage *tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget()))
        tabPage->textEdit()->paste();
}
void FPwin::deleteText()
{
    if (TabPage *tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget()))
    {
        TextEdit *textEdit = tabPage->textEdit();
        if (!textEdit->isReadOnly())
            textEdit->insertPlainText ("");
    }
}
void FPwin::selectAllText()
{
    if (TabPage *tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget()))
        tabPage->textEdit()->selectAll();
}
void FPwin::makeEditable()
{
    if (!isReady()) return;

    TabPage *tabPage = qobject_cast< TabPage *>(ui->tabWidget->currentWidget());
    if (tabPage == nullptr) return;

    TextEdit *textEdit = tabPage->textEdit();
    bool textIsSelected = textEdit->textCursor().hasSelection();

    textEdit->setReadOnly (false);
    Config config = static_cast<FPsingleton*>(qApp)->getConfig();
    if (!textEdit->hasDarkScheme())
    {
        textEdit->viewport()->setStyleSheet (QString (".QWidget {"
                                                      "color: black;"
                                                      "background-color: rgb(%1, %1, %1);}")
                                             .arg (config.getLightBgColorValue()));
    }
    else
    {
        textEdit->viewport()->setStyleSheet (QString (".QWidget {"
                                                      "color: white;"
                                                      "background-color: rgb(%1, %1, %1);}")
                                             .arg (config.getDarkBgColorValue()));
    }
    ui->actionEdit->setVisible (false);

    ui->actionPaste->setEnabled (true);
    ui->actionCopy->setEnabled (textIsSelected);
    ui->actionCut->setEnabled (textIsSelected);
    ui->actionDelete->setEnabled (textIsSelected);
    connect (textEdit, &QPlainTextEdit::copyAvailable, ui->actionCut, &QAction::setEnabled);
    connect (textEdit, &QPlainTextEdit::copyAvailable, ui->actionDelete, &QAction::setEnabled);
    if (config.getSaveUnmodified())
        ui->actionSave->setEnabled (true);
}
void FPwin::undoing()
{
    if (TabPage *tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget()))
        tabPage->textEdit()->undo();
}
void FPwin::redoing()
{
    if (TabPage *tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget()))
        tabPage->textEdit()->redo();
}
void FPwin::changeTab (QListWidgetItem *current, QListWidgetItem* /*previous*/)
{
    if (sideItems_.isEmpty()) return;
    ui->tabWidget->setCurrentWidget (sideItems_.value (current));
}
void FPwin::onTabChanged (int index)
{
    if (index > -1)
    {
        QString fname = qobject_cast< TabPage *>(ui->tabWidget->widget (index))->textEdit()->getFileName();
        if (fname.isEmpty() || QFile::exists (fname))
            closeWarningBar();
    }
    else closeWarningBar();
}
void FPwin::tabSwitch (int index)
{
    TabPage *tabPage = qobject_cast< TabPage *>(ui->tabWidget->widget (index));
    if (tabPage == nullptr)
    {
        setWindowTitle ("FeatherPad[*]");
        setWindowModified (false);
        return;
    }

    TextEdit *textEdit = tabPage->textEdit();
    if (!tabPage->isSearchBarVisible())
        textEdit->setFocus();
    QString fname = textEdit->getFileName();
    bool modified (textEdit->document()->isModified());

    QFileInfo info;
    QString shownName;
    if (fname.isEmpty())
    {
        if (textEdit->getProg() == "help")
            shownName = "** " + tr ("Help") + " **";
        else
            shownName = tr ("Untitled");
    }
    else
    {
        info.setFile (fname);
        shownName = (fname.contains ("/") ? fname
                                          : info.absolutePath() + "/" + fname);
        if (!QFile::exists (fname))
            onOpeningNonexistent();
        else if (textEdit->getLastModified() != info.lastModified())
            showWarningBar ("<center><b><big>" + tr ("This file has been modified elsewhere or in another way!") + "</big></b></center>\n"
                            + "<center>" + tr ("Please be careful about reloading or saving this document!") + "</center>");
    }
    if (modified)
        shownName.prepend ("*");
    setWindowTitle (shownName);

    /* although the window size, wrapping state or replacing text may have changed or
       the replace dock may have been closed, hlight() will be called automatically */
    //if (!textEdit->getSearchedText().isEmpty()) hlight();

    /* correct the encoding menu */
    encodingToCheck (textEdit->getEncoding());

    Config config = static_cast<FPsingleton*>(qApp)->getConfig();

    /* correct the states of some buttons */
    ui->actionUndo->setEnabled (textEdit->document()->isUndoAvailable());
    ui->actionRedo->setEnabled (textEdit->document()->isRedoAvailable());
    bool readOnly = textEdit->isReadOnly();
    if (!config.getSaveUnmodified())
        ui->actionSave->setEnabled (modified);
    else
        ui->actionSave->setDisabled (readOnly || textEdit->isUneditable());
    ui->actionReload->setEnabled (!fname.isEmpty());
    if (fname.isEmpty()
        && !modified
        && !textEdit->document()->isEmpty()) // 'Help' is an exception
    {
        ui->actionEdit->setVisible (false);
        ui->actionSaveAs->setEnabled (true);
    }
    else
    {
        ui->actionEdit->setVisible (readOnly && !textEdit->isUneditable());
        ui->actionSaveAs->setEnabled (!textEdit->isUneditable());
    }
    ui->actionPaste->setEnabled (!readOnly);
    bool textIsSelected = textEdit->textCursor().hasSelection();
    ui->actionCopy->setEnabled (textIsSelected);
    ui->actionCut->setEnabled (!readOnly && textIsSelected);
    ui->actionDelete->setEnabled (!readOnly && textIsSelected);

    if (isScriptLang (textEdit->getProg()) && info.isExecutable())
        ui->actionRun->setVisible (config.getExecuteScripts());
    else
        ui->actionRun->setVisible (false);

    /* handle the spinbox */
    if (ui->spinBox->isVisible())
        ui->spinBox->setMaximum (textEdit->document()->blockCount());

    /* handle the statusbar */
    if (ui->statusBar->isVisible())
    {
        statusMsgWithLineCount (textEdit->document()->blockCount());
        QToolButton *wordButton = ui->statusBar->findChild<QToolButton *>("wordButton");
        if (textEdit->getWordNumber() == -1)
        {
            if (wordButton)
                wordButton->setVisible (true);
            if (textEdit->document()->isEmpty()) // make an exception
                updateWordInfo();
        }
        else
        {
            if (wordButton)
                wordButton->setVisible (false);
            QLabel *statusLabel = ui->statusBar->findChild<QLabel *>("statusLabel");
            statusLabel->setText (QString ("%1 <i>%2</i>")
                                  .arg (statusLabel->text())
                                  .arg (textEdit->getWordNumber()));
        }
        showCursorPos();
    }
    if (config.getShowLangSelector() && config.getSyntaxByDefault())
        updateLangBtn (textEdit);

    /* al last, set the title of Replacment dock */
    if (ui->dockReplace->isVisible())
    {
        QString title = textEdit->getReplaceTitle();
        if (!title.isEmpty())
            ui->dockReplace->setWindowTitle (title);
        else
            ui->dockReplace->setWindowTitle (tr ("Replacement"));
    }
    else
        textEdit->setReplaceTitle (QString());
}
/*************************/
void FPwin::fontDialog()
{
    if (isLoading()) return;

    TabPage *tabPage = qobject_cast< TabPage *>(ui->tabWidget->currentWidget());
    if (tabPage == nullptr) return;

    if (hasAnotherDialog()) return;
    updateShortcuts (true);

    TextEdit *textEdit = tabPage->textEdit();

    QFont currentFont = textEdit->getDefaultFont();
    FontDialog fd (currentFont, this);
    fd.setWindowModality (Qt::WindowModal);
    fd.move (x() + width()/2 - fd.width()/2,
             y() + height()/2 - fd.height()/ 2);
    if (fd.exec())
    {
        QFont newFont = fd.selectedFont();
        Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
        if (config.getRemFont())
        {
            config.setFont (newFont);
            config.writeConfig();

            FPsingleton *singleton = static_cast<FPsingleton*>(qApp);
            for (int i = 0; i < singleton->Wins.count(); ++i)
            {
                FPwin *thisWin = singleton->Wins.at (i);
                for (int j = 0; j < thisWin->ui->tabWidget->count(); ++j)
                {
                    TextEdit *thisTextEdit = qobject_cast< TabPage *>(thisWin->ui->tabWidget->widget (j))->textEdit();
                    thisTextEdit->setEditorFont (newFont);
                }
            }
        }
        else
            textEdit->setEditorFont (newFont);

        /* the font can become larger... */
        textEdit->adjustScrollbars();
        /* ... or smaller */
        reformat (textEdit);
    }
    updateShortcuts (false);
}
/*************************/
void FPwin::changeEvent (QEvent *event)
{
    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
    if (config.getRemSize() && event->type() == QEvent::WindowStateChange)
    {
        if (windowState() == Qt::WindowFullScreen)
        {
            config.setIsFull (true);
            config.setIsMaxed (false);
        }
        else if (windowState() == (Qt::WindowFullScreen ^ Qt::WindowMaximized))
        {
            config.setIsFull (true);
            config.setIsMaxed (true);
        }
        else
        {
            config.setIsFull (false);
            if (windowState() == Qt::WindowMaximized)
                config.setIsMaxed (true);
            else
                config.setIsMaxed (false);
        }
    }
    QWidget::changeEvent (event);
}
/*************************/
bool FPwin::event (QEvent *event)
{
    if (event->type() == QEvent::ActivationChange && isActiveWindow())
    {
        if (TabPage *tabPage = qobject_cast< TabPage *>(ui->tabWidget->currentWidget()))
        {
            TextEdit *textEdit = tabPage->textEdit();
            QString fname = textEdit->getFileName();
            if (!fname.isEmpty())
            {
                if (!QFile::exists (fname))
                {
                    if (isLoading())
                        connect (this, &FPwin::finishedLoading, this, &FPwin::onOpeningNonexistent, Qt::UniqueConnection);
                    else
                        onOpeningNonexistent();
                }
                else if (textEdit->getLastModified() != QFileInfo (fname).lastModified())
                    showWarningBar ("<center><b><big>" + tr ("This file has been modified elsewhere or in another way!") + "</big></b></center>\n"
                                    + "<center>" + tr ("Please be careful about reloading or saving this document!") + "</center>");
            }
        }
    }
    return QMainWindow::event (event);
}
/*************************/
void FPwin::showHideSearch()
{
    if (!isReady()) return;

    TabPage *tabPage = qobject_cast< TabPage *>(ui->tabWidget->currentWidget());
    if (tabPage == nullptr) return;
    
    int count = ui->tabWidget->count();
    for (int indx = 0; indx < count; ++indx)
    {
	  TabPage *page = qobject_cast< TabPage *>(ui->tabWidget->widget (indx));
        page->setSearchBarVisible (true);
    }
    
    tabPage->focusSearchBar();
    
}
/*************************/
void FPwin::jumpTo()
{
    if (!isReady()) return;

    bool visibility = ui->spinBox->isVisible();    
    
    
    QFont font_bold = ui->spinBox->font();
    QFont font_demi = ui->spinBox->font();
    
    font_bold.setPointSize( 20 );
    font_bold.setWeight( QFont::Black );
    font_demi.setPointSize( 20 );
    font_demi.setWeight( QFont::DemiBold );
    
    ui->spinBox->setFont( font_bold );
    ui->label->setFont( font_demi );
    ui->checkBox->setFont( font_demi );
    
    
    for (int i = 0; i < ui->tabWidget->count(); ++i)
    {
        TextEdit *thisTextEdit = qobject_cast< TabPage *>(ui->tabWidget->widget (i))->textEdit();
        if (!ui->actionLineNumbers->isChecked())
            thisTextEdit->showLineNumbers (!visibility);

        if (!visibility)
        {
            /* setMaximum() isn't a slot */
            connect (thisTextEdit->document(),
                     &QTextDocument::blockCountChanged,
                     this,
                     &FPwin::setMax);
        }
        else
            disconnect (thisTextEdit->document(),
                        &QTextDocument::blockCountChanged,
                        this,
                        &FPwin::setMax);
    }

    TabPage *tabPage = qobject_cast< TabPage *>(ui->tabWidget->currentWidget());
    if (tabPage)
    {
        if (!visibility && ui->tabWidget->count() > 0)
            ui->spinBox->setMaximum (tabPage->textEdit()
                                     ->document()
                                     ->blockCount());
    }
    
    ui->spinBox->setVisible (true);
    ui->label->setVisible (true);
    ui->checkBox->setVisible (true);

    ui->spinBox->setFocus();
    ui->spinBox->selectAll();
}
/*************************/
void FPwin::setMax (const int max)
{
    ui->spinBox->setMaximum (max);
}
/*************************/
void FPwin::goTo()
{
    /* workaround for not being able to use returnPressed()
       because of protectedness of spinbox's QLineEdit */
    if (!ui->spinBox->hasFocus()) return;

    if (TabPage *tabPage = qobject_cast< TabPage *>(ui->tabWidget->currentWidget()))
    {
        TextEdit *textEdit = tabPage->textEdit();
        QTextBlock block = textEdit->document()->findBlockByNumber (ui->spinBox->value() - 1);
        int pos = block.position();
        QTextCursor start = textEdit->textCursor();
        if (ui->checkBox->isChecked())
            start.setPosition (pos, QTextCursor::KeepAnchor);
        else
            start.setPosition (pos);
        textEdit->setTextCursor (start);
        
        ui->spinBox->setVisible(false);
        ui->label->setVisible(false);
        ui->checkBox->setVisible(false);
        
    }
}
/*************************/
void FPwin::showLN (bool checked)
{
    int count = ui->tabWidget->count();
    if (count == 0) return;

    if (checked)
    {
        for (int i = 0; i < count; ++i)
            qobject_cast< TabPage *>(ui->tabWidget->widget (i))->textEdit()->showLineNumbers (true);
    }
    else if (!ui->spinBox->isVisible()) // also the spinBox affects line numbers visibility
    {
        for (int i = 0; i < count; ++i)
            qobject_cast< TabPage *>(ui->tabWidget->widget (i))->textEdit()->showLineNumbers (false);
    }
}
/*************************/
void FPwin::toggleWrapping()
{
    int count = ui->tabWidget->count();
    if (count == 0) return;

    bool wrapLines = ui->actionWrap->isChecked();
    for (int i = 0; i < count; ++i)
        qobject_cast<TabPage*>(ui->tabWidget->widget (i))->textEdit()->setLineWrapMode (wrapLines ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
    if (TabPage *tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget()))
        reformat (tabPage->textEdit());
}
/*************************/
void FPwin::toggleIndent()
{
    int count = ui->tabWidget->count();
    if (count == 0) return;

    if (ui->actionIndent->isChecked())
    {
        for (int i = 0; i < count; ++i)
            qobject_cast< TabPage *>(ui->tabWidget->widget (i))->textEdit()->setAutoIndentation (true);
    }
    else
    {
        for (int i = 0; i < count; ++i)
            qobject_cast< TabPage *>(ui->tabWidget->widget (i))->textEdit()->setAutoIndentation (false);
    }
}
/*************************/
void FPwin::encodingToCheck (const QString& encoding)
{
    if (encoding != "UTF-8")
        ui->actionOther->setDisabled (true);

    if (encoding == "UTF-8")
        ui->actionUTF_8->setChecked (true);
    else if (encoding == "UTF-16")
        ui->actionUTF_16->setChecked (true);
    else if (encoding == "CP1256")
        ui->actionWindows_Arabic->setChecked (true);
    else if (encoding == "ISO-8859-1")
        ui->actionISO_8859_1->setChecked (true);
    else if (encoding == "ISO-8859-15")
        ui->actionISO_8859_15->setChecked (true);
    else if (encoding == "CP1252")
        ui->actionWindows_1252->setChecked (true);
    else if (encoding == "CP1251")
        ui->actionCyrillic_CP1251->setChecked (true);
    else if (encoding == "KOI8-U")
        ui->actionCyrillic_KOI8_U->setChecked (true);
    else if (encoding == "ISO-8859-5")
        ui->actionCyrillic_ISO_8859_5->setChecked (true);
    else if (encoding == "BIG5")
        ui->actionChinese_BIG5->setChecked (true);
    else if (encoding == "GB18030")
        ui->actionChinese_GB18030->setChecked (true);
    else if (encoding == "ISO-2022-JP")
        ui->actionJapanese_ISO_2022_JP->setChecked (true);
    else if (encoding == "ISO-2022-JP-2")
        ui->actionJapanese_ISO_2022_JP_2->setChecked (true);
    else if (encoding == "ISO-2022-KR")
        ui->actionJapanese_ISO_2022_KR->setChecked (true);
    else if (encoding == "CP932")
        ui->actionJapanese_CP932->setChecked (true);
    else if (encoding == "EUC-JP")
        ui->actionJapanese_EUC_JP->setChecked (true);
    else if (encoding == "CP949")
        ui->actionKorean_CP949->setChecked (true);
    else if (encoding == "CP1361")
        ui->actionKorean_CP1361->setChecked (true);
    else if (encoding == "EUC-KR")
        ui->actionKorean_EUC_KR->setChecked (true);
    else
    {
        ui->actionOther->setDisabled (false);
        ui->actionOther->setChecked (true);
    }
}
/*************************/
const QString FPwin::checkToEncoding() const
{
    QString encoding;

    if (ui->actionUTF_8->isChecked())
        encoding = "UTF-8";
    else if (ui->actionUTF_16->isChecked())
        encoding = "UTF-16";
    else if (ui->actionWindows_Arabic->isChecked())
        encoding = "CP1256";
    else if (ui->actionISO_8859_1->isChecked())
        encoding = "ISO-8859-1";
    else if (ui->actionISO_8859_15->isChecked())
        encoding = "ISO-8859-15";
    else if (ui->actionWindows_1252->isChecked())
        encoding = "CP1252";
    else if (ui->actionCyrillic_CP1251->isChecked())
        encoding = "CP1251";
    else if (ui->actionCyrillic_KOI8_U->isChecked())
        encoding = "KOI8-U";
    else if (ui->actionCyrillic_ISO_8859_5->isChecked())
        encoding = "ISO-8859-5";
    else if (ui->actionChinese_BIG5->isChecked())
        encoding = "BIG5";
    else if (ui->actionChinese_GB18030->isChecked())
        encoding = "GB18030";
    else if (ui->actionJapanese_ISO_2022_JP->isChecked())
        encoding = "ISO-2022-JP";
    else if (ui->actionJapanese_ISO_2022_JP_2->isChecked())
        encoding = "ISO-2022-JP-2";
    else if (ui->actionJapanese_ISO_2022_KR->isChecked())
        encoding = "ISO-2022-KR";
    else if (ui->actionJapanese_CP932->isChecked())
        encoding = "CP932";
    else if (ui->actionJapanese_EUC_JP->isChecked())
        encoding = "EUC-JP";
    else if (ui->actionKorean_CP949->isChecked())
        encoding = "CP949";
    else if (ui->actionKorean_CP1361->isChecked())
        encoding = "CP1361";
    else if (ui->actionKorean_EUC_KR->isChecked())
        encoding = "EUC-KR";
    else
        encoding = "UTF-8";

    return encoding;
}
/*************************/
void FPwin::docProp()
{
    bool showCurPos = static_cast<FPsingleton*>(qApp)->getConfig().getShowCursorPos();
    if (ui->statusBar->isVisible())
    {
        for (int i = 0; i < ui->tabWidget->count(); ++i)
        {
            TextEdit *thisTextEdit = qobject_cast< TabPage *>(ui->tabWidget->widget (i))->textEdit();
            disconnect (thisTextEdit, &QPlainTextEdit::blockCountChanged, this, &FPwin::statusMsgWithLineCount);
            disconnect (thisTextEdit, &QPlainTextEdit::selectionChanged, this, &FPwin::statusMsg);
            if (showCurPos)
                disconnect (thisTextEdit, &QPlainTextEdit::cursorPositionChanged, this, &FPwin::showCursorPos);
            /* don't delete the cursor position label because the statusbar might be shown later */
        }
        ui->statusBar->setVisible (false);
        return;
    }

    TabPage *tabPage = qobject_cast< TabPage *>(ui->tabWidget->currentWidget());
    if (tabPage == nullptr) return;

    statusMsgWithLineCount (tabPage->textEdit()->document()->blockCount());
    for (int i = 0; i < ui->tabWidget->count(); ++i)
    {
        TextEdit *thisTextEdit = qobject_cast< TabPage *>(ui->tabWidget->widget (i))->textEdit();
        connect (thisTextEdit, &QPlainTextEdit::blockCountChanged, this, &FPwin::statusMsgWithLineCount);
        connect (thisTextEdit, &QPlainTextEdit::selectionChanged, this, &FPwin::statusMsg);
        if (showCurPos)
            connect (thisTextEdit, &QPlainTextEdit::cursorPositionChanged, this, &FPwin::showCursorPos);
    }

    ui->statusBar->setVisible (true);
    if (showCurPos)
    {
        addCursorPosLabel();
        showCursorPos();
    }
    if (QToolButton *wordButton = ui->statusBar->findChild<QToolButton *>("wordButton"))
        wordButton->setVisible (true);
    updateWordInfo();
}
/*************************/
// Set the status bar text according to the block count.
void FPwin::statusMsgWithLineCount (const int lines)
{
    TextEdit *textEdit = qobject_cast< TabPage *>(ui->tabWidget->currentWidget())->textEdit();
    /* ensure that the signal comes from the active tab if this is about a tab a signal */
    if (qobject_cast<TextEdit*>(QObject::sender()) && QObject::sender() != textEdit)
        return;

    QLabel *statusLabel = ui->statusBar->findChild<QLabel *>("statusLabel");

    /* the order: Encoding -> Syntax -> Lines -> Sel. Chars -> Words */
    QString encodStr = "<b>" + tr ("Encoding") + QString (":</b> <i>%1</i>").arg (textEdit->getEncoding());
    QString syntaxStr;
    if (textEdit->getProg() != "help" && textEdit->getProg() != "url")
        syntaxStr = "&nbsp;&nbsp;&nbsp;<b>" + tr ("Syntax") + QString (":</b> <i>%1</i>").arg (textEdit->getProg());
    QString lineStr = "&nbsp;&nbsp;&nbsp;<b>" + tr ("Lines") + QString (":</b> <i>%1</i>").arg (lines);
    QString selStr = "&nbsp;&nbsp;&nbsp;<b>" + tr ("Sel. Chars")
                     + QString (":</b> <i>%1</i>").arg (textEdit->textCursor().selectedText().size());
    QString wordStr = "&nbsp;&nbsp;&nbsp;<b>" + tr ("Words") + ":</b>";

    statusLabel->setText (encodStr + syntaxStr + lineStr + selStr + wordStr);
}
/*************************/
// Change the status bar text when the selection changes.
void FPwin::statusMsg()
{
    QLabel *statusLabel = ui->statusBar->findChild<QLabel *>("statusLabel");
    int sel = qobject_cast< TabPage *>(ui->tabWidget->currentWidget())->textEdit()
              ->textCursor().selectedText().size();
    QString str = statusLabel->text();
    QString selStr = tr ("Sel. Chars");
    QString wordStr = "&nbsp;&nbsp;&nbsp;<b>" + tr ("Words");
    int i = str.indexOf (selStr) + selStr.count();
    int j = str.indexOf (wordStr);
    if (sel == 0)
    {
        QString prevSel = str.mid (i + 9, j - i - 13); // j - i - 13 --> j - (i + 9[":</b> <i>]") - 4["</i>"]
        if (prevSel.toInt() == 0) return;
    }
    QString charN;
    charN.setNum (sel);
    str.replace (i + 9, j - i - 13, charN);
    statusLabel->setText (str);
}
/*************************/
void FPwin::showCursorPos()
{
    QLabel *posLabel = ui->statusBar->findChild<QLabel *>("posLabel");
    if (!posLabel) return;

    TabPage *tabPage = qobject_cast< TabPage *>(ui->tabWidget->currentWidget());
    if (tabPage == nullptr) return;

    int pos = tabPage->textEdit()->textCursor().positionInBlock();
    QString charN;
    charN.setNum (pos); charN = "<i> " + charN + "</i>";
    QString str = posLabel->text();
    QString scursorStr = "<b>" + tr ("Position:") + "</b>";
    int i = scursorStr.count();
    str.replace (i, str.count() - i, charN);
    posLabel->setText (str);
}
/*************************/
void FPwin::updateLangBtn (TextEdit *textEdit)
{
    QToolButton *langButton = ui->statusBar->findChild<QToolButton *>("langButton");
    if (!langButton) return;

    langButton->setEnabled (!textEdit->isUneditable() && textEdit->getHighlighter());

    QString lang = textEdit->getLang().isEmpty() ? textEdit->getProg()
                                                 : textEdit->getLang();
    QAction *action = langs_.value (lang);
    if (!action) // it's "help", "url" or a bug (some language isn't included)
    {
        lang = tr ("Normal");
        action = langs_.value (lang); // "Normal" is the last action
    }
    langButton->setText (lang);
    if (action) // always the case
        action->setChecked (true);
}
/*************************/
void FPwin::enforceLang (QAction *action)
{
    QToolButton *langButton = ui->statusBar->findChild<QToolButton *>("langButton");
    if (!langButton) return;

    TabPage *tabPage = qobject_cast< TabPage *>(ui->tabWidget->currentWidget());
    if (tabPage == nullptr) return;

    TextEdit *textEdit = tabPage->textEdit();
    QString lang = action->text();
    lang.remove ('&'); // because of KAcceleratorManager
    langButton->setText (lang);
    if (lang == tr ("Normal"))
    {
        if (textEdit->getProg() == "srt" || textEdit->getProg() == "gtkrc"
            || textEdit->getProg() == "changelog")
        { // not listed
            lang = textEdit->getProg();
        }
        else
            lang = "url"; // the default highlighter
    }
    if (textEdit->getProg() == lang || textEdit->getProg() == "help")
        textEdit->setLang (QString()); // not enforced
    else
        textEdit->setLang (lang);
    if (ui->actionSyntax->isChecked())
    {
        syntaxHighlighting (textEdit, false);
        waitToMakeBusy(); // it may take a while with huge texts
        syntaxHighlighting (textEdit, true, lang);
        QTimer::singleShot (0, this, [this]() {unbusy();});
    }
}
/*************************/
void FPwin::updateWordInfo (int /*position*/, int charsRemoved, int charsAdded)
{
    QToolButton *wordButton = ui->statusBar->findChild<QToolButton *>("wordButton");
    if (!wordButton) return;
    TabPage *tabPage = qobject_cast< TabPage *>(ui->tabWidget->currentWidget());
    if (tabPage == nullptr) return;
    TextEdit *textEdit = tabPage->textEdit();
    /* ensure that the signal comes from the active tab (when the info is going to be removed) */
    if (qobject_cast<QTextDocument*>(QObject::sender()) && QObject::sender() != textEdit->document())
        return;

    if (wordButton->isVisible())
    {
        QLabel *statusLabel = ui->statusBar->findChild<QLabel *>("statusLabel");
        int words = textEdit->getWordNumber();
        if (words == -1)
        {
            words = textEdit->toPlainText()
#if (QT_VERSION >= QT_VERSION_CHECK(5,15,0))
                    .split (QRegularExpression ("(\\s|\\n|\\r)+"), Qt::SkipEmptyParts)
#else
                    .split (QRegularExpression ("(\\s|\\n|\\r)+"), QString::SkipEmptyParts)
#endif
                    .count();
            textEdit->setWordNumber (words);
        }

        wordButton->setVisible (false);
        statusLabel->setText (QString ("%1 <i>%2</i>")
                              .arg (statusLabel->text())
                              .arg (words));
        connect (textEdit->document(), &QTextDocument::contentsChange, this, &FPwin::updateWordInfo);
    }
    else if (charsRemoved > 0 || charsAdded > 0) // not if only the format is changed
    {
        disconnect (textEdit->document(), &QTextDocument::contentsChange, this, &FPwin::updateWordInfo);
        textEdit->setWordNumber (-1);
        wordButton->setVisible (true);
        statusMsgWithLineCount (textEdit->document()->blockCount());
    }
}
/*************************/
void FPwin::filePrint()
{
    if (isLoading()) return;

    TabPage *tabPage = qobject_cast< TabPage *>(ui->tabWidget->currentWidget());
    if (tabPage == nullptr) return;

    if (hasAnotherDialog()) return;
    updateShortcuts (true);

    TextEdit *textEdit = tabPage->textEdit();

    /* complete the syntax highlighting when printing
       because the whole document may not be highlighted */
    waitToMakeBusy();
    if (Highlighter *highlighter = qobject_cast< Highlighter *>(textEdit->getHighlighter()))
    {
        QTextCursor start = textEdit->textCursor();
        start.movePosition (QTextCursor::Start);
        QTextCursor end = textEdit->textCursor();
        end.movePosition (QTextCursor::End);
        highlighter->setLimit (start, end);
        QTextBlock block = start.block();
        while (block.isValid() && block.blockNumber() <= end.blockNumber())
        {
            if (TextBlockData *data = static_cast<TextBlockData *>(block.userData()))
            {
                if (!data->isHighlighted())
                    highlighter->rehighlightBlock (block);
            }
            block = block.next();
        }
    }
    QTimer::singleShot (0, this, [this]() {unbusy();}); // wait for the dialog too

    QPrinter printer (QPrinter::HighResolution);

    /* choose an appropriate name and directory */
    QString fileName = textEdit->getFileName();
    if (fileName.isEmpty())
    {
        QDir dir = QDir::home();
        fileName= dir.filePath (tr ("Untitled"));
    }
    if (printer.outputFormat() == QPrinter::PdfFormat)
        printer.setOutputFileName (fileName.append (".pdf"));
    /*else if (printer.outputFormat() == QPrinter::PostScriptFormat)
        printer.setOutputFileName (fileName.append (".ps"));*/

    QPrintDialog dlg (&printer, this);
    dlg.setWindowModality (Qt::WindowModal);
    if (textEdit->textCursor().hasSelection())
        dlg.setOption (QAbstractPrintDialog::PrintSelection);
    dlg.setWindowTitle (tr ("Print Document"));
    if (dlg.exec() == QDialog::Accepted)
    {
        waitToMakeBusy();
        textEdit->print (&printer);
        QTimer::singleShot (0, this, [this]() {unbusy();});
    }

    updateShortcuts (false);
}
void FPwin::nextTab()
{
    if (isLoading()) return;

    int index = ui->tabWidget->currentIndex();
    if (index == -1) return;
        if (QWidget *widget = ui->tabWidget->widget (index + 1))
            ui->tabWidget->setCurrentWidget (widget);
        else if (static_cast<FPsingleton*>(qApp)->getConfig().getTabWrapAround())
            ui->tabWidget->setCurrentIndex (0);
}
void FPwin::previousTab()
{
    if (isLoading()) return;

    int index = ui->tabWidget->currentIndex();
    if (index == -1) return;
        if (QWidget *widget = ui->tabWidget->widget (index - 1))
            ui->tabWidget->setCurrentWidget (widget);
        else if (static_cast<FPsingleton*>(qApp)->getConfig().getTabWrapAround())
        {
            int count = ui->tabWidget->count();
            if (count > 0)
                ui->tabWidget->setCurrentIndex (count - 1);
        }
}
void FPwin::dropTab (const QString& str)
{
#if (QT_VERSION >= QT_VERSION_CHECK(5,15,0))
    QStringList list = str.split ("+", Qt::SkipEmptyParts);
#else
    QStringList list = str.split ("+", QString::SkipEmptyParts);
#endif
    if (list.count() != 2)
    {
        ui->tabWidget->tabBar()->finishMouseMoveEvent();
        return;
    }
    int index = list.at (1).toInt();
    if (index <= -1) // impossible
    {
        ui->tabWidget->tabBar()->finishMouseMoveEvent();
        return;
    }

    FPsingleton *singleton = static_cast<FPsingleton*>(qApp);
    FPwin *dragSource = nullptr;
    for (int i = 0; i < singleton->Wins.count(); ++i)
    {
        if (singleton->Wins.at (i)->winId() == list.at (0).toULongLong())
        {
            dragSource = singleton->Wins.at (i);
            break;
        }
    }
    if (dragSource == this
        || dragSource == nullptr) // impossible
    {
        ui->tabWidget->tabBar()->finishMouseMoveEvent();
        return;
    }

    Config config = static_cast<FPsingleton*>(qApp)->getConfig();

    closeWarningBar();
    dragSource->closeWarningBar();

    QString tooltip = dragSource->ui->tabWidget->tabToolTip (index);
    QString tabText = dragSource->ui->tabWidget->tabText (index);
    bool spin = false;
    bool ln = false;
    if (dragSource->ui->spinBox->isVisible())
        spin = true;
    if (dragSource->ui->actionLineNumbers->isChecked())
        ln = true;

    TabPage *tabPage = qobject_cast< TabPage *>(dragSource->ui->tabWidget->widget (index));
    if (tabPage == nullptr)
    {
        ui->tabWidget->tabBar()->finishMouseMoveEvent();
        return;
    }
    TextEdit *textEdit = tabPage->textEdit();

    disconnect (textEdit, &TextEdit::resized, dragSource, &FPwin::hlight);
    disconnect (textEdit, &TextEdit::updateRect, dragSource, &FPwin::hlight);
    disconnect (textEdit, &QPlainTextEdit::textChanged, dragSource, &FPwin::hlight);
    if (dragSource->ui->statusBar->isVisible())
    {
        disconnect (textEdit, &QPlainTextEdit::blockCountChanged, dragSource, &FPwin::statusMsgWithLineCount);
        disconnect (textEdit, &QPlainTextEdit::selectionChanged, dragSource, &FPwin::statusMsg);
        if (dragSource->ui->statusBar->findChild<QLabel *>("posLabel"))
            disconnect (textEdit, &QPlainTextEdit::cursorPositionChanged, dragSource, &FPwin::showCursorPos);
    }
    disconnect (textEdit, &QPlainTextEdit::copyAvailable, dragSource->ui->actionCut, &QAction::setEnabled);
    disconnect (textEdit, &QPlainTextEdit::copyAvailable, dragSource->ui->actionDelete, &QAction::setEnabled);
    disconnect (textEdit, &QPlainTextEdit::copyAvailable, dragSource->ui->actionCopy, &QAction::setEnabled);
    disconnect (textEdit, &QWidget::customContextMenuRequested, dragSource, &FPwin::editorContextMenu);
    disconnect (textEdit, &TextEdit::fileDropped, dragSource, &FPwin::newTabFromName);
    disconnect (textEdit, &TextEdit::updateBracketMatching, dragSource, &FPwin::matchBrackets);
    disconnect (textEdit, &QPlainTextEdit::blockCountChanged, dragSource, &FPwin::formatOnBlockChange);
    disconnect (textEdit, &TextEdit::updateRect, dragSource, &FPwin::formatTextRect);
    disconnect (textEdit, &TextEdit::resized, dragSource, &FPwin::formatTextRect);

    disconnect (textEdit->document(), &QTextDocument::contentsChange, dragSource, &FPwin::updateWordInfo);
    disconnect (textEdit->document(), &QTextDocument::contentsChange, dragSource, &FPwin::formatOnTextChange);
    disconnect (textEdit->document(), &QTextDocument::blockCountChanged, dragSource, &FPwin::setMax);
    disconnect (textEdit->document(), &QTextDocument::modificationChanged, dragSource, &FPwin::asterisk);
    disconnect (textEdit->document(), &QTextDocument::undoAvailable, dragSource->ui->actionUndo, &QAction::setEnabled);
    disconnect (textEdit->document(), &QTextDocument::redoAvailable, dragSource->ui->actionRedo, &QAction::setEnabled);
    if (!config.getSaveUnmodified())
        disconnect (textEdit->document(), &QTextDocument::modificationChanged, dragSource, &FPwin::enableSaving);

    disconnect (tabPage, &TabPage::find, dragSource, &FPwin::find);
    disconnect (tabPage, &TabPage::searchFlagChanged, dragSource, &FPwin::searchFlagChanged);

    /* it's important to release mouse before tab removal because otherwise, the source
       tabbar might not be updated properly with tab reordering during a fast drag-and-drop */
    dragSource->ui->tabWidget->tabBar()->releaseMouse();

    dragSource->ui->tabWidget->removeTab (index); // there can't be a side-pane here
    int count = dragSource->ui->tabWidget->count();
    if (count == 1)
        dragSource->updateGUIForSingleTab (true);

    /***************************************************************************
     ***** The tab is dropped into this window; so insert it as a new tab. *****
     ***************************************************************************/

    int insertIndex = ui->tabWidget->currentIndex() + 1;

    /* first, set the new info... */
    lastFile_ = textEdit->getFileName();
    textEdit->setGreenSel (QList<QTextEdit::ExtraSelection>());
    textEdit->setRedSel (QList<QTextEdit::ExtraSelection>());
    /* ... then insert the detached widget,
       considering whether the searchbar should be shown... */
    if (!textEdit->getSearchedText().isEmpty())
    {
        if (insertIndex == 0 // the window has no tab yet
            || !qobject_cast< TabPage *>(ui->tabWidget->widget (insertIndex - 1))->isSearchBarVisible())
        {
            for (int i = 0; i < ui->tabWidget->count(); ++i)
                qobject_cast< TabPage *>(ui->tabWidget->widget (i))->setSearchBarVisible (true);
        }
    }
    else if (insertIndex > 0)
    {
        tabPage->setSearchBarVisible (qobject_cast< TabPage *>(ui->tabWidget->widget (insertIndex - 1))
                                      ->isSearchBarVisible());
    }
    if (ui->tabWidget->count() == 0) // the tab will be inserted and switched to below
        enableWidgets (true);
    else if (ui->tabWidget->count() == 1)
        updateGUIForSingleTab (false); // tab detach and switch actions
    bool isLink = lastFile_.isEmpty() ? false : QFileInfo (lastFile_).isSymLink();
    ui->tabWidget->insertTab (insertIndex, tabPage,
                              isLink ? QIcon (":icons/link.svg") : QIcon(),
                              tabText);
    ui->tabWidget->setCurrentIndex (insertIndex);
    QList<QTextEdit::ExtraSelection> es;
    if ((ln || spin)
        && (ui->actionLineNumbers->isChecked() || ui->spinBox->isVisible()))
    {
        es.prepend (textEdit->currentLineSelection());
    }
    textEdit->setExtraSelections (es);

    /* at last, set all properties correctly */
    ui->tabWidget->setTabToolTip (insertIndex, tooltip);
    /* reload buttons, syntax highlighting, jump bar, line numbers */
    if (ui->actionSyntax->isChecked())
    {
        waitToMakeBusy(); // it may take a while with huge texts
        syntaxHighlighting (textEdit, true, textEdit->getLang());
        QTimer::singleShot (0, this, [this]() {unbusy();});
    }
    else if (!ui->actionSyntax->isChecked() && textEdit->getHighlighter())
    { // there's no connction to the drag target yet
        textEdit->setDrawIndetLines (false);
        Highlighter *highlighter = qobject_cast< Highlighter *>(textEdit->getHighlighter());
        textEdit->setHighlighter (nullptr);
        delete highlighter; highlighter = nullptr;
    }
    if (ui->spinBox->isVisible())
        connect (textEdit->document(), &QTextDocument::blockCountChanged, this, &FPwin::setMax);
    if (ui->actionLineNumbers->isChecked() || ui->spinBox->isVisible())
        textEdit->showLineNumbers (true);
    else
        textEdit->showLineNumbers (false);
    /* searching */
    if (!textEdit->getSearchedText().isEmpty())
    {
        connect (textEdit, &QPlainTextEdit::textChanged, this, &FPwin::hlight);
        connect (textEdit, &TextEdit::updateRect, this, &FPwin::hlight);
        connect (textEdit, &TextEdit::resized, this, &FPwin::hlight);
        /* restore yellow highlights, which will automatically
           set the current line highlight if needed because the
           spin button and line number menuitem are set above */
        hlight();
    }
    /* status bar */
    if (ui->statusBar->isVisible())
    {
        connect (textEdit, &QPlainTextEdit::blockCountChanged, this, &FPwin::statusMsgWithLineCount);
        connect (textEdit, &QPlainTextEdit::selectionChanged, this, &FPwin::statusMsg);
        if (ui->statusBar->findChild<QLabel *>("posLabel"))
        {
            showCursorPos();
            connect (textEdit, &QPlainTextEdit::cursorPositionChanged, this, &FPwin::showCursorPos);
        }
        if (textEdit->getWordNumber() != -1)
            connect (textEdit->document(), &QTextDocument::contentsChange, this, &FPwin::updateWordInfo);
    }
    if (ui->actionWrap->isChecked() && textEdit->lineWrapMode() == QPlainTextEdit::NoWrap)
        textEdit->setLineWrapMode (QPlainTextEdit::WidgetWidth);
    else if (!ui->actionWrap->isChecked() && textEdit->lineWrapMode() == QPlainTextEdit::WidgetWidth)
        textEdit->setLineWrapMode (QPlainTextEdit::NoWrap);
    /* auto indentation */
    if (ui->actionIndent->isChecked() && textEdit->getAutoIndentation() == false)
        textEdit->setAutoIndentation (true);
    else if (!ui->actionIndent->isChecked() && textEdit->getAutoIndentation() == true)
        textEdit->setAutoIndentation (false);
    /* the remaining signals */
    connect (textEdit->document(), &QTextDocument::undoAvailable, ui->actionUndo, &QAction::setEnabled);
    connect (textEdit->document(), &QTextDocument::redoAvailable, ui->actionRedo, &QAction::setEnabled);
    if (!config.getSaveUnmodified())
        connect (textEdit->document(), &QTextDocument::modificationChanged, this, &FPwin::enableSaving);
    connect (textEdit->document(), &QTextDocument::modificationChanged, this, &FPwin::asterisk);
    connect (textEdit, &QPlainTextEdit::copyAvailable, ui->actionCopy, &QAction::setEnabled);

    connect (tabPage, &TabPage::find, this, &FPwin::find);
    connect (tabPage, &TabPage::searchFlagChanged, this, &FPwin::searchFlagChanged);

    if (!textEdit->isReadOnly())
    {
        connect (textEdit, &QPlainTextEdit::copyAvailable, ui->actionCut, &QAction::setEnabled);
        connect (textEdit, &QPlainTextEdit::copyAvailable, ui->actionDelete, &QAction::setEnabled);
    }
    connect (textEdit, &TextEdit::fileDropped, this, &FPwin::newTabFromName);
    connect (textEdit, &QWidget::customContextMenuRequested, this, &FPwin::editorContextMenu);

    textEdit->setFocus();

    stealFocus();

    if (count == 0)
        QTimer::singleShot (0, dragSource, &QWidget::close);
}
/*************************/
void FPwin::tabContextMenu (const QPoint& p)
{
    int tabNum = ui->tabWidget->count();
    QTabBar *tbar = ui->tabWidget->tabBar();
    rightClicked_ = tbar->tabAt (p);
    if (rightClicked_ < 0) return;

    QString fname = qobject_cast< TabPage *>(ui->tabWidget->widget (rightClicked_))
                    ->textEdit()->getFileName();
    QMenu menu;
    bool showMenu = false;
    if (tabNum > 1)
    {
        QWidgetAction *labelAction = new QWidgetAction (&menu);
        QLabel *label = new QLabel ("<center><b>" + tr ("%1 Pages").arg (tabNum) + "</b></center>");
        label->setMargin (4);
        labelAction->setDefaultWidget (label);
        menu.addAction (labelAction);
        menu.addSeparator();

        showMenu = true;
        if (rightClicked_ < tabNum - 1)
            menu.addAction (ui->actionCloseRight);
        if (rightClicked_ > 0)
            menu.addAction (ui->actionCloseLeft);
        menu.addSeparator();
        if (rightClicked_ < tabNum - 1 && rightClicked_ > 0)
            menu.addAction (ui->actionCloseOther);
        menu.addAction (ui->actionCloseAll);
        if (!fname.isEmpty())
            menu.addSeparator();
    }
    if (!fname.isEmpty())
    {
        showMenu = true;
        menu.addAction (ui->actionCopyName);
        menu.addAction (ui->actionCopyPath);
        QFileInfo info (fname);
        if (info.isSymLink())
        {
            menu.addSeparator();
            QAction *action = menu.addAction (QIcon (":icons/link.svg"), tr ("Copy Target Path"));
            connect (action, &QAction::triggered, [info] {
                QApplication::clipboard()->setText (info.symLinkTarget());
            });
            action = menu.addAction (QIcon (":icons/link.svg"), tr ("Open Target Here"));
            connect (action, &QAction::triggered, this, [this, info] {
                QString targetName = info.symLinkTarget();
                for (int i = 0; i < ui->tabWidget->count(); ++i)
                {
                    TabPage *thisTabPage = qobject_cast<TabPage*>(ui->tabWidget->widget (i));
                    if (targetName == thisTabPage->textEdit()->getFileName())
                    {
                        ui->tabWidget->setCurrentWidget (thisTabPage);
                        return;
                    }
                }
                newTabFromName (targetName, 0, 0);
            });
        }
        if (QFile::exists (fname))
        {
            menu.addSeparator();
            QAction *action = menu.addAction (symbolicIcon::icon (":icons/document-open.svg"), tr ("Open Containing Folder"));
            connect (action, &QAction::triggered, this, [fname] {
                QString folder = fname.section ("/", 0, -2);
                if (!QProcess::startDetached ("gio", QStringList() << "open" << folder))
                    QDesktopServices::openUrl (QUrl::fromLocalFile (folder));
            });
        }
    }
    if (showMenu)
        menu.exec (tbar->mapToGlobal (p));
    rightClicked_ = -1;
}
void FPwin::prefDialog()
{
    if (isLoading()) return;
    if (hasAnotherDialog()) return;
    updateShortcuts (true);
    PrefDialog dlg (this);
    dlg.exec();
    updateShortcuts (false);
}
static inline void moveToWordStart (QTextCursor& cur, bool forward)
{
    const QString blockText = cur.block().text();
    const int l = blockText.length();
    int indx = cur.positionInBlock();
    if (indx < l)
    {
        QChar ch = blockText.at (indx);
        while (!ch.isLetterOrNumber() && ch != '\'' && ch != '-'
               && ch != QChar (QChar::Nbsp) && ch != QChar (0x200C))
        {
            cur.movePosition (QTextCursor::NextCharacter);
            ++indx;
            if (indx == l)
            {
                if (cur.movePosition (QTextCursor::NextBlock))
                    moveToWordStart (cur, forward);
                return;
            }
            ch = blockText.at (indx);
        }
    }
    if (!forward && indx > 0)
    {
        QChar ch = blockText.at (indx - 1);
        while (ch.isLetterOrNumber() || ch == '\'' || ch == '-'
               || ch == QChar (QChar::Nbsp) || ch == QChar (0x200C))
        {
            cur.movePosition (QTextCursor::PreviousCharacter);
            --indx;
            ch = blockText.at (indx);
            if (indx == 0) break;
        }
    }
}

static inline void selectWord (QTextCursor& cur)
{
    moveToWordStart (cur, true);
    const QString blockText = cur.block().text();
    const int l = blockText.length();
    int indx = cur.positionInBlock();
    if (indx < l)
    {
        QChar ch = blockText.at (indx);
        while (ch.isLetterOrNumber() || ch == '\'' || ch == '-'
               || ch == QChar (QChar::Nbsp) || ch == QChar (0x200C))
        {
            cur.movePosition (QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
            ++indx;
            if (indx == l) break;
            ch = blockText.at (indx);
        }
    }

    /* no dash, single quote mark or number at the start */
    while (!cur.selectedText().isEmpty()
           && (cur.selectedText().at (0) == '-' || cur.selectedText().at (0) == '\''
               || cur.selectedText().at (0).isNumber()))
    {
        int p = cur.position();
        cur.setPosition (cur.anchor() + 1);
        cur.setPosition (p, QTextCursor::KeepAnchor);
    }
    /* no dash or single quote mark at the end */
    while (!cur.selectedText().isEmpty()
           && (cur.selectedText().endsWith ("-") || cur.selectedText().endsWith ("\'")))
    {
        cur.setPosition (cur.position() - 1, QTextCursor::KeepAnchor);
    }
}
void FPwin::manageSessions()
{
    if (!isReady()) return;
    FPsingleton *singleton = static_cast<FPsingleton*>(qApp);
    for (int i = 0; i < singleton->Wins.count(); ++i)
    {
        QList<QDialog*> dialogs  = singleton->Wins.at (i)->findChildren<QDialog*>();
        for (int j = 0; j < dialogs.count(); ++j)
        {
            if (dialogs.at (j)->objectName() == "sessionDialog")
            {
                dialogs.at (j)->raise();
                dialogs.at (j)->activateWindow();
                return;
            }
        }
    }
    SessionDialog *dlg = new SessionDialog (this);
    dlg->setAttribute (Qt::WA_DeleteOnClose);
    dlg->show();
    dlg->raise();
    dlg->activateWindow();
}
void FPwin::pauseAutoSaving (bool pause)
{
    if (!autoSaver_) return;
    if (pause)
    {
        autoSaverPause_.start();
        autoSaverRemainingTime_ = autoSaver_->remainingTime();
    }
    else if (autoSaverPause_.isValid())
    {
        if (autoSaverPause_.hasExpired (autoSaverRemainingTime_))
        {
            autoSaverPause_.invalidate();
            autoSave();
        }
        else
            autoSaverPause_.invalidate();
    }
}
/*************************/
void FPwin::startAutoSaving (bool start, int interval)
{
    if (start)
    {
        if (!autoSaver_)
        {
            autoSaver_ = new QTimer (this);
            connect (autoSaver_, &QTimer::timeout, this, &FPwin::autoSave);
        }
        autoSaver_->setInterval (interval * 1000 * 60);
        autoSaver_->start();
    }
    else if (autoSaver_)
    {
        if (autoSaver_->isActive())
            autoSaver_->stop();
        delete autoSaver_; autoSaver_ = nullptr;
    }
}
/*************************/
void FPwin::autoSave()
{
    /* since there are important differences between this
       and saveFile(), we can't use the latter here.
       We especially don't show any prompt or warning here. */
    if (autoSaverPause_.isValid()) return;
    QTimer::singleShot (0, this, [=]() {
        if (!autoSaver_ || !autoSaver_->isActive())
            return;
        saveAllFiles (false); // without warning
    });
}
/*************************/
void FPwin::saveAllFiles (bool showWarning)
{
    int index = ui->tabWidget->currentIndex();
    if (index == -1) return;

    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();

    bool error = false;
    for (int indx = 0; indx < ui->tabWidget->count(); ++indx)
    {
        TabPage *thisTabPage = qobject_cast< TabPage *>(ui->tabWidget->widget (indx));
        TextEdit *thisTextEdit = thisTabPage->textEdit();
        if (thisTextEdit->isUneditable() || !thisTextEdit->document()->isModified())
            continue;
        QString fname = thisTextEdit->getFileName();
        if (fname.isEmpty() || !QFile::exists (fname))
            continue;
        /* make changes to the document if needed */
        if (config.getRemoveTrailingSpaces() && thisTextEdit->getProg() != "diff")
        {
            waitToMakeBusy();
            QTextBlock block = thisTextEdit->document()->firstBlock();
            QTextCursor tmpCur = thisTextEdit->textCursor();
            tmpCur.beginEditBlock();
            while (block.isValid())
            {
                if (const int num = trailingSpaces (block.text()))
                {
                    tmpCur.setPosition (block.position() + block.text().length());
                    if (num > 1 && (thisTextEdit->getProg() == "markdown" || thisTextEdit->getProg() == "fountain"))
                        tmpCur.movePosition (QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor, num - 2);
                    else
                        tmpCur.movePosition (QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor, num);
                    tmpCur.removeSelectedText();
                }
                block = block.next();
            }
            tmpCur.endEditBlock();
            unbusy();
        }
        if (config.getAppendEmptyLine()
            && !thisTextEdit->document()->lastBlock().text().isEmpty())
        {
            QTextCursor tmpCur = thisTextEdit->textCursor();
            tmpCur.beginEditBlock();
            tmpCur.movePosition (QTextCursor::End);
            tmpCur.insertBlock();
            tmpCur.endEditBlock();
        }

        QTextDocumentWriter writer (fname, "plaintext");
        if (writer.write (thisTextEdit->document()))
        {
            inactiveTabModified_ = (indx != index);
            thisTextEdit->document()->setModified (false);
            QFileInfo fInfo (fname);
            thisTextEdit->setSize (fInfo.size());
            thisTextEdit->setLastModified (fInfo.lastModified());
            setTitle (fname, (!inactiveTabModified_ ? -1 : indx));
            config.addRecentFile (fname); // recently saved also means recently opened
            /* uninstall and reinstall the syntax highlgihter if the programming language is changed */
            QString prevLan = thisTextEdit->getProg();
            setProgLang (thisTextEdit);
            if (prevLan != thisTextEdit->getProg())
            {
                if (config.getShowLangSelector() && config.getSyntaxByDefault())
                {
                    if (thisTextEdit->getLang() == thisTextEdit->getProg())
                        thisTextEdit->setLang (QString()); // not enforced because it's the real syntax
                    if (!inactiveTabModified_)
                        updateLangBtn (thisTextEdit);
                }

                if (!inactiveTabModified_ && ui->statusBar->isVisible()
                    && thisTextEdit->getWordNumber() != -1)
                { // we want to change the statusbar text below
                    disconnect (thisTextEdit->document(), &QTextDocument::contentsChange, this, &FPwin::updateWordInfo);
                }

                if (thisTextEdit->getLang().isEmpty())
                { // restart the syntax highlighting only when the language isn't forced
                    syntaxHighlighting (thisTextEdit, false);
                    if (ui->actionSyntax->isChecked())
                        syntaxHighlighting (thisTextEdit);
                }

                if (!inactiveTabModified_ && ui->statusBar->isVisible())
                { // correct the statusbar text just by replacing the old syntax info
                    QLabel *statusLabel = ui->statusBar->findChild<QLabel *>("statusLabel");
                    QString str = statusLabel->text();
                    QString syntaxStr = tr ("Syntax");
                    int i = str.indexOf (syntaxStr);
                    if (i == -1) // there was no real language before saving (prevLan was "url")
                    {
                        QString lineStr = "&nbsp;&nbsp;&nbsp;<b>" + tr ("Lines");
                        int j = str.indexOf (lineStr);
                        syntaxStr = "&nbsp;&nbsp;&nbsp;<b>" + tr ("Syntax") + QString (":</b> <i>%1</i>")
                                                                              .arg (thisTextEdit->getProg());
                        str.insert (j, syntaxStr);
                    }
                    else
                    {
                        if (thisTextEdit->getProg() == "url") // there's no real language after saving
                        {
                            syntaxStr = "&nbsp;&nbsp;&nbsp;<b>" + tr ("Syntax");
                            QString lineStr = "&nbsp;&nbsp;&nbsp;<b>" + tr ("Lines");
                            int j = str.indexOf (syntaxStr);
                            int k = str.indexOf (lineStr);
                            str.remove (j, k - j);
                        }
                        else // the language is changed by saving
                        {
                            QString lineStr = "</i>&nbsp;&nbsp;&nbsp;<b>" + tr ("Lines");
                            int j = str.indexOf (lineStr);
                            int offset = syntaxStr.count() + 9; // size of ":</b> <i>"
                            str.replace (i + offset, j - i - offset, thisTextEdit->getProg());
                        }
                    }
                    statusLabel->setText (str);
                    if (thisTextEdit->getWordNumber() != -1)
                        connect (thisTextEdit->document(), &QTextDocument::contentsChange, this, &FPwin::updateWordInfo);
                }
            }
            inactiveTabModified_ = false;
        }
        else error = true;
    }
    if (showWarning && error)
        showWarningBar ("<center><b><big>" + tr ("Some files cannot be saved!") + "</big></b></center>");
}
/*************************/
void FPwin::aboutDialog()
{
    if (isLoading()) return;

    if (hasAnotherDialog()) return;
    updateShortcuts (true);

    class AboutDialog : public QDialog {
    public:
#if (QT_VERSION >= QT_VERSION_CHECK(5,15,0))
        explicit AboutDialog (QWidget* parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags()) : QDialog (parent, f) {
#else
        explicit AboutDialog (QWidget* parent = nullptr, Qt::WindowFlags f = nullptr) : QDialog (parent, f) {
#endif
            aboutUi.setupUi (this);
            aboutUi.textLabel->setOpenExternalLinks (true);
        }
        void setTabTexts (const QString& first, const QString& sec) {
            aboutUi.tabWidget->setTabText (0, first);
            aboutUi.tabWidget->setTabText (1, sec);
        }
        void setMainIcon (const QIcon& icn) {
            aboutUi.iconLabel->setPixmap (icn.pixmap (64, 64));
        }
        void settMainTitle (const QString& title) {
            aboutUi.titleLabel->setText (title);
        }
        void setMainText (const QString& txt) {
            aboutUi.textLabel->setText (txt);
        }
    private:
        Ui::AboutDialog aboutUi;
    };

    AboutDialog dialog (this);
    QIcon FPIcon = QIcon::fromTheme ("featherpad");
    if (FPIcon.isNull())
        FPIcon = QIcon (":icons/featherpad.svg");
    dialog.setMainIcon (FPIcon);
    dialog.settMainTitle (QString ("<center><b><big>%1 %2</big></b></center><br>").arg (qApp->applicationName()).arg (qApp->applicationVersion()));
    dialog.setMainText ("<center> " + tr ("A lightweight, tabbed, plain-text editor") + " </center>\n<center> "
                        + tr ("based on Qt") + " </center><br><center> "
                        + tr ("Author")+": <a href='mailto:tsujan2000@gmail.com?Subject=My%20Subject'>Pedram Pourang ("
                        + tr ("aka.") + " Tsu Jan)</a> </center><p></p>");
    dialog.setTabTexts (tr ("About FeatherPad"), tr ("Translators"));
    dialog.setWindowTitle (tr ("About FeatherPad"));
    dialog.setWindowModality (Qt::WindowModal);
    dialog.exec();
    updateShortcuts (false);
}
void FPwin::stealFocus()
{
    raise();
    activateWindow();
    QTimer::singleShot (0, this, [this]() {
        if (QWindow *win = windowHandle())
            win->requestActivate();
    });
}

}
