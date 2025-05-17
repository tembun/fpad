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

#include "fpwin.h"
#include "ui_fp.h"

namespace fpad {

void FPwin::removeGreenSel()
{
    int count = ui->tabWidget->count();
    for (int i = 0; i < count; ++i)
    {
        TextEdit *textEdit = qobject_cast< TabPage *>(ui->tabWidget->widget (i))->textEdit();
        QTextEdit::ExtraSelection curLineSel;
        QList<QTextEdit::ExtraSelection> es = textEdit->extraSelections();
        if (ui->spinBox->isVisible())
        {
            curLineSel = textEdit->currentLineSelection();
            if (!es.isEmpty())
                es.removeFirst();
        }
        int n = textEdit->getGreenSel().count();
        while (n > 0 && !es.isEmpty())
        {
            es.removeFirst();
            --n;
        }
        es.prepend (curLineSel);
        textEdit->setGreenSel (QList<QTextEdit::ExtraSelection>());
        textEdit->setExtraSelections (es);
    }
}
void FPwin::replaceDock()
{
    if (!isReady()) return;

    int count = ui->tabWidget->count();
    for (int i = 0; i < count; ++i)
    {
        qobject_cast< TabPage *>(ui->tabWidget->widget (i))->setSearchBarVisible (true);
        ui->dockReplace->setWindowTitle("Replacement");

        QFont font_bold = ui->dockReplace->font();
        QFont font_demi = ui->dockReplace->font();
        
        font_bold.setPointSize( 20 );
        font_bold.setWeight( QFont::Black );
        font_demi.setPointSize( 20 );
        font_demi.setWeight( QFont::DemiBold );
        
        ui->dockReplace->setFont( font_bold );
        ui -> label_2 -> setFont( font_demi );
        ui -> label_3 -> setFont( font_demi );
        
        ui->dockReplace->setVisible (true);
        ui->dockReplace->raise();
        ui->dockReplace->activateWindow();
        if (!ui->lineEditFind->hasFocus())
            ui->lineEditFind->setFocus();
    };
    return;
}
void FPwin::closeReplaceDock (bool visible)
{
    if (visible || isMinimized()) return;
    txtReplace_.clear();
    removeGreenSel();
    if (ui->tabWidget->count() > 0)
    {
        TextEdit *textEdit = qobject_cast< TabPage *>(ui->tabWidget->currentWidget())->textEdit();
        textEdit->setFocus();
        textEdit->setReplaceTitle (QString());
    }
}
void FPwin::resizeDock (bool topLevel)
{
    if (topLevel)
        ui->dockReplace->resize (ui->dockReplace->minimumWidth(),
                                 ui->dockReplace->minimumHeight());
}
void FPwin::replace()
{
    if (!isReady()) return;

    TabPage *tabPage = qobject_cast< TabPage *>(ui->tabWidget->currentWidget());
    if (tabPage == nullptr) return;

    TextEdit *textEdit = tabPage->textEdit();
    if (textEdit->isReadOnly()) return;

    textEdit->setReplaceTitle (QString());
    ui->dockReplace->setWindowTitle("Replacement");

    QString txtFind = ui->lineEditFind->text();
    if (txtFind.isEmpty()) return;
    if (txtReplace_ != ui->lineEditReplace->text())
    {
        txtReplace_ = ui->lineEditReplace->text();
        textEdit->setGreenSel (QList<QTextEdit::ExtraSelection>());
    }
    QTextDocument::FindFlags searchFlags = getSearchFlags();
    QTextCursor start = textEdit->textCursor();
    QTextCursor tmp = start;
    QTextCursor found;
    if (QObject::sender() == ui->toolButtonNext)
        found = textEdit->finding (txtFind, start, searchFlags, tabPage->matchRegex());
    else
        found = textEdit->finding (txtFind, start, searchFlags | QTextDocument::FindBackward, tabPage->matchRegex());
    QColor color = QColor (Qt::black);
    int pos;
    QList<QTextEdit::ExtraSelection> es = textEdit->getGreenSel();
    if (!found.isNull())
    {
        start.setPosition (found.anchor());
        pos = found.anchor();
        start.setPosition (found.position(), QTextCursor::KeepAnchor);
        textEdit->setTextCursor (start);
        textEdit->insertPlainText (txtReplace_);

        start = textEdit->textCursor();
        tmp.setPosition (pos);
        tmp.setPosition (start.position(), QTextCursor::KeepAnchor);
        QTextEdit::ExtraSelection extra;
        extra.format.setBackground (color);
        extra.cursor = tmp;
        es.append (extra);

        if (QObject::sender() != ui->toolButtonNext)
        {
            start.setPosition (start.position() - txtReplace_.length());
            textEdit->setTextCursor (start);
        }
    }
    textEdit->setGreenSel (es);
    es.prepend (textEdit->currentLineSelection());
    es.append (textEdit->getBlueSel());
    es.append (textEdit->getRedSel());
    textEdit->setExtraSelections (es);
    hlight();
}
void FPwin::replaceAll()
{
    if (!isReady()) return;
    TabPage *tabPage = qobject_cast< TabPage *>(ui->tabWidget->currentWidget());
    if (tabPage == nullptr) return;
    TextEdit *textEdit = tabPage->textEdit();
    if (textEdit->isReadOnly()) return;
    QString txtFind = ui->lineEditFind->text();
    if (txtFind.isEmpty()) return;
    if (txtReplace_ != ui->lineEditReplace->text())
    {
        txtReplace_ = ui->lineEditReplace->text();
        textEdit->setGreenSel (QList<QTextEdit::ExtraSelection>());
    }

    QTextDocument::FindFlags searchFlags = getSearchFlags();

    QTextCursor orig = textEdit->textCursor();
    orig.setPosition (orig.anchor());
    textEdit->setTextCursor (orig);
    QColor color = QColor (Qt::black);
    int pos; QTextCursor found;
    QTextCursor start = orig;
    start.beginEditBlock();
    start.setPosition (0);
    QTextCursor tmp = start;
    QList<QTextEdit::ExtraSelection> es = textEdit->getGreenSel();
    int count = 0;
    QTextEdit::ExtraSelection extra;
    extra.format.setBackground (color);

    waitToMakeBusy();
    while (!(found = textEdit->finding (txtFind, start, searchFlags, tabPage->matchRegex())).isNull())
    {
        start.setPosition (found.anchor());
        pos = found.anchor();
        start.setPosition (found.position(), QTextCursor::KeepAnchor);
        start.insertText (txtReplace_);

        if (count < 1000)
        {
            tmp.setPosition (pos);
            tmp.setPosition (start.position(), QTextCursor::KeepAnchor);
            extra.cursor = tmp;
            es.append (extra);
        }
        start.setPosition (start.position());
        ++count;
    }
    unbusy();

    textEdit->setGreenSel (es);
    start.endEditBlock();
    es.prepend (textEdit->currentLineSelection());
    es.append (textEdit->getBlueSel());
    es.append (textEdit->getRedSel());
    textEdit->setExtraSelections (es);
    hlight();

    QString title;
    if (count == 0)
        title = QString("No Replacement");
    else if (count == 1)
        title = QString("One Replacement");
    else
        title = QString("%1 Replacements").arg(count);
    ui->dockReplace->setWindowTitle (title);
    textEdit->setReplaceTitle (title);
}

}
