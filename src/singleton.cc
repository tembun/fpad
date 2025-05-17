/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2014-2020 <tsujan2000@gmail.com>
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

#include "singleton.h"

namespace fpad {

FPsingleton::FPsingleton (int &argc, char **argv, bool standalone) : QApplication (argc, argv)
{
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
    uniqueKey_ = "fpad-" + QString::fromLocal8Bit (user) + QLatin1Char ('-')
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
    if (!localSocket || !localSocket->waitForReadyRead (timeout_))
    {
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
        return false;
    }

    localSocket.write (message.toUtf8());
    if (!localSocket.waitForBytesWritten (timeout_))
    {
        socketFailure_ = true;
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
    return filesList;
}

void FPsingleton::firstWin (const QString& message)
{
    int lineNum = 0, posInLine = 0;
    long d = -1;
    bool openNewWin;
    const QStringList filesList = processInfo (message, d, lineNum, posInLine,
        &openNewWin);
    newWin(filesList, lineNum, posInLine);
    lastFiles_ = QStringList();
}

bool
FPsingleton::check_file_exists(QString filename) {
	bool res = (access(filename.toStdString().c_str(), F_OK) != -1);
	if (!res) {
		QTextStream err(stderr);
		err << "[fpad]: File doesn't exist: " <<
		    filename << Qt::endl;
	}
	return res;
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
    Wins.append(fp);

	const QStringList* files = NULL;
	int line_num = 0, pos_in_line = 0;
	bool multiple = false;
	
	if (!filesList.isEmpty()) {
		files = &filesList;
		line_num = lineNum;
		pos_in_line = posInLine;
	}
	else if (!lastFiles_.isEmpty()) {
		files = &lastFiles_;
		line_num = -1;
		pos_in_line = 0;
	}
	else
		goto end;
	
	multiple = files->count() > 1 || fp->isLoading();
	for (int i = 0; i < files->count(); ++i){
        	QString filename = files->at(i);
        	if (!check_file_exists(filename))
        		continue;
        	fp->newTabFromName(filename, line_num, pos_in_line, multiple);
        }

end:
    return (fp);
}

void FPsingleton::removeWin (FPwin *win)
{
    Wins.removeOne (win);
    win->deleteLater();
}

void
FPsingleton::handleMessage(const QString& message)
{
	int lineNum = 0, posInLine = 0;
	long d = -1;
	bool openNewWin;
	FPwin* fpw = Wins.at(0);
	
	const QStringList filesList = processInfo(message, d, lineNum,
	    posInLine, &openNewWin);
	
	if (openNewWin) {
		newWin(filesList, lineNum, posInLine);
		return;
	}
	
        if (filesList.isEmpty())
		fpw->newTab();
        else {
		bool multiple (filesList.count() > 1 || fpw->isLoading());
		
		for (int i = 0; i < filesList.count(); ++i) {
			QString filename = filesList.at(i);
			if (!check_file_exists(filename))
				continue;
			
			/*
			 * If this file is already opened in a tab, switch
			 * to this tab (make it active).
			 */
			int exists_idx = fpw->already_opened_idx(filename);
			if (exists_idx != -2) {
				fpw->ui->tabWidget->setCurrentIndex(exists_idx);
				/*
				 * Because we don't actually open a _new_
				 * tab, the window, by default, will not
				 * be focused, so we force this action.
				 */
				fpw->stealFocus();
			}
			/*
			 * Otherwise, open a new tab with this file.
			 */
			else
				fpw->newTabFromName(filename, lineNum,
				    posInLine, multiple);
		}
	}
}


}
