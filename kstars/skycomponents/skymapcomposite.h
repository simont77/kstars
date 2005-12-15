/***************************************************************************
                          skymapcomposite.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/07/08
    copyright            : (C) 2005 by Thomas Kabelmann
    email                : thomas.kabelmann@gmx.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef SKYMAPCOMPOSITE_H
#define SKYMAPCOMPOSITE_H

#include <QList>

#include "skycomposite.h"
#include "solarsystemcomposite.h"
#include "starcomponent.h"
#include "constellationboundarycomponent.h"
#include "constellationnamescomponent.h"
#include "deepskycomponent.h"

class KStarsData;

/**@class SkyMapComposite
*SkyMapComposite is the root object in the object hierarchy of the sky map.
*All requests to update, init, draw etc. will be done with this class.
*The requests will be delegated to it's children.
*The object hierarchy will created by adding new objects via addComponent().
*
*@author Thomas Kabelmann
*@version 0.1
*/

class SkyMapComposite : public QObject, public SkyComposite
{
	Q_OBJECT

	public:
		SkyMapComposite(SkyComponent *parent, KStarsData *data);

		/**
			*@short Delegate init requests to all sub components
			*@p data Pointer to the KStarsData object
			*/
		virtual void init(KStarsData *data);
	
		/**
			*@short Delegate planet position updates to the SolarSystemComposite
			*
			*Planet positions change over time, so they need to be recomputed 
			*periodically, but not on every call to update().  This function 
			*will recompute the positions of all solar system bodies except the 
			*Earth's Moon and Jupiter's Moons (because these objects' positions 
			*change on a much more rapid timescale).
			*@p data Pointer to the KStarsData object
			*@p num Pointer to the KSNumbers object
			*@sa update()
			*@sa updateMoons()
			*@sa SolarSystemComposite::updatePlanets()
			*/
		virtual void updatePlanets( KStarsData *data, KSNumbers *num );

		/**
			*@short Delegate moon position updates to the SolarSystemComposite
			*
			*Planet positions change over time, so they need to be recomputed 
			*periodically, but not on every call to update().  This function 
			*will recompute the positions of the Earth's Moon and Jupiter's four
			*Galilean moons.  These objects are done separately from the other 
			*solar system bodies, because their positions change more rapidly,
			*and so updateMoons() must be called more often than updatePlanets().
			*@p data Pointer to the KStarsData object
			*@p num Pointer to the KSNumbers object
			*@sa update()
			*@sa updatePlanets()
			*@sa SolarSystemComposite::updateMoons()
			*/
		virtual void updateMoons( KStarsData *data, KSNumbers *num );

		/**
			*@short Add a Trail to the specified SkyObject
			*Loop over all child SkyComponents; if the SkyObject
			*in the argument is found, add a Trail to it.
			*@p o Pointer to the SkyObject to which a Trail will be added
			*@return true if the object was found and a Trail was added 
			*/
		virtual bool addTrail( SkyObject *o );
		virtual bool hasTrail( SkyObject *o, bool &found );
		virtual bool removeTrail( SkyObject *o );

		void addCustomCatalog( const QString &filename, bool (*visibleMethod)() );
		void removeCustomCatalog( const QString &name );

		void reloadDeepSky( KStarsData *data );
		void reloadAsteroids( KStarsData *data );
		void reloadComets( KStarsData *data );

		//Accessors for StarComponent
		SkyObject* findStarByGenetiveName( const QString &name );
		void setFaintStarMagnitude( float newMag );
		float faintStarMagnitude() const { return m_StarComponent->faintMagnitude(); }
		void setStarColorMode( int newMode );
		int starColorMode() const { return m_StarComponent->starColorMode(); }
		void setStarColorIntensity( int newIntensity );
		int starColorIntensity() const { return m_StarComponent->starColorIntensity(); }

		QString constellation( SkyPoint *p );

		void emitProgressText( const QString &message );

		QList<SkyComponent*> solarSystem() { return m_SSComposite->components(); }
		KSPlanet* earth() { return m_SSComposite->earth(); }

		QList<SkyComponent*> customCatalogs() { 
			return m_CustomCatalogComposite->components(); 
		}

		QStringList& objectNames() { return m_ObjectNames; }

	signals:
		void progressText( const QString &message );

	private:
		SolarSystemComposite *m_SSComposite;
		SkyComposite *m_CustomCatalogComposite;
		StarComponent *m_StarComponent;
		ConstellationBoundaryComponent *m_CBoundsComponent;
		ConstellationNamesComponent *m_CNamesComponent;
		DeepSkyComponent *m_DeepSkyComponent;
		QStringList m_ObjectNames;
};

#endif
