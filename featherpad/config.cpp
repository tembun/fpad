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

#include "config.h"
#include <QKeySequence>

namespace FeatherPad {

Config::Config():
    remSize_ (true),
    remPos_ (false),
    remSplitterPos_ (true),
    noToolbar_ (false),
    noMenubar_ (false),
    hideSearchbar_ (false),
    showStatusbar_ (true),
    showCursorPos_ (false),
    isMaxed_ (false),
    isFull_ (false),
    thickCursor_ (false),
    tabWrapAround_ (false),
    appendEmptyLine_ (true),
    removeTrailingSpaces_ (false),
    inertialScrolling_ (false),
    autoSave_ (false),
    skipNonText_ (true),
    saveUnmodified_ (false),
    pastePaths_ (false),
    sharedSearchHistory_ (false),
    tabPosition_ (0),
    maxSHSize_ (2),
    lightBgColorValue_ (255),
    recentFilesNumber_ (10),
    curRecentFilesNumber_ (10),
    autoSaveInterval_ (1),
    textTabSize_ (6),
    winSize_ (QSize (700, 500)),
    startSize_ (QSize (700, 500)),
    winPos_ (QPoint (0, 0)),
    splitterPos_ (20),
    font_ (QFont ("Monospace")),
    recentOpened_ (false),
    saveLastFilesList_ (false),
    cursorPosRetrieved_ (false) {}

Config::~Config() {}
void Config::readConfig()
{
    QVariant v;
    Settings settings ("featherpad", "fp");
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

    if (settings.value ("noToolbar").toBool())
        noToolbar_ = true; // false by default

    if (settings.value ("noMenubar").toBool())
        noMenubar_ = true; // false by default

    if (noToolbar_ && noMenubar_)
    {
        noToolbar_ = false;
        noMenubar_ = true;
    }

    if (settings.value ("hideSearchbar").toBool())
        hideSearchbar_ = true;

    v = settings.value ("showStatusbar");
    if (v.isValid())
        showStatusbar_ = v.toBool();

    if (settings.value ("showCursorPos").toBool())
        showCursorPos_ = true;

    int pos = settings.value ("tabPosition").toInt();
    if (pos > 0 && pos <= 3)
        tabPosition_ = pos; // 0 by default

    if (settings.value ("tabWrapAround").toBool())
        tabWrapAround_ = true; // false by default

    if (settings.value ("sharedSearchHistory").toBool())
        sharedSearchHistory_ = true; // false by default

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

    if (settings.value ("thickCursor").toBool())
        thickCursor_ = true; // false by default

    if (settings.value ("inertialScrolling").toBool())
        inertialScrolling_ = true; // false by default

    if (settings.value ("autoSave").toBool())
        autoSave_ = true; // false by default

    v = settings.value ("skipNonText");
    if (v.isValid()) // true by default
        skipNonText_ = v.toBool();

    if (settings.value ("saveUnmodified").toBool())
        saveUnmodified_ = true; // false by default

    if (settings.value ("pastePaths").toBool())
        pastePaths_ = true; // false by default
    maxSHSize_ = qBound (1, settings.value ("maxSHSize", 2).toInt(), 10);
    lightBgColorValue_ = qBound (230, settings.value ("lightBgColorValue", 255).toInt(), 255);
    v = settings.value ("appendEmptyLine");
    if (v.isValid()) // true by default
        appendEmptyLine_ = v.toBool();
    if (settings.value ("removeTrailingSpaces").toBool())
        removeTrailingSpaces_ = true; // false by default
    recentFilesNumber_ = qBound (0, settings.value ("recentFilesNumber", 10).toInt(), 20);
    curRecentFilesNumber_ = recentFilesNumber_; // fixed
    recentFiles_ = settings.value ("recentFiles").toStringList();
    recentFiles_.removeAll ("");
    recentFiles_.removeDuplicates();
    while (recentFiles_.count() > recentFilesNumber_)
        recentFiles_.removeLast();
    if (settings.value ("recentOpened").toBool())
        recentOpened_ = true; // false by default

    if (settings.value ("saveLastFilesList").toBool())
        saveLastFilesList_ = true; // false by default
    autoSaveInterval_ = qBound (1, settings.value ("autoSaveInterval", 1).toInt(), 60);
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
    Settings tmp ("featherpad", "fp");
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
    if (!saveLastFilesList_)
        return QStringList();

    Settings settingsLastCur ("featherpad", "fp_last_cursor_pos");
    lasFilesCursorPos_ = settingsLastCur.value ("cursorPositions").toHash();

    QStringList lastFiles = lasFilesCursorPos_.keys();
    lastFiles.removeAll ("");
    lastFiles.removeDuplicates();
    while (lastFiles.count() > 50) // never more than 50 files
        lastFiles.removeLast();
    return lastFiles;
}
void Config::writeConfig()
{
    Settings settings ("featherpad", "fp");
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
    settings.setValue ("noToolbar", noToolbar_);
    settings.setValue ("noMenubar", noMenubar_);
    settings.setValue ("hideSearchbar", hideSearchbar_);
    settings.setValue ("showStatusbar", showStatusbar_);
    settings.setValue ("showCursorPos", showCursorPos_);
    settings.setValue ("tabPosition", tabPosition_);
    settings.setValue ("tabWrapAround", tabWrapAround_);
    settings.setValue ("sharedSearchHistory", sharedSearchHistory_);
    settings.endGroup();
    settings.beginGroup ("text");
    settings.setValue ("font", font_.toString());
    settings.setValue ("thickCursor", thickCursor_);
    settings.setValue ("inertialScrolling", inertialScrolling_);
    settings.setValue ("autoSave", autoSave_);
    settings.setValue ("skipNonText", skipNonText_);
    settings.setValue ("saveUnmodified", saveUnmodified_);
    settings.setValue ("pastePaths", pastePaths_);
    settings.setValue ("maxSHSize", maxSHSize_);
    settings.setValue ("lightBgColorValue", lightBgColorValue_);
    settings.setValue ("appendEmptyLine", appendEmptyLine_);
    settings.setValue ("removeTrailingSpaces", removeTrailingSpaces_);
    settings.setValue ("recentFilesNumber", recentFilesNumber_);
    while (recentFiles_.count() > recentFilesNumber_)
        recentFiles_.removeLast();
    if (recentFiles_.isEmpty())
        settings.setValue ("recentFiles", "");
    else
        settings.setValue ("recentFiles", recentFiles_);
    settings.setValue ("recentOpened", recentOpened_);

    settings.setValue ("saveLastFilesList", saveLastFilesList_);

    settings.setValue ("autoSaveInterval", autoSaveInterval_);

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
        Settings settings ("featherpad", "fp_cursor_pos");
        cursorPos_ = settings.value ("cursorPositions").toHash();
        cursorPosRetrieved_ = true;
    }
}
void Config::writeCursorPos()
{
    Settings settings ("featherpad", "fp_cursor_pos");
    if (settings.isWritable())
    {
        if (!cursorPos_.isEmpty())
        {
            settings.setValue ("cursorPositions", cursorPos_);
        }
    }

    Settings settingsLastCur ("featherpad", "fp_last_cursor_pos");
    if (settingsLastCur.isWritable())
    {
        if (saveLastFilesList_ && !lasFilesCursorPos_.isEmpty())
            settingsLastCur.setValue ("cursorPositions", lasFilesCursorPos_);
        else
            settingsLastCur.remove ("cursorPositions");
    }
}
void Config::addRecentFile (const QString& file)
{
    if (curRecentFilesNumber_ > 0)
    {
        recentFiles_.removeAll (file);
        recentFiles_.prepend (file);
        while (recentFiles_.count() > curRecentFilesNumber_)
            recentFiles_.removeLast();
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
                // prevent ambiguous shortcuts at startup as far as possible
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
