// $Id$

#include "DockableWidgetLayout.h"
#include "DockableWidget.h"
#include <QLayoutItem>
#include <QtGlobal>
#include <QMap>


static const int SNAP_DISTANCE = 16;

DockableWidgetLayout::DockableWidgetLayout(QWidget *parent, int margin, int spacing)
	: QLayout(parent)
{
	setMargin(margin);
	setSpacing(spacing);
	minWidth = minHeight = 0;
}

DockableWidgetLayout::DockableWidgetLayout(int spacing)
{
	setSpacing(spacing);
}

DockableWidgetLayout::~DockableWidgetLayout()
{
	while ( dockedWidgets.size() ) {
		DockInfo *info = dockedWidgets.takeAt(0);
		delete info->item;
		delete info->widget;
		delete info;
	}
}

void DockableWidgetLayout::addItem( QLayoutItem *item )
{
	addItem( item, -1 );
}

void DockableWidgetLayout::addItem( QLayoutItem *item, int index, DockSide side, int dist, int w, int h )
{
	DockInfo *info = new DockInfo;
	
	info->item = item;
	info->widget = qobject_cast<DockableWidget*>(item->widget());
	info->dockSide = side;
	info->dockDistance = dist;
	info->left = 0;
	info->top = 0;
	info->width = -1;
	info->height = -1;
	info->useHintWidth = true;
	info->useHintHeight = true;
	info->distExtra = 0;
	
	if( info->widget->sizePolicy().horizontalPolicy() != QSizePolicy::Fixed && w > 0 ) {
		info->width = w;
		info->useHintWidth = false;
	}

	if( info->widget->sizePolicy().verticalPolicy() != QSizePolicy::Fixed && h > 0 ) {
		info->height = h;
		info->useHintHeight = false;
	}

	// first widget is the resize widget, set initial size
	if( !dockedWidgets.size() ) {
		if( info->width == -1 )
			info->width = item->sizeHint().width();
		if( info->height == -1 )
			info->height = item->sizeHint().height();
		info->useHintWidth = false;
		info->useHintHeight = false;
	}

	if( index > -1 && index < dockedWidgets.size() )
		dockedWidgets.insert( index, info );
	else
		dockedWidgets.append( info );

	// recalculate size limits		
	minValid = false;
}

void DockableWidgetLayout::addWidget( DockableWidget *widget, const QRect& rect )
{
	int index;
	DockSide side;
	QRect r(rect);
	if( insertLocation( r, index, side, widget->sizePolicy() ) ) {
		DockInfo *d = dockedWidgets.at(0);
		int dist;
		switch( side ) {
			case TOP:
			case BOTTOM:
				dist = r.left() - d->left;
				break;
			case LEFT:
			case RIGHT:
				dist = r.top() - d->top;
				break;
		}
		widget->show();
		addItem( new QWidgetItem(widget), index, side, dist, r.width(), r.height() );
		update();
	}
}

void DockableWidgetLayout::addWidget( DockableWidget *widget, DockSide side, int dist, int w, int h )
{
	// append item at the back
	addItem( new QWidgetItem(widget), -1, side, dist, w, h );
}

void DockableWidgetLayout::changed()
{
	minValid = false;
	update();
}

Qt::Orientations DockableWidgetLayout::expandingDirections() const
{
	return Qt::Horizontal | Qt::Vertical;
}

bool DockableWidgetLayout::hasHeightForWidth() const
{
	return false;
}

int DockableWidgetLayout::count() const
{
	return dockedWidgets.size();
}

QLayoutItem *DockableWidgetLayout::takeAt(int index)
{
	if( index<0 || index>=dockedWidgets.size() ) return 0;
	
	DockInfo *info = dockedWidgets.takeAt( index );
	QLayoutItem *item = info->item;
	delete info;
	
	minValid = false;
	return item;
}

QLayoutItem *DockableWidgetLayout::itemAt(int index) const
{
	if( index>=0 && index<dockedWidgets.size() )
		return dockedWidgets.at(index)->item;
	else 
		return 0;
}

QSize DockableWidgetLayout::minimumSize() const
{
	return QSize( minWidth, minHeight );
}

QSize DockableWidgetLayout::sizeHint() const
{
	return QSize( 700, 500 );
}

void DockableWidgetLayout::setGeometry(const QRect &rect)
{
	QLayout::setGeometry( rect );
	
	if( !minValid ) calcMinimumSize();
	// set reasonable start size
	DockInfo *d = dockedWidgets[0];
	int dx = minBaseWidgetWidth + rect.width() - minWidth - d->width;
	int dy = minBaseWidgetHeight + rect.height() - minHeight - d->height;
	sizeMove( dx, dy );
	doLayout();
	if( layoutWidth != rect.width() || layoutHeight != rect.height() ) {
		// second iteration to account for odd effects
		dx = rect.width() - layoutWidth;
		dy = rect.height() - layoutHeight;
		sizeMove( dx, dy );
		doLayout();
	}
	// resize the widgets
	for ( int i = 0; i < dockedWidgets.size(); i++ ) {
		DockInfo *d = dockedWidgets[i];
		if( d->widget->isVisible() ) 
			d->item->setGeometry( QRect( d->left, d->top, d->width, d->height ) );
	}
}

void DockableWidgetLayout::calcMinimumSize()
{
	DockInfo *d = dockedWidgets.at(0);
	int storeWidth = d->width;
	int storeHeight = d->height;
	// calculating the minimum size requires three trials.
	// small
	sizeMove( d->item->minimumSize().width() - d->width, d->item->minimumSize().height() - d->height );
	doLayout(true);
	int w0 = minWidth;
	int h0 = minHeight;
	// large horizontal base size
	sizeMove( 2048 - d->width, 0 );
	doLayout(true);
	int w1 = minWidth;
	int h1 = minHeight;
	// large vertical base size
	sizeMove( d->item->minimumSize().width() - d->width, 2048 - d->height );
	doLayout(true);
	int w2 = minWidth;
	int h2 = minHeight;
	// calculate size limits
	minWidth = qMax( w0, w2 );
	minHeight = qMax( h0, h1 );
	minBaseWidgetWidth = 2048 - (w1 - w0);
	minBaseWidgetHeight = 2048 - (h2 - h0);
	// restore base widget size and normal layout
	sizeMove( storeWidth - d->width, storeHeight - d->height );
	minValid = true;
}

void DockableWidgetLayout::sizeMove( int dx, int dy )
{
	DockInfo *d0 = dockedWidgets.at(0);
	for( int i = 1; i < dockedWidgets.size(); i++ ) {
		DockInfo *d = dockedWidgets.at(i);
		if( d->dockSide == TOP || d->dockSide == BOTTOM ) {
			if( d->dockDistance > 0 || d->distExtra != 0 ) {
				d->dockDistance += dx;
				if( d->dockDistance < 0 ) {
					d->distExtra += d->dockDistance;
					d->dockDistance = 0;
				} else if( d->distExtra < 0 ) {
					if( -d->distExtra <= d->dockDistance ) {
						d->dockDistance += d->distExtra;
						d->distExtra = 0;
					} else {
						d->distExtra += d->dockDistance;
						d->dockDistance = 0;
					}
				}
			}
		}
		if( d->dockSide == LEFT || d->dockSide == RIGHT ) {
			if( d->dockDistance > 0 || d->distExtra != 0 ) {
				d->dockDistance += dy;
				if( d->dockDistance < 0 ) {
					d->distExtra += d->dockDistance;
					d->dockDistance = 0;
				} else if( d->distExtra < 0 ) {
					if( -d->distExtra <= d->dockDistance ) {
						d->dockDistance += d->distExtra;
						d->distExtra = 0;
					} else {
						d->distExtra += d->dockDistance;
						d->dockDistance = 0;
					}
				}
			}
		}
	}
	d0->width += dx;
	d0->height += dy;
}

void DockableWidgetLayout::doLayout( bool calcMin )
{
	if( dockedWidgets.size() == 0 ) return;

	DockInfo *d = dockedWidgets[0];

	d->left = 0;
	d->top = 0;
	int W = d->width;
	int H = d->height;

	int dx = 0, dy = 0;
	for( int i = 1; i < dockedWidgets.size(); i++ ) {
		d = dockedWidgets[i];
		// only process visible widgets
		if( d->widget->isHidden() ) {
			d->left = -10000;
			d->top = -10000;
			continue;
		}
		// determine size
		if( d->useHintWidth )
			d->width = d->item->sizeHint().width();
		if( d->useHintHeight )
			d->height = d->item->sizeHint().height();
		// determine location
		switch( d->dockSide ) {
			case TOP:
			{
				d->left =  d->dockDistance;
				if( d->dockDistance >= W || d->dockDistance+d->width <= 0 )
					d->top = H - d->height;
				else
					d->top  = -d->height;
				// adjust position until it doesn't overlap  other widgets
				for( int j = 1; j < i; j++ ) {
					DockInfo *d2 = dockedWidgets[j];
					if( QRect(d->left,d->top,d->width,d->height)
					              .intersects(QRect(d2->left,d2->top,d2->width,d2->height)) )
						d->top = d2->top - d->height;
				}
				break;
			}
			case LEFT:
			{
				if( d->dockDistance >= H || d->dockDistance+d->height <= 0 )
					d->left = W - d->width;
				else
					d->left = -d->width;
				d->top  =  d->dockDistance;
				// adjust position until it doesn't overlap  other widgets
				for( int j = 1; j < i; j++ ) {
					DockInfo *d2 = dockedWidgets[j];
					if( QRect(d->left,d->top,d->width,d->height)
					              .intersects(QRect(d2->left,d2->top,d2->width,d2->height)) )
						d->left = d2->left - d->width;
				}
				break;
			}
			case RIGHT:
			{
				if( d->dockDistance >= H || d->dockDistance+d->height <= 0 )
					d->left = 0;
				else
					d->left = W;
				d->top  = d->dockDistance;
				// adjust position until it doesn't overlap  other widgets
				for( int j = 1; j < i; j++ ) {
					DockInfo *d2 = dockedWidgets[j];
					if( QRect(d->left,d->top,d->width,d->height)
					              .intersects(QRect(d2->left,d2->top,d2->width,d2->height)) )
						d->left = d2->left + d2->width;
				}
				break;
			}
			case BOTTOM:
			{
				d->left = d->dockDistance;
				if( d->dockDistance >= W || d->dockDistance+d->width <= 0 )
					d->top = 0;
				else
					d->top  = H;
				// adjust position until it doesn't overlap  other widgets
				for( int j = 1; j < i; j++ ) {
					DockInfo *d2 = dockedWidgets[j];
					if( QRect(d->left,d->top,d->width,d->height)
					              .intersects(QRect(d2->left,d2->top,d2->width,d2->height)) )
						d->top = d2->top + d2->height;
				}
				break;
			}
		}
		// check negative coordinates
		if( d->left < dx ) dx = d->left;
		if( d->top < dy ) dy = d->top;
	}	
	
	// translate widgets and calculate size
	if( calcMin ) {
		minWidth = minHeight = 0;
		for ( int i = 0; i < dockedWidgets.size(); i++ ) {
			DockInfo *d = dockedWidgets[i];
			if( d->left-dx+d->width > minWidth ) minWidth = d->left - dx + d->width;
			if( d->top-dy+d->height > minHeight ) minHeight = d->top - dy + d->height;
		}
	} else {
		layoutWidth = layoutHeight = 0;
		for ( int i = 0; i < dockedWidgets.size(); i++ ) {
			DockInfo *d = dockedWidgets[i];
			if( d->widget->isVisible() ) {
				d->left -= dx;
				d->top -= dy;
				if( d->left+d->width > layoutWidth ) layoutWidth = d->left + d->width;
				if( d->top+d->height > layoutHeight ) layoutHeight = d->top + d->height;
			}
		}
	}

}

bool DockableWidgetLayout::insertLocation( QRect& rect, int& index, DockSide& side, const QSizePolicy& sizePol )
{
	// best insertion data
	// Distance is a number that represents the how far
	// the insertion rectangle is from the final location.
	unsigned int bestDistance = 0xFFFFFFFF;
	int bestIndex = 0;
	DockSide bestSide;
	QRect bestRect;
	
	// loop over all widgets and find appropriate matching sides
	for( int i = 0; i < dockedWidgets.size(); i++ ) {
		DockInfo *d = dockedWidgets[i];
		/*****************************************************
		 * Check for placement against the top of the widget *
		 *****************************************************/
		if( i==0 || d->dockSide != BOTTOM ) {
			if( !(rect.left() > d->left+d->width-SNAP_DISTANCE ||
			      rect.right() < d->left+SNAP_DISTANCE) &&
			    abs(rect.bottom()-d->top)<SNAP_DISTANCE )
			{
				// rectangle is close to the edge
				unsigned int dist = 8*abs(rect.bottom()-d->top);
				// now find all points on this side
				// (abuse map to get sorted unique list)
				QMap<int,int> sidePoints;
				// loop over all widgets
				for (int j = 0; j <= i ; j++) {
					DockInfo *d2 = dockedWidgets[j];
					if( d->top == d2->top ) {
						// add the two edges
						sidePoints.insert( d2->left, 0 );
						sidePoints.insert( d2->left+d2->width, 0 );
						// check if any other widget rest against this side
						for (int k = i+1; k < dockedWidgets.size() ; k++) {
							DockInfo *d3 = dockedWidgets[k];
							if( d3->top+d3->height == d2->top ) {
								// add the two edges
								sidePoints.insert( d3->left, 0 );
								sidePoints.insert( d3->left+d3->width, 0 );
							}
						}
					}
				}
				// widget placement can occur at all points, find the closest
				QMap<int,int>::iterator it = sidePoints.begin();
				for( int j = 0; j < sidePoints.size()-1; j++ ) {
					// check after point
					if( dist+abs(it.key()-rect.left()) < bestDistance && abs(it.key()-rect.left()) < SNAP_DISTANCE ) {
						// verify if it does not overlap a placed widget
						int k;
						for( k = 0; k < i; k++ ) {
							DockInfo *d3 = dockedWidgets[k];
							if( QRect( QPoint( it.key(), d->top-rect.height() ), rect.size() )
							       .intersects( QRect( d3->left, d3->top, d3->width, d3->height ) ) )
								break;
							
						}
						if( k == i ) {
							bestDistance = dist+abs(it.key()-rect.left());
							bestIndex = i+1;
							bestSide = TOP;
							bestRect = rect;
							bestRect.moveBottomLeft( QPoint( it.key(), d->top-1 ) );
						}
					}
					it++;
					// check before point
					if( dist+abs(it.key()-rect.right()) < bestDistance && abs(it.key()-rect.right()) < SNAP_DISTANCE ) {
						// verify if it does not overlap a placed widget
						int k;
						for( k = 0; k < i; k++ ) {
							DockInfo *d3 = dockedWidgets[k];
							if( QRect( QPoint( it.key()-rect.width(), d->top-rect.height() ), rect.size() )
							       .intersects( QRect( d3->left, d3->top, d3->width, d3->height ) ) )
									break;
						}
						if( k == i ) {
							bestDistance = dist+abs(it.key()-rect.right());
							bestIndex = i+1;
							bestSide = TOP;
							bestRect = rect;
							bestRect.moveBottomRight( QPoint( it.key()-1, d->top-1 ) );
						}
					}
				}
				// check for resized placement options
				if( sizePol.horizontalPolicy() != QSizePolicy::Fixed ) {
					int mid = rect.left()+rect.width()/2;
					QMap<int,int>::iterator ita = sidePoints.begin() + 1;
					while( ita != sidePoints.end() )	{
						QMap<int,int>::iterator itb = sidePoints.begin();
						while( ita != itb ) {
							// check if rect middle is close to the middle between two points
							if( abs((ita.key()+itb.key())/2 - mid) < SNAP_DISTANCE ) {
								// verify if this area is free
								int k;
								for( k = 0; k < i; k++ ) {
									DockInfo *d3 = dockedWidgets[k];
									if( QRect( itb.key(), d->top-rect.height(), ita.key()-itb.key(), rect.height() )
									       .intersects( QRect( d3->left, d3->top, d3->width, d3->height ) ) )
										break;
								}
								if( k == i ) {
									bestDistance = dist+abs((ita.key()+itb.key())/2 - mid);
									bestIndex = i+1;
									bestSide = TOP;
									bestRect = QRect( itb.key(), d->top-rect.height(), ita.key()-itb.key(), rect.height() );
								}
							}
							itb++;
						}
						ita++;						
					}
				}
			}
		}
		
		/********************************************************
		 * Check for placement against the bottom of the widget *
		 ********************************************************/
		if( i==0 || d->dockSide != TOP ) {
			if( !(rect.left() > d->left+d->width-SNAP_DISTANCE ||
			      rect.right() < d->left+SNAP_DISTANCE) &&
			    abs(rect.top()-d->top-d->height)<SNAP_DISTANCE )
			{
				// rectangle is close to the edge
				unsigned int dist = 8*abs(rect.top()-d->top-d->height);
				// now find all points on this side
				// (abuse map to get sorted unique list)
				QMap<int,int> sidePoints;
				// loop over all widgets
				for (int j = 0; j <= i ; j++) {
					DockInfo *d2 = dockedWidgets[j];
					if( d->top+d->height == d2->top+d2->height ) {
						// add the two edges
						sidePoints.insert( d2->left, 0 );
						sidePoints.insert( d2->left+d2->width, 0 );
						// check if any other widget rest against this side
						for (int k = i+1; k < dockedWidgets.size() ; k++) {
							DockInfo *d3 = dockedWidgets[k];
							if( d3->top == d2->top+d2->height ) {
								// add the two edges
								sidePoints.insert( d3->left, 0 );
								sidePoints.insert( d3->left+d3->width, 0 );
							}
						}
					}
				}
				// widget placement can occur at all points, find the closest
				QMap<int,int>::iterator it = sidePoints.begin();
				for( int j = 0; j < sidePoints.size()-1; j++ ) {
					// check after point
					if( dist+abs(it.key()-rect.left()) < bestDistance && abs(it.key()-rect.left()) < SNAP_DISTANCE ) {
						// verify if it does not overlap a placed widget
						int k;
						for( k = 0; k < i; k++ ) {
							DockInfo *d3 = dockedWidgets[k];
							if( QRect( QPoint( it.key(), d->top+d->height ), rect.size() )
							       .intersects( QRect( d3->left, d3->top, d3->width, d3->height ) ) )
								break;
						}
						if( k == i ) {
							bestDistance = dist+abs(it.key()-rect.left());
							bestIndex = i+1;
							bestSide = BOTTOM;
							bestRect = rect;
							bestRect.moveTopLeft( QPoint( it.key(), d->top+d->height ) );
						}
					}
					it++;
					// check before point
					if( dist+abs(it.key()-rect.right()) < bestDistance && abs(it.key()-rect.right()) < SNAP_DISTANCE ) {
						// verify if it does not overlap a placed widget
						int k;
						for( k = 0; k < i; k++ ) {
							DockInfo *d3 = dockedWidgets[k];
							if( QRect( QPoint( it.key()-rect.width(), d->top+d->height ), rect.size() )
							       .intersects( QRect( d3->left, d3->top, d3->width, d3->height ) ) )
								break;
						}
						if( k == i ) {
							bestDistance = dist+abs(it.key()-rect.right());
							bestIndex = i+1;
							bestSide = BOTTOM;
							bestRect = rect;
							bestRect.moveTopRight( QPoint( it.key()-1, d->top+d->height ) );
						}
					}
				}
				// check for resized placement options
				if( sizePol.horizontalPolicy() != QSizePolicy::Fixed ) {
					int mid = rect.left()+rect.width()/2;
					QMap<int,int>::iterator ita = sidePoints.begin() + 1;
					while( ita != sidePoints.end() )	{
						QMap<int,int>::iterator itb = sidePoints.begin();
						while( ita != itb ) {
							// check if rect middle is close to the middle between two points
							if( abs((ita.key()+itb.key())/2 - mid) < SNAP_DISTANCE ) {
								// verify if this area is free
								int k;
								for( k = 0; k < i; k++ ) {
									DockInfo *d3 = dockedWidgets[k];
									if( QRect( itb.key(), d->top+d->height, ita.key()-itb.key(), rect.height() )
									       .intersects( QRect( d3->left, d3->top, d3->width, d3->height ) ) )
											break;
								}
								if( k == i ) {
									bestDistance = dist+abs((ita.key()+itb.key())/2 - mid);
									bestIndex = i+1;
									bestSide = BOTTOM;
									bestRect = QRect( itb.key(), d->top+d->height, ita.key()-itb.key(), rect.height() );
								}
							}
							itb++;
						}
						ita++;						
					}
				}
			}
		}
		/******************************************************
		 * Check for placement against the left of the widget *
		 ******************************************************/
		if( i==0 || d->dockSide != RIGHT ) {
			if( !(rect.top() > d->top+d->width-SNAP_DISTANCE ||
			      rect.bottom() < d->top+SNAP_DISTANCE) &&
			    abs(rect.right()-d->left)<SNAP_DISTANCE )
			{
				// rectangle is close to the edge
				unsigned int dist = 8*abs(rect.right()-d->left);
				// now find all points on this side
				// (abuse map to get sorted unique list)
				QMap<int,int> sidePoints;
				// loop over all widgets
				for (int j = 0; j <= i ; j++) {
					DockInfo *d2 = dockedWidgets[j];
					if( d->left == d2->left ) {
						// add the two edges
						sidePoints.insert( d2->top, 0 );
						sidePoints.insert( d2->top+d2->height, 0 );
						// check if any other widget rest against this side
						for (int k = i+1; k < dockedWidgets.size() ; k++) {
							DockInfo *d3 = dockedWidgets[k];
							if( d3->left+d3->width == d2->left ) {
								// add the two edges
								sidePoints.insert( d3->top, 0 );
								sidePoints.insert( d3->top+d3->height, 0 );
							}
						}
					}
				}
				// widget placement can occur at all points, find the closest
				QMap<int,int>::iterator it = sidePoints.begin();
				for( int j = 0; j < sidePoints.size()-1; j++ ) {
					// check after point
					if( dist+abs(it.key()-rect.top()) < bestDistance && abs(it.key()-rect.top()) < SNAP_DISTANCE ) {
						// verify if it does not overlap a placed widget
						int k;
						for( k = 0; k < i; k++ ) {
							DockInfo *d3 = dockedWidgets[k];
							if( QRect( QPoint( d->left-rect.width(), it.key() ), rect.size() )
							       .intersects( QRect( d3->left, d3->top, d3->width, d3->height ) ) )
								break;
							
						}
						if( k == i ) {
							bestDistance = dist+abs(it.key()-rect.top());
							bestIndex = i+1;
							bestSide = LEFT;
							bestRect = rect;
							bestRect.moveTopRight( QPoint( d->left-1, it.key() ) );
						}
					}
					it++;
					// check before point
					if( dist+abs(it.key()-rect.bottom()) < bestDistance && abs(it.key()-rect.bottom()) < SNAP_DISTANCE ) {
						// verify if it does not overlap a placed widget
						int k;
						for( k = 0; k < i; k++ ) {
							DockInfo *d3 = dockedWidgets[k];
							if( QRect( QPoint( d->left-rect.width(), it.key()-rect.height() ), rect.size() )
							       .intersects( QRect( d3->left, d3->top, d3->width, d3->height ) ) )
									break;
						}
						if( k == i ) {
							bestDistance = dist+abs(it.key()-rect.bottom());
							bestIndex = i+1;
							bestSide = LEFT;
							bestRect = rect;
							bestRect.moveBottomRight( QPoint( d->left-1, it.key()-1 ) );
						}
					}
				}
				// check for resized placement options
				if( sizePol.verticalPolicy() != QSizePolicy::Fixed ) {
					int mid = rect.top()+rect.height()/2;
					QMap<int,int>::iterator ita = sidePoints.begin() + 1;
					while( ita != sidePoints.end() )	{
						QMap<int,int>::iterator itb = sidePoints.begin();
						while( ita != itb ) {
							// check if rect middle is close to the middle between two points
							if( abs((ita.key()+itb.key())/2 - mid) < SNAP_DISTANCE ) {
								// verify if this area is free
								int k;
								for( k = 0; k < i; k++ ) {
									DockInfo *d3 = dockedWidgets[k];
									if( QRect( d->left-rect.width(), itb.key(), rect.width(), ita.key()-itb.key() )
									       .intersects( QRect( d3->left, d3->top, d3->width, d3->height ) ) )
										break;
								}
								if( k == i ) {
									bestDistance = dist+abs((ita.key()+itb.key())/2 - mid);
									bestIndex = i+1;
									bestSide = LEFT;
									bestRect = QRect( d->left-rect.width(), itb.key(), rect.width(), ita.key()-itb.key() );
								}
							}
							itb++;
						}
						ita++;						
					}
				}
			}
		}
		/*******************************************************
		 * Check for placement against the right of the widget *
		 *******************************************************/
		if( i==0 || d->dockSide != LEFT ) {
			if( !(rect.top() > d->top+d->width-SNAP_DISTANCE ||
			      rect.bottom() < d->top+SNAP_DISTANCE) &&
			    abs(rect.left()-d->left-d->width)<SNAP_DISTANCE )
			{
				// rectangle is close to the edge
				unsigned int dist = 8*abs(rect.left()-d->left-d->width);
				// now find all points on this side
				// (abuse map to get sorted unique list)
				QMap<int,int> sidePoints;
				// loop over all widgets
				for (int j = 0; j <= i ; j++) {
					DockInfo *d2 = dockedWidgets[j];
					if( d->left+d->width == d2->left+d2->width ) {
						// add the two edges
						sidePoints.insert( d2->top, 0 );
						sidePoints.insert( d2->top+d2->height, 0 );
						// check if any other widget rest against this side
						for (int k = i+1; k < dockedWidgets.size() ; k++) {
							DockInfo *d3 = dockedWidgets[k];
							if( d3->left == d2->left+d2->width ) {
								// add the two edges
								sidePoints.insert( d3->top, 0 );
								sidePoints.insert( d3->top+d3->height, 0 );
							}
						}
					}
				}
				// widget placement can occur at all points, find the closest
				QMap<int,int>::iterator it = sidePoints.begin();
				for( int j = 0; j < sidePoints.size()-1; j++ ) {
					// check after point
					if( dist+abs(it.key()-rect.top()) < bestDistance && abs(it.key()-rect.top()) < SNAP_DISTANCE ) {
						// verify if it does not overlap a placed widget
						int k;
						for( k = 0; k < i; k++ ) {
							DockInfo *d3 = dockedWidgets[k];
							if( QRect( QPoint( d->left+d->width, it.key() ), rect.size() )
							       .intersects( QRect( d3->left, d3->top, d3->width, d3->height ) ) )
								break;
							
						}
						if( k == i ) {
							bestDistance = dist+abs(it.key()-rect.top());
							bestIndex = i+1;
							bestSide = RIGHT;
							bestRect = rect;
							bestRect.moveTopLeft( QPoint( d->left+d->width, it.key() ) );
						}
					}
					it++;
					// check before point
					if( dist+abs(it.key()-rect.bottom()) < bestDistance && abs(it.key()-rect.bottom()) < SNAP_DISTANCE ) {
						// verify if it does not overlap a placed widget
						int k;
						for( k = 0; k < i; k++ ) {
							DockInfo *d3 = dockedWidgets[k];
							if( QRect( QPoint( d->left+d->width, it.key()-rect.height() ), rect.size() )
							       .intersects( QRect( d3->left, d3->top, d3->width, d3->height ) ) )
									break;
						}
						if( k == i ) {
							bestDistance = dist+abs(it.key()-rect.bottom());
							bestIndex = i+1;
							bestSide = RIGHT;
							bestRect = rect;
							bestRect.moveBottomLeft( QPoint( d->left+d->width, it.key()-1 ) );
						}
					}
				}
				// check for resized placement options
				if( sizePol.verticalPolicy() != QSizePolicy::Fixed ) {
					int mid = rect.top()+rect.height()/2;
					QMap<int,int>::iterator ita = sidePoints.begin() + 1;
					while( ita != sidePoints.end() )	{
						QMap<int,int>::iterator itb = sidePoints.begin();
						while( ita != itb ) {
							// check if rect middle is close to the middle between two points
							if( abs((ita.key()+itb.key())/2 - mid) < SNAP_DISTANCE ) {
								// verify if this area is free
								int k;
								for( k = 0; k < i; k++ ) {
									DockInfo *d3 = dockedWidgets[k];
									if( QRect( d->left+d->width, itb.key(), rect.width(), ita.key()-itb.key() )
									       .intersects( QRect( d3->left, d3->top, d3->width, d3->height ) ) )
										break;
								}
								if( k == i ) {
									bestDistance = dist+abs((ita.key()+itb.key())/2 - mid);
									bestIndex = i+1;
									bestSide = RIGHT;
									bestRect = QRect( d->left+d->width, itb.key(), rect.width(), ita.key()-itb.key() );
								}
							}
							itb++;
						}
						ita++;						
					}
				}
			}
		}
	}
	
	if( bestIndex ) {
		rect = bestRect;
		index = bestIndex;
		side = bestSide;
		return true;
	} else {
		return false;
	}
}

bool DockableWidgetLayout::insertLocation( QRect& rect, const QSizePolicy& sizePol )
{
	int index;
	DockSide side;
	return insertLocation( rect, index, side, sizePol );
}

void DockableWidgetLayout::getConfig( QStringList& list )
{
	for( int i = 0; i < dockedWidgets.size(); i++ ) {
		// string format D [Hidden/Visible] [Side] [Distance] [Width] [Height]
		QString s("%1 D %2 %3 %4 %5 %6");
		DockInfo *d = dockedWidgets.at(i);
		
		s = s.arg( d->widget->id() );
		
		if( d->widget->isHidden() )
			s = s.arg("H");
		else
			s = s.arg("V");
		
		switch( d->dockSide ) {
			case TOP:
				s = s.arg("T");
				break;
			case LEFT:
				s = s.arg("L");
				break;
			case RIGHT:
				s = s.arg("R");
				break;
			case BOTTOM:
				s = s.arg("B");
				break;
		}
		
		s = s.arg( d->dockDistance );

		if( d->useHintWidth )
			s = s.arg(-1);
		else
			s = s.arg( d->width );

		if( d->useHintHeight )
			s = s.arg(-1);
		else
			s = s.arg( d->height );
			
		list.append( s );
	}
}
