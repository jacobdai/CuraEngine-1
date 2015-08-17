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
        if (shorterThen(p0 - p1, MICRON2INT(10)))
        {
            poly.remove(i);
            i --;
        }else if (shorterThen(p0 - p1, MICRON2INT(500)))
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
  Point p1 = path->points[0];
  if (shorterThen(p0 - p1, MICRON2INT(10)))
  {
    path->points[0]=path->points[path->points.size()-1];
    path->points->erase(path->points->begin() );
  }else if (shorterThen(p0 - p1, MICRON2INT(500)))
  {
    Point p2;
            if ( path->points.size() > 1)
                p2 = path->points[1];
            else
                p2 = path->points[0];
          
            Point diff0 = normal(p1 - p0, 10000000);
            Point diff2 = normal(p1 - p2, 10000000);
            
            int64_t d = dot(diff0, diff2);
            if (d < -99999999999999LL)
            {
                path->points[0]=path->points[path->points.size()-1];
            }
  }
p0 = path->points[0];
  for(unsigned int i=1;i<path->points.size();i++)
    {
        Point p1 = path->points[i];
        if (shorterThen(p0 - p1, MICRON2INT(10)))
        {
            path->points[i]=path->points[i-1];
        }else if (shorterThen(p0 - p1, MICRON2INT(500)))
        {
            Point p2;
            if (i < path->points.size() - 1)
                p2 = path->points[i+1];
            else
                p2 = path->points[0];
            
            Point diff0 = normal(p1 - p0, 10000000);
            Point diff2 = normal(p1 - p2, 10000000);
            
            int64_t d = dot(diff0, diff2);
            if (d < -99999999999999LL)
            {
                path->points[i]=path->points[i-1];
            }else{
                p0 = p1;
            }
        }else{
            p0 = p1;
        }
    }
}




}//namespace cura
