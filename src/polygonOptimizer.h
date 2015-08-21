/** Copyright (C) 2013 David Braam - Released under terms of the AGPLv3 License */
#ifndef POLYGON_OPTIMIZER_H
#define POLYGON_OPTIMIZER_H

#include "utils/polygon.h"
#include "gcodeExport.h"

namespace cura {

void optimizePolygon(PolygonRef poly);

void optimizePolygons(Polygons& polys);

void optimizePolygonadd(GCodePath* path);

void optimizeacuteangle(Point p0,Point p1,Point p2);

}//namespace cura

#endif//POLYGON_OPTIMIZER_H
