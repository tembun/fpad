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

#ifndef CONFIG_H
#define CONFIG_H

#include <QSettings>
#include <QSize>
#include <QPoint>
#include <QFont>
#include <QColor>

namespace FeatherPad {
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

    bool getRemSize() const {
        return remSize_;
    }
    void setRemSize (bool rem) {
        remSize_ = rem;
    }

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
    int getLightBgColorValue() const {
        return lightBgColorValue_;
    }
    void setLightBgColorValue (int lightness) {
        lightBgColorValue_ = lightness;
    }
    int getTextTabSize() const {
        return textTabSize_;
    }
    void setTextTabSize (int textTab) {
        textTabSize_ = textTab;
    }
    int getRecentFilesNumber() const {
        return recentFilesNumber_;
    }
    void setRecentFilesNumber (int number) {
        recentFilesNumber_ = number;
    }
    int getCurRecentFilesNumber() const {
        return curRecentFilesNumber_;
    }

    bool getTabWrapAround() const {
        return tabWrapAround_;
    }
    void setTabWrapAround (bool wrap) {
        tabWrapAround_ = wrap;
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

    bool getNoToolbar() const {
        return noToolbar_;
    }
    void setNoToolbar (bool noTB) {
        noToolbar_ = noTB;
    }

    bool getNoMenubar() const {
        return noMenubar_;
    }
    void setNoMenubar (bool noMB) {
        noMenubar_ = noMB;
    }

    bool getHideSearchbar() const {
        return hideSearchbar_;
    }
    void setHideSearchbar (bool hide) {
        hideSearchbar_ = hide;
    }

    bool getShowStatusbar() const {
        return showStatusbar_;
    }
    void setShowStatusbar (bool show) {
        showStatusbar_ = show;
    }

    bool getShowCursorPos() const {
        return showCursorPos_;
    }
    void setShowCursorPos (bool show) {
        showCursorPos_ = show;
    }
    int getTabPosition() const {
        return tabPosition_;
    }
    void setTabPosition (int pos) {
        tabPosition_ = pos;
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
    bool getRecentOpened() const {
        return recentOpened_;
    }
    void setRecentOpened (bool opened) {
        recentOpened_ = opened;
    }
    QStringList getRecentFiles() const {
        return recentFiles_;
    }
    void clearRecentFiles() {
        recentFiles_ = QStringList();
    }
    void addRecentFile (const QString &file);

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
    bool getSharedSearchHistory() const {
        return sharedSearchHistory_;
    }
    void setSharedSearchHistory (bool share) {
        sharedSearchHistory_ = share;
    }

private:
    QString validatedShortcut (const QVariant v, bool *isValid);
    void readCursorPos();
    void writeCursorPos();

    bool remSize_, remPos_, remSplitterPos_,
         noToolbar_, noMenubar_,
         hideSearchbar_,
         showStatusbar_, showCursorPos_,
         isMaxed_, isFull_,
         tabWrapAround_,
         saveUnmodified_,
         sharedSearchHistory_;
    int tabPosition_,
        maxSHSize_,
        lightBgColorValue_,
        recentFilesNumber_,
        curRecentFilesNumber_,
        textTabSize_;
    QSize winSize_, startSize_, prefSize_;
    QPoint winPos_;
    int splitterPos_;
    QFont font_;
    bool recentOpened_;
    QStringList recentFiles_;
    QHash<QString, QString> actions_;
    QStringList removedActions_, reservedShortcuts_;
    QHash<QString, QVariant> cursorPos_;
    QStringList removedCursorPos_;
    bool cursorPosRetrieved_;
    QHash<QString, QVariant> lasFilesCursorPos_;
};

}

#endif // CONFIG_H
