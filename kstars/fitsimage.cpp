/***************************************************************************
                          FITSImage.cpp  -  FITS Image
                             -------------------
    begin                : Thu Jan 22 2004
    copyright            : (C) 2004 by Jasem Mutlaq
    email                : mutlaqja@ikarustech.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   Some code fragments were adapted from Peter Kirchgessner's FITS plugin*
 *   See http://members.aol.com/pkirchg for more details.                  *
 ***************************************************************************/


#include <klocale.h>
#include <kmessagebox.h>
#include <kfiledialog.h>
#include <kaction.h>
#include <kaccel.h>
#include <kdebug.h>
#include <ktoolbar.h>
#include <kapplication.h>
#include <kpixmap.h> 
#include <ktempfile.h>
#include <kimageeffect.h> 
#include <kmenubar.h>
#include <kprogress.h>
#include <kstatusbar.h>

#include <qfile.h>
#include <qvbox.h>
#include <qcursor.h>
#include <qpixmap.h>
#include <qframe.h>

#include <math.h>
#include <unistd.h>
#include <stdlib.h>
#include <netinet/in.h>

#include "fitsimage.h"
#include "fitsviewer.h"
//#include "focusdialog.h" 
#include "ksutils.h"

/* Load info */
typedef struct
{
  uint replace;    /* replacement for blank/NaN-values */
  uint use_datamin;/* Use DATAMIN/MAX-scaling if possible */
  uint compose;    /* compose images with naxis==3 */
} FITSLoadVals;

static FITSLoadVals plvals =
{
  0,        /* Replace with black */
  0,        /* Do autoscale on pixel-values */
  0         /* Dont compose images */
};

FITSImage::FITSImage(QWidget * parent, const char * name) : QScrollView(parent, name), zoomFactor(1.2)
{
  viewer = (FITSViewer *) parent;
  reducedImgBuffer = NULL;
  displayImage     = NULL;
  
  imgFrame = new FITSFrame(this, viewport());
  addChild(imgFrame);
  
  currentZoom = 0.0;
  
  viewport()->setMouseTracking(true);
  imgFrame->setMouseTracking(true);
  
}

FITSImage::~FITSImage()
{
  free(reducedImgBuffer);
}
	
void FITSImage::drawContents ( QPainter * p, int clipx, int clipy, int clipw, int cliph )
{
  //kdDebug() << "in draw contents " << endl;
  //imgFrame->update();
  
}

/**Bitblt the image onto the viewer widget */
void FITSImage::paintEvent (QPaintEvent *ev)
{
 //kdDebug() << "in paint event " << endl;
 //bitBlt(imgFrame, 0, 0, &qpix);
}

/* Resize event */
void FITSImage::resizeEvent (QResizeEvent *ev)
{
	updateScrollBars();
}

void FITSImage::contentsMouseMoveEvent ( QMouseEvent * e )
{

  double x,y;
  bool validPoint = true;
  if (!displayImage) return;
  
  
   x = e->x();
   y = e->y();
  
  if (imgFrame->x() > 0)
    x -= imgFrame->x();
  
  if (imgFrame->y() > 0)
    y -= imgFrame->y();
  
   //kdDebug() << "X= " << x << " -- Y= " << y << endl;      
   
  if (currentZoom > 0)
  {
    x /= pow(zoomFactor, currentZoom);
    y /= pow(zoomFactor, currentZoom);
  }
  else if (currentZoom < 0)
  {
    x *= pow(zoomFactor, abs(currentZoom));
    //kdDebug() << "The X power is " << pow(zoomFactor, abs(currentZoom)) << " -- X final = " << x << endl;
    y *= pow(zoomFactor, abs(currentZoom));
  }
  
  if (x < 0 || x > width)
    validPoint = false;
  
  //kdDebug() << "regular x= " << e->x() << " -- X= " << x << " -- imgFrame->x()= " << imgFrame->x() << " - displayImageWidth= " << viewer->displayImage->width() << endl;
  
  
  if (y < 0 || y > height)
    validPoint = false;
  else    
  // invert the Y since we read FITS buttom up
  y = height - y;
  
  //kdDebug() << "Zoom= " << currentZoom << " -- X= " << x << " -- Y= " << y << endl;
  
  if (viewer->imgBuffer == NULL)
   kdDebug() << "viewer buffer is NULL " << endl;
  
  if (validPoint)
  {
  viewer->statusBar()->changeItem(QString("(%1,%2) Value= %3").arg( (int) x,4).arg( (int) y,-4).arg(viewer->imgBuffer[(int) (y * width + x)], -6), 0);
  setCursor(Qt::CrossCursor);
  }
  else
  {
  viewer->statusBar()->changeItem(QString("(X,Y)"), 0);
  setCursor(Qt::ArrowCursor);
  }
}

void FITSImage::viewportResizeEvent ( QResizeEvent * e)
{
        int w, h, x, y;
	if (!displayImage) return;
	
	w = viewport()->width();
        h = viewport()->height();
        if ( w > contentsWidth() )
           x = (int) ( (w - contentsWidth()) / 2.);
        else
           x = 0;
        if ( h > contentsHeight() )
          y = (int) ( (h - contentsHeight()) / 2.);
       else
          y = 0;
	 
	//kdDebug() << "X= " << x << " -- Y= " << y << endl;
        moveChild( imgFrame, x, y );

}

void FITSImage::reLoadTemplateImage()
{
  *displayImage = templateImage->copy();
}

void FITSImage::saveTemplateImage()
{
  templateImage = new QImage(displayImage->copy());
  //*templateImage = displayImage->copy();

}

void FITSImage::destroyTemplateImage()
{
  delete (templateImage);
}

void FITSImage::updateReducedBuffer()
{

  for (int i=0; i < height; i++)
      for (int j=0; j < width; j++)
        reducedImgBuffer[i * width + j] = qRed(displayImage->pixel(j, i));

}

void FITSImage::rescale(FITSImage::scaleType type, int min, int max)
{
  
  int val, bufferVal;
  double coeff;
 
  switch (type)
  {
    case FITSAuto:
    case FITSLinear:
    for (int i=0; i < width; i++)
      for (int j=0; j < height; j++)
      {
              bufferVal = viewer->imgBuffer[i * width + j];
	      if (bufferVal < min) bufferVal = min;
	      else if (bufferVal > max) bufferVal = max;
	      val = (int) (255. * ((double) (bufferVal - min) / (double) (max - min)));
      	      displayImage->setPixel(j, height - i - 1, qRgb(val, val, val));
      }
      updateReducedBuffer();
     break;
     
    case FITSLog:
    coeff = 255. / log(1 + max);
    
    for (int i=0; i < height; i++)
      for (int j=0; j < width; j++)
      {
              bufferVal = viewer->imgBuffer[i * width + j];
	      if (bufferVal < min) bufferVal = min;
	      else if (bufferVal > max) bufferVal = max;
	      val = (int) (coeff * log(1 + bufferVal));
      	      displayImage->setPixel(j, height - i - 1, qRgb(val, val, val));
      }
      updateReducedBuffer();
      break;
      
    case FITSExp:
    coeff = 255. / exp(max);
    
    for (int i=0; i < height; i++)
      for (int j=0; j < width; j++)
      {
              bufferVal = viewer->imgBuffer[i * width + j];
	      if (bufferVal < min) bufferVal = min;
	      else if (bufferVal > max) bufferVal = max;
	      val = (int) (coeff * exp(bufferVal));
      	      displayImage->setPixel(j, height - i - 1, qRgb(val, val, val));
      }
      updateReducedBuffer();
      break;
    
    case FITSSqrt:
    coeff = 255. / sqrt(max);
    
    for (int i=0; i < height; i++)
      for (int j=0; j < width; j++)
      {
              bufferVal = viewer->imgBuffer[i * width + j];
	      if (bufferVal < min) bufferVal = min;
	      else if (bufferVal > max) bufferVal = max;
	      val = (int) (coeff * sqrt(bufferVal));
      	      displayImage->setPixel(j, height - i - 1, qRgb(val, val, val));
      }
      updateReducedBuffer();
      break;
    
     
    default:
     break;
  }
  
  //convertImageToPixmap();
  //update();
  zoomToCurrent();
}


int FITSImage::loadFits (const char *filename)
{
 FILE *fp;
 FITS_FILE *ifp;
 //FITS_HDU_LIST *hdulist;
 FITS_PIX_TRANSFORM trans;
 register unsigned char *dest; 
 unsigned char *data;
 int i, j, val;
 double a, b;
 int err = 0;
 
 fp = fopen (filename, "rb");
 if (!fp)
 {
   KMessageBox::error(0, i18n("Can't open file for reading"));
   return (-1);
 }
 fclose (fp);

 ifp = fits_open (filename, "r");
 if (ifp == NULL)
 {
   KMessageBox::error(0, i18n("Error during open of FITS file"));
   return (-1);
 }
 if (ifp->n_pic <= 0)
 {
   KMessageBox::error(0, i18n("FITS file keeps no displayable images"));
   fits_close (ifp);
   return (-1);
 }

 displayImage  = new QImage();
 KProgressDialog fitsProgress(this, 0, i18n("FITS Viewer"), i18n("Loading FITS..."));
 
 hdulist = fits_seek_image (ifp, 1);
 if (hdulist == NULL) return (-1);

 width  = currentWidth = hdulist->naxisn[0]; 
 height = currentHeight = hdulist->naxisn[1];
 
 bitpix = hdulist->bitpix;
 bpp    = hdulist->bpp;
 
 imgFrame->setGeometry(0, 0, width, height);
 
 data = (unsigned char  *) malloc (height * width * sizeof(unsigned char));
 if (data == NULL)
 {
  KMessageBox::error(0, i18n("Not enough memory to load FITS."));
  return (-1);
 }
 
 /* If the transformation from pixel value to */
 /* data value has been specified, use it */
 if (   plvals.use_datamin
     && hdulist->used.datamin && hdulist->used.datamax
     && hdulist->used.bzero && hdulist->used.bscale)
 {
   a = (hdulist->datamin - hdulist->bzero) / hdulist->bscale;
   b = (hdulist->datamax - hdulist->bzero) / hdulist->bscale;
   if (a < b) trans.pixmin = a, trans.pixmax = b;
   else trans.pixmin = b, trans.pixmax = a;
 }
 else
 {
   trans.pixmin = hdulist->pixmin;
   trans.pixmax = hdulist->pixmax;
 }
 trans.datamin = 0.0;
 trans.datamax = 255.0;
 trans.replacement = plvals.replace;
 trans.dsttyp = 'c';

 displayImage->create(width, height, 32); 
 currentRect.setX(0);
 currentRect.setY(0);
 currentRect.setWidth(width);
 currentRect.setHeight(height);
 
 fitsProgress.progressBar()->setTotalSteps(height);
 fitsProgress.setMinimumWidth(300);
 fitsProgress.show();

 /* FITS stores images with bottom row first. Therefore we have */
 /* to fill the image from bottom to top. */
   dest = data + height * width;
   
   for (i = height - 1; i >= 0 ; i--)
   {
     /* Read FITS line */
     dest -= width;
     if (fits_read_pixel (ifp, hdulist, width, &trans, dest) != width)
     {
       err = 1;
       break;
     }
     
       for (j = 0 ; j < width; j++)
       {
       val = dest[j];
       displayImage->setPixel(j, i, qRgb(val, val, val));
      }
      
      fitsProgress.progressBar()->setProgress(height - i);
   }
   
 reducedImgBuffer = data;
 convertImageToPixmap();
 
 if (err)
   KMessageBox::error(0, i18n("EOF encountered on reading."));
 
 return (err ? -1 : 0);
}

void FITSImage::convertImageToPixmap()
{
    qpix = kpix.convertToPixmap ( *(displayImage) );
}

void FITSImage::zoomToCurrent()
{

 double cwidth, cheight;
 
 if (currentZoom >= 0)
 {
   cwidth  = ((double) displayImage->width()) * pow(zoomFactor, currentZoom) ;
   cheight = ((double) displayImage->height()) * pow(zoomFactor, currentZoom);
 }
 else
 { 
   cwidth  = ((double) displayImage->width()) / pow(zoomFactor, abs(currentZoom)) ;
   cheight = ((double) displayImage->height()) / pow(zoomFactor, abs(currentZoom));
 }
  
 if (cwidth != displayImage->width() || cheight != displayImage->height())
 {
 	qpix = kpix.convertToPixmap (displayImage->smoothScale( (int) cwidth, (int) cheight));
        //imgFrame->resize( (int) width, (int) height);
        viewportResizeEvent (NULL);
	imgFrame->update();
 }
 else
 {
   qpix = kpix.convertToPixmap ( *displayImage );
   imgFrame->update();
  }
 
}


void FITSImage::fitsZoomIn()
{
   currentZoom++;
   viewer->actionCollection()->action("view_zoom_out")->setEnabled (true);
   if (currentZoom > 5)
     viewer->actionCollection()->action("view_zoom_in")->setEnabled (false);
   
   currentWidth  *= zoomFactor; //pow(zoomFactor, abs(currentZoom)) ;
   currentHeight *= zoomFactor; //pow(zoomFactor, abs(currentZoom));

   //kdDebug() << "Current width= " << currentWidth << " -- Current height= " << currentHeight << endl;
   
   qpix = kpix.convertToPixmap (displayImage->smoothScale( (int) currentWidth, (int) currentHeight));
   imgFrame->resize( (int) currentWidth, (int) currentHeight);

   update();
   viewportResizeEvent (NULL);  
}

void FITSImage::fitsZoomOut()
{
  currentZoom--;
  if (currentZoom < -5)
     viewer->actionCollection()->action("view_zoom_out")->setEnabled (false);
  viewer->actionCollection()->action("view_zoom_in")->setEnabled (true);
  
  currentWidth  /= zoomFactor; //pow(zoomFactor, abs(currentZoom));
  currentHeight /= zoomFactor;//pow(zoomFactor, abs(currentZoom));
  
  qpix = kpix.convertToPixmap (displayImage->smoothScale( (int) currentWidth, (int) currentHeight));
  imgFrame->resize( (int) currentWidth, (int) currentHeight);
   
  update();
  viewportResizeEvent (NULL);
}

void FITSImage::fitsZoomDefault()
{

  viewer->actionCollection()->action("view_zoom_out")->setEnabled (true);
  viewer->actionCollection()->action("view_zoom_in")->setEnabled (true);
  
  currentZoom   = 0;
  currentWidth  = width;
  currentHeight = height;
  
  qpix = kpix.convertToPixmap (*displayImage);
  imgFrame->resize( (int) currentWidth, (int) currentHeight);
  
  update();
  viewportResizeEvent (NULL);

}

FITSFrame::FITSFrame(FITSImage * img, QWidget * parent, const char * name) : QFrame(parent, name)
{
  image = img;
}

FITSFrame::~FITSFrame() {}

void FITSFrame::paintEvent(QPaintEvent * e)
{

 bitBlt(this, 0, 0, &(image->qpix));

}



#include "fitsimage.moc"
