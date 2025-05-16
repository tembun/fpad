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

#include <unistd.h>
#include "ui_fp.h"
#include <QDir>
#include <QScreen>
#include <QLocalSocket>
#include <QDialog>
#include <QStandardPaths>
#include <QCryptographicHash>
#include <QThread>

#if defined Q_OS_LINUX || defined Q_OS_FREEBSD || defined Q_OS_OPENBSD || defined Q_OS_NETBSD || defined Q_OS_HURD
#include <unistd.h>
#endif

#ifdef HAS_X11
#if defined Q_WS_X11 || defined Q_OS_LINUX || defined Q_OS_FREEBSD || defined Q_OS_OPENBSD || defined Q_OS_NETBSD || defined Q_OS_HURD
#include <QX11Info>
#endif
#endif

#include "singleton.h"

#ifdef HAS_X11
#include "x11.h"
#endif

namespace FeatherPad {

FPsingleton::FPsingleton (int &argc, char **argv, bool standalone) : QApplication (argc, argv)
{
#ifdef HAS_X11
#if defined Q_WS_X11 || defined Q_OS_LINUX || defined Q_OS_FREEBSD || defined Q_OS_OPENBSD || defined Q_OS_NETBSD || defined Q_OS_HURD
    isX11_ = QX11Info::isPlatformX11();
#else
    isX11_ = false;
#endif
#else
    isX11_ = false;
#endif

    standalone_ = standalone;
    socketFailure_ = false;
    config_.readConfig();
    lastFiles_ = config_.getLastFiles();
    if (standalone)
    {
        lockFile_ = nullptr;
        localServer_ = nullptr;
        return;
    }
    QByteArray user = qgetenv ("USER");
    uniqueKey_ = "featherpad-" + QString::fromLocal8Bit (user) + QLatin1Char ('-')
                 + QCryptographicHash::hash (user, QCryptographicHash::Sha1).toHex();
    QString lockFilePath = QStandardPaths::writableLocation (QStandardPaths::TempLocation)
                           + "/" + uniqueKey_ + ".lock";
    lockFile_ = new QLockFile (lockFilePath);

    if (lockFile_->tryLock())
    {
        localServer_ = new QLocalServer (this);
        connect (localServer_, &QLocalServer::newConnection, this, &FPsingleton::receiveMessage);
        if (!localServer_->listen (uniqueKey_))
        {
            if (localServer_->removeServer (uniqueKey_))
                localServer_->listen (uniqueKey_);
            else
                qDebug ("Unable to remove server instance (from a previous crash).");
        }
    }
    else
    {
        delete lockFile_; lockFile_ = nullptr;
        localServer_ = nullptr;
    }
}

FPsingleton::~FPsingleton()
{
    if (lockFile_)
    {
        lockFile_->unlock();
        delete lockFile_;
    }
}

void FPsingleton::quitting()
{
    config_.writeConfig();
}

void FPsingleton::receiveMessage()
{
    QLocalSocket *localSocket = localServer_->nextPendingConnection();
    if (!localSocket)
    {
        qDebug ("Unable to find local socket.");
        return;
    }
    if (!localSocket->waitForReadyRead (timeout_))
    {
        qDebug ("%s", (const char *) localSocket->errorString().toLatin1());
        return;
    }
    QByteArray byteArray = localSocket->readAll();
    QString message = QString::fromUtf8 (byteArray.constData());
    emit messageReceived (message);
    localSocket->disconnectFromServer();
}
bool FPsingleton::sendMessage (const QString& message)
{
    if (standalone_
        || localServer_ != nullptr)
    {
        return false;
    }

    QLocalSocket localSocket (this);
    localSocket.connectToServer (uniqueKey_, QIODevice::WriteOnly);
    int waiting = 0;
    while (waiting < 5 && !localSocket.waitForConnected (timeout_))
    {
        QThread::msleep(500);
        localSocket.connectToServer (uniqueKey_, QIODevice::WriteOnly);
        ++ waiting;
    }
    if (waiting == 5 && !localSocket.waitForConnected (timeout_))
    {
        socketFailure_ = true;
        qDebug ("%s", (const char *) localSocket.errorString().toLatin1());
        return false;
    }

    localSocket.write (message.toUtf8());
    if (!localSocket.waitForBytesWritten (timeout_))
    {
        socketFailure_ = true;
        qDebug ("%s", (const char *) localSocket.errorString().toLatin1());
        return false;
    }
    localSocket.disconnectFromServer();
    return true;
}

bool FPsingleton::cursorInfo (const QString& commndOpt, int& lineNum, int& posInLine)
{
    if (commndOpt.isEmpty()) return false;
    lineNum = 0;
    posInLine = 0;
    if (commndOpt == "+")
    {
        lineNum = -2;
        posInLine = 0;
        return true;
    }
    else if (commndOpt.startsWith ("+"))
    {
        bool ok;
        lineNum = commndOpt.toInt (&ok);
        if (ok)
        {
            if (lineNum > 0)
                lineNum += 1;
            return true;
        }
        else
        {
            QStringList l = commndOpt.split (",");
            if (l.count() == 2)
            {
                lineNum = l.at (0).toInt (&ok);
                if (ok)
                {
                    posInLine = l.at (1).toInt (&ok);
                    if (ok)
                    {
                        if (lineNum > 0)
                            lineNum += 1;
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

QStringList FPsingleton::processInfo (const QString& message,
                                      long &desktop, int& lineNum, int& posInLine,
                                      bool *newWindow)
{
    desktop = -1;
    lineNum = 0;
    posInLine = 0;
    QStringList sl = message.split ("\n\r");
    if (sl.count() < 3)
    {
        *newWindow = true;
        return QStringList();
    }
    desktop = sl.at (0).toInt();
    sl.removeFirst();
    QDir curDir (sl.at (0));
    sl.removeFirst();
    if (standalone_)
    {
        *newWindow = true;
        sl.removeFirst();
        if (sl.isEmpty())
            return QStringList();
    }
    else
        *newWindow = false;
    bool hasCurInfo = cursorInfo (sl.at (0), lineNum, posInLine);
    if (hasCurInfo)
    {
        sl.removeFirst();
        if (!sl.isEmpty())
        {
            if (sl.at (0) == "--win" || sl.at (0) == "-w")
            {
                *newWindow = true;
                sl.removeFirst();
            }
        }
    }
    else if (sl.at (0) == "--win" || sl.at (0) == "-w")
    {
        *newWindow = true;
        sl.removeFirst();
        if (!sl.isEmpty())
            hasCurInfo = cursorInfo (sl.at (0), lineNum, posInLine);
        if (hasCurInfo)
            sl.removeFirst();
    }
    QStringList filesList;
    for (const auto &path : qAsConst (sl))
    {
        if (!path.isEmpty())
        {
            QString realPath = path;
            if (realPath.startsWith ("file://"))
                realPath = QUrl (realPath).toLocalFile();
            realPath = curDir.absoluteFilePath (realPath);
            filesList << QDir::cleanPath (realPath);
        }
    }
    if (filesList.isEmpty() && hasCurInfo)
        qDebug ("FeatherPad: File path/name is missing.");
    return filesList;
}

void FPsingleton::firstWin (const QString& message)
{
    int lineNum = 0, posInLine = 0;
    long d = -1;
    bool openNewWin;
    const QStringList filesList = processInfo (message, d, lineNum, posInLine, &openNewWin);
    newWin (filesList, lineNum, posInLine);
    lastFiles_ = QStringList();
}

FPwin* FPsingleton::newWin (const QStringList& filesList,
                            int lineNum, int posInLine)
{
    FPwin *fp = new FPwin (nullptr, standalone_);
    fp->show();
    if (socketFailure_)
        fp->showCrashWarning();
#if defined Q_OS_LINUX || defined Q_OS_FREEBSD || defined Q_OS_OPENBSD || defined Q_OS_NETBSD || defined Q_OS_HURD
    else if (geteuid() == 0)
        fp->showRootWarning();
#endif
    Wins.append (fp);

    if (!filesList.isEmpty())
    {
        bool multiple (filesList.count() > 1 || fp->isLoading());
        for (int i = 0; i < filesList.count(); ++i){
        	if(access( filesList.at(i).toStdString().c_str(), F_OK ) == -1) {
        		dprintf(1,"no such file: %s.\n",filesList.at(i).toStdString().c_str());
        	}
        	fp->newTabFromName (filesList.at (i), lineNum, posInLine, multiple);
        }
        
    }
    else if (!lastFiles_.isEmpty())
    {
        bool multiple (lastFiles_.count() > 1 || fp->isLoading());
        for (int i = 0; i < lastFiles_.count(); ++i){
        	if(access( lastFiles_.at(i).toStdString().c_str(), F_OK ) == -1) {
        		dprintf(1,"no such file: %s.\n",lastFiles_.at(i).toStdString().c_str());
        	}
            fp->newTabFromName (lastFiles_.at (i), -1, 0, multiple);
         }
    }

    return fp;
}

void FPsingleton::removeWin (FPwin *win)
{
    Wins.removeOne (win);
    win->deleteLater();
}

void FPsingleton::handleMessage (const QString& message)
{
    int lineNum = 0, posInLine = 0;
    long d = -1;
    bool openNewWin;
    const QStringList filesList = processInfo (message, d, lineNum, posInLine, &openNewWin);
    if (openNewWin)
    {
        newWin (filesList, lineNum, posInLine);
        return;
    }
    bool found = false;
        QRect sr;
        if (QScreen *pScreen = QApplication::primaryScreen())
            sr = pScreen->virtualGeometry();
        for (int i = 0; i < Wins.count(); ++i)
        {
           if (filesList.isEmpty())
           {
          	 Wins.at (i)->newTab();
           }
           else
           {
           	  bool multiple (filesList.count() > 1 || Wins.at (i)->isLoading());
           	  for (int j = 0; j < filesList.count(); ++j)
           	  {
				if(access( filesList.at(i).toStdString().c_str(), F_OK ) == -1) {
					continue;
				}
				
                  FPwin* fp = Wins.at( i );
                 	
                 	
                	int exists_tab_idx = fp->already_opened_idx( filesList.at (i) );
                 	
                 	if( exists_tab_idx != -2 )
      			{
        				fp->ui->tabWidget->setCurrentIndex(  exists_tab_idx  );
        				
        				fp->stealFocus();
      			}
        			
        			else
        			{
                        	fp->newTabFromName (filesList.at (j), lineNum, posInLine, multiple);
                  	}
            }
        }
        found = true;
        break;
    }
    if (!found)
    {
        newWin (filesList, lineNum, posInLine);
    }
}


}
