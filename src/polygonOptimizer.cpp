/** Copyright (C) 2013 David Braam - Released under terms of the AGPLv3 License */
#include "polygonOptimizer.h"
#include "gcodeExport.h"
#include <vector>
#include <assert.h>
#include <float.h>
using std::vector;
#include <clipper/clipper.hpp>


namespace cura {

void optimizePolygon(PolygonRef poly)
{ 
  Point p0 = poly[poly.size()-1];
    for(unsigned int i=0;i<poly.size();i++)
    {
        Point p1 = poly[i];
        if (shorterThen(p0 - p1, MICRON2INT(10000)))
        {
            poly.remove(i);
            i --;
        }else if (shorterThen(p0 - p1, MICRON2INT(50000)))
        {
            Point p2;
            if (i < poly.size() - 1)
                p2 = poly[i+1];
            else
                p2 = poly[0];
            
            Point diff0 = normal(p1 - p0, 10000000);
            Point diff2 = normal(p1 - p2, 10000000);
            
            int64_t d = dot(diff0, diff2);
            if (d < -99999999999999LL)
            {
                poly.remove(i);
                i --;
            }else{
                p0 = p1;
            }
        }else{
            p0 = p1;
        }
    }
}

void optimizePolygons(Polygons& polys)
{
    for(unsigned int n=0;n<polys.size();n++)
    {
        optimizePolygon(polys[n]);
        if (polys[n].size() < 3)
        {
            polys.remove(n);
            n--;
        }
    }
}
void optimizePolygonadd(GCodePath* path)
{
    Point p0 = path->points[path->points.size()-1];
    for(unsigned int i=0;i<path->points.size();i++)
    {
        Point p1 = path->points[i];
        if (shorterThen(p0 - p1, MICRON2INT(10000)))
        {
        path->points.erase(path->points.begin()+i);
            i --;
        }
        else{
            p0 = p1;
        }
    }
}

void optimizeacuteangle(Point p0,Point p1,Point p2)
{
   if(p1.X>=p0.X&&p1.Y>=p0.Y)
   {
     if(p2.X<p1.X&&p2.Y>=p1.Y)
     {
       p2.X=p1.X;
     }
     if(p2.X<p1.X&&p2.Y<p1.Y)
     {
       p2=p1;
     }
     if(p2.X>=p1.X&&p2.Y<p1.Y)
     {
       p2.Y=p1.Y;
     }
   }
   if(p1.X>=p0.X&&p1.Y<=p0.Y)
   {
     if(p2.X>=p1.X&&p2.Y>p1.Y)
     {
       p2.Y=p1.Y;
     }
     if(p2.X<p1.X&&p2.Y<=p1.Y)
     {
       p2.X=p1.X;
     }
     if(p2.X<p1.X&&p2.Y>p1.Y)
     {
       p2=p1;
     }
   }
   if(p1.X<=p0.X&&p1.Y<=p0.Y)
   {
     if(p2.X>p1.X&&p2.Y>p1.Y)
     {
       p2=p1;
     }
     if(p2.X>p1.X&&p2.Y<=p1.Y)
     {
       p2.X=p1.X;
     }
     if(p2.X<=p1.X&&p2.Y>p1.Y)
     {
       p2.Y=p1.Y;
     }
   }
   if(p1.X<=p0.X&&p1.Y>=p0.Y)
   {
     if(p2.X>p1.X&&p2.Y>=p1.Y)
     {
       p2.X=p1.X;
     }
     if(p2.X>p1.X&&p2.Y<p1.Y)
     {
       p2=p1;
     }
     if(p2.X<=p1.X&&p2.Y<p1.Y)
     {
       p2.Y=p1.Y;
     }
   }
}
void optimizeacuteanglepoint(Point p3,Point p4,Point p5)
{

if(((p4.X>=p3.X)&&(p4.Y>=p3.Y)&&(p4.X>=p5.X)&&(p1.Y>=p5.Y))||((p4.X<=p3.X)&&(p4.Y<=p3.Y)&&(p4.X<=p5.X)&&(p4.Y<=p5.Y)))
                 {
                          if(((p4.x-p3.x)*(p4.y-p5.y))>=((p4.x-p5.x)*(p4.y-p3.y)))
                               {
                                Point insertp;
	                              insertp.x=p4.x;
	                              insertp.y=(p4.y+p5.y)*0.5;
                               }
                          else
                               {
                          	   Point insertp;
	                             insertp.x=(p4.x+p5.x)*0.5;
	                             insertp.y=p4.y;
                               }
        	     }
if(((p4.X>=p3.X)&&(p4.Y<=p3.Y))&&(p4.X>=p5.X)&&(p4.Y<=p5.Y))||((p4.X<=p3.X)&&(p4.Y>=p3.Y)&&(p4.X<=p5.X)&&(p4.Y>=p5.Y))))
        	     {
        	     	 if(((p4.x-p3.x)*(p4.y-p5.y))<=((p4.x-p5.x)*(p4.y-p3.y)))
                               {
                               Point insertp;
	                             insertp.x=p4.x;
	                             insertp.y=(p4.y+p5.y)*0.5;
                               }
                          else
                               {
                          	   Point insertp;
	                             insertp.x=(p4.x+p5.x)*0.5;
	                             insertp.y=p4.y;
                               }
        	     }

}

}//namespace cura
