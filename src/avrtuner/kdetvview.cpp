/*
 *   Kdetv view class
 *   Copyright (C) 2002 George Staikos (staikos@kde.org)
 *                 2004 Dirk Ziegelmeier <dziegel@gmx.de>
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

#include <qtimer.h>

#include <kcursor.h>
#include <kdebug.h>

#include "kdetvview.h"

#include <qapplication.h>
#include <qpaintdevice.h>
#include <qpaintdevicemetrics.h>


KdetvView::KdetvView(QWidget *parent, const char *name)
    : QWidget(parent,name ? name : "kdetv_view")
{
    setWFlags(WResizeNoErase | WRepaintNoErase);
    setFocusPolicy(StrongFocus);
    KCursor::setAutoHideCursor( this, true );
    KCursor::setHideCursorDelay( 500 );
    setFocus();
    topLevelWidget()->installEventFilter(this);
}

KdetvView::~KdetvView()
{
}

void KdetvView::resizeEvent(QResizeEvent *e)
{
    //  kdDebug() << "KdetvView::resizeEvent()" << endl;
    QWidget::resizeEvent(e);

    // If the aspect ratio is set, use it. Otherwise, do nothing.
    if ( aspectRatio != ASPECT_RATIO_NONE )
        resizeWithFixedAR();
}

bool KdetvView::eventFilter(QObject *, QEvent *e)
{
    if (e->type() == QEvent::Move)
    KCursor::autoHideEventFilter(this, e);
    return false;
}

void KdetvView::setAspectRatio( double aspect, int mode )
{
    aspectRatio = aspect;
    aspectMode = mode;
}

void KdetvView::resizeWithFixedAR() {
    int tmp;
    int am = aspectMode;
    QPaintDeviceMetrics m(QApplication::desktop()->screen((QApplication::desktop()->screenNumber(this))));

    /*
      Aspect ratio calculation (QPaintDeviceMetrics calls):

                  RESOLUTIONx      width()
      WIDTHpix = ------------- = -----------
                  WIDTHx,phy      widthMM()

                   RESOLUTIONy      height()
      HEIGHTpix = ------------- = ------------
                   HEIGHTy,phy     heightMM()


                   width() * heightMM()
      ASPECTpix = ----------------------
                   height() * widthMM()


                       desiredASPECT
      ASPECTlogical = ---------------
                         ASPECTpix
    */

    float aspectPix = ((float)m.width() * (float)m.heightMM()) / ((float)m.height() * (float)m.widthMM());
    float ar        = aspectRatio / aspectPix;

    if (am == AR_BEST_FIT) {
        if( (int)(height() * ar) <= width() ) {
            am = AR_WIDTH_TO_HEIGHT;
        } else {
            am = AR_HEIGHT_TO_WIDTH;
        }
    }

    switch( am ) {
    case AR_HEIGHT_TO_WIDTH:
        // -1 for float->int rounding problems
        tmp = ((height() - (int)(width() / ar)) / 2) - 1;
        if (tmp > 0) move(0, tmp);
        resize( width(), (int)(width() / ar) );
        break;
    case AR_WIDTH_TO_HEIGHT:
        // -1 for float->int rounding problems
        tmp = ((width() - (int)(height() * ar)) / 2) - 1;
        if (tmp > 0) move(tmp, 0);
        resize( (int)(height() * ar), height() );
        break;
    default:
        kdWarning() << "KdetvView::resizeWithFixedAR(). AR mode unknown."
                    << "We should never reach this point!" << endl;
        break;
    }
}

void KdetvView::setFixedAspectRatio( bool fixed, int mode )
{
    if ( !fixed ) {
        setAspectRatio( ASPECT_RATIO_NONE, mode );
        resize( width(), height() );
    } else {
        setAspectRatio( ASPECT_RATIO_NORMAL, mode );
        resizeWithFixedAR();
    }
}

#include "kdetvview.moc"
