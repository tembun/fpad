/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2019 <tsujan2000@gmail.com>
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

#include "highlighter.h"

namespace FeatherPad {

/* NOTE: We cheat and include "q", "qq", "qw", "qx" and "qr" here to have a simpler code.
         Moreover, the => operator is excluded. */
static const QRegularExpression rExp ("/|\\b(?<!(@|#|\\$))(m|qr|q|qq|qw|qx|qr|(?<!-)s|y|tr)\\s*(?!\\=>)[^\\w\\}\\)\\]>\\s]");

// This is only for the start.
bool Highlighter::isEscapedPerlRegex (const QString &text, const int pos)
{
    if (pos < 0) return false;

    if (format (pos) == quoteFormat || format (pos) == altQuoteFormat
        || format (pos) == commentFormat || format (pos) == urlFormat)
    {
        return true;
    }

    static QRegularExpression perlKeys;

    int i = pos - 1;
    if (i < 0) return false;

    if (text.at (pos) != '/')
    {
        if (format (i) == regexFormat)
            return true;
        return false;
    }

    /* FIXME: Why? */
    int slashes = 0;
    while (i >= 0 && format (i) != regexFormat && text.at (i) == '/')
    {
        -- i;
        ++ slashes;
    }
    if (slashes % 2 != 0)
        return true;

    i = pos - 1;
    while (i >= 0 && (text.at (i) == ' ' || text.at (i) == '\t'))
        --i;
    if (i >= 0)
    {
        QChar ch = text.at (i);
        if (format (i) != regexFormat && (ch.isLetterOrNumber() || ch == '_'
                                          || ch == ')' || ch == ']' || ch == '}'
                                          || ch == '#'|| ch == '$'|| ch == '@'))
        { // a regex isn't escaped if it follows a Perl keyword
            if (perlKeys.pattern().isEmpty())
                perlKeys.setPattern (keywords (progLan).join ('|'));
            int len = qMin (12, i + 1);
            QString str = text.mid (i - len + 1, len);
            int j;
            QRegularExpressionMatch keyMatch;
            if ((j = str.lastIndexOf (perlKeys, -1, &keyMatch)) > -1 && j + keyMatch.capturedLength() == len)
                return false;
            return true;
        }
    }

    return false;
}
/*************************/
// Takes care of open braces too.
int Highlighter::findDelimiter (const QString &text, const int index,
                                const QRegularExpression &delimExp, int &capturedLength) const
{
    int i = qMax (index, 0);
    const QString pattern = delimExp.pattern();
    if (pattern.startsWith ("\\")
        && (pattern.endsWith (")") || pattern.endsWith ("}")
            || pattern.endsWith ("]") || pattern.endsWith (">")))
    { // the end delimiter should be found
        int N = 1;
        if (index < 0)
        {
            QTextBlock prevBlock = currentBlock().previous();
            if (prevBlock.isValid())
            {
                if (TextBlockData *prevData = static_cast<TextBlockData *>(prevBlock.userData()))
                    N = prevData->openNests();
            }
        }
        QChar endBrace = pattern.at (pattern.size() - 1);
        QChar startBrace = endBrace == ')' ? '(' : endBrace == '}' ? '{'
                           : endBrace == ']' ? '[' : '<';
        const int L = text.length();
        while (N > 0 && i < L)
        {
            QChar c = text.at (i);
            if (c == endBrace) -- N;
            else if (c == startBrace) ++ N;
            ++ i;
        }
        if (N == 0)
        {
            if (i == 0) // doesn't happen
            {
                int res = text.indexOf (delimExp, 0);
                capturedLength = res == -1 ? 0 : 1;
                return res;
            }
            else
            {
                capturedLength = 1;
                return i - 1;
            }
        }
        else
        {
            static_cast<TextBlockData *>(currentBlock().userData())->insertNestInfo (N);
            capturedLength = 0;
            return -1;
        }
    }
    else
    {
        QRegularExpressionMatch match;
        int res = text.indexOf (delimExp, i, &match);
        capturedLength = match.capturedLength();
        return res;
    }
}
/*************************/
static inline QString getEndDelimiter (const QString &brace)
{
    if (brace == "(")
        return ")";
    if (brace == "{")
        return "}";
    if (brace == "[")
        return "]";
    if (brace == "<")
        return ">";
    return brace;
}
/*************************/
static inline QString startBrace (QString &brace)
{
    if (brace == ")")
        return  "(";
    if (brace == "}")
        return "{";
    if (brace == "]")
        return "[";
    if (brace == ">")
        return "<";
    return QString();
}
/*************************/
bool Highlighter::isInsidePerlRegex (const QString &text, const int index)
{
    if (index < 0) return false;

    int pos = -1;

    if (format (index) == regexFormat)
        return true;
    if (TextBlockData *data = static_cast<TextBlockData *>(currentBlock().userData()))
    {
        pos = data->lastFormattedRegex() - 1;
        if (index <= pos) return false;
    }

    QRegularExpression exp;
    bool res = false;
    bool searchedToReplace = false;
    int N;

    if (pos == -1)
    {
        if (previousBlockState() != regexState && previousBlockState() != regexSearchState)
        {
            exp = rExp;
            N = 0;
        }
        else
        {
            QString delimStr;
            QTextBlock prevBlock = currentBlock().previous();
            if (prevBlock.isValid())
            {
                if (TextBlockData *prevData = static_cast<TextBlockData *>(prevBlock.userData()))
                    delimStr = prevData->labelInfo();
            }
            if (previousBlockState() == regexState)
            {
                N = 1;
                if (delimStr.isEmpty())
                    exp.setPattern ("/");
                else
                {
                    QString sb = startBrace (delimStr);
                    if (!sb.isEmpty())
                    {
                        pos = text.indexOf (QRegularExpression ("\\" + sb), 0);
                        if (pos > -1)
                            setFormat (0, pos + 1, regexFormat);
                        else
                        {
                            setFormat (0, text.length(), regexFormat);
                            return true;
                        }
                        exp.setPattern ("\\" + delimStr);
                    }
                    else
                    {
                        exp.setPattern ("\\" + getEndDelimiter (delimStr));
                        pos = -2; // to know that the search in continued from the previous line
                    }
                }
            }
            else// if (previousBlockState() == regexSearchState) // for "s" and "tr"
            {
                N = 0;
                searchedToReplace = true;
                exp.setPattern ("\\" + getEndDelimiter (delimStr));
            }
            res = true;
        }
    }
    else // a new search from the last position
    {
        exp = rExp;
        N = 0;
    }

    int nxtPos, capturedLength;
    while ((nxtPos = findDelimiter (text, pos + 1, exp, capturedLength)) >= 0)
    {
        /* skip formatted comments and quotes */
        QTextCharFormat fi = format (nxtPos);
        if (N % 2 == 0
            && (fi == commentFormat || fi == urlFormat
                || fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat))
        {
            pos = nxtPos +capturedLength - 1;
            continue;
        }

        ++N;
        if (N % 2 == 0
            ? isEscapedRegexEndSign (text, qMax (0, pos + 1), nxtPos) // an escaped end sign
            : (capturedLength > 1
              ? isEscapedPerlRegex (text, nxtPos) // an escaped start sign
              : (isEscapedRegexEndSign (text, qMax (0, pos + 1), nxtPos) // an escaped middle sign
                 || (exp == rExp
                     && isEscapedPerlRegex (text, nxtPos))))) // an escaped start slash
        {
            if (res)
            {
                pos = qMax (pos, 0);
                setFormat (pos, nxtPos - pos + capturedLength, regexFormat);
            }
            --N;
            pos = nxtPos + capturedLength - 1;
            continue;
        }

        if (N % 2 == 0 || searchedToReplace)
        {
            if (TextBlockData *data = static_cast<TextBlockData *>(currentBlock().userData()))
                data->insertLastFormattedRegex (nxtPos + capturedLength);
            pos = qMax (pos, 0);
            setFormat (pos, nxtPos - pos + capturedLength, regexFormat);
        }

        if (index <= nxtPos) // they may be equal, as when "//" is at the end of "/...//"
        {
            if (N % 2 == 0)
            {
                res = true;
                break;
            }
            else if (!searchedToReplace)
            {
                res = false;
                break;
            }
            // otherwise, the formatting should continue until the replacement is done
        }

        /* NOTE: We should set "res" below because "pos" might be negative next time. */
        searchedToReplace = false;
        if (N % 2 == 0)
        {
            exp = rExp;
            res = false;
        }
        else
        {
            res = true;
            if (capturedLength > 1)
            {
                exp.setPattern ("\\" + getEndDelimiter (QString (text.at (nxtPos + capturedLength - 1))));
                if (text.at (nxtPos) == 's' || text.at (nxtPos) == 't' || text.at (nxtPos) == 'y')
                {
                    --N;
                    searchedToReplace = true;
                }
            }
            else
            {
                QString d (QString (text.at (nxtPos)));
                exp.setPattern ("\\" + d);
                QString sb = startBrace (d);
                if (!sb.isEmpty())
                {
                    pos = text.indexOf (QRegularExpression ("\\" + sb), nxtPos + 1);
                    if (pos > -1)
                    {
                        setFormat (nxtPos, pos - nxtPos + 1, regexFormat);
                        continue;
                    }
                    else
                    {
                        setFormat (nxtPos, text.length() - nxtPos, regexFormat);
                        return true;
                    }
                }
            }
        }

        pos = nxtPos + capturedLength - 1;
    }

    return res;
}
/*************************/
void Highlighter::multiLinePerlRegex(const QString &text)
{
    int startIndex = 0;
    QRegularExpressionMatch startMatch;
    QRegularExpressionMatch endMatch;
    QRegularExpression endExp;
    QTextCharFormat fi;
    QString delimStr;

    int prevState = previousBlockState();
    if (prevState == regexState || prevState == regexSearchState)
    {
        QTextBlock prevBlock = currentBlock().previous();
        if (prevBlock.isValid())
        {
            if (TextBlockData *prevData = static_cast<TextBlockData *>(prevBlock.userData()))
                delimStr = prevData->labelInfo();
        }
        if (prevState == regexState)
        {
            QString sb = startBrace (delimStr);
            if (!sb.isEmpty())
            {
                startIndex = text.indexOf (QRegularExpression ("\\" + sb), 0, &startMatch);
                if (startIndex == -1)
                {
                    setCurrentBlockState (regexState);
                    setFormat (0, text.length(), regexFormat);
                    static_cast<TextBlockData *>(currentBlock().userData())->insertInfo (delimStr);
                    return;
                }
                setFormat (0, startIndex + startMatch.capturedLength(), regexFormat);
                delimStr = sb;
            }
        }
    }
    else
    {
        startIndex = text.indexOf (rExp, startIndex, &startMatch);
        /* skip comments and quotations (all formatted to this point) */
        fi = format (startIndex);
        while (startIndex >= 0
               && (isEscapedPerlRegex (text, startIndex)
                   || fi == commentFormat || fi == urlFormat
                   || fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat))
        {
            startIndex = text.indexOf (rExp, startIndex + startMatch.capturedLength(), &startMatch);
            fi = format (startIndex);
        }
        if (startIndex >= 0 && startMatch.capturedLength() > 1)
            delimStr = QString (text.at (startIndex + startMatch.capturedLength() - 1));
    }

    while (startIndex >= 0)
    {
        bool continued (startIndex == 0
                        && (prevState == regexState || prevState == regexSearchState));
        if (continued)
        {
            if (prevState == regexState)
            {
                if (delimStr.isEmpty())
                    endExp.setPattern ("/");
                else
                    endExp.setPattern ("\\" + getEndDelimiter (delimStr));
            }
            else
                endExp.setPattern ("\\" + getEndDelimiter (delimStr));
        }
        else
        {
            if (delimStr.isEmpty())
                endExp.setPattern ("/");
            else
                endExp.setPattern ("\\" + getEndDelimiter (delimStr));
        }

        int endLength;
        int endIndex = findDelimiter (text,
                                      continued
                                          ? -1 // to know that the search is continued from the previous line
                                          : startIndex + startMatch.capturedLength(),
                                      endExp, endLength);

        int indx = continued ? 0 : startIndex + startMatch.capturedLength();
        while (isEscapedRegexEndSign (text, indx, endIndex))
        {
            indx = endIndex + 1;
            endIndex = findDelimiter (text, indx, endExp, endLength);
        }

        int len;
        int keywordLength = qMax (startMatch.capturedLength() - 1, 0);
        if (endIndex == -1)
        {
            if (!continued)
            {
                if (startMatch.capturedLength() > 1
                    && (text.at (startIndex) == 's' || text.at (startIndex) == 't' || text.at (startIndex) == 'y'))
                {
                    setCurrentBlockState (regexSearchState);
                }
                else
                    setCurrentBlockState (regexState);
                static_cast<TextBlockData *>(currentBlock().userData())->insertInfo (delimStr);
            }
            else
                setCurrentBlockState (prevState);

            if (!delimStr.isEmpty())
            {
                static_cast<TextBlockData *>(currentBlock().userData())->insertInfo (delimStr);
                /* NOTE: The next block will be rehighlighted at highlightBlock()
                         (-> multiLineRegex (text, 0);) if the delimiter is changed. */
            }
            len = text.length() - startIndex;
        }
        else
        {
            len = endIndex - startIndex + endLength;

            if (continued)
            {
                if (prevState == regexSearchState)
                {
                    setFormat (startIndex + keywordLength, len - keywordLength, regexFormat);
                    QString ed = getEndDelimiter (delimStr);
                    if (ed != delimStr) // regex replacement with braces
                    {
                        startIndex = text.indexOf (QRegularExpression ("\\" + delimStr), endIndex + 1, &startMatch);
                        if (startIndex == -1)
                        {
                            setFormat (endIndex + 1, text.length() - endIndex - 1, regexFormat);
                            setCurrentBlockState (regexState);
                            static_cast<TextBlockData *>(currentBlock().userData())->insertInfo (ed);
                            return;
                        }
                        setFormat (endIndex + 1, startIndex + startMatch.capturedLength() - endIndex - 1, regexFormat);
                    }
                    else
                    {
                        if (endIndex == text.length() - 1)
                            break;
                        startMatch = QRegularExpressionMatch();
                        startIndex = endIndex + 1;
                    }
                    continue;
                }
            }
            else
            {
                if (startMatch.capturedLength() > 1
                    && (text.at (startIndex) == 's' || text.at (startIndex) == 't' || text.at (startIndex) == 'y'))
                {
                    setFormat (startIndex + keywordLength, len - keywordLength, regexFormat);
                    QString ed = getEndDelimiter (delimStr);
                    if (ed != delimStr) // regex replacement with braces
                    {
                        startIndex = text.indexOf (QRegularExpression ("\\" + delimStr), endIndex + 1, &startMatch);
                        if (startIndex == -1)
                        {
                            setFormat (endIndex + 1, text.length() - endIndex - 1, regexFormat);
                            setCurrentBlockState (regexState);
                            static_cast<TextBlockData *>(currentBlock().userData())->insertInfo (ed);
                            return;
                        }
                        setFormat (endIndex + 1, startIndex + startMatch.capturedLength() - endIndex - 1, regexFormat);
                    }
                    else
                    {
                        if (endIndex == text.length() - 1)
                            break;
                        startMatch = QRegularExpressionMatch();
                        startIndex = endIndex + 1;
                    }
                    continue;
                }
            }
        }
        setFormat (startIndex + keywordLength, len - keywordLength, regexFormat);

        startIndex = text.indexOf (rExp, startIndex + len, &startMatch);

        /* skip comments and quotations again */
        fi = format (startIndex);
        while (startIndex >= 0
               && (isEscapedPerlRegex (text, startIndex)
                   || fi == commentFormat || fi == urlFormat
                   || fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat))
        {
            startIndex = text.indexOf (rExp, startIndex + startMatch.capturedLength(), &startMatch);
            fi = format (startIndex);
        }
        if (startIndex >= 0 && startMatch.capturedLength() > 1)
            delimStr = QString (text.at (startIndex + startMatch.capturedLength() - 1));
        else
            delimStr = QString();
    }
}

}
