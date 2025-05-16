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

#include "singleton.h"
#include "ui_fp.h"

#include "pref.h"
#include "ui_prefDialog.h"
#include "filedialog.h"

#include <QScreen>
#include <QWindow>
#include <QWhatsThis>
#include <QFileInfo>
#include <QFileDialog>
#include <QColorDialog>

namespace FeatherPad {

static QHash<QString, QString> OBJECT_NAMES;
static QHash<QString, QString> DEFAULT_SHORTCUTS;
FPKeySequenceEdit::FPKeySequenceEdit (QWidget *parent) : QKeySequenceEdit (parent) {}

void FPKeySequenceEdit::keyPressEvent (QKeyEvent *event)
{
    clear();
    int k = event->key();
    if ((k < Qt::Key_F1 || k > Qt::Key_F35)
        && (event->modifiers() == Qt::NoModifier || event->modifiers() == Qt::KeypadModifier))
    {
        return;
    }
    QKeySequenceEdit::keyPressEvent (event);
}
Delegate::Delegate (QObject *parent) : QStyledItemDelegate (parent) {}

QWidget* Delegate::createEditor (QWidget *parent,
                                 const QStyleOptionViewItem& /*option*/,
                                 const QModelIndex& /*index*/) const
{
    return new FPKeySequenceEdit (parent);
}
bool Delegate::eventFilter (QObject *object, QEvent *event)
{
    QWidget *editor = qobject_cast<QWidget*>(object);
    if (editor && event->type() == QEvent::KeyPress)
    {
        int k = static_cast<QKeyEvent *>(event)->key();
        if (k == Qt::Key_Return || k == Qt::Key_Enter)
        {
            emit QAbstractItemDelegate::commitData (editor);
            emit QAbstractItemDelegate::closeEditor (editor);
            return true;
        }
    }
    return QStyledItemDelegate::eventFilter (object, event);
}
PrefDialog::PrefDialog (QWidget *parent)
    : QDialog (parent), ui (new Ui::PrefDialog)
{
    ui->setupUi (this);
    parent_ = parent;
    setWindowModality (Qt::WindowModal);
    ui->promptLabel->setStyleSheet ("QLabel {background-color: #7d0000; color: white; border-radius: 3px; margin: 2px; padding: 5px;}");
    ui->promptLabel->hide();
    promptTimer_ = nullptr;

    Delegate *del = new Delegate (ui->tableWidget);
    ui->tableWidget->setItemDelegate (del);
    ui->tableWidget->horizontalHeader()->setSectionResizeMode (QHeaderView::Stretch);
    ui->tableWidget->horizontalHeader()->setSectionsClickable (true);
    ui->tableWidget->sortByColumn (0, Qt::AscendingOrder);
    ui->tableWidget->setToolTip (tr ("Press a modifier key to clear a shortcut\nin the editing mode."));
    Config config = static_cast<FPsingleton*>(qApp)->getConfig();
    textTabSize_ = config.getTextTabSize();
    saveUnmodified_ = config.getSaveUnmodified();
    ui->winSizeBox->setChecked (config.getRemSize());
    connect (ui->winSizeBox, &QCheckBox::stateChanged, this, &PrefDialog::prefSize);
    if (ui->winSizeBox->isChecked())
    {
        ui->spinX->setEnabled (false);
        ui->spinY->setEnabled (false);
        ui->mLabel->setEnabled (false);
        ui->sizeLable->setEnabled (false);
    }
    QSize ag;
    if (parent != nullptr)
    {
        if (QWindow *win = parent->windowHandle())
        {
            if (QScreen *sc = win->screen())
                ag = sc->availableGeometry().size();
        }
    }
    if (ag.isEmpty()) ag = QSize (qMax (700, config.getStartSize().width()), qMax (500, config.getStartSize().height()));
    ui->spinX->setMaximum (ag.width());
    ui->spinY->setMaximum (ag.height());
    ui->spinX->setValue (config.getStartSize().width());
    ui->spinY->setValue (config.getStartSize().height());
    connect (ui->spinX, QOverload<int>::of(&QSpinBox::valueChanged), this, &PrefDialog::prefStartSize);
    connect (ui->spinY, QOverload<int>::of(&QSpinBox::valueChanged), this, &PrefDialog::prefStartSize);
    ui->winPosBox->setChecked (config.getRemPos());
    connect (ui->winPosBox, &QCheckBox::stateChanged, this, &PrefDialog::prefPos);
    ui->textTabSpin->setValue (textTabSize_);
    connect (ui->textTabSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &PrefDialog::prefTextTabSize);
    ui->unmodifiedSaveBox->setChecked (saveUnmodified_);
    if (FPwin *win = static_cast<FPwin *>(parent_))
    {
        if (DEFAULT_SHORTCUTS.isEmpty())
        { // NOTE: Shortcut strings should be in the PortableText format.
            const auto defaultShortcuts = win->defaultShortcuts();
            QHash<QAction*, QKeySequence>::const_iterator iter = defaultShortcuts.constBegin();
            while (iter != defaultShortcuts.constEnd())
            {
                const QString name = iter.key()->objectName();
                DEFAULT_SHORTCUTS.insert (name, iter.value().toString());
                OBJECT_NAMES.insert (iter.key()->text().remove (QRegularExpression ("\\s*\\(&[a-zA-Z0-9]\\)\\s*")).remove ("&"), name);
                ++ iter;
            }
        }
    }

    QHash<QString, QString> ca = config.customShortcutActions();

    QList<QString> keys = ca.keys();
    QHash<QString, QString>::const_iterator iter = OBJECT_NAMES.constBegin();
    while (iter != OBJECT_NAMES.constEnd())
    {
        shortcuts_.insert (iter.key(),
                           keys.contains (iter.value()) ? ca.value (iter.value())
                                                        : DEFAULT_SHORTCUTS.value (iter.value()));
        ++ iter;
    }

    QList<QString> val = shortcuts_.values();
    for (int i = 0; i < val.size(); ++i)
    {
        if (!val.at (i).isEmpty() && val.indexOf (val.at (i), i + 1) > -1)
        {
            showPrompt (tr ("Warning: Ambiguous shortcut detected!"), false);
            break;
        }
    }

    ui->tableWidget->setRowCount (shortcuts_.size());
    ui->tableWidget->setSortingEnabled (false);
    int index = 0;
    QHash<QString, QString>::const_iterator it = shortcuts_.constBegin();
    while (it != shortcuts_.constEnd())
    {
        QTableWidgetItem *item = new QTableWidgetItem (it.key());
        item->setFlags (item->flags() & ~Qt::ItemIsEditable & ~Qt::ItemIsSelectable);
        ui->tableWidget->setItem (index, 0, item);
        ui->tableWidget->setItem (index, 1, new QTableWidgetItem (QKeySequence (it.value(), QKeySequence::PortableText)
                                                                  .toString (QKeySequence::NativeText)));
        ++ it;
        ++ index;
    }
    ui->tableWidget->setSortingEnabled (true);
    ui->tableWidget->setCurrentCell (0, 1);
    connect (ui->tableWidget, &QTableWidget::itemChanged, this, &PrefDialog::onShortcutChange);
    connect (ui->defaultButton, &QAbstractButton::clicked, this, &PrefDialog::restoreDefaultShortcuts);
    ui->defaultButton->setDisabled (ca.isEmpty());
    connect (ui->closeButton, &QAbstractButton::clicked, this, &QDialog::close);
    connect (ui->helpButton, &QAbstractButton::clicked, this, &PrefDialog::showWhatsThis);
    connect (this, &QDialog::rejected, this, &PrefDialog::onClosing);
    const auto widgets = findChildren<QWidget*>();
    for (QWidget *w : widgets)
    {
        QString tip = w->toolTip();
        if (!tip.isEmpty())
        {
            w->setWhatsThis (tip.replace ('\n', ' ').replace ("  ", "\n\n"));
            w->setToolTip ("<p style='white-space:pre'>" + w->toolTip() + "</p>");
        }
    }

    if (parent != nullptr)
        ag -= parent->window()->frameGeometry().size() - parent->window()->geometry().size();
    if (config.getPrefSize().isEmpty())
    {
        resize (QSize (sizeHint().width() + style()->pixelMetric (QStyle::PM_ScrollBarExtent), size().height())
                .boundedTo(ag));
    }
    else
        resize (config.getPrefSize().boundedTo(ag));
}
PrefDialog::~PrefDialog()
{
    if (promptTimer_)
    {
        promptTimer_->stop();
        delete promptTimer_;
    }
    delete ui; ui = nullptr;
}
void PrefDialog::closeEvent (QCloseEvent *event)
{
    onClosing();
    event->accept();
}
void PrefDialog::onClosing()
{
    prefShortcuts();
    prefTextTab();
    prefSaveUnmodified();

    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
    config.setPrefSize (size());
    config.writeConfig();
}
void PrefDialog::showPrompt (const QString& str, bool temporary)
{
    Config config = static_cast<FPsingleton*>(qApp)->getConfig();
    if (!str.isEmpty())
    {
        ui->promptLabel->setText ("<b>" + str + "</b>");
        if (temporary)
        {
            if (!promptTimer_)
            {
                promptTimer_ = new QTimer();
                promptTimer_->setSingleShot (true);
                connect (promptTimer_, &QTimer::timeout, [this] {
                    if (!prevtMsg_.isEmpty()
                        && ui->tabWidget->currentIndex() == 3)
                    {
                        ui->promptLabel->setText (prevtMsg_);
                    }
                    else showPrompt();
                });
            }
            promptTimer_->start (3300);
        }
        else
            prevtMsg_ = "<b>" + str + "</b>";
    }
    else
    {
        if (prevtMsg_.isEmpty())
        {
            ui->promptLabel->clear();
            ui->promptLabel->hide();
            return;
        }
        else
            ui->promptLabel->setText (prevtMsg_);
    }
    ui->promptLabel->show();
}
void PrefDialog::showWhatsThis()
{
    QWhatsThis::enterWhatsThisMode();
}
void PrefDialog::prefSize (int checked)
{
    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
    if (checked == Qt::Checked)
    {
        config.setRemSize (true);
        ui->spinX->setEnabled (false);
        ui->spinY->setEnabled (false);
        ui->mLabel->setEnabled (false);
        ui->sizeLable->setEnabled (false);
    }
    else if (checked == Qt::Unchecked)
    {
        config.setRemSize (false);
        ui->spinX->setEnabled (true);
        ui->spinY->setEnabled (true);
        ui->mLabel->setEnabled (true);
        ui->sizeLable->setEnabled (true);
    }
}
void PrefDialog::prefPos (int checked)
{
    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
    if (checked == Qt::Checked)
        config.setRemPos (true);
    else if (checked == Qt::Unchecked)
        config.setRemPos (false);
}
void PrefDialog::prefMaxSHSize (int value)
{
    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
    config.setMaxSHSize (value);
}
void PrefDialog::prefStartSize (int value)
{
    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
    QSize startSize = config.getStartSize();
    if (QObject::sender() == ui->spinX)
        startSize.setWidth (value);
    else if (QObject::sender() == ui->spinY)
        startSize.setHeight (value);
    config.setStartSize (startSize);
}
void PrefDialog::prefSplitterPos (int checked)
{
    FPsingleton *singleton = static_cast<FPsingleton*>(qApp);
    Config& config = singleton->getConfig();
    if (checked == Qt::Checked)
        config.setRemSplitterPos (true);
    else if (checked == Qt::Unchecked)
        config.setRemSplitterPos (false);
}
// NOTE: Custom shortcuts will be saved in the PortableText format.
void PrefDialog::onShortcutChange (QTableWidgetItem *item)
{
    Config config = static_cast<FPsingleton*>(qApp)->getConfig();
    QString desc = ui->tableWidget->item (ui->tableWidget->currentRow(), 0)->text();

    QString txt = item->text();
    if (!txt.isEmpty())
    {
        QKeySequence keySeq (txt);
        txt = keySeq.toString();
    }

    if (!txt.isEmpty() && config.reservedShortcuts().contains (txt)
        && DEFAULT_SHORTCUTS.value (OBJECT_NAMES.value (desc)) != txt)
    {
        showPrompt (tr ("The typed shortcut was reserved."), true);
        disconnect (ui->tableWidget, &QTableWidget::itemChanged, this, &PrefDialog::onShortcutChange);
        item->setText (shortcuts_.value (desc));
        connect (ui->tableWidget, &QTableWidget::itemChanged, this, &PrefDialog::onShortcutChange);
    }
    else
    {
        shortcuts_.insert (desc, txt);
        newShortcuts_.insert (OBJECT_NAMES.value (desc), txt);
        bool ambiguous = false;
        QList<QString> val = shortcuts_.values();
        for (int i = 0; i < val.size(); ++i)
        {
            if (!val.at (i).isEmpty() && val.indexOf (val.at (i), i + 1) > -1)
            {
                showPrompt (tr ("Warning: Ambiguous shortcut detected!"), false);
                ambiguous = true;
                break;
            }
        }
        if (!ambiguous && ui->promptLabel->isVisible())
        {
            prevtMsg_ = QString();
            showPrompt();
        }
        QHash<QString, QString>::const_iterator it = shortcuts_.constBegin();
        while (it != shortcuts_.constEnd())
        {
            if (DEFAULT_SHORTCUTS.value (OBJECT_NAMES.value (it.key())) != it.value())
            {
                ui->defaultButton->setEnabled (true);
                return;
            }
            ++it;
        }
        ui->defaultButton->setEnabled (false);
    }
}
void PrefDialog::restoreDefaultShortcuts()
{
    if (newShortcuts_.isEmpty()
        && static_cast<FPsingleton*>(qApp)->getConfig().customShortcutActions().isEmpty())
    {
        return;
    }

    disconnect (ui->tableWidget, &QTableWidget::itemChanged, this, &PrefDialog::onShortcutChange);
    int cur = ui->tableWidget->currentColumn() == 0
                  ? 0
                  : ui->tableWidget->currentRow();
    ui->tableWidget->setSortingEnabled (false);
    newShortcuts_ = DEFAULT_SHORTCUTS;
    int index = 0;
    QMutableHashIterator<QString, QString> it (shortcuts_);
    while (it.hasNext())
    {
        it.next();
        ui->tableWidget->item (index, 0)->setText (it.key());
        QString s = DEFAULT_SHORTCUTS.value (OBJECT_NAMES.value (it.key()));
        ui->tableWidget->item (index, 1)->setText (s);
        it.setValue (s);
        ++ index;
    }
    ui->tableWidget->setSortingEnabled (true);
    ui->tableWidget->setCurrentCell (cur, 1);
    connect (ui->tableWidget, &QTableWidget::itemChanged, this, &PrefDialog::onShortcutChange);

    ui->defaultButton->setEnabled (false);
    if (ui->promptLabel->isVisible())
    {
        prevtMsg_ = QString();
        showPrompt();
    }
}
void PrefDialog::prefShortcuts()
{
    FPsingleton *singleton = static_cast<FPsingleton*>(qApp);
    Config& config = singleton->getConfig();
    QHash<QString, QString>::const_iterator it = newShortcuts_.constBegin();
    while (it != newShortcuts_.constEnd())
    {
        if (DEFAULT_SHORTCUTS.value (it.key()) == it.value())
            config.removeShortcut (it.key());
        else
            config.setActionShortcut (it.key(), it.value());
        ++it;
    }
    for (int i = 0; i < singleton->Wins.count(); ++i)
    {
        FPwin *win = singleton->Wins.at (i);
        if (win != parent_)
            win->updateCustomizableShortcuts();
    }
}
void PrefDialog::prefSaveUnmodified()
{
    FPsingleton *singleton = static_cast<FPsingleton*>(qApp);
    Config& config = singleton->getConfig();
    if (ui->unmodifiedSaveBox->isChecked() == saveUnmodified_)
        return;
    config.setSaveUnmodified (!saveUnmodified_);
    for (int i = 0; i < singleton->Wins.count(); ++i)
    {
        FPwin *win = singleton->Wins.at (i);
        if (TabPage *tabPage = qobject_cast<TabPage*>(win->ui->tabWidget->currentWidget()))
        {
            TextEdit *textEdit = tabPage->textEdit();
            if (!saveUnmodified_)
            {
                if (!textEdit->isReadOnly() && !textEdit->isUneditable())
                    win->ui->actionSave->setEnabled (true);
            }
            else
                win->ui->actionSave->setEnabled (textEdit->document()->isModified());
        }
        for (int j = 0; j < win->ui->tabWidget->count(); ++j)
        {
            TextEdit *textEdit = qobject_cast< TabPage *>(win->ui->tabWidget->widget (j))->textEdit();
            if (!saveUnmodified_)
                disconnect (textEdit->document(), &QTextDocument::modificationChanged, win, &FPwin::enableSaving);
            else
                connect (textEdit->document(), &QTextDocument::modificationChanged, win, &FPwin::enableSaving);
        }
    }
}
void PrefDialog::prefTextTabSize (int value)
{
    if (value >= 2 && value <= 10)
    {
        textTabSize_ = value;
        showPrompt();
    }
}
void PrefDialog::prefTextTab()
{
    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
    config.setTextTabSize (textTabSize_);
}

}
