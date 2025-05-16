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

#ifndef WARNINGBAR_H
#define WARNINGBAR_H

#include <QPointer>
#include <QEvent>
#include <QMouseEvent>
#include <QTimer>
#include <QGridLayout>
#include <QPalette>
#include <QLabel>
#include <QPropertyAnimation>

#define DURATION 150

namespace fpad {

class WarningBar : public QWidget
{
    Q_OBJECT
public:
    WarningBar (const QString& message, const int verticalOffset = 0, QWidget *parent = nullptr) : QWidget (parent) {
        int anotherBar (false);
        if (parent)
        { // show only one warning bar at a time
            const QList<WarningBar*> warningBars = parent->findChildren<WarningBar*>();
            for (WarningBar *wb : warningBars)
            {
                if (wb != this)
                {
                    wb->closeBar();
                    anotherBar = true;
                }
            }
        }

        message_ = message;
        vOffset_ = verticalOffset;
        isClosing_ = false;

        /* make it like a translucent layer */
        setAutoFillBackground (true);
        QPalette p = palette();
        p.setColor (foregroundRole(), Qt::white);
        p.setColor (backgroundRole(), Qt::black);
        setPalette (p);

        grid_ = new QGridLayout;
        grid_->setContentsMargins (5, 0, 5 ,5); // the top margin is added when setting the geometry
        /* use a spacer to compress the label vertically */
        QSpacerItem *spacer = new QSpacerItem (0, 0, QSizePolicy::Minimum, QSizePolicy::MinimumExpanding);
        grid_->addItem (spacer, 0, 0);
        /* add the label */
        QLabel *warningLabel = new QLabel (message);
        warningLabel->setAttribute (Qt::WA_TransparentForMouseEvents, true); // not needed
        warningLabel->setWordWrap (true);
        grid_->addWidget (warningLabel, 1, 0);
        setLayout (grid_);

        if (parent)
        { // compress the bar vertically and show it with animation
            QTimer::singleShot (anotherBar ? DURATION + 10 : 0, this, [=]() {
                parent->installEventFilter (this);
                int h = grid_->minimumHeightForWidth (parent->width()) + grid_->contentsMargins().bottom();
                QRect g (0, parent->height() - h - vOffset_, parent->width(), h);
                setGeometry (g);

                animation_ = new QPropertyAnimation (this, "geometry", this);
                animation_->setEasingCurve (QEasingCurve::Linear);
                animation_->setDuration (DURATION);
                animation_->setStartValue (QRect (0, parent->height() - vOffset_, parent->width(), 0));
                animation_->setEndValue (g);
                animation_->start();
                show();
            });
        }
        else show();

        /* auto-close after 10 seconds */
        QTimer::singleShot (10000, this, &WarningBar::closeBar);

    }

    bool eventFilter (QObject *o, QEvent *e) {
        if (e->type() == QEvent::Resize)
        {
            if (QWidget *w = qobject_cast<QWidget*>(o))
            {
                if (w == parentWidget())
                { // compress the bar as far as its text is shown completely
                    int h = grid_->minimumHeightForWidth (w->width()) + grid_->contentsMargins().bottom();
                    setGeometry (QRect (0, w->height() - h - vOffset_, w->width(), h));
                }
            }
        }
        return false;
    }

    QString getMessage() const {
        return message_;
    }

    bool isClosing() const {
        return isClosing_;
    }

public slots:
    void closeBar() {
        if (animation_ && parentWidget())
        {
            if (!isClosing_)
            {
                isClosing_ = true;
                parentWidget()->removeEventFilter (this); // no movement on closing
                animation_->stop();
                animation_->setStartValue (geometry());
                animation_->setEndValue (QRect (0, parentWidget()->height() - vOffset_, parentWidget()->width(), 0));
                animation_->start();
                connect (animation_, &QAbstractAnimation::finished, this, &QObject::deleteLater);
            }
        }
        else delete this;
    }

protected:
    void mousePressEvent (QMouseEvent *event) {
        QWidget::mousePressEvent (event);
        QTimer::singleShot (0, this, &WarningBar::closeBar);
    }

private:
    QString message_;
    int vOffset_;
    bool isClosing_;
    QGridLayout *grid_;
    QPointer<QPropertyAnimation> animation_;

};

}

#endif // WARNINGBAR_H
