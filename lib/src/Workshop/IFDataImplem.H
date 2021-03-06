#ifdef CH_LANG_CC
/*
 *      _______              __
 *     / ___/ /  ___  __ _  / /  ___
 *    / /__/ _ \/ _ \/  V \/ _ \/ _ \
 *    \___/_//_/\___/_/_/_/_.__/\___/
 *    Please refer to Copyright.txt, in Chombo's root directory.
 */
#endif

#ifndef _IFDATAIMPLEM_H_
#define _IFDATAIMPLEM_H_

#if defined(CH_Darwin) && defined(__GNUC__) && ( __GNUC__ == 3 )
// deal with the broken isnan()/isinf() in GCC on MacOS
#include <unistd.h>
#define _GLIBCPP_USE_C99 1
#endif

#include <iostream>
#include <iomanip>

#include "MultiIndex.H"
#include "NormalDerivative.H"
#include "NormalDerivativeNew.H"
#include "ParmParse.H"
#include "NamespaceHeader.H"

// empty constructor
template <int dim> IFData<dim>::IFData()
{
  m_function = NULL;
}


template <int dim> IFData<dim>::IFData(const IFData<dim>& a_IFData)
  :m_cornerSigns       (a_IFData.m_cornerSigns),
   m_intersections     (a_IFData.m_intersections),
   m_globalCoord       (a_IFData.m_globalCoord),
   m_cellCenterCoord   (a_IFData.m_cellCenterCoord),
   m_parentCoord       (a_IFData.m_parentCoord),
   m_localCoord        (a_IFData.m_localCoord),
   m_maxOrder          (a_IFData.m_maxOrder),
   m_normalDerivatives (a_IFData.m_normalDerivatives),
   m_badNormal         (a_IFData.m_badNormal),
   m_allVerticesIn     (a_IFData.m_allVerticesIn),
   m_allVerticesOut    (a_IFData.m_allVerticesOut),
   m_allVerticesOn     (a_IFData.m_allVerticesOn)
{
  CH_TIME("ifdata constructor 1");
  if (a_IFData.m_function != NULL)
    {
      // IFData always owns the data pointed to by m_function
      m_function = new IFSlicer<dim>(*(a_IFData.m_function));
    }
  else
    {
      m_function = NULL;
    }
}



// Constructor from the implicit function at the highest dimension
template <int dim> IFData<dim>::IFData(const BaseIF & a_function,
                                       const RvDim  & a_dx,
                                       const RvDim  & a_cellCenter,
                                       const int    & a_maxOrder)
  :m_globalCoord    (a_cellCenter,a_dx),
   m_cellCenterCoord(IndexTM<Real,dim>::Zero,a_dx),
   m_parentCoord    (IndexTM<Real,dim>::Zero,a_dx),
   m_maxOrder       (a_maxOrder)
{
  CH_TIME("ifdata constructor 2");
  // Copy the implicit function; m_function is a pointer
  // IFData always owns the data pointed to by m_function
  m_function = new IFSlicer<dim>(a_function);

  // makecornerSigns sets the bools m_allVerticesIn and m_allVerticesOut,
  // and m_allVerticesOn
  makeCornerSigns();

  // find an intersection point, if it exists, of the interface with each edge
  findIntersectionPts();

  // will ultimately assign m_localCoord.m_origin to the the average of
  // intersection points.  If full or empty cell, m_localCoord.m_origin = m_parentCoord.m_origin
  defineLocalCoords();

  // set the normal derivatives
  setNormalDerivatives();
}

// Constructor using IFSlicer, used for refinement
template <int dim> IFData<dim>::IFData(IFSlicer<dim> * a_function,
                                       const RvDim   & a_dx,
                                       const RvDim   & a_cellCenter,
                                       const int     & a_maxOrder)
  :m_globalCoord    (a_cellCenter,a_dx),
   m_cellCenterCoord(IndexTM<Real,dim>::Zero,a_dx),
   m_parentCoord    (IndexTM<Real,dim>::Zero,a_dx),
   m_maxOrder       (a_maxOrder)
{
  CH_TIME("ifdata constructor 3");
  // IFData always owns the data pointed to by m_function
  m_function = new IFSlicer<dim>(*a_function);

  // makeCornerSigns sets the bools m_allVerticesIn and m_allVerticesOut,
  // and m_allVerticesOn
  makeCornerSigns();

  // Find an intersection point, if it exists, of the interface with each edge
  findIntersectionPts();

  //
  defineLocalCoords();

  // Set the normal derivatives
  setNormalDerivatives();
}

template <int dim> IFData<dim>::IFData(const IFData<dim+1> & a_hIFData,
                                       const int           & a_maxOrder,
                                       const int           & a_idir,
                                       const int           & a_hilo)
{
  CH_TIME("ifdata constructor 4");
  typedef IndexTM<int,dim+1>HDEdgeIndex;
  typedef map<HDEdgeIndex,Real > HDEdgeIntersections;
  typedef IndexTM<int,dim+1>HDVertex;
  typedef map<HDVertex,int > HDCornerSigns;

  // pm = -1 for lo and 1 for hi
  int pm = a_hilo;
  pm *= 2;
  pm -= 1;

  // fixedComp and fixed value are used by the slicer to pull points into one higher dimension
  int fixedComp   = a_idir;
  Real fixedValue = a_hIFData.m_globalCoord.convertDir(pm*0.5*a_hIFData.m_cellCenterCoord.m_dx[fixedComp],
                                                       a_hIFData.m_cellCenterCoord,
                                                       fixedComp);

  // IFData always owns the data pointed to by m_function
  m_function = new IFSlicer<dim>(a_hIFData.m_function,fixedComp,fixedValue);

  // reduce the global coordinate system
  m_globalCoord = CoordinateSystem<dim>(a_hIFData.m_globalCoord,fixedComp);

  // Construct cell centered coord sys
  m_cellCenterCoord = CoordinateSystem<dim>(a_hIFData.m_cellCenterCoord,fixedComp);

  // reduce the parent coordinate system
  m_parentCoord = CoordinateSystem<dim>(a_hIFData.m_localCoord,fixedComp);

  m_allVerticesIn  = true;
  m_allVerticesOut = true;
  m_allVerticesOn  = true;

  for (typename HDCornerSigns::const_iterator it = a_hIFData.m_cornerSigns.begin();
       it != a_hIFData.m_cornerSigns.end(); ++it)
    {
      const HDVertex& hVertex = it->first;
      if (hVertex[fixedComp] == a_hilo)
        {
          Vertex vertex;
           for (int j = 0; j < dim; ++j)
             {
               if (j < fixedComp)
                 {
                   vertex[j] = hVertex[j];
                 }
               else
                 {
                   vertex[j] = hVertex[j+1];
                 }
             }

           m_cornerSigns[vertex] = it->second;
           if (m_cornerSigns[vertex] == IN)
             {
               m_allVerticesOut = false;
               m_allVerticesOn  = false;
             }
           else if (m_cornerSigns[vertex] == OUT)
             {
               m_allVerticesIn = false;
               m_allVerticesOn = false;
             }
        }
    }

  for (typename HDEdgeIntersections::const_iterator it = a_hIFData.m_intersections.begin();
       it != a_hIFData.m_intersections.end(); ++it)
    {
      const HDEdgeIndex& hEdgeIndex = it->first;
      EdgeIndex edgeIndex;
      int hEdgeDir = hEdgeIndex[0];

      // only interested in edgeDir != fixedComp(ie. a_idir)
      // Among these, need to find edges in the higher dim that are a_hilo in the fixedComp component.
      // Example: fixedComp = 1; a_hilo = 0. Observe that an edge in the x (N.B 0 < 1) direction
      // might look like (edgeDir= 0:yhilo,zhilo ). In other words, y information is in index 1.
      // On the other hand, an edge in the z direction(N.B 2 > 1)
      // might look like (edgeDir= 0:xhilo,yhilo ). In other words, y information is in index 2.
      if (hEdgeDir < fixedComp)
        {
          if (hEdgeIndex[fixedComp] == a_hilo)
            {
              edgeIndex[0] = hEdgeDir;

              for (int j = 1; j < dim; ++j)
                {
                  if (j < fixedComp)
                    {
                      edgeIndex[j] = hEdgeIndex[j];
                    }
                  else
                    {
                      edgeIndex[j] = hEdgeIndex[j+1];
                    }
                }
              m_intersections[edgeIndex] = it->second;
            }
        }
      else if (hEdgeDir > fixedComp)
        {
           if (hEdgeIndex[fixedComp+1] == a_hilo)
            {
              edgeIndex[0] = hEdgeDir - 1;

              for (int j = 1; j < dim; ++j)
                {
                  if (j < fixedComp+1)
                    {
                      edgeIndex[j] = hEdgeIndex[j];
                    }
                  else
                    {
                      edgeIndex[j] = hEdgeIndex[j+1];
                    }
                }
              m_intersections[edgeIndex] = it->second;
            }
        }
    }

  // assigns m_localCoord.m_origin to the the average of intersection points
  // or to m_parentCoord.m_origin if there are no interesection points
  defineLocalCoords();

  m_maxOrder = a_maxOrder;

  if (!m_allVerticesOut && !m_allVerticesIn)
    {
      setNormalDerivatives();
    }
  else
    {
      m_badNormal = false;
    }
}

// Destructor
template <int dim> IFData<dim>::~IFData()
{
  if (m_function != NULL)
  {
    delete m_function;
  }
}

template <int dim> void IFData<dim>::setNormalDerivatives()
{
  CH_TIME("ifdata::setNormalDerivatives");
  NormalDerivativeNew<dim> normalDerivative;
  typename NormalDerivativeNew<dim>::NormalDerivativeMap ndMap = 
    normalDerivative.calculateAll(m_maxOrder,
                                  m_globalCoord.convert(RvDim::Zero,m_localCoord),
                                  m_function);


  for (int order = 0; order <= m_maxOrder; order++)
  {
    Vector<IvDim> derivatives;

    generateMultiIndices(derivatives,order);

    for (int i = 0; i < derivatives.size(); ++i)
    {
      const IvDim & derivative = derivatives[i];

      for (int idir = 0; idir < dim; idir++)
      {
        m_normalDerivatives[derivative][idir] = ndMap[derivative][idir];
      }
    }
  }

  //took out a bad Normal Check  --- normalDerivativeNew does not have a getMagnitudeOfGradient
  m_badNormal = false;
}

template <int dim> void IFData<dim>::makeCornerSigns()
{
  CH_TIME("ifdata::makeCornerSigns");
  m_allVerticesIn  = true;
  m_allVerticesOut = true;
  m_allVerticesOn  = true;

  Vertex vertex;

  // there are 2^dim vertices
  int numVertices = 1 << dim;

  for (int i = 0; i < numVertices; ++i)
  {
    int ii = i;

    //label the ith vertex by i represented in base 2
    for (int j = 0; j < dim; ++j)
    {
      //convert to base 2 by successivly "anding" with 1 and shifting
      vertex[j] = (ii & 1);
      ii = ii >> 1;
    }

    // represent the vertex as an RvDim in cell centered coordinates
    RvDim corner;
    for (int idir = 0; idir < dim; ++idir)
      {
        corner[idir] = vertex[idir] - 0.5;
        corner[idir] *= m_cellCenterCoord.m_dx[idir];
      }

    // compute coordinates of a_vertex in global coordinates
    RvDim cornerCoord = m_globalCoord.convert(corner,m_cellCenterCoord);

    // evaluate (zeroth derivative of) the function at the corner
    Real val = m_function->value(IndexTM<int,dim>::Zero,cornerCoord);

    // true = negative = in the computational domain = in the fluid
    if (val < -MACHINEPRECISION)
    {
      m_cornerSigns[vertex] = IN;
      m_allVerticesOut      = false;
      m_allVerticesOn       = false;
    }
    else if (val > MACHINEPRECISION)
    {
      m_cornerSigns[vertex] = OUT;
      m_allVerticesIn       = false;
      m_allVerticesOn       = false;
    }
    else
    {
      m_cornerSigns[vertex] = ON;
    }
  }
}

template <int dim> void IFData<dim>::findIntersectionPts()
{
  CH_TIME("ifdata::findIntersectionPoints");
  // double iterate through corner signs map,writing to a edge map if corner1 is connected to corner2
  for (typename CornerSigns::const_iterator it0 = m_cornerSigns.begin();
       it0 != m_cornerSigns.end(); ++it0)
  {
    for (typename CornerSigns::const_iterator it1 = m_cornerSigns.begin();
         it1 != m_cornerSigns.end(); ++it1)
    {
      int edgeDir = LARGEINTVAL;
      if (isConnected(edgeDir,it0->first,it1->first))
      {
        // make edge key:m_intersections[thisEdge] = pt;
        makeEdgeKey(edgeDir,it0->first,it1->first);
      }
    }
  }
}
template <int dim> void IFData<dim>::defineLocalCoords()
{
  CH_TIME("ifdata::defineLocalCoords");
  // default for full cells
  m_localCoord.m_origin = m_cellCenterCoord.m_origin;
  /**
     the stuff below makes convergence tests do weird things
     but makes for better answers
  */
  if(!LocalCoordMoveSwitch::s_turnOffMoveLocalCoords)
    {
      if (m_intersections.size() == 0)
        {
          m_localCoord.m_origin = m_cellCenterCoord.m_origin;
        }
      else
        {
          m_localCoord.m_origin = RvDim::Zero;

          for (typename EdgeIntersections::const_iterator it = m_intersections.begin();it != m_intersections.end(); ++it)
            {
              const EdgeIndex& edgeIndex = it->first;
              const Real&      intercept = it->second;

              int varyDir = edgeIndex[0];

              RvDim intersectPt = RvDim::Zero;
              intersectPt[varyDir] = intercept;

              for (int i = 1; i < dim; i++)
                {
                  int curDir;
                  int loHi;

                  if (i <= varyDir)
                    {
                      curDir = i-1;
                    }
                  else
                    {
                      curDir = i;
                    }

                  loHi = edgeIndex[i];
                  intersectPt[curDir]  = loHi - 0.5;
                  intersectPt[curDir] *= m_globalCoord.m_dx[curDir];
                }

              m_localCoord.m_origin -= intersectPt;
            }

          m_localCoord.m_origin /= m_intersections.size();
        }
    }
  /**/
  m_localCoord.m_dx =  m_globalCoord.m_dx;
}

template <int dim> bool IFData<dim>::isConnected(int          & a_edgeDir,
                                                 const Vertex & a_vertex1,
                                                 const Vertex & a_vertex2)
{
  CH_TIME("ifdata::isConnected");
  bool connected = true;
  int numDif = 0;

  // connected = true if and only if  (a_vertex1 and a_vertex2 differ in
  // exactly one coordinate.)
  for (int idir = 0; idir < dim; ++idir)
  {
    if (a_vertex1[idir] != a_vertex2[idir])
    {
      // vertices differ in idir direction
      a_edgeDir = idir;
      numDif += 1;
    }
  }

  if (numDif == 1)
  {
    connected = true;
  }
  else
  {
    connected = false;
    a_edgeDir = LARGEINTVAL;
  }

  return connected;
}

template <int dim> void IFData<dim>::makeEdgeKey(const int    & a_edgeDir,
                                                 const Vertex & a_vertex0,
                                                 const Vertex & a_vertex1)
{
  CH_TIME("ifdata::makeedgekey");
  EdgeIndex thisEdge;

  thisEdge[0] = a_edgeDir;

  for (int idir = 0; idir < dim; ++idir)
  {
    if (idir < a_edgeDir)
    {
      thisEdge[idir + 1] = a_vertex0[idir];
    }
    else if (idir > a_edgeDir)
    {
      thisEdge[idir] = a_vertex0[idir];
    }
  }

  int lo = LARGEINTVAL;
  int hi = LARGEINTVAL;

  if (m_cornerSigns.find(a_vertex0)!= m_cornerSigns.end())
  {
    lo = m_cornerSigns.find(a_vertex0)->second;
  }
  else
  {
    MayDay::Abort("Vertex not well defined in makeEdgeKey");
  }

  if (m_cornerSigns.find(a_vertex1)!= m_cornerSigns.end())
  {
    hi = m_cornerSigns.find(a_vertex1)->second;
  }
  else
  {
    MayDay::Abort("Vertex not well defined in makeEdgeKey");
  }

  if ((lo == OUT && hi == IN) || (lo == IN && hi == OUT))
  {
    // calculate a value between -1.0 and 1.0
    Real pt = rootFinder(thisEdge);

    // check whether intersection is at the end point
    bool hiOn = false;
    bool loOn = false;

    checkIntersection(hiOn,loOn,pt);

    if (hiOn || loOn)
    {
      if (loOn)
      {
        m_cornerSigns[a_vertex0] = ON;
        lo = ON;
      }

      if (hiOn)
      {
        m_cornerSigns[a_vertex1] = ON;
        hi = ON;
      }

      remakeCornerSigns();
    }
    else
    {
      // Scale the intersection between -dx/2 and dx/2
      m_intersections[thisEdge] = 0.5*m_cellCenterCoord.m_dx[thisEdge[0]]*pt;
    }
  }

  // If the edge in full and the EB intersects a vertex, record an
  // intersection point
  if (lo == IN && hi == ON)
  {
    // Intersection at -dx/2
    m_intersections[thisEdge] = -0.5*m_cellCenterCoord.m_dx[thisEdge[0]];
  }
  else if (lo == ON && hi == IN)
  {
    // Intersection at dx/2
    m_intersections[thisEdge] =  0.5*m_cellCenterCoord.m_dx[thisEdge[0]];
  }
}

template <int dim> Real IFData<dim>::rootFinder(const EdgeIndex& a_thisEdge)
{
  CH_TIME("ifdata::rootfinder");
  Real pt = LARGEREALVAL;

  // the first component of an EdgeIndex gives the direction of the edge
  // the other components select an edge by using the same (lo-hi) logic as Vertex
  int edgeDir = a_thisEdge[0];

  Vertex loEnd;
  loEnd[edgeDir] = 0;

  Vertex hiEnd;
  hiEnd[edgeDir] = 1;

  for (int idir = 0; idir < dim; ++idir)
  {
    if (idir < edgeDir)
    {
      loEnd[idir] = a_thisEdge[idir + 1];
      hiEnd[idir] = a_thisEdge[idir + 1];
    }
    else if (idir > edgeDir)
    {
      loEnd[idir] = a_thisEdge[idir];
      hiEnd[idir] = a_thisEdge[idir];
    }
  }

  // computes coordinates of a_vertex relative to the cell-center = (0,0...0)
  // represent the vertex as an RvDim in cell centered coordinates
  RvDim loCorner;
  RvDim hiCorner;
  for (int idir = 0; idir < dim; ++idir)
    {
      loCorner[idir] = (loEnd[idir] - 0.5) * m_cellCenterCoord.m_dx[idir];
      hiCorner[idir] = (hiEnd[idir] - 0.5) * m_cellCenterCoord.m_dx[idir];
    }

  // compute coordinates of a_vertex in global coordinates
  RvDim loPt = m_globalCoord.convert(loCorner,m_cellCenterCoord);
  RvDim hiPt = m_globalCoord.convert(hiCorner,m_cellCenterCoord);

  // range of possible roots
  // Real smallestRoot = -0.5;
  // Real biggestRoot  =  0.5;

  // returns a number between -1.0 and 1.0
  pt = BrentRootFinder(loPt,hiPt,edgeDir); // ,smallestRoot,biggestRoot);

  // return this result
  return pt;
}

template <int dim> Real IFData<dim>::BrentRootFinder(const RvDim & a_loPt,
                                                     const RvDim & a_hiPt,
                                                     const int   & a_edgeDir) const
//                                                     const Real  & a_smallestRoot,
//                                                     const Real  & a_biggestRoot) const
{
  CH_TIME("ifdata::brentrootfinder");
  // Max allowed iterations and floating point precision
  const unsigned int MAXITER = 100;
  const Real         EPS   = 3.0e-15;

  unsigned int i;
  Real aPt;
  Real bPt;
  Real c, fa, fb, fc;
  Real d, e;
  Real tol1, xm;
  Real p, q, r, s;

  aPt = -1.0; // a_smallestRoot;
  bPt =  1.0; // a_biggestRoot;

  RvDim physCoordAPt = a_loPt;
  RvDim physCoordBPt = a_hiPt;

  fa = -m_function->value(IndexTM<int,dim>::Zero,physCoordAPt);
  fb = -m_function->value(IndexTM<int,dim>::Zero,physCoordBPt);

  //  Init these to be safe
  c = d = e = 0.0;

  if (fb*fa > 0)
  {
    pout() << "fa " << fa << " fb " << fb <<endl;
    MayDay::Abort("IFData::BrentRootFinder. Root must be bracketed, but instead the supplied end points have the same sign.");
  }

  fc = fb;

  for (i = 0; i < MAXITER; i++)
  {
    if (fb*fc > 0)
    {
      //  Rename a, b, c and adjust bounding interval d
      c = aPt;
      fc  = fa;
      d = bPt - aPt;
      e = d;
    }

    if (Abs(fc) < Abs(fb))
    {
      aPt = bPt;
      bPt = c;
      c = aPt;
      fa  = fb;
      fb  = fc;
      fc  = fa;
    }

    //  Convergence check
    tol1  = 2.0 * EPS * Abs(bPt) + 0.5 * TOLERANCE;
    xm    = 0.5 * (c - bPt);

    if (Abs(xm) <= tol1 || fb == 0.0)
    {
      break;
    }

    if (Abs(e) >= tol1 && Abs(fa) > Abs(fb))
    {
      //  Attempt inverse quadratic interpolation
      s = fb / fa;
      if (aPt == c)
      {
        p = 2.0 * xm * s;
        q = 1.0 - s;
      }
      else
      {
        q = fa / fc;
        r = fb / fc;
        p = s * (2.0 * xm * q * (q-r) - (bPt-aPt) * (r-1.0));
        q = (q-1.0) * (r-1.0) * (s-1.0);
      }

      //  Check whether in bounds
      if (p > 0) q = -q;

      p = Abs(p);

      if (2.0 * p < Min(((Real)3.0)*xm*q-Abs(tol1*q), Abs(e*q)))
      {
        //  Accept interpolation
        e = d;
        d = p / q;
      }
      else
      {
        //  Interpolation failed, use bisection
        d = xm;
        e = d;
      }
    }
    else
    {
      //  Bounds decreasing too slowly, use bisection
      d = xm;
      e = d;
    }

    //  Move last best guess to a
    aPt = bPt;
    fa  = fb;

    //  Evaluate new trial root
    if (Abs(d) > tol1)
    {
      bPt = bPt + d;
    }
    else
    {
      if (xm < 0) bPt = bPt - tol1;
      else        bPt = bPt + tol1;
    }

    physCoordBPt[a_edgeDir] = ((1.0 - bPt)/2.0) * a_loPt[a_edgeDir] + ((1.0 + bPt)/2.0) * a_hiPt[a_edgeDir];
    fb = -m_function->value(IndexTM<int,dim>::Zero,physCoordBPt);
  }

  if (i >= MAXITER)
  {
    cerr << "BrentRootFinder: exceeding maximum iterations: "
         << MAXITER
         << "\n";
  }

  return bPt;
}

template <int dim> void IFData<dim>::checkIntersection(bool       & a_hiOn,
                                                       bool       & a_loOn,
                                                       const Real & a_pt) const
{
  a_hiOn = false;
  a_loOn = false;

  if (a_pt >= 1.0 - MACHINEPRECISION)
  {
    a_hiOn = true;
  }
  else if (a_pt <= -1.0 + MACHINEPRECISION)
  {
    a_loOn = true;
  }
}

template <int dim> void IFData<dim>::remakeCornerSigns()
{
  m_allVerticesIn  = true;
  m_allVerticesOut = true;
  m_allVerticesOn  = true;

  for (typename CornerSigns::const_iterator it = m_cornerSigns.begin();
       it != m_cornerSigns.end(); ++it)
  {
    if (it->second == IN)
    {
      m_allVerticesOut = false;
    }

    if (it->second == OUT)
    {
      m_allVerticesIn = false;
    }

    if (it->second != ON)
    {
      m_allVerticesOn = false;
    }
  }
}

template <int dim> void IFData<dim>::print(ostream& a_out) const
{

  // EdgeIndex first component is direction that edge varies. Subsequent components indicate hi-lo position of edge
  string padding = "  ";
  for (int i = 0; i < GLOBALDIM - dim; i++)
  {
    padding += "  ";
  }

  if (m_function == NULL)
  {
    a_out << padding << "undefined IFData\n";
  }
  else
  {
    for (typename CornerSigns::const_iterator it = m_cornerSigns.begin();
         it != m_cornerSigns.end(); ++it)
    {
      a_out << padding << "Vertex "
                       << "("
                       << it->first
                       << ") = "
                       << it->second
                       << "\n";
    }
    a_out << padding << "\n";

    for (typename EdgeIntersections::const_iterator it = m_intersections.begin();
         it != m_intersections.end(); ++it)
    {
      //save output format
      std::ios::fmtflags origFlags = a_out.flags();
      int origWidth = a_out.width();
      int origPrecision = a_out.precision();

      a_out << padding << "EdgeIndex "
                       << it->first
                       << " = "
                       << setprecision(16)
                       << setiosflags(ios::showpoint)
                       << setiosflags(ios::scientific)
                       << it->second
                       << "\n";

      //restore output format
      a_out.flags(origFlags);
      a_out.width(origWidth);
      a_out.precision(origPrecision);
    }
    a_out << padding << "\n";

    a_out << padding << "m_allVerticesIn  = " << m_allVerticesIn  << "\n";
    a_out << padding << "m_allVerticesOut = " << m_allVerticesOut << "\n";
    a_out << padding << "m_allVerticesOn  = " << m_allVerticesOn  << "\n";
    a_out << padding << "m_badNormal      = " << m_badNormal      << "\n";
    a_out << padding << "\n";

    if (!m_allVerticesOut)
    {
      a_out << padding << "m_globalCoord     = " << m_globalCoord    ;
      a_out << padding << "m_cellCenterCoord = " << m_cellCenterCoord;
      a_out << padding << "m_parentCoord     = " << m_parentCoord    ;
      a_out << padding << "m_localCoord      = " << m_localCoord     ;
      a_out << padding << "\n";

      a_out << padding << "m_maxOrder = " << m_maxOrder << "\n";

      std::ios::fmtflags origFlags = a_out.flags();
      int origWidth = a_out.width();
      int origPrecision = a_out.precision();

      a_out << padding << "m_normalDerivatives = " << "\n";

      for (typename NormalDerivatives::const_iterator it = m_normalDerivatives.begin();
           it != m_normalDerivatives.end();
           ++it)
      {
        for (int idir = 0; idir < dim; idir++)
        {
          a_out << padding << "  "
                           << it->first
                           << ", "
                           << idir
                           << ": "
                           << setprecision(16)
                           << setiosflags(ios::showpoint)
                           << setiosflags(ios::scientific)
                           << (it->second)[idir]
                           << "\n";
        }
        a_out << padding << "\n";
      }

      a_out.flags(origFlags);
      a_out.width(origWidth);
      a_out.precision(origPrecision);
    }
    else
    {
      a_out << padding << "All vertices out" << "\n";
    }
    a_out << padding << "\n";
  }
}

template <int dim> ostream& operator<<(ostream           & a_out,
                                       const IFData<dim> & a_IFData)
{
  a_IFData.print(a_out);
  return a_out;
}

// equals operator
template <int dim> void IFData<dim>::operator=(const IFData & a_IFData)
{
  // Only do something if the objects are distinct
  if (this != &a_IFData)
  {
    // Delete the existing m_function (if there is one)
    if (m_function != NULL)
    {
      delete m_function;
    }

    // IFData always owns the data pointed to by m_function
    if (a_IFData.m_function != NULL)
    {
      m_function = new IFSlicer<dim>(*(a_IFData.m_function));
    }
    else
    {
      m_function = NULL;
    }

    m_cornerSigns       = a_IFData.m_cornerSigns;
    m_intersections     = a_IFData.m_intersections;

    m_globalCoord       = a_IFData.m_globalCoord;
    m_cellCenterCoord   = a_IFData.m_cellCenterCoord;
    m_parentCoord       = a_IFData.m_parentCoord;
    m_localCoord        = a_IFData.m_localCoord;

    m_maxOrder          = a_IFData.m_maxOrder;
    m_normalDerivatives = a_IFData.m_normalDerivatives;
    m_badNormal         = a_IFData.m_badNormal;

    m_allVerticesIn     = a_IFData.m_allVerticesIn;
    m_allVerticesOut    = a_IFData.m_allVerticesOut;
    m_allVerticesOn     = a_IFData.m_allVerticesOn;
  }
}

#include "NamespaceFooter.H"

#endif
