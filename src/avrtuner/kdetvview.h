/*
 *   Kdetv view class
 *   Copyright (C) 2002 George Staikos (staikos@kde.org)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef KDETV_VIEW_H
#define KDETV_VIEW_H

#define ASPECT_RATIO_NORMAL     1.3333  /* 4:3 aspect ratio  */
#define ASPECT_RATIO_WIDESCREEN 1.7777  /* 16:9 aspect ratio */
#define ASPECT_RATIO_NONE       0.0     /* no aspect ratio   */

// Aspect ratio algorithms:
#define AR_HEIGHT_TO_WIDTH 0
#define AR_WIDTH_TO_HEIGHT 1
#define AR_BEST_FIT        2

#include <qwidget.h>
#include <qcursor.h>

class QTimer;

/**
 * The TV screen widget.
 */
class KdetvView : public QWidget
{
    Q_OBJECT
public:
    KdetvView(QWidget *parent, const char *name=0);
    virtual ~KdetvView();

    /**
     * Set the aspect ratio to fix this view to.
     * @param aspect a double argument to set the aspect ratio.
     * @param mode sets the mode of the aspect ratio fix
     *
     * The aspect parameter should be one of the following:
     * ASPECT_RATIO_NONE, ASPECT_RATIO_NORMAL, or ASPECT_RATIO_WIDESCREEN
     * where the last one is not used at the moment.
     *
     * By choosing either AR_BEST_FIT, AR_H_TO_W or AR_W_TO_H as the
     * mode you can set the algorithm for the aspect ratio.
     */
    void setAspectRatio( double aspect, int mode = AR_BEST_FIT );

public slots:

    /**
     * Slot to set "Fix Aspect Ratio" on the fly.
     * @param fixed a bool value indicating whether or not to fix aspect ratio.
     * @param mode defines the algorithm/mode of the aspect ratio fix
     */
    void setFixedAspectRatio( bool fixed, int mode = AR_BEST_FIT );

protected:
    virtual bool eventFilter(QObject *o, QEvent *e);
    virtual void resizeEvent(QResizeEvent *e);

private:
    double aspectRatio;
    int aspectMode;

    void resizeWithFixedAR();
};

#endif

