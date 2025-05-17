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

#ifndef SINGLETON_H
#define SINGLETON_H

#include <QApplication>
#include <QLocalServer>
#include <QLockFile>
#include "fpwin.h"
#include "config.h"

namespace fpad {

// A single-instance approach based on QSharedMemory.
class FPsingleton : public QApplication
{
    Q_OBJECT
public:
    FPsingleton (int &argc, char **argv, bool standalone);
    ~FPsingleton();

    bool sendMessage (const QString& message);
    void firstWin(const QString& message);
    FPwin* newWin (const QStringList &filesList = QStringList(),
                   int lineNum = 0, int posInLine = 0);
    void removeWin (FPwin *win);

    QList<FPwin*> Wins; // All fpad windows.

    Config& getConfig() {
        return config_;
    }

public slots:
    void receiveMessage();
    void handleMessage (const QString& message);
    void quitting();

signals:
    void messageReceived (const QString& message);

private:
    bool cursorInfo (const QString &commndOpt, int& lineNum, int& posInLine);
    bool check_file_exists(QString filename);
    QStringList processInfo (const QString& message,
                             long &desktop, int& lineNum, int& posInLine,
                             bool *newWindow);

    QString uniqueKey_;
    QLockFile *lockFile_;
    QLocalServer *localServer_;
    static const int timeout_ = 1000;
    Config config_;
    QStringList lastFiles_;
    bool socketFailure_;
    bool standalone_;
};

}

#endif // SINGLETON_H
