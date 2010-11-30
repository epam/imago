#include "Draw.h"
#include "../src/Parameters.h"

namespace gga
{
    namespace Draw
    {        
        Image PointsToImage(const Points& src, int border)
        {
            Image result;
            Bounds b(src);
            result.setSize(b.getWidth() + border * 2, b.getHeight() + border * 2, IT_BW);
            result.clear();
            for (Points::const_iterator it = src.begin(); it != src.end(); it++)
                result.setPixel(it->X-b.getLeft() + border, it->Y-b.getTop() + border, INK);
            return result;
        }        

        Image RangeArrayToImage(const RangeArray& src)
        {
            return PointsToImage(src.toPoints());
        }
        
        void LineToImage(const Polyline& src, Image& image)
        {
            LineDefinition def = LineDefinition(127, getGlobalParams().getLineWidth() / 3);
            for (size_t u = 0; u < src.size() - 1; u++)
            {
                image.drawLine(src[u].X, src[u].Y, src[u+1].X, src[u+1].Y, def);
            }
        }
        
        Image LineToImage(const Polyline& line)
        {
            Points pts;
            for (size_t u = 0; u < line.size(); u++) // add line points to range points
                pts.push_back(line[u]);
            
            Bounds b(pts);
            Image result;
            
            LineDefinition def = LineDefinition(127, getGlobalParams().getLineWidth());
            result.setSize(b.getWidth() + def.Width * 2, b.getHeight() + def.Width * 2, IT_BW);            
            
            for (size_t u = 0; u < line.size() - 1; u++)
            {
                result.drawLine(line[u].X - b.getLeft() + def.Width, 
                                line[u].Y - b.getTop() + def.Width, 
                                line[u+1].X - b.getLeft() + def.Width, 
                                line[u+1].Y - b.getTop() + def.Width, 
                                def);
            }
            
            return result;
        }

        Image LineAprxToImage(const LinearApproximation& src)
        {
            if (!src.isGood())
                return RangeArrayToImage(src.getRange());

            Image result = LineToImage(src.getLine());
            result.drawImage(0, 0, PointsToImage(src.getRange().toPoints()), false);

            return result;
        }
        
        Image TriangleToImage(const Triangle& src)
        {
            Polyline p;
            
            // produce somewhat like line-loop:
            p.push_back(src.Vertex[0]);
            p.push_back(src.Vertex[1]);
            p.push_back(src.Vertex[2]);
            p.push_back(src.Vertex[0]);
            
            return LineToImage(p);
        }
    }
}

