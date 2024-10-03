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
#include "encoding.h"
#include "filedialog.h"
#include "messagebox.h"
#include "pref.h"
#include "session.h"
#include "fontDialog.h"
#include "loading.h"
#include "warningbar.h"

#include <QPrintDialog>
#include <QToolTip>
#include <QWindow>
#include <QScrollBar>
#include <QWidgetAction>
#include <fstream>
#include <QPrinter>
#include <QClipboard>
#include <QProcess>
#include <QTextDocumentWriter>
#include <QTextCodec>
#include <QTextBlock>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDesktopServices>
#include <QPushButton>


#ifdef HAS_X11
#include "x11.h"
#endif

namespace FeatherPad {
QString modified_prefix = QString("[ * ]");
QString noname = QString("[ noname ]");
QString program_name = QString( "fpad" );
QString title_prefix = program_name + QString( " - " );
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
    inactiveTabModified_ = false;
    ui->spinBox->hide();
    ui->label->hide();
    ui->checkBox->hide();
    QToolButton *wordButton = new QToolButton();
    wordButton->setObjectName ("wordButton");
    wordButton->setFocusPolicy (Qt::NoFocus);
    wordButton->setAutoRaise (true);
    wordButton->setToolButtonStyle (Qt::ToolButtonIconOnly);
    wordButton->setToolTip ("<p style='white-space:pre'>"
                            + tr ("Calculate number of words\n(For huge texts, this may be CPU-intensive.)")
                            + "</p>");
    QWidget::setTabOrder (ui->lineEditFind, ui->lineEditReplace);
    QWidget::setTabOrder (ui->lineEditReplace, ui->toolButtonNext);
    ui->toolButtonNext->setToolTip (tr ("Next") + " (" + QKeySequence (Qt::Key_F8).toString (QKeySequence::NativeText) + ")");
    ui->toolButtonPrv->setToolTip (tr ("Previous") + " (" + QKeySequence (Qt::Key_F9).toString (QKeySequence::NativeText) + ")");
    ui->toolButtonAll->setToolTip (tr ("Replace all") + " (" + QKeySequence (Qt::Key_F10).toString (QKeySequence::NativeText) + ")");
    ui->dockReplace->setVisible (false);
    const auto allMenus = ui->menuBar->findChildren<QMenu*>();
    for (const auto &thisMenu : allMenus)
    {
        const auto menuActions = thisMenu->actions();
        for (const auto &menuAction : menuActions)
        {
            QKeySequence seq = menuAction->shortcut();
            if (!seq.isEmpty())
                defaultShortcuts_.insert (menuAction, seq);
        }
    }
    defaultShortcuts_.insert (ui->actionSaveAllFiles, QKeySequence());
    defaultShortcuts_.insert (ui->actionFont, QKeySequence());
    applyConfigOnStarting();
    QWidget* spacer = new QWidget();
    spacer->setSizePolicy (QSizePolicy::Expanding, QSizePolicy::Preferred);
    QMenu *menu = new QMenu (ui->menuBar);
    menu->addMenu (ui->menuFile);
    menu->addMenu (ui->menuEdit);
    menu->addMenu (ui->menuOptions);
    menu->addMenu (ui->menuSearch);
    ui->actionMenu->setMenu(menu);
    newTab();
    aGroup_ = new QActionGroup (this);
    ui->actionUTF_8->setActionGroup (aGroup_);
    ui->actionUTF_16->setActionGroup (aGroup_);
    ui->actionISO_8859_1->setActionGroup (aGroup_);
    ui->actionISO_8859_15->setActionGroup (aGroup_);
    ui->actionWindows_1252->setActionGroup (aGroup_);
    ui->actionCyrillic_CP1251->setActionGroup (aGroup_);
    ui->actionCyrillic_KOI8_U->setActionGroup (aGroup_);
    ui->actionCyrillic_ISO_8859_5->setActionGroup (aGroup_);
    ui->actionUTF_8->setChecked (true);
    if (standalone_
        || !static_cast<FPsingleton*>(qApp)->isX11())
    {
        ui->tabWidget->noTabDND();
    }
    connect (ui->actionNew, &QAction::triggered, this, &FPwin::newTab);
    connect (ui->tabWidget->tabBar(), &TabBar::addEmptyTab, this, &FPwin::newTab);
    QShortcut* next_tab_shortcut = new QShortcut(QKeySequence(Qt::ALT + Qt::Key_W), this);
    connect (next_tab_shortcut, &QShortcut::activated, this, &FPwin::nextTab);
    QShortcut* prev_tab_shortcut = new QShortcut(QKeySequence(Qt::ALT + Qt::Key_Q), this);
    connect (prev_tab_shortcut , &QShortcut::activated, this, &FPwin::previousTab);
    QShortcut* close_tab_shortcut = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_W), this);
    connect (close_tab_shortcut , &QShortcut::activated, this, &FPwin::closeTab);
    connect (ui->tabWidget, &QTabWidget::tabCloseRequested, this, &FPwin::closeTabAtIndex);
    connect (ui->actionOpen, &QAction::triggered, this, &FPwin::fileOpen);
    connect (ui->actionReload, &QAction::triggered, this, &FPwin::reload);
    connect (aGroup_, &QActionGroup::triggered, this, &FPwin::enforceEncoding);
    connect (ui->actionSave, &QAction::triggered, [=]{saveFile ();});
    connect (ui->actionSaveAs, &QAction::triggered, this, [=]{saveFile ();});
    connect (ui->actionSaveAllFiles, &QAction::triggered, this, [=]{saveAllFiles (true);});
    QShortcut* cut_shortcut = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_X), this);
    connect (cut_shortcut , &QShortcut::activated, this, &FPwin::cutText);
    QShortcut* copy_shortcut = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_C), this);
    connect (copy_shortcut , &QShortcut::activated, this, &FPwin::copyText);
    QShortcut* paste_shortcut = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_V), this);
    connect (paste_shortcut , &QShortcut::activated, this, &FPwin::pasteText);
    QShortcut* del_shortcut = new QShortcut(QKeySequence(Qt::Key_Delete), this);
    connect (del_shortcut , &QShortcut::activated, this, &FPwin::deleteText);
    QShortcut* sel_all_shortcut = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_A), this);
    connect (sel_all_shortcut , &QShortcut::activated, this, &FPwin::selectAllText);
    QShortcut* undo_shortcut = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_Z), this);
    connect (undo_shortcut , &QShortcut::activated, this, &FPwin::undoing);
    QShortcut* redo_shortcut = new QShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_Z), this);
    connect (redo_shortcut , &QShortcut::activated, this, &FPwin::redoing);
    connect (ui->tabWidget, &QTabWidget::currentChanged, this, &FPwin::onTabChanged);
    connect (ui->tabWidget, &TabWidget::currentTabChanged, this, &FPwin::tabSwitch);
    ui->tabWidget->tabBar()->setContextMenuPolicy (Qt::CustomContextMenu);
    QShortcut* close_other_shortcut = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q), this);
    connect (close_other_shortcut , &QShortcut::activated, this, &FPwin::closeOtherTabs);
    connect (ui->actionFont, &QAction::triggered, this, &FPwin::fontDialog);
    QShortcut* find_shortcut = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_F), this);
    connect (find_shortcut , &QShortcut::activated, this, &FPwin::showHideSearch);
    QShortcut* jump_shortcut = new QShortcut(QKeySequence(Qt::ALT + Qt::Key_1), this);
    connect (jump_shortcut , &QShortcut::activated, this, &FPwin::jumpTo);
    connect (ui->spinBox, &QAbstractSpinBox::editingFinished, this, &FPwin::goTo);
    connect (ui->actionWrap, &QAction::triggered, this, &FPwin::toggleWrapping);
    connect (ui->actionIndent, &QAction::triggered, this, &FPwin::toggleIndent);
    connect (ui->actionPreferences, &QAction::triggered, this, &FPwin::prefDialog);
    QShortcut* replace_shortcut = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_H), this);
    connect (replace_shortcut , &QShortcut::activated, this, &FPwin::replaceDock);
    connect (ui->toolButtonNext, &QAbstractButton::clicked, this, &FPwin::replace);
    connect (ui->toolButtonPrv, &QAbstractButton::clicked, this, &FPwin::replace);
    connect (ui->toolButtonAll, &QAbstractButton::clicked, this, &FPwin::replaceAll);
    connect (ui->dockReplace, &QDockWidget::visibilityChanged, this, &FPwin::closeReplaceDock);
    connect (ui->dockReplace, &QDockWidget::topLevelChanged, this, &FPwin::resizeDock);
    connect (this, &FPwin::finishedLoading, []{});
    ui->toolButtonNext->setShortcut (QKeySequence (Qt::Key_F8));
    ui->toolButtonPrv->setShortcut (QKeySequence (Qt::Key_F9));
    ui->toolButtonAll->setShortcut (QKeySequence (Qt::Key_F10));
    QShortcut *fullscreen = new QShortcut (QKeySequence (Qt::Key_F11), this);
    QShortcut *defaultsize = new QShortcut (QKeySequence (Qt::CTRL + Qt::SHIFT + Qt::Key_W), this);
    connect (fullscreen, &QShortcut::activated,
    	[this] {setWindowState (windowState() ^ Qt::WindowFullScreen);});
    connect (defaultsize, &QShortcut::activated, this, &FPwin::defaultSize);
    QShortcut *focus_view_hard = new QShortcut (QKeySequence (Qt::Key_Escape), this);
    connect (focus_view_hard, &QShortcut::activated, this, &FPwin::focus_view_hard);    
    QShortcut *focus_view_soft = (
    	new QShortcut( QKeySequence( Qt::ALT | Qt::Key_2 ) , this )
    );
    connect( focus_view_soft , &QShortcut::activated , this, &FPwin::focus_view_soft );
    dummyWidget = new QWidget();
    setAcceptDrops (true);
    setAttribute (Qt::WA_AlwaysShowToolTips);
    setAttribute (Qt::WA_DeleteOnClose, false);    
}
FPwin::~FPwin()
{
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
        lastWinFilesCur_.clear();
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

    ui->menuBar->setVisible (true);
    ui->menuBar->actions().at( 1 )->setVisible( false );
    ui->menuBar->actions().at( 3 )->setVisible( false );
    ui->actionWrap->setChecked( false );
    ui->tabWidget->setTabPosition(QTabWidget::North);
    ui->actionSave->setEnabled (config.getSaveUnmodified());
    QIcon icn = QIcon::fromTheme ("featherpad");
    if (icn.isNull())
        icn = QIcon (":icons/featherpad.svg");
    setWindowIcon (icn);

    if (!config.hasReservedShortcuts())
    {
        QStringList reserved;
        reserved << QKeySequence (Qt::CTRL + Qt::SHIFT + Qt::Key_Z).toString() << QKeySequence (Qt::CTRL + Qt::Key_Z).toString() << QKeySequence (Qt::CTRL + Qt::Key_X).toString() << QKeySequence (Qt::CTRL + Qt::Key_C).toString() << QKeySequence (Qt::CTRL + Qt::Key_V).toString() << QKeySequence (Qt::CTRL + Qt::Key_A).toString()
                 << QKeySequence (Qt::SHIFT + Qt::Key_Insert).toString() << QKeySequence (Qt::SHIFT + Qt::Key_Delete).toString() << QKeySequence (Qt::CTRL + Qt::Key_Insert).toString()
                 << QKeySequence (Qt::CTRL + Qt::Key_Left).toString() << QKeySequence (Qt::CTRL + Qt::Key_Right).toString() << QKeySequence (Qt::CTRL + Qt::Key_Up).toString() << QKeySequence (Qt::CTRL + Qt::Key_Down).toString()
                 << QKeySequence (Qt::CTRL + Qt::Key_Home).toString() << QKeySequence (Qt::CTRL + Qt::Key_End).toString()
                 << QKeySequence (Qt::CTRL + Qt::SHIFT + Qt::Key_Up).toString() << QKeySequence (Qt::CTRL + Qt::SHIFT + Qt::Key_Down).toString()
                 << QKeySequence (Qt::META + Qt::Key_Up).toString() << QKeySequence (Qt::META + Qt::Key_Down).toString() << QKeySequence (Qt::META + Qt::SHIFT + Qt::Key_Up).toString() << QKeySequence (Qt::META + Qt::SHIFT + Qt::Key_Down).toString()

                 << QKeySequence (Qt::Key_F3).toString() << QKeySequence (Qt::Key_F4).toString() << QKeySequence (Qt::Key_F5).toString() << QKeySequence (Qt::Key_F6).toString() << QKeySequence (Qt::Key_F7).toString()
                 << QKeySequence (Qt::Key_F8).toString() << QKeySequence (Qt::Key_F9).toString() << QKeySequence (Qt::Key_F10).toString()
                 << QKeySequence (Qt::Key_F11).toString() << QKeySequence (Qt::CTRL + Qt::SHIFT + Qt::Key_W).toString()

                 << QKeySequence (Qt::CTRL + Qt::ALT  +Qt::Key_E).toString()
                 << QKeySequence (Qt::SHIFT + Qt::Key_Enter).toString() << QKeySequence (Qt::SHIFT + Qt::Key_Return).toString() << QKeySequence (Qt::CTRL + Qt::Key_Tab).toString() << QKeySequence (Qt::CTRL + Qt::META + Qt::Key_Tab).toString()
                 << QKeySequence (Qt::CTRL + Qt::SHIFT + Qt::Key_J).toString() // select text on jumping (not an action)
                 << QKeySequence (Qt::CTRL + Qt::Key_K).toString();
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
void FPwin::deleteTabPage (int tabIndex, bool saveToList)
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
        if (saveToList && QFile::exists (fileName))
            lastWinFilesCur_.insert (fileName, textEdit->textCursor().position());
    }
    ui->tabWidget->removeTab (tabIndex);
    delete tabPage; tabPage = nullptr;
}
bool FPwin::closeTabs (int first, int last, bool saveFilesList)
{
    if (!isReady()) return true;
    TabPage *curPage = nullptr;
        int cur = ui->tabWidget->currentIndex();
        if (!(first < cur && (cur < last || last == -1)))
            curPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget());
    bool keep = false;
    int index, count;
    DOCSTATE state = SAVED;
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
        if (first == index - 1)
            state = savePrompt (tabIndex, false);
        else
            state = savePrompt (tabIndex, true);
        switch (state) {
        case SAVED:
            keep = false;
            if (lastWinFilesCur_.size() >= 50)
                saveFilesList = false;
            deleteTabPage (tabIndex, saveFilesList);

            if (last > -1)
                --last;
            count = ui->tabWidget->count();
            if (count == 0)
            {
                ui->actionReload->setDisabled (true);
                ui->actionSave->setDisabled (true);
                enableWidgets (false);
            }
            break;
        case UNDECIDED:
            keep = true;
            lastWinFilesCur_.clear();
            break;
        case DISCARDED:
            keep = false;
            while (index > first)
            {
                if (last == 0) break;
                if (lastWinFilesCur_.size() >= 50)
                    saveFilesList = false;
                deleteTabPage (tabIndex, saveFilesList);

                if (last < 0)
                    index = ui->tabWidget->count() - 1;
                else
                {
                    --last;
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
    return keep;
}
void FPwin::closeOtherTabs()
{
    int cur = ui->tabWidget->currentIndex();
    closeTabs( cur , -1 );
    closeTabs( -1 , cur );
}
void FPwin::dragEnterEvent (QDragEnterEvent *event)
{
    if (findChildren<QDialog *>().count() > 0)
        return;
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
    else if (event->mimeData()->hasFormat ("application/featherpad-tab")
             && event->source() != nullptr)
    {
        event->acceptProposedAction();
    }
}
void FPwin::dropEvent (QDropEvent *event)
{
    if (event->mimeData()->hasFormat ("application/featherpad-tab"))
        dropTab (QString::fromUtf8 (event->mimeData()->data ("application/featherpad-tab").constData()));
    else
    {
        const QList<QUrl> urlList = event->mimeData()->urls();
        bool multiple (urlList.count() > 1 || isLoading());
        for (const QUrl &url : urlList)
            newTabFromName (url.adjusted (QUrl::NormalizePathSegments)
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
        switch (msgBox.exec()) {
        case QMessageBox::Save:
            if (!saveFile ())
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
    ui->actionSaveAs->setEnabled (enable);
    ui->actionSaveAllFiles->setEnabled (enable);
    ui->menuEncoding->setEnabled (enable);
    ui->actionFont->setEnabled (enable);
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
    {
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
void FPwin::updateShortcuts (bool disable, bool page)
{
    if (disable)
    {
        ui->toolButtonNext->setShortcut (QKeySequence());
        ui->toolButtonPrv->setShortcut (QKeySequence());
        ui->toolButtonAll->setShortcut (QKeySequence());
    }
    else
    {
        ui->toolButtonNext->setShortcut (QKeySequence (Qt::Key_F8));
        ui->toolButtonPrv->setShortcut (QKeySequence (Qt::Key_F9));
        ui->toolButtonAll->setShortcut (QKeySequence (Qt::Key_F10));
    }
    updateCustomizableShortcuts (disable);
    if (page)
    {
        if (TabPage *tabPage = qobject_cast< TabPage *>(ui->tabWidget->currentWidget()))
            tabPage->updateShortcuts (disable);
    }
}
void FPwin::newTab()
{
    createEmptyTab (!isLoading());
}
TabPage* FPwin::createEmptyTab (bool setCurrent)
{
    FPsingleton *singleton = static_cast<FPsingleton*>(qApp);
    Config config = singleton->getConfig();

    static const QList<QKeySequence> searchShortcuts = {QKeySequence (Qt::Key_F3), QKeySequence (Qt::Key_F4), QKeySequence (Qt::Key_F5), QKeySequence (Qt::Key_F6), QKeySequence (Qt::Key_F7)};
    TabPage *tabPage = new TabPage (config.getLightBgColorValue(),
                                    searchShortcuts,
                                    nullptr);
    TextEdit *textEdit = tabPage->textEdit();
    connect (textEdit, &QWidget::customContextMenuRequested, this, &FPwin::editorContextMenu);
    textEdit->setTtextTab (config.getTextTabSize());
    textEdit->setEditorFont (config.getFont());
    int index = ui->tabWidget->currentIndex();
    if (index == -1) enableWidgets (true);
    tabPage->setSearchBarVisible (false);
    ui->tabWidget->insertTab (index + 1, tabPage, noname);
    ui->tabWidget->setTabToolTip (index + 1, tr ("Unsaved"));
    if (!ui->actionWrap->isChecked())
        textEdit->setLineWrapMode (QPlainTextEdit::NoWrap);
    if (!ui->actionIndent->isChecked())
        textEdit->setAutoIndentation (false);
    if (ui->spinBox->isVisible())
        connect (textEdit->document(), &QTextDocument::blockCountChanged, this, &FPwin::setMax);
    if (!config.getSaveUnmodified())
        connect (textEdit->document(), &QTextDocument::modificationChanged, this, &FPwin::enableSaving);
    connect (textEdit->document(), &QTextDocument::modificationChanged, this, &FPwin::asterisk);
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
            QString txt = thisAction->text();
            if (!txt.isEmpty())
                txt = txt.split ('\t').first();
            if (!txt.isEmpty())
                thisAction->setText(txt);
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
                if (!QProcess::startDetached ("gio", QStringList() << "open" << url.toString()))
                    QDesktopServices::openUrl (url);
            });
            if (str.startsWith ("mailto:"))
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
void FPwin::defaultSize()
{
    QSize s = static_cast<FPsingleton*>(qApp)->getConfig().getStartSize();
    if (size() == s) return;
    if (isMaximized() || isFullScreen())
        showNormal();
    resize (s);
}
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
        	
        	int count = ui->tabWidget->count();
    		for (int indx = 0; indx < count; ++indx)
    		{
        		TabPage *page = qobject_cast< TabPage *>(ui->tabWidget->widget (indx));
			TextEdit *textEdit = page->textEdit();
			textEdit->setSearchedText (QString());
			QList<QTextEdit::ExtraSelection> es;
			textEdit->setGreenSel (es);
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
        	
		tabPage->textEdit()->setFocus();
        	
        }
    }
};
void FPwin::closeTab()
{
    if (!isReady()) return;
    QListWidgetItem *curItem = nullptr;
    int index = -1;
    index = ui->tabWidget->currentIndex();
    if (index == -1)
    {
        return;
    }
    if (savePrompt (index, false) != SAVED)
    {
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
        if (curItem)
        if (TabPage *tabPage = qobject_cast< TabPage *>(ui->tabWidget->currentWidget()))
            tabPage->textEdit()->setFocus();
    }
}
void FPwin::closeTabAtIndex (int index)
{
    TabPage *curPage = nullptr;
    if (index != ui->tabWidget->currentIndex())
        curPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget());
    if (savePrompt (index, false) != SAVED)
    {
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
        if (curPage)
            ui->tabWidget->setCurrentWidget (curPage);

        if (TabPage *tabPage = qobject_cast< TabPage *>(ui->tabWidget->currentWidget()))
            tabPage->textEdit()->setFocus();
    }
}
void FPwin::setTitle (const QString& fileName, int tabIndex)
{
    int index = tabIndex;
    if (index < 0)
        index = ui->tabWidget->currentIndex();
    QString shownName;
    if (fileName.isEmpty())
    {
        shownName = noname;
        if (tabIndex < 0)
            setWindowTitle (shownName);
    }
    else
    {
        QFileInfo fInfo (fileName);
        if (tabIndex < 0)
            setWindowTitle (title_prefix + (fileName.contains ("/") ? fileName
                                                    : fInfo.absolutePath() + "/" + fileName));
        shownName = fileName.section ('/', -1);
        shownName.replace ("\n", " ");
    }
    shownName.replace ("&", "&&");
    shownName.replace ('\t', ' ');
    ui->tabWidget->setTabText (index, shownName);
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
        shownName = noname;
        setWindowTitle ((modified ? modified_prefix : QString()) + title_prefix + shownName);
    }
    else
    {
        shownName = fname.section ('/', -1);
        setWindowTitle ((modified ? modified_prefix : QString())
                        + title_prefix + (fname.contains ("/") ? fname
                                                : QFileInfo (fname).absolutePath() + "/" + fname));
    }
    shownName.replace ("\n", " ");
    if (modified)
        shownName.prepend ( modified_prefix );
    shownName.replace ("&", "&&");
    shownName.replace ('\t', ' ');
    ui->tabWidget->setTabText (index, shownName);
}
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
    connect (thread, &Loading::completed, this, &FPwin::addText);
    connect (thread, &Loading::finished, thread, &QObject::deleteLater);
    thread->start();

    waitToMakeBusy();
    updateShortcuts (true, false);
}
void FPwin::addText (const QString& text, const QString& fileName, const QString& charset,
                     bool enforceEncod, bool reload,
                     int restoreCursor, int posInLine,
                     bool uneditable,
                     bool multiple)
{
    if (fileName.isEmpty() || charset.isEmpty())
    {
        if (!fileName.isEmpty() && charset.isEmpty())
            connect (this, &FPwin::finishedLoading, this, &FPwin::onOpeningHugeFiles, Qt::UniqueConnection);
        else if (fileName.isEmpty() && !charset.isEmpty())
            connect (this, &FPwin::finishedLoading, this, &FPwin::onOpeninNonTextFiles, Qt::UniqueConnection);
        else
            connect (this, &FPwin::finishedLoading, this, &FPwin::onPermissionDenied, Qt::UniqueConnection);
        -- loadingProcesses_;
        if (!isLoading())
        {
            updateShortcuts (false, false);
            closeWarningBar();
            emit finishedLoading();
            QTimer::singleShot (0, this, [this]() {unbusy();});
        }
        return;
    }

    if (enforceEncod || reload)
        multiple = false;

    static bool scrollToFirstItem (false);
    static TabPage *firstPage = nullptr;
    TextEdit *textEdit;
    TabPage *tabPage = nullptr;
    if (ui->tabWidget->currentIndex() == -1)
        tabPage = createEmptyTab (!multiple);
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
        tabPage = createEmptyTab (!multiple);
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
    QFileInfo fInfo (fileName);
    if (scrollToFirstItem
        && (!firstPage
            || firstPage->textEdit()->getFileName().section ('/', -1).
               compare (fInfo.fileName(), Qt::CaseInsensitive) > 0))
    {
        firstPage = tabPage;
    }
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
    inactiveTabModified_ = true;
    textEdit->setPlainText (text);
    inactiveTabModified_ = false;
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
        if (restoreCursor == 1 || restoreCursor == -1)
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
                    textEdit->setTextCursor (cur);
                });
            }
        }
        else if (restoreCursor < -1)
        {
            QTextCursor cur = textEdit->textCursor();
            cur.movePosition (QTextCursor::End);
            QTimer::singleShot (0, textEdit, [textEdit, cur]() {
                textEdit->setTextCursor (cur);
            });
        }
        else
        {
            restoreCursor -= 2;
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
    textEdit->setEncoding (charset);
    textEdit->setWordNumber (-1);
    if (uneditable)
    {
        connect (this, &FPwin::finishedLoading, this, &FPwin::onOpeningUneditable, Qt::UniqueConnection);
        textEdit->makeUneditable (uneditable);
    }
    setTitle (fileName, (multiple && !openInCurrentTab) ?
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
    
    if (uneditable)
    {
        textEdit->setReadOnly (true);
      if (uneditable)
            textEdit->viewport()->setStyleSheet (".QWidget {"
                                          	"color: black;"
                                                "background-color: rgb(225, 238, 255);}");
       if (!multiple || openInCurrentTab)
        {
            ui->actionSaveAs->setDisabled (true);
            if (config.getSaveUnmodified())
                ui->actionSave->setDisabled (true);
        }
    }
    if (!multiple || openInCurrentTab)
    {
        if (!fInfo.exists())
            connect (this, &FPwin::finishedLoading, this, &FPwin::onOpeningNonexistent, Qt::UniqueConnection);
        encodingToCheck (charset);
        ui->actionReload->setEnabled (true);
        textEdit->setFocus();
    }

    -- loadingProcesses_;
    if (!isLoading())
    {
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
void FPwin::onOpeninNonTextFiles()
{
    disconnect (this, &FPwin::finishedLoading, this, &FPwin::onOpeninNonTextFiles);
    QTimer::singleShot (0, this, [=]() {
        showWarningBar ("<center><b><big>" + tr ("Non-text file(s) not opened!") + "</big></b></center>\n"
                        + "<center><i>" + tr ("See Preferences → Files → Do not permit opening of non-text files") + "</i></center>");
    });
}
void FPwin::onPermissionDenied()
{
    disconnect (this, &FPwin::finishedLoading, this, &FPwin::onPermissionDenied);
    QTimer::singleShot (0, this, [=]() {
        showWarningBar ("<center><b><big>" + tr ("Some file(s) could not be opened!") + "</big></b></center>\n"
                        + "<center>" + tr ("You may not have the permission to read.") + "</center>");
    });
}
void FPwin::onOpeningUneditable()
{
    disconnect (this, &FPwin::finishedLoading, this, &FPwin::onOpeningUneditable);
    QTimer::singleShot (0, this, [=]() {
        showWarningBar ("<center><b><big>" + tr ("Uneditable file(s)!") + "</big></b></center>\n"
                        + "<center>" + tr ("Non-text files or files with huge lines cannot be edited.") + "</center>");
    });
}
void FPwin::onOpeningNonexistent()
{
    disconnect (this, &FPwin::finishedLoading, this, &FPwin::onOpeningNonexistent);
    QTimer::singleShot (0, this, [=]() {
        if (TabPage *tabPage = qobject_cast< TabPage *>(ui->tabWidget->currentWidget()))
        {
            QString fname = tabPage->textEdit()->getFileName();
            if (!fname.isEmpty() && !QFile::exists (fname))
                showWarningBar ("<center><b><big>" + tr ("The file does not exist.") + "</big></b></center>");
        }
    });
}
void FPwin::showWarningBar (const QString& message, bool startupBar)
{
    QList<QDialog*> dialogs = findChildren<QDialog*>();
    for (int i = 0; i < dialogs.count(); ++i)
    {
        if (dialogs.at (i)->isModal())
            return;
    }
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
void FPwin::fileOpen()
{
    if (isLoading()) return;

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
        filter = tr ("All Files (*);;.%1 Files (*.%1)").arg (fname.section ('.', -1, -1));
    }
    FileDialog dialog (this);
    dialog.setAcceptMode (QFileDialog::AcceptOpen);
    dialog.setWindowTitle (title_prefix + QString("Open file..."));
    dialog.setFileMode (QFileDialog::ExistingFiles);
    dialog.setNameFilter (filter);
    if (QFileInfo (path).isDir())
        dialog.setDirectory (path);
    else
    {
        dialog.setDirectory (path.section ("/", 0, -2));
        dialog.selectFile (path);
    }
    if (dialog.exec())
    {
        const QStringList files = dialog.selectedFiles();
        bool multiple (files.count() > 1 || isLoading());
        for (const QString &file : files)
        {
        	int exists_tab_idx = already_opened_idx( file );
        	if( exists_tab_idx != -2 )
        	{
        		ui->tabWidget->setCurrentIndex(  exists_tab_idx  );
        	}
        	
        	else
        	{
        		newTabFromName (file, 0, 0, multiple);
        	}
        }
    }
    updateShortcuts (false);
}
int FPwin::already_opened_idx (const QString& fileName) const
{
    int res = -2;

    QFileInfo info (fileName);
    QString target = info.isSymLink() ? info.symLinkTarget()
                                      : fileName;
    FPsingleton *singleton = static_cast<FPsingleton*>(qApp);
    for (int i = 0; i < singleton->Wins.count(); ++i)
    {
        FPwin *thisOne = singleton->Wins.at (i);
        for (int j = 0; j < thisOne->ui->tabWidget->count(); ++j)
        {
            TabPage *thisTabPage = qobject_cast<TabPage*>(thisOne->ui->tabWidget->widget (j));
            TextEdit *thisTextEdit = thisTabPage->textEdit();
            if (thisTextEdit->isReadOnly())
                continue;
            QFileInfo thisInfo (thisTextEdit->getFileName());
            QString thisTarget = thisInfo.isSymLink() ? thisInfo.symLinkTarget()
                                                      : thisTextEdit->getFileName();
            if (thisTarget == target)
            {
                res = j;
                break;
            }
        }
        if (res!=-2) break;
    }
    return res;
}
void FPwin::enforceEncoding (QAction*)
{
    int index = ui->tabWidget->currentIndex();
    TabPage *tabPage = qobject_cast< TabPage *>(ui->tabWidget->widget (index));
    if (tabPage == nullptr) return;

    TextEdit *textEdit = tabPage->textEdit();
    QString fname = textEdit->getFileName();
    if (!fname.isEmpty())
    {
        if (savePrompt (index, false) != SAVED)
        {
            encodingToCheck (textEdit->getEncoding());
            return;
        }
        if (!QFile::exists (fname))
            deleteTabPage (index, false);
        loadText (fname, true, true,
                  0, 0,
                  textEdit->isUneditable(), false);
    }
    else
    {
        textEdit->setEncoding (checkToEncoding());
    }
}
void FPwin::reload()
{
    if (isLoading()) return;

    int index = ui->tabWidget->currentIndex();
    TabPage *tabPage = qobject_cast< TabPage *>(ui->tabWidget->widget (index));
    if (tabPage == nullptr) return;

    if (savePrompt (index, false) != SAVED) return;

    TextEdit *textEdit = tabPage->textEdit();
    QString fname = textEdit->getFileName();
    if (!QFile::exists (fname))
        deleteTabPage (index, false);
    if (!fname.isEmpty())
    {
        loadText (fname, false, true,
                  textEdit->getSaveCursor() ? 1 : 0);
    }
}
// This is for both "Save" and "Save As"
bool FPwin::saveFile ()
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
        filter = tr (".%1 Files (*.%1);;All Files (*)").arg (fname.section ('.', -1, -1));
    }
    if (fname.isEmpty()
        || !QFile::exists (fname)
        || textEdit->getFileName().isEmpty())
    {
        bool restorable = false;
        if (fname.isEmpty())
        {
            QDir dir = QDir::home();
            fname = dir.filePath (noname);
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
            else if (!textEdit->getFileName().isEmpty())
                restorable = true;
            if (!textEdit->getFileName().isEmpty())
                fname = dir.filePath (QFileInfo (fname).fileName());
            else
                fname = dir.filePath (noname);
        }
        else
            fname = QFileInfo (fname).absoluteDir().filePath (noname);
        if (!restorable
            && QObject::sender() != ui->actionSaveAs
        )
        {
            if (hasAnotherDialog()) return false;
            updateShortcuts (true);
            FileDialog dialog (this);
            dialog.setAcceptMode (QFileDialog::AcceptSave);
            dialog.setWindowTitle (title_prefix + QString("Save as..."));
            dialog.setFileMode (QFileDialog::AnyFile);
            dialog.setNameFilter (filter);
            dialog.setDirectory (fname.section ("/", 0, -2));
            dialog.selectFile (fname);
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
        FileDialog dialog (this);
        dialog.setAcceptMode (QFileDialog::AcceptSave);
        dialog.setWindowTitle (title_prefix + QString("Save as..."));
        dialog.setFileMode (QFileDialog::AnyFile);
        dialog.setNameFilter (filter);
        dialog.setDirectory (fname.section ("/", 0, -2));
        dialog.selectFile (fname);
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
    }
    else
    {
        QString str = writer.device()->errorString();
        showWarningBar ("<center><b><big>" + tr ("Cannot be saved!") + "</big></b></center>\n"
                        + "<center><i>" + QString ("<center><i>%1.</i></center>").arg (str) + "<i/></center>");
    }
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
        setWindowTitle (program_name);
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
       shownName = noname;
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
        shownName.prepend (modified_prefix);
    setWindowTitle (title_prefix + shownName);
    encodingToCheck (textEdit->getEncoding());
    Config config = static_cast<FPsingleton*>(qApp)->getConfig();
    bool readOnly = textEdit->isReadOnly();
    if (!config.getSaveUnmodified())
        ui->actionSave->setEnabled (modified);
    else
        ui->actionSave->setDisabled (readOnly || textEdit->isUneditable());
    ui->actionReload->setEnabled (!fname.isEmpty());
    if (fname.isEmpty()
        && !modified
        && !textEdit->document()->isEmpty())
    {
        ui->actionSaveAs->setEnabled (true);
    }
    else
    {
        ui->actionSaveAs->setEnabled (!textEdit->isUneditable());
    }
    if (ui->spinBox->isVisible())
        ui->spinBox->setMaximum (textEdit->document()->blockCount());
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
        
        textEdit->adjustScrollbars();
    }
    updateShortcuts (false);
}
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
        if (!visibility)
        {
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
void FPwin::setMax (const int max)
{
    ui->spinBox->setMaximum (max);
}
void FPwin::goTo()
{
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
void FPwin::toggleWrapping()
{
    int count = ui->tabWidget->count();
    if (count == 0) return;

    bool wrapLines = ui->actionWrap->isChecked();
    for (int i = 0; i < count; ++i)
        qobject_cast<TabPage*>(ui->tabWidget->widget (i))->textEdit()->setLineWrapMode (wrapLines ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
}
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
void FPwin::encodingToCheck (const QString& encoding)
{
    if (encoding == "UTF-8")
        ui->actionUTF_8->setChecked (true);
    else if (encoding == "UTF-16")
        ui->actionUTF_16->setChecked (true);
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
}
const QString FPwin::checkToEncoding() const
{
    QString encoding;

    if (ui->actionUTF_8->isChecked())
        encoding = "UTF-8";
    else if (ui->actionUTF_16->isChecked())
        encoding = "UTF-16";
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
    else
        encoding = "UTF-8";

    return encoding;
}
void FPwin::nextTab()
{
    if (isLoading()) return;

    int index = ui->tabWidget->currentIndex();
    if (index == -1) return;
        if (QWidget *widget = ui->tabWidget->widget (index + 1))
            ui->tabWidget->setCurrentWidget (widget);
        else
            ui->tabWidget->setCurrentIndex (0);
}
void FPwin::previousTab()
{
    if (isLoading()) return;

    int index = ui->tabWidget->currentIndex();
    if (index == -1) return;
        if (QWidget *widget = ui->tabWidget->widget (index - 1))
            ui->tabWidget->setCurrentWidget (widget);
        else
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
    if (index <= -1)
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
        || dragSource == nullptr)
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
    bool ln = true;
    if (dragSource->ui->spinBox->isVisible())
        spin = true;

    TabPage *tabPage = qobject_cast< TabPage *>(dragSource->ui->tabWidget->widget (index));
    if (tabPage == nullptr)
    {
        ui->tabWidget->tabBar()->finishMouseMoveEvent();
        return;
    }
    TextEdit *textEdit = tabPage->textEdit();
    disconnect (textEdit, &QWidget::customContextMenuRequested, dragSource, &FPwin::editorContextMenu);
    disconnect (textEdit, &TextEdit::fileDropped, dragSource, &FPwin::newTabFromName);
    disconnect (textEdit->document(), &QTextDocument::blockCountChanged, dragSource, &FPwin::setMax);
    disconnect (textEdit->document(), &QTextDocument::modificationChanged, dragSource, &FPwin::asterisk);
    if (!config.getSaveUnmodified())
    {
        disconnect (textEdit->document(), &QTextDocument::modificationChanged, dragSource, &FPwin::enableSaving);
    }
    disconnect (tabPage, &TabPage::find, dragSource, &FPwin::find);
    disconnect (tabPage, &TabPage::searchFlagChanged, dragSource, &FPwin::searchFlagChanged);
    dragSource->ui->tabWidget->tabBar()->releaseMouse();
    dragSource->ui->tabWidget->removeTab (index);
    int count = dragSource->ui->tabWidget->count();
    int insertIndex = ui->tabWidget->currentIndex() + 1;
    lastFile_ = textEdit->getFileName();
    textEdit->setGreenSel (QList<QTextEdit::ExtraSelection>());
    textEdit->setRedSel (QList<QTextEdit::ExtraSelection>());
    if (!textEdit->getSearchedText().isEmpty())
    {
        if (insertIndex == 0
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
    if (ui->tabWidget->count() == 0)
        enableWidgets (true);
    ui->tabWidget->setCurrentIndex (insertIndex);
    QList<QTextEdit::ExtraSelection> es;
    if ((ln || spin)
        && (ui->spinBox->isVisible()))
    {
        es.prepend (textEdit->currentLineSelection());
    }
    textEdit->setExtraSelections (es);
    ui->tabWidget->setTabToolTip (insertIndex, tooltip);
    if (ui->spinBox->isVisible())
        connect (textEdit->document(), &QTextDocument::blockCountChanged, this, &FPwin::setMax);
    if (ui->actionWrap->isChecked() && textEdit->lineWrapMode() == QPlainTextEdit::NoWrap)
        textEdit->setLineWrapMode (QPlainTextEdit::WidgetWidth);
    else if (!ui->actionWrap->isChecked() && textEdit->lineWrapMode() == QPlainTextEdit::WidgetWidth)
        textEdit->setLineWrapMode (QPlainTextEdit::NoWrap);
    if (ui->actionIndent->isChecked() && textEdit->getAutoIndentation() == false)
        textEdit->setAutoIndentation (true);
    else if (!ui->actionIndent->isChecked() && textEdit->getAutoIndentation() == true)
        textEdit->setAutoIndentation (false);
    if (!config.getSaveUnmodified())
        connect (textEdit->document(), &QTextDocument::modificationChanged, this, &FPwin::enableSaving);
    connect (textEdit->document(), &QTextDocument::modificationChanged, this, &FPwin::asterisk);
    connect (tabPage, &TabPage::find, this, &FPwin::find);
    connect (tabPage, &TabPage::searchFlagChanged, this, &FPwin::searchFlagChanged);
    connect (textEdit, &TextEdit::fileDropped, this, &FPwin::newTabFromName);
    connect (textEdit, &QWidget::customContextMenuRequested, this, &FPwin::editorContextMenu);
    textEdit->setFocus();
    stealFocus();
    if (count == 0)
        QTimer::singleShot (0, dragSource, &QWidget::close);
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
void FPwin::saveAllFiles (bool showWarning)
{
    int index = ui->tabWidget->currentIndex();
    if (index == -1) return;
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
        QTextDocumentWriter writer (fname, "plaintext");
        if (writer.write (thisTextEdit->document()))
        {
            inactiveTabModified_ = (indx != index);
            thisTextEdit->document()->setModified (false);
            QFileInfo fInfo (fname);
            thisTextEdit->setSize (fInfo.size());
            thisTextEdit->setLastModified (fInfo.lastModified());
            setTitle (fname, (!inactiveTabModified_ ? -1 : indx));
            inactiveTabModified_ = false;
        }
        else error = true;
    }
    if (showWarning && error)
        showWarningBar ("<center><b><big>" + tr ("Some files cannot be saved!") + "</big></b></center>");
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
