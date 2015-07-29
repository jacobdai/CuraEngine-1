/** Copyright (C) 2013 David Braam - Released under terms of the AGPLv3 License */
#include "polygonOptimizer.h"

namespace cura {

void optimizePolygon(PolygonRef poly)
{
   
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

}//namespace cura
