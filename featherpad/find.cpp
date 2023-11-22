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

#include "fpwin.h"
#include "ui_fp.h"
#include <QTextDocumentFragment>

namespace FeatherPad {
void FPwin::find (bool forward)
{
    if (!isReady()) return;

    TabPage *tabPage = qobject_cast< TabPage *>(ui->tabWidget->currentWidget());
    if (tabPage == nullptr) return;

    TextEdit *textEdit = tabPage->textEdit();
    QString txt = tabPage->searchEntry();
    bool newSrch = false;
    if (textEdit->getSearchedText() != txt)
    {
        textEdit->setSearchedText (txt);
        newSrch = true;
    }

    disconnect (textEdit, &TextEdit::resized, this, &FPwin::hlight);
    disconnect (textEdit, &TextEdit::updateRect, this, &FPwin::hlight);
    disconnect (textEdit, &QPlainTextEdit::textChanged, this, &FPwin::hlight);

    if (txt.isEmpty())
    {
        QList<QTextEdit::ExtraSelection> es;
        textEdit->setGreenSel (es); // not needed
        if (ui->spinBox->isVisible())
            es.prepend (textEdit->currentLineSelection());
        es.append (textEdit->getBlueSel());
        es.append (textEdit->getRedSel());
        textEdit->setExtraSelections (es);
        return;
    }

    QTextDocument::FindFlags searchFlags = getSearchFlags();
    QTextDocument::FindFlags newFlags = searchFlags;
    if (!forward)
        newFlags = searchFlags | QTextDocument::FindBackward;
    QTextCursor start = textEdit->textCursor();
    QTextCursor found = textEdit->finding (txt, start, newFlags, tabPage->matchRegex());

    if (found.isNull())
    {
        if (!forward)
            start.movePosition (QTextCursor::End, QTextCursor::MoveAnchor);
        else
            start.movePosition (QTextCursor::Start, QTextCursor::MoveAnchor);
        found = textEdit->finding (txt, start, newFlags, tabPage->matchRegex());
    }

    if (!found.isNull())
    {
        start.setPosition (found.anchor());
        if (newSrch) textEdit->setTextCursor (start);
        start.setPosition (found.position(), QTextCursor::KeepAnchor);
        textEdit->setTextCursor (start);
    }
    hlight();
    connect (textEdit, &QPlainTextEdit::textChanged, this, &FPwin::hlight);
    connect (textEdit, &TextEdit::updateRect, this, &FPwin::hlight);
    connect (textEdit, &TextEdit::resized, this, &FPwin::hlight);
}
void FPwin::hlight() const
{
    TabPage *tabPage = qobject_cast< TabPage *>(ui->tabWidget->currentWidget());
    if (tabPage == nullptr) return;
    TextEdit *textEdit = tabPage->textEdit();
    const QString txt = textEdit->getSearchedText();
    if (txt.isEmpty()) return;
    QTextDocument::FindFlags searchFlags = getSearchFlags();
    QList<QTextEdit::ExtraSelection> es = textEdit->getGreenSel();
    
    QTextCursor found;
    QPoint Point (0, 0);
    QTextCursor start = textEdit->cursorForPosition (Point);
    int startPos = start.position() - (!tabPage->matchRegex() ? txt.length() : 0);
    if (startPos >= 0)
        start.setPosition (startPos);
    else
        start.setPosition (0);
    Point = QPoint (textEdit->geometry().width(), textEdit->geometry().height());
    QTextCursor end = textEdit->cursorForPosition (Point);
    int endLimit = end.anchor();
    int endPos = end.position() + (!tabPage->matchRegex() ? txt.length() : 0);
    end.movePosition (QTextCursor::End);
    if (endPos <= end.position())
        end.setPosition (endPos);
    QTextCursor visCur = start;
    visCur.setPosition (end.position(), QTextCursor::KeepAnchor);
    const QString str = visCur.selection().toPlainText();
    Qt::CaseSensitivity cs = tabPage->matchCase() ? Qt::CaseSensitive : Qt::CaseInsensitive;
    
    QColor bg = QColor( 0 , 0 , 0 );
    QColor fg = QColor( 255 , 255 , 255 );
    
    if( textEdit->hasDarkScheme() )
    {
	  bg = QColor( 255 , 255 , 255 );
	  fg = QColor( 0 , 0 , 0 );
    }
    
    if (tabPage->matchRegex() || str.contains (txt, cs))
    {
        while (!(found = textEdit->finding (txt, start, searchFlags,  tabPage->matchRegex(), endLimit)).isNull())
        {
            QTextEdit::ExtraSelection extra;
            extra.format.setBackground ( bg );
            extra.format.setForeground ( fg );
            extra.cursor = found;
            es.append (extra);
            start.setPosition (found.position());
        }
    }
    if (ui->spinBox->isVisible())
        es.prepend (textEdit->currentLineSelection());
    es.append (textEdit->getBlueSel());
    es.append (textEdit->getRedSel());
    textEdit->setExtraSelections (es);
}
void FPwin::searchFlagChanged()
{
    if (!isReady()) return;
    TabPage *tabPage = qobject_cast< TabPage *>(ui->tabWidget->currentWidget());
    if (tabPage == nullptr) return;
    TextEdit *textEdit = tabPage->textEdit();
    QTextCursor start = textEdit->textCursor();
    if (start.hasSelection())
    {
        start.setPosition (start.anchor());
        textEdit->setTextCursor (start);
    }
    hlight();
}
QTextDocument::FindFlags FPwin::getSearchFlags() const
{
    TabPage *tabPage = qobject_cast< TabPage *>(ui->tabWidget->currentWidget());
    QTextDocument::FindFlags searchFlags = QTextDocument::FindFlags();
    if (tabPage != nullptr)
    {
        if (tabPage->matchWhole())
            searchFlags = QTextDocument::FindWholeWords;
        if (tabPage->matchCase())
            searchFlags |= QTextDocument::FindCaseSensitively;
    }
    return searchFlags;
}

}
