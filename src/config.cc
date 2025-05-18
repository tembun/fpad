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

#include "config.h"
#include <QKeySequence>

namespace fpad {

Config::Config():
    remSize_ (true),
    remPos_ (false),
    remSplitterPos_ (true),
    isMaxed_ (false),
    isFull_ (false),
    saveUnmodified_ (false),
    maxSHSize_ (2),
    textTabSize_ (6),
    winSize_ (QSize (700, 500)),
    startSize_ (QSize (700, 500)),
    winPos_ (QPoint (0, 0)),
    splitterPos_ (20),
    font_ (QFont ("Monospace")),
    cursorPosRetrieved_ (false) {}

Config::~Config() {}
void Config::readConfig()
{
    QVariant v;
    Settings settings ("fpad", "fp");
    settings.beginGroup ("window");

    if (settings.value ("size") == "none")
        remSize_ = false;
    else
    {
        winSize_ = settings.value ("size", QSize (700, 500)).toSize();
        if (!winSize_.isValid() || winSize_.isNull())
            winSize_ = QSize (700, 500);
        isMaxed_ = settings.value ("max", false).toBool();
        isFull_ = settings.value ("fullscreen", false).toBool();
    }
    startSize_ = settings.value ("startSize", QSize (700, 500)).toSize();
    if (!startSize_.isValid() || startSize_.isNull())
        startSize_ = QSize (700, 500);

    v = settings.value ("position");
    if (v.isValid() && settings.value ("position") != "none")
    {
        remPos_ = true; // false by default
        winPos_ = settings.value ("position", QPoint (0, 0)).toPoint();
    }

    if (settings.value ("splitterPos") == "none")
        remSplitterPos_ = false; // true by default
    else
        splitterPos_ = qMin (qMax (settings.value ("splitterPos", 20).toInt(), 0), 100);

    prefSize_ = settings.value ("prefSize").toSize();

    settings.endGroup();

    settings.beginGroup ("text");

    if (settings.value ("font") == "none")
    {
        font_.setPointSize (qMax (QFont().pointSize(), 9));
    }
    else
    {
        QString fontStr = settings.value ("font").toString();
        if (!fontStr.isEmpty())
            font_.fromString (fontStr);
        else
            font_.setPointSize (qMax (QFont().pointSize(), 9));
    }
    if (settings.value ("saveUnmodified").toBool())
        saveUnmodified_ = true; // false by default
    maxSHSize_ = qBound (1, settings.value ("maxSHSize", 2).toInt(), 10);
    v = settings.value ("appendEmptyLine");
    textTabSize_ = qBound (2, settings.value ("textTabSize", 4).toInt(), 10);
    settings.endGroup();
}
void Config::resetFont()
{
    font_ = QFont ("Monospace");
    font_.setPointSize (qMax (QFont().pointSize(), 9));
}
void Config::readShortcuts()
{
    Settings tmp ("fpad", "fp");
    Settings settings (tmp.fileName(), QSettings::NativeFormat);

    settings.beginGroup ("shortcuts");
    QStringList actions = settings.childKeys();
    for (int i = 0; i < actions.size(); ++i)
    {
        QVariant v = settings.value (actions.at (i));
        bool isValid;
        QString vs = validatedShortcut (v, &isValid);
        if (isValid)
            setActionShortcut (actions.at (i), vs);
        else
            removedActions_ << actions.at (i);
    }
    settings.endGroup();
}
QStringList Config::getLastFiles()
{
    Settings settingsLastCur ("fpad", "fp_last_cursor_pos");
    lasFilesCursorPos_ = settingsLastCur.value ("cursorPositions").toHash();

    QStringList lastFiles = lasFilesCursorPos_.keys();
    lastFiles.removeAll ("");
    lastFiles.removeDuplicates();
    while (lastFiles.count() > 50)
        lastFiles.removeLast();
    return lastFiles;
}
void Config::writeConfig()
{
    Settings settings ("fpad", "fp");
    if (!settings.isWritable()) return;

    settings.beginGroup ("window");

    if (remSize_)
    {
        settings.setValue ("size", winSize_);
        settings.setValue ("max", isMaxed_);
        settings.setValue ("fullscreen", isFull_);
    }
    else
    {
        settings.setValue ("size", "none");
        settings.remove ("max");
        settings.remove ("fullscreen");
    }

    if (remPos_)
        settings.setValue ("position", winPos_);
    else
        settings.setValue ("position", "none");

    if (remSplitterPos_)
        settings.setValue ("splitterPos", splitterPos_);
    else
        settings.setValue ("splitterPos", "none");

    settings.setValue ("prefSize", prefSize_);
    settings.setValue ("startSize", startSize_);
    settings.endGroup();
    settings.beginGroup ("text");
    settings.setValue ("font", font_.toString());
    settings.setValue ("saveUnmodified", saveUnmodified_);
    settings.setValue ("maxSHSize", maxSHSize_);
    settings.setValue ("textTabSize", textTabSize_);
    settings.endGroup();
    settings.beginGroup ("shortcuts");

    for (int i = 0; i < removedActions_.size(); ++i)
        settings.remove (removedActions_.at (i));

    QHash<QString, QString>::const_iterator it = actions_.constBegin();
    while (it != actions_.constEnd())
    {
        settings.setValue (it.key(), it.value());
        ++it;
    }
    settings.endGroup();
    writeCursorPos();
}
void Config::readCursorPos()
{
    if (!cursorPosRetrieved_)
    {
        Settings settings ("fpad", "fp_cursor_pos");
        cursorPos_ = settings.value ("cursorPositions").toHash();
        cursorPosRetrieved_ = true;
    }
}
void Config::writeCursorPos()
{
    Settings settings ("fpad", "fp_cursor_pos");
    if (settings.isWritable())
    {
        if (!cursorPos_.isEmpty())
        {
            settings.setValue ("cursorPositions", cursorPos_);
        }
    }

    Settings settingsLastCur ("fpad", "fp_last_cursor_pos");
    if (settingsLastCur.isWritable())
    {
        if (!lasFilesCursorPos_.isEmpty())
            settingsLastCur.setValue ("cursorPositions", lasFilesCursorPos_);
    }
}
QString Config::validatedShortcut (const QVariant v, bool *isValid)
{
    static QStringList added;
    if (v.isValid())
    {
        QString str = v.toString();
        if (str.isEmpty())
        {
            *isValid = true;
            return QString();
        }

        QKeySequence keySeq (str);
        if (str == keySeq.toString (QKeySequence::NativeText))
            str = keySeq.toString();

        if (!QKeySequence (str, QKeySequence::PortableText).toString().isEmpty())
        {
            if (!reservedShortcuts_.contains (str)
                && !added.contains (str))
            {
                *isValid = true;
                added << str;
                return str;
            }
        }
    }

    *isValid = false;
    return QString();
}

}
