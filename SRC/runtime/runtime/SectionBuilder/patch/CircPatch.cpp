/* ****************************************************************** **
**    OpenSees - Open System for Earthquake Engineering Simulation    **
**          Pacific Earthquake Engineering Research Center            **
** ****************************************************************** */
//
// File: CircPatch.C
// Written by Remo M. de Souza
// December 1998
//
#include <CircPatch.h>
#include <CircSectionCell.h>
#include <Matrix.h>
#include <OPS_Stream.h>
#include <Patch.h>
#include <math.h>
#include <string>

CircPatch::CircPatch(int materialID, int numSubdivCircunf, int numSubdivRadial,
                     const VectorND<2>& centerPosition, double internRadius, double externRadius,
                     double initialAngle, double finalAngle)
 : matID(materialID),
   nDivCirc(numSubdivCircunf),
   nDivRad(numSubdivRadial),
   centerPosit(centerPosition),
   intRad(internRadius),
   extRad(externRadius),
   initAng(initialAngle),
   finalAng(finalAngle)
{
}

CircPatch::~CircPatch() {}

void
CircPatch::setMaterialID(int materialID)
{
  matID = materialID;
}

void
CircPatch::setDiscretization(int numSubdivCircunf, int numSubdivRadial)
{
  nDivRad  = numSubdivRadial;
  nDivCirc = numSubdivCircunf;
}

void
CircPatch::setRadii(double internRadius, double externRadius)
{
  intRad = internRadius;
  extRad = externRadius;
}

void
CircPatch::setAngles(double initialAngle, double finalAngle)
{
  initAng  = initialAngle;
  finalAng = finalAngle;
}

int
CircPatch::getMaterialID() const
{
  return matID;
}

void
CircPatch::getDiscretization(int& numSubdivCircunf, int& numSubdivRadial) const
{
  numSubdivCircunf = nDivCirc;
  numSubdivRadial  = nDivRad;
}

void
CircPatch::getRadii(double& internRadius, double& externRadius) const
{
  internRadius = intRad;
  externRadius = extRad;
}

void
CircPatch::getAngles(double& initialAngle, double& finalAngle) const
{
  initialAngle = initAng;
  finalAngle   = finalAng;
}

int
CircPatch::getNumCells() const
{
  return nDivCirc * nDivRad;
}

Cell**
CircPatch::getCells() const
{
  double pi = acos(-1.0);
  double deltaRad, deltaTheta;
  double initAngRadians, finalAngRadians;
  double rad_j, rad_j1, theta_i, theta_i1;
  Matrix cellVertCoord(4, 2);

  int numCells;
  Cell** cells;

  if (nDivRad > 0 && nDivCirc > 0) {
    numCells = this->getNumCells();

    cells = new Cell*[numCells];

    initAngRadians  = pi * initAng / 180.0;
    finalAngRadians = pi * finalAng / 180.0;

    deltaRad   = (extRad - intRad) / nDivRad;
    deltaTheta = (finalAngRadians - initAngRadians) / nDivCirc;

    int k = 0;
    for (int j = 0; j < nDivRad; j++) {
      rad_j  = intRad + deltaRad * j;
      rad_j1 = rad_j + deltaRad;

      for (int i = 0; i < nDivCirc; i++) {
        // compute coordinates

        theta_i = initAngRadians + deltaTheta * i;

        theta_i1 = theta_i + deltaTheta / 2.0;
        cells[k] = new CircSectionCell(rad_j, rad_j1, deltaTheta, theta_i1, centerPosit(0), centerPosit(1));

        k++;
      }
    }
  } else
    return 0;

  return cells;
}
