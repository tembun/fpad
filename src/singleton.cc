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

#include "ui_fp.h"
#include <QDir>
#include <QLocalSocket>
#include <QStandardPaths>
#include <QTextBlock>
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

QString FPsingleton::makeRealPath(QString pwd, QString path)
{
	return QDir(pwd).absoluteFilePath(QDir::cleanPath(path));
}

bool FPsingleton::cursorInfo (QString pwd, QString path, int& lineNum, int& posInLine, QString& realPath)
{
    QString lineNumSep = ":";
    QString lineNumEndMarker = "+";
    QString lineNumPosSep = ":";
    int lineNumSepIdx = path.indexOf(lineNumSep);
    realPath = makeRealPath(pwd, path);
    if (lineNumSepIdx == -1)
    	return false;
    QString _realPath = path.left(lineNumSepIdx);
    QString lineNumStr = path.right(path.length() - lineNumSepIdx - 1);
    if (lineNumStr == lineNumEndMarker) {
    	lineNum = -2;
    	realPath = makeRealPath(pwd, _realPath);
    	return true;
    }
    int _lineNum, _posInLine;
    bool lineNumOk, posInLineOk;
    QStringList numList = lineNumStr.split(lineNumPosSep);
    if (numList.count() == 2) {
    	_lineNum = numList.at(0).toInt(&lineNumOk);
    	_posInLine = numList.at(1).toInt(&posInLineOk);
    	if (!posInLineOk)
    		return false;
    	posInLine = _posInLine;
    }
    else
    	_lineNum = lineNumStr.toInt(&lineNumOk);
    if (!lineNumOk || _lineNum <= 0)
    	return false;
    lineNum = _lineNum + 1;
    realPath = makeRealPath(pwd, _realPath);
    return true;
}

QStringList FPsingleton::processInfo (const QString& message,
                                      long &desktop, bool *newWindow)
{
    desktop = -1;
    QStringList sl = message.split ("\n\r");
    QString pwd = sl.at(1);
    if (sl.count() < 3)
    {
        *newWindow = true;
        return QStringList();
    }
    desktop = sl.at (0).toInt();
    sl.removeFirst();
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
    if (sl.at (0) == "--win" || sl.at (0) == "-w")
    {
        *newWindow = true;
        sl.removeFirst();
    }
    QStringList filesList;
    for (const auto &path : qAsConst (sl))
    {
        if (!path.isEmpty())
        {
            int lineNum = 0, posInLine = 0;
            QString realPath;
            cursorInfo(pwd, path, lineNum, posInLine, realPath);
            filesList << path;
        }
    }
    return filesList;
}

void FPsingleton::firstWin (const QString& message)
{
    long d = -1;
    bool openNewWin;
    const QStringList filesList = processInfo(message, d, &openNewWin);
    QString pwd = message.split ("\n\r").at(1);
    newWin(pwd, filesList);
    lastFiles_ = QStringList();
}

bool
FPsingleton::check_file_exists(QString filename) {
	bool res = (access(filename.toStdString().c_str(), F_OK) != -1);
	if (!res) {
		QTextStream err(stderr);
		err << "File doesn't exist: " << filename << Qt::endl;
	}
	return res;
}

FPwin* FPsingleton::newWin (QString pwd, const QStringList& filesList)
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
	bool multiple = false;
	if (!filesList.isEmpty())
		files = &filesList;
	else if (!lastFiles_.isEmpty())
		files = &lastFiles_;
	else
		goto end;
	multiple = files->count() > 1 || fp->isLoading();
	for (int i = 0; i < files->count(); ++i){
        	QString filename = files->at(i);
        	int lineNum = 0, posInLine = 0;
        	QString realPath;
        	cursorInfo(pwd, filename, lineNum, posInLine, realPath);
        	if (!check_file_exists(realPath))
        		continue;
        	fp->newTabFromName(realPath, lineNum, posInLine, multiple);
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
FPsingleton::switchToExistingTab(FPwin* fpw, int idx, int lineNum, int posInLine,
    bool hasCursorInfo)
{
	TabPage *thisTabPage = qobject_cast<TabPage*>(fpw->ui->tabWidget->widget(idx));
	TextEdit *textEdit = thisTabPage->textEdit();
	fpw->ui->tabWidget->setCurrentIndex(idx);
	if (hasCursorInfo) {
		QTextCursor curs = textEdit->textCursor();
		bool isLastLine = lineNum == -2 || lineNum > textEdit->document()->blockCount();
		if (isLastLine)
			curs.movePosition(QTextCursor::End);
		else {
			QTextBlock block = textEdit->document()->findBlockByNumber(lineNum - 2);
			int pos = block.position();
			if (posInLine < 0)
				curs.movePosition(QTextCursor::EndOfLine, QTextCursor::MoveAnchor);
			else
				curs.setPosition(pos + posInLine, QTextCursor::MoveAnchor);
		}
		textEdit->setTextCursor(curs);
	}
	/*
	 * Because we don't actually open a _new_
	 * tab, the window, by default, will not
	 * be focused, so we force this action.
	 */
	fpw->stealFocus();
}

void
FPsingleton::handleMessage(const QString& message)
{
	long d = -1;
	bool openNewWin;
	FPwin* fpw = Wins.at(0);
	const QStringList filesList = processInfo(message, d, &openNewWin);
	QString pwd = message.split ("\n\r").at(1);
	if (openNewWin) {
		newWin(pwd, filesList);
		return;
	}
        if (filesList.isEmpty())
		fpw->newTab();
        else {
		bool multiple (filesList.count() > 1 || fpw->isLoading());
		for (int i = 0; i < filesList.count(); ++i) {
			QString filename = filesList.at(i);
			int lineNum = 0, posInLine = 0;
        		QString realPath;
        		bool hasCursorInfo = cursorInfo(pwd, filename, lineNum, posInLine, realPath);
			if (!check_file_exists(realPath))
				continue;
			bool modified;
			/*
			 * If this file is already opened in a tab, switch
			 * to this tab (make it active).
			 */
			int exists_idx = fpw->already_opened_idx(realPath, modified);
			if (exists_idx != -2 && !modified)
				switchToExistingTab(fpw, exists_idx, lineNum, posInLine, hasCursorInfo);
			/*
			 * Otherwise, open a new tab with this file.
			 */
			else
				fpw->newTabFromName(realPath, lineNum,
				    posInLine, multiple);
		}
	}
}


}
