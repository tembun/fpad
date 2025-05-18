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

#ifndef CONFIG_H
#define CONFIG_H

#include <QSettings>
#include <QSize>
#include <QPoint>
#include <QFont>
#include <QColor>

namespace fpad {
class Settings : public QSettings
{
    Q_OBJECT
public:
    Settings (const QString &organization, const QString &application = QString(), QObject *parent = nullptr)
             : QSettings (organization, application, parent) {}
    Settings (const QString &fileName, QSettings::Format format, QObject *parent = nullptr)
             : QSettings (fileName, format, parent) {}

    void setValue (const QString &key, const QVariant &v) {
        if (value (key) == v)
            return;
        QSettings::setValue (key, v);
    }
};
class Config {
public:
    Config();
    ~Config();

    void readConfig();
    void readShortcuts();
    void writeConfig();
    
    /*
     * Remember the window size on close.
     */
    bool getRemSize() const {
        return remSize_;
    }
    void setRemSize (bool rem) {
        remSize_ = rem;
    }
    
    /*
     * Remember window position on close.
     */
    bool getRemPos() const {
        return remPos_;
    }
    void setRemPos (bool rem) {
        remPos_ = rem;
    }

    bool getRemSplitterPos() const {
        return remSplitterPos_;
    }
    void setRemSplitterPos (bool rem) {
        remSplitterPos_ = rem;
    }

    bool getIsMaxed() const {
        return isMaxed_;
    }
    void setIsMaxed (bool isMaxed) {
        isMaxed_ = isMaxed;
    }

    bool getIsFull() const {
        return isFull_;
    }
    void setIsFull (bool isFull) {
        isFull_ = isFull;
    }
    int getTextTabSize() const {
        return textTabSize_;
    }
    void setTextTabSize (int textTab) {
        textTabSize_ = textTab;
    }
    QSize getWinSize() const {
        return winSize_;
    }
    void setWinSize (const QSize &s) {
        winSize_ = s;
    }
    QSize getPrefSize() const {
        return prefSize_;
    }
    void setPrefSize (const QSize &s) {
        prefSize_ = s;
    }
    QSize getStartSize() const {
        return startSize_;
    }
    void setStartSize (const QSize &s) {
        startSize_ = s;
    }
    QPoint getWinPos() const {
        return winPos_;
    }
    void setWinPos (const QPoint &p) {
        winPos_ = p;
    }

    int getSplitterPos() const {
        return splitterPos_;
    }
    void setSplitterPos (int pos) {
        splitterPos_ = pos;
    }
    QFont getFont() const {
        return font_;
    }
    void setFont (const QFont &font) {
        font_ = font;
    }
    void resetFont();
    int getMaxSHSize() const {
        return maxSHSize_;
    }
    void setMaxSHSize (int max) {
        maxSHSize_ = max;
    }
    QHash<QString, QString> customShortcutActions() const {
        return actions_;
    }
    void setActionShortcut (const QString &action, const QString &shortcut) {
        actions_.insert (action, shortcut);
    }
    void removeShortcut (const QString &action) {
        actions_.remove (action);
        removedActions_ << action;
    }
    bool hasReservedShortcuts() const {
        return (!reservedShortcuts_.isEmpty());
    }
    QStringList reservedShortcuts() const {
        return reservedShortcuts_;
    }
    void setReservedShortcuts (const QStringList &s) {
        reservedShortcuts_ = s;
    }
    QHash<QString, QVariant> savedCursorPos() {
        readCursorPos();
        return cursorPos_;
    }
    void saveCursorPos (const QString& name, int pos) {
        readCursorPos();
        if (removedCursorPos_.contains (name))
            removedCursorPos_.removeOne (name);
        else
            cursorPos_.insert (name, pos);
    }
    void removeCursorPos (const QString& name) {
        readCursorPos();
        cursorPos_.remove (name);
        removedCursorPos_ << name;
    }
    void removeAllCursorPos() {
        readCursorPos();
        removedCursorPos_.append (cursorPos_.keys());
        cursorPos_.clear();
    }
    QStringList getLastFiles();
    QHash<QString, QVariant> getLastFilesCursorPos() const {
        return lasFilesCursorPos_;
    }
    void setLastFileCursorPos (const QHash<QString, QVariant>& curPos) {
        lasFilesCursorPos_ = curPos;
    }
    bool getSaveUnmodified() const {
        return saveUnmodified_;
    }
    void setSaveUnmodified (bool save) {
        saveUnmodified_ = save;
    }

private:
    QString validatedShortcut (const QVariant v, bool *isValid);
    void readCursorPos();
    void writeCursorPos();

    bool remSize_, remPos_, remSplitterPos_,
         isMaxed_, isFull_,
         saveUnmodified_;
    int maxSHSize_,
        textTabSize_;
    QSize winSize_, startSize_, prefSize_;
    QPoint winPos_;
    int splitterPos_;
    QFont font_;
    QHash<QString, QString> actions_;
    QStringList removedActions_, reservedShortcuts_;
    QHash<QString, QVariant> cursorPos_;
    QStringList removedCursorPos_;
    bool cursorPosRetrieved_;
    QHash<QString, QVariant> lasFilesCursorPos_;
};

}

#endif // CONFIG_H
