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

#include <QGridLayout>
#include <QCompleter>
#include <QTimer>
#include "searchbar.h"

namespace FeatherPad {

static const int MAX_ROW_COUNT = 40;

void ComboBox::keyPressEvent (QKeyEvent *event)
{
    if (!(event->modifiers() & Qt::ControlModifier))
    {
        if (event->key() == Qt::Key_Up)
        {
            emit moveInHistory (true);
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Down)
        {
            emit moveInHistory (false);
            event->accept();
            return;
        }
    }
    QComboBox::keyPressEvent (event);
}

bool ComboBox::hasPopup() const
{
    return hasPopup_;
}

void ComboBox::showPopup()
{
    hasPopup_ = true;
    QComboBox::showPopup();
}

void ComboBox::hidePopup()
{
    QComboBox::hidePopup();
    /* wait for the popup to be closed */
    QTimer::singleShot (0, this, [this]() {
        hasPopup_ = false;
    });
}

SearchBar::SearchBar(QWidget *parent,
                     const QList<QKeySequence> &shortcuts,
                     Qt::WindowFlags f)
    : QFrame (parent, f)
{
    searchStarted_ = false;
    combo_ = new ComboBox (this);
    combo_->setMinimumWidth (150);
    combo_->setMinimumHeight( 45 );
    
    lineEdit_ = new LineEdit();
        
    QFont font_bold = lineEdit_->font();
    
    font_bold.setPointSize( 20 );
    font_bold.setWeight( QFont::Black );
    
    lineEdit_->setFont( font_bold );
    
    lineEdit_->setPlaceholderText (tr ("Search..."));
    combo_->setLineEdit (lineEdit_);
    combo_->setInsertPolicy (QComboBox::NoInsert);
    combo_->setCompleter (nullptr);
    combo_->setMaxCount (MAX_ROW_COUNT + 1);

    shortcuts_ = shortcuts;
    QKeySequence nxtShortcut, prevShortcut;
    if (shortcuts.size() >= 5)
    {
        nxtShortcut = shortcuts.at (0);
        prevShortcut = shortcuts.at (1);
    }
    toolButton_nxt_ = new QToolButton (this);
    toolButton_prv_ = new QToolButton (this);
    toolButton_nxt_->setAutoRaise (true);
    toolButton_prv_->setAutoRaise (true);
    toolButton_nxt_->setShortcut (nxtShortcut);
    toolButton_prv_->setShortcut (prevShortcut);
    toolButton_nxt_->setToolTip (tr ("Next") + " (" + nxtShortcut.toString (QKeySequence::NativeText) + ")");
    toolButton_prv_->setToolTip (tr ("Previous") + " (" + prevShortcut.toString (QKeySequence::NativeText) + ")");
    button_case_ = new QToolButton (this);
    QFont _font = button_case_->font();
    _font.setPointSize( 27 );
    button_case_->setText( "I" );
    button_case_->setFont(_font);
    button_case_->setToolTip ("Match Case ALT+I");
    button_case_->setShortcut (QKeySequence(Qt::ALT + Qt::Key_I));
    button_case_->setCheckable (true);
    button_case_->setFocusPolicy (Qt::NoFocus);
    button_whole_ = new QToolButton (this);
    button_whole_->setText( "W" );
    button_whole_->setFont(_font);
    button_whole_->setToolTip ("Whole Word ALT+W");
    button_whole_->setShortcut (QKeySequence(Qt::ALT + Qt::Key_W));
    button_whole_->setCheckable (true);
    button_whole_->setFocusPolicy (Qt::NoFocus);
    button_regex_ = new QToolButton (this);
    button_regex_->setText( "R" );
    button_regex_->setFont(_font);
    button_regex_->setToolTip ("RegEx ALT+R");
    button_regex_->setShortcut (QKeySequence(Qt::ALT + Qt::Key_R));
    button_regex_->setCheckable (true);
    button_regex_->setFocusPolicy (Qt::NoFocus);
    toolButton_nxt_->setFocusPolicy (Qt::NoFocus);
    toolButton_prv_->setFocusPolicy (Qt::NoFocus);
    QGridLayout *mainGrid = new QGridLayout;
    mainGrid->setHorizontalSpacing (3);
    mainGrid->setContentsMargins (2, 0, 2, 0);
    mainGrid->addWidget (combo_, 0, 0);
    mainGrid->addWidget (toolButton_nxt_, 0, 1);
    mainGrid->addWidget (toolButton_prv_, 0, 2);
    mainGrid->addItem (new QSpacerItem (6, 3), 0, 3);
    mainGrid->addWidget (button_case_, 0, 4);
    mainGrid->addWidget (button_whole_, 0, 5);
    mainGrid->addWidget (button_regex_, 0, 6);
    setLayout (mainGrid);
    connect (lineEdit_, &QLineEdit::returnPressed, this, &SearchBar::findForward);
    connect (lineEdit_, &FeatherPad::LineEdit::shift_enter_pressed, this, &SearchBar::findBackward);
    connect (toolButton_nxt_, &QAbstractButton::clicked, this, &SearchBar::findForward);
    connect (toolButton_prv_, &QAbstractButton::clicked, this, &SearchBar::findBackward);
    connect (button_case_, &QAbstractButton::clicked, this, &SearchBar::searchFlagChanged);
    connect (button_whole_, &QAbstractButton::clicked, [this] (bool checked) {
        button_regex_->setEnabled (!checked);
        emit searchFlagChanged();
    });
    connect (button_regex_, &QAbstractButton::clicked, [this] (bool checked) {
        button_whole_->setEnabled (!checked);
        if (checked)
            lineEdit_->setPlaceholderText (tr ("Search with regex..."));
        else
            lineEdit_->setPlaceholderText (tr ("Search..."));
        emit searchFlagChanged();
    });
    connect (lineEdit_, &LineEdit::showComboPopup, [this] {
        combo_->showPopup();
    });
    connect (combo_, &ComboBox::moveInHistory, lineEdit_, [this] (bool up) {
        int count = combo_->count();
        if (count == 0) return;
        int index = combo_->findText (lineEdit_->text(), Qt::MatchExactly);
        if (index < 0)
            combo_->setCurrentIndex (0);
        else
        {
            if (up)
            {
                if (index > 0)
                    combo_->setCurrentIndex (index - 1);
            }
            else if (index < count - 1)
                combo_->setCurrentIndex (index + 1);
        }
    });
}
void SearchBar::searchStarted()
{
    searchStarted_ = true;
    const QString txt = lineEdit_->text();
    if (txt.isEmpty()) return;
    int index = combo_->findText (txt, Qt::MatchExactly);
    if (index != 0)
    {
        if (index > 0)
            combo_->removeItem (index);
        else if (combo_->count() == MAX_ROW_COUNT)
            combo_->removeItem (MAX_ROW_COUNT - 1);
        combo_->insertItem (0, txt);
    }
    combo_->setCurrentIndex (0);
    searchStarted_ = false;
}
void SearchBar::focusLineEdit()
{
    lineEdit_->setFocus();
    lineEdit_->selectAll();
}
bool SearchBar::lineEditHasFocus() const
{
    return lineEdit_->hasFocus();
}

QString SearchBar::searchEntry() const
{
    return lineEdit_->text();
}

void SearchBar::clearSearchEntry()
{
    return;
}

void SearchBar::findForward()
{
    searchStarted();
    emit find (true);
}

void SearchBar::findBackward()
{
    searchStarted();
    emit find (false);
}

bool SearchBar::matchCase() const
{
    return button_case_->isChecked();
}

bool SearchBar::matchWhole() const
{
    return button_whole_->isChecked();
}

bool SearchBar::matchRegex() const
{
    return button_regex_->isChecked();
}

bool SearchBar::hasPopup() const
{
    return combo_->hasPopup();
}

// Used only in a workaround (-> FPwin::updateShortcuts())
void SearchBar::updateShortcuts (bool disable)
{
    if (disable)
    {
        toolButton_nxt_->setShortcut (QKeySequence());
        toolButton_prv_->setShortcut (QKeySequence());
        button_case_->setShortcut (QKeySequence());
        button_whole_->setShortcut (QKeySequence());
        button_regex_->setShortcut (QKeySequence());
    }
    else if (shortcuts_.size() >= 5)
    {
        toolButton_nxt_->setShortcut (shortcuts_.at (0));
        toolButton_prv_->setShortcut (shortcuts_.at (1));
        button_case_->setShortcut (shortcuts_.at (2));
        button_whole_->setShortcut (shortcuts_.at (3));
        button_regex_->setShortcut (shortcuts_.at (4));
    }
}

}
