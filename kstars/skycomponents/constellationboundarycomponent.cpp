/***************************************************************************
                          constellationboundarycomponent.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 12/09/2005
    copyright            : (C) 2005 by Jason Harris
    email                : kstars@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <QFile>
#include <QPainter>
#include <QTextStream>

#include "csegment.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "ksutils.h"
#include "skymap.h"
#include "Options.h"

#include "constellationboundarycomponent.h"

ConstellationBoundaryComponent::ConstellationBoundaryComponent( SkyComponent *parent, bool (*visibleMethod)() )
: SkyComponent( parent, visibleMethod )
{
}

ConstellationBoundaryComponent::~ConstellationBoundaryComponent() 
{
	while ( ! segmentList().isEmpty() ) 
		delete segmentList().takeFirst();
}

// bool KStarsData::readCLineData( void )
void ConstellationBoundaryComponent::init(KStarsData *)
{
	QFile file;
	if ( KSUtils::openDataFile( file, "cbound.dat" ) ) {
		emitProgressText( i18n("Loading constellation boundaries") );

	  QTextStream stream( &file );
		unsigned int nn(0);
		double ra(0.0), dec(0.0);
		QString d1, d2;
		bool ok(false), comment(false);
		
		//read the stream one field at a time.  Individual segments can span
		//multiple lines, so our normal readLine() is less convenient here.
		//Read fields into strings and then attempt to recast them as ints 
		//or doubles..this lets us do error checking on the stream.
		while ( !stream.atEnd() ) {
			stream >> d1;
			if ( d1.isEmpty() ) continue;

			if ( d1.at(0) == '#' ) { 
				comment = true; 
				ok = true; 
			} else {
				comment = false;
				nn = d1.toInt( &ok );
			}
			
			if ( !ok || comment ) {
				d1 = stream.readLine();
				
				if ( !ok ) 
					kWarning() << i18n( "Unable to parse boundary segment." ) << endl;
				
			} else { 
				CSegment *seg = new CSegment();
				for ( unsigned int i=0; i<nn; ++i ) {
					stream >> d1 >> d2;
					ra = d1.toDouble( &ok );
					if ( ok ) dec = d2.toDouble( &ok );
					if ( !ok ) break;
					
					seg->addPoint( ra, dec );
				}
				
				if ( !ok ) {
					//uh oh, this entry was not parsed.  Skip to the next line.
					kWarning() << i18n( "Unable to parse boundary segment." ) << endl;
					delete seg;
					d1 = stream.readLine();
					
				} else {
					stream >> d1; //this should always equal 2
					
					nn = d1.toInt( &ok );
					//error check
					if ( !ok || nn != 2 ) {
						kWarning() << i18n( "Bad Constellation Boundary data." ) << endl;
						delete seg;
						d1 = stream.readLine();
					}
				}

				if ( ok ) {
					stream >> d1 >> d2;
					ok = seg->setNames( d1, d2 );
					if ( ok ) m_SegmentList.append( seg );
				}
			}
		}
	}
}

void ConstellationBoundaryComponent::draw(KStars *ks, QPainter& psky, double scale)
{
	if ( ! visible() ) return;

	SkyMap *map = ks->map();
	float Width = scale * map->width();
	float Height = scale * map->height();

	QString abbrev = QString(); 
	QString cname = ks->data()->skyComposite()->constellation( map->focus() ).toLower();
	foreach ( SkyObject *o, ks->data()->skyComposite()->constellationNames() ) {
		if ( o->name().toLower() == cname ) 
			abbrev = o->name2().toLower();
	}

	psky.setPen( QPen( QColor( ks->data()->colorScheme()->colorNamed( "CBoundColor" ) ), 1, Qt::SolidLine ) );
	psky.setBrush( Qt::NoBrush );

	foreach ( CSegment *seg, m_SegmentList ) {
		bool started( false );
		SkyPoint *p = seg->nodes()[0];
		QPointF oStart = map->toScreen( p, scale );
		if ( ( oStart.x() >= -1000. && oStart.x() <= Width+1000.
				&& oStart.y() >= -1000. && oStart.y() <= Height+1000. ) ) {
			started = true;
		}

		//Highlight current constellation
		bool highlightConstellation = false;
		if ( seg->name1().toLower() == abbrev || seg->name2().toLower() == abbrev ) {
			psky.setPen( QPen( QColor( ks->data()->colorScheme()->colorNamed( "CBoundHighColor" ) ), 3, Qt::SolidLine ) );
			highlightConstellation = true;
		}

		foreach ( SkyPoint *p, seg->nodes() ) {
			QPointF o = map->toScreen( p, scale );

			if ( ( o.x() >= -1000. && o.x() <= Width+1000.
					&& o.y() >= -1000. && o.y() <= Height+1000. ) ) {
				if ( started ) {
					if ( Options::useAntialias() )
						psky.drawLine( oStart, o );
					else
						psky.drawLine( QPoint(int(oStart.x()),int(oStart.y())), 
									QPoint(int(o.x()), int(o.y())) );
				} else {
					started = true;
				}
				oStart = o;
			} else {
				started = false;
			}
		}

		if ( highlightConstellation ) {
			psky.setPen( QPen( QColor( ks->data()->colorScheme()->colorNamed( "CBoundColor" ) ), 1, Qt::SolidLine ) );
		}

	}
}

void ConstellationBoundaryComponent::update(KStarsData *data, KSNumbers *num ) {
	if ( visible() ) {  
	  foreach ( CSegment *seg, segmentList() ) {
	    foreach ( SkyPoint *p, seg->nodes() ) {
				if ( num ) p->updateCoords( num );
				p->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
			}
		}
	}
}

QList<CSegment*>& ConstellationBoundaryComponent::segmentList() { return m_SegmentList; }
