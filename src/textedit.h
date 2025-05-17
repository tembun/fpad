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

#ifndef TEXTEDIT_H
#define TEXTEDIT_H

#include <QPlainTextEdit>
#include <QUrl>
#include <QDateTime>
#include <QElapsedTimer>

namespace fpad {
class TextEdit : public QPlainTextEdit
{
    Q_OBJECT

public:
    TextEdit (QWidget *parent = nullptr, int bgColorValue = 255);
    ~TextEdit();

    void setTextCursor (const QTextCursor &cursor)
    {
        QPlainTextEdit::setTextCursor (cursor);
        emit QPlainTextEdit::updateRequest (rect(), 1);
    }
    void sync_cursor();
    void setEditorFont (const QFont &f, bool setDefault = true);
    void adjustScrollbars();
    void lineNumberAreaPaintEvent (QPaintEvent *event);
    int lineNumberAreaWidth();
    QString getUrl (const int pos) const;
    QFont getDefaultFont() const {
        return font_;
    }

    QString getTextTab_() const {
        return textTab_;
    }
    void setTtextTab (int textTabSize) {
        textTab_ = textTab_.leftJustified (textTabSize, ' ', true);
    }

    QTextEdit::ExtraSelection currentLineSelection() {
        return currentLine_;
    }

    void setAutoIndentation (bool indent) {
        autoIndentation_ = indent;
    }
    bool getAutoIndentation() const {
        return autoIndentation_;
    }
    qint64 getSize() const {
        return size_;
    }
    void setSize (qint64 size) {
        size_ = size;
    }
    QDateTime getLastModified() const {
        return lastModified_;
    }
    void setLastModified (const QDateTime& m) {
        lastModified_ = m;
    }
    int getWordNumber() const {
        return wordNumber_;
    }
    void setWordNumber (int n) {
        wordNumber_ = n;
    }
    QString getSearchedText() const {
        return searchedText_;
    }
    void setSearchedText (const QString &text) {
        searchedText_ = text;
    }
    QString getReplaceTitle() const {
        return replaceTitle_;
    }
    void setReplaceTitle (const QString &title) {
        replaceTitle_ = title;
    }
    QString getFileName() const {
        return fileName_;
    }
    void setFileName (const QString &name) {
        fileName_ = name;
    }

    QString getEncoding() const {
        return encoding_;
    }
    void setEncoding (const QString &encoding) {
        encoding_ = encoding;
    }
    QList<QTextEdit::ExtraSelection> getGreenSel() const {
        return greenSel_;
    }
    void setGreenSel (QList<QTextEdit::ExtraSelection> sel) {
        greenSel_ = sel;
    }

    QList<QTextEdit::ExtraSelection> getRedSel() const {
        return redSel_;
    }
    void setRedSel (QList<QTextEdit::ExtraSelection> sel) {
        redSel_ = sel;
    }

    QList<QTextEdit::ExtraSelection> getBlueSel() const {
        return blueSel_;
    }

    bool isUneditable() const {
        return uneditable_;
    }
    void makeUneditable (bool readOnly) {
        uneditable_ = readOnly;
    }
    bool getSaveCursor() const {
        return saveCursor_;
    }
    void setSaveCursor (bool save) {
        saveCursor_ = save;
    }
    void forgetTxtCurHPos() {
        keepTxtCurHPos_ = false;
        txtCurHPos_ = -1;
    }

signals:
    void resized();
    void updateRect();

public slots:
    void copy();
    void cut();
    void undo();
    void redo();
    void paste();
    void selectAll();
    void insertPlainText (const QString &text);
    QTextCursor finding (const QString& str, const QTextCursor& start,
                         QTextDocument::FindFlags flags = QTextDocument::FindFlags(),
                         bool isRegex = false, const int end = 0) const;

protected:
    void keyPressEvent (QKeyEvent *event);
    void keyReleaseEvent (QKeyEvent *event);
    void wheelEvent (QWheelEvent *event);
    void resizeEvent (QResizeEvent *event);
    void timerEvent (QTimerEvent *event);
    void paintEvent (QPaintEvent *event);
    void showEvent (QShowEvent *event);
    void mouseMoveEvent (QMouseEvent *event);
    void mousePressEvent (QMouseEvent *event);
    void mouseReleaseEvent (QMouseEvent *event);
    void mouseDoubleClickEvent (QMouseEvent *event);
    bool event (QEvent *event);
    bool eventFilter (QObject *watched, QEvent *event);

private slots:
    void updateLineNumberAreaWidth (int newBlockCount);
    void updateLineNumberArea (const QRect &rect, int dy);
    void onUpdateRequesting (const QRect&, int dy);
    void onSelectionChanged();
    void scrollWithInertia();

private:
    QString computeIndentation (const QTextCursor &cur) const;
    QString remainingSpaces (const QString& spaceTab, const QTextCursor& cursor) const;
    QTextCursor backTabCursor(const QTextCursor& cursor, bool twoSpace) const;

    int prevAnchor_, prevPos_;
    QWidget *lineNumberArea_;
    QTextEdit::ExtraSelection currentLine_;
    QRect lastCurrentLine_;
    int widestDigit_;
    bool autoIndentation_;
    QColor separatorColor_;
    QColor lineHColor_;
    int resizeTimerId_, selectionTimerId_;
    QPoint pressPoint_;
    QPoint selectionPressPoint_;
    QFont font_;
    QString textTab_;
    QElapsedTimer tripleClickTimer_;
    bool keepTxtCurHPos_;
    int txtCurHPos_;
    qint64 size_;
    QDateTime lastModified_;
    int wordNumber_;
    QString searchedText_;
    QString replaceTitle_;
    QString fileName_;
    QString encoding_;
    QList<QTextEdit::ExtraSelection> greenSel_;
    QList<QTextEdit::ExtraSelection> blueSel_;
    QList<QTextEdit::ExtraSelection> redSel_;
    bool uneditable_;
    bool saveCursor_;
    struct scrollData {
      int delta;
      int leftSteps;
      int totalSteps;
    };
    QList<scrollData> queuedScrollSteps_;
    QTimer *scrollTimer_;
};
class LineNumberArea : public QWidget
{
    Q_OBJECT

public:
    LineNumberArea (TextEdit *Editor) : QWidget (Editor) {
        editor = Editor;
    }

    QSize sizeHint() const {
        return QSize (editor->lineNumberAreaWidth(), 0);
    }

protected:
    void paintEvent (QPaintEvent *event) {
        editor->lineNumberAreaPaintEvent (event);
    }

private:
    TextEdit *editor;
};

}

#endif
