#ifdef CH_LANG_CC
/*
 *      _______              __
 *     / ___/ /  ___  __ _  / /  ___
 *    / /__/ _ \/ _ \/  V \/ _ \/ _ \
 *    \___/_//_/\___/_/_/_/_.__/\___/
 *    Please refer to Copyright.txt, in Chombo's root directory.
 */
#endif

#ifndef _NWOEBVISCOUSTENSOROPFACTORY_H_
#define _NWOEBVISCOUSTENSOROPFACTORY_H_

#include "REAL.H"
#include "Box.H"
#include "FArrayBox.H"
#include "Vector.H"
#include <map>
#include "RefCountedPtr.H"

#include "AMRMultiGrid.H"

#include "EBIndexSpace.H"
#include "EBCellFAB.H"
#include "EBCellFactory.H"
#include "NWOEBViscousTensorOp.H"
#include "EBLevelDataOps.H"
#include "BaseEBBC.H"
#include "BaseDomainBC.H"
#include "CFIVS.H"
#include "EBFluxRegister.H"
#include "EBMGAverage.H"
#include "EBMGInterp.H"
#include "EBCoarsen.H"
#include "PolyGeom.H"
#include "EBAMRPoissonOp.H"
#include "EBLevelGrid.H"
#include "BaseDomainBC.H"
#include "BaseEBBC.H"
#include "NamespaceHeader.H"

///
/**
   Factory class to generate NWOEBViscousTensorOps.  This follows the AMRLevelOpFactory interface.
*/
class NWOEBViscousTensorOpFactory: public AMRLevelOpFactory<LevelData<EBCellFAB> >
{
public:

  ///
  virtual ~NWOEBViscousTensorOpFactory();

  //! Constructs a factory that builds EBViscousTensorOps with time-independent
  //! A and B coefficients.
  NWOEBViscousTensorOpFactory(const Vector<EBLevelGrid>&                                  a_eblgs,
                              const Real&                                                 a_alpha,
                              const Real                                         &        a_beta,
                              const Vector<RefCountedPtr<LevelData<EBCellFAB> > >&        a_acoef,
                              const Vector<RefCountedPtr<LevelData<EBFluxFAB> > >&        a_eta,
                              const Vector<RefCountedPtr<LevelData<EBFluxFAB> > >&        a_lambda,
                              const Vector<RefCountedPtr<LevelData<BaseIVFAB<Real> > > >& a_etaIrreg,
                              const Vector<RefCountedPtr<LevelData<BaseIVFAB<Real> > > >& a_lambdaIrreg,
                              const Real&                                                 a_dxCoarse,
                              const Vector<int>&                                          a_refRatio,
                              const RefCountedPtr<BaseDomainBCFactory>&                   a_domainBCFactory,
                              const RefCountedPtr<BaseEBBCFactory>    &                   a_ebBcFactory,
                              const IntVect&                                              a_ghostCellsPhi,
                              const IntVect&                                              a_ghostCellsRhs,
                              int a_numLevels = -1,
                              bool a_noMG = false);


  ///just for convenience in load balancing manufactures coefficients and everything
  NWOEBViscousTensorOpFactory(EBLevelGrid                             &    a_eblgs,
                              const RefCountedPtr<BaseDomainBCFactory>&    a_domainBCFactory,
                              const RefCountedPtr<BaseEBBCFactory>    &    a_ebBcFactory);

  ///
  virtual NWOEBViscousTensorOp*
  MGnewOp(const ProblemDomain& a_FineindexSpace,
          int                  a_depth,
          bool                 a_homoOnly = true);


  /// This for introspective load balance
  virtual NWOEBViscousTensorOp*
  newOpForTimingOnly(const ProblemDomain& a_FineindexSpace);


  NWOEBViscousTensorOp* createOperator(const EBLevelGrid&             a_eblgMGLevel,
                                       const EBLevelGrid&             a_eblgCoarMG,
                                       const bool&                    a_hasMGObjects,
                                       const RealVect&                a_dxMGLevel,
                                       const RealVect&                a_dxCoar,
                                       const int&                     a_whichLevel);
  ///
  virtual void reclaim(MGLevelOp<LevelData<EBCellFAB> >* a_reclaim);

  ///
  virtual NWOEBViscousTensorOp*
  AMRnewOp(const ProblemDomain& a_FineindexSpace);

  ///
  virtual void AMRreclaim(NWOEBViscousTensorOp* a_reclaim);

  ///
  /** Refinement ratio between this level and coarser level.
      Returns 1 when there are no coarser AMRLevelOp objects */
  virtual int refToFiner(const ProblemDomain& a_domain) const;


  void resetCoefficients(const Real&                                                 a_alpha,
                         const Real                                         &        a_beta,
                         const Vector<RefCountedPtr<LevelData<EBCellFAB> > >&        a_acoef,
                         const Vector<RefCountedPtr<LevelData<EBFluxFAB> > >&        a_eta,
                         const Vector<RefCountedPtr<LevelData<EBFluxFAB> > >&        a_lambda,
                         const Vector<RefCountedPtr<LevelData<BaseIVFAB<Real> > > >& a_etaIrreg,
                         const Vector<RefCountedPtr<LevelData<BaseIVFAB<Real> > > >& a_lambdaIrreg)
  {
    m_alpha       = a_alpha      ;
    m_beta        = a_beta       ;
    m_acoef       = a_acoef      ;
    m_eta         = a_eta        ;
    m_lambda      = a_lambda     ;
    m_etaIrreg    = a_etaIrreg   ;
    m_lambdaIrreg = a_lambdaIrreg;
  }

  IntVect getGhostPhi() const
  {
    return m_ghostCellsPhi;
  }
  IntVect getGhostRHS() const
  {
    return m_ghostCellsRhs;
  }
protected:
  Vector< Vector<EBLevelGrid> >                                  m_eblgsMG;

  //! Time-independent a multigrid coefficients
  Vector< Vector< RefCountedPtr<LevelData<EBCellFAB> > > >       m_acoefMG;

  Vector< Vector<RefCountedPtr<LevelData<EBFluxFAB> > >        > m_etaMG;
  Vector< Vector<RefCountedPtr<LevelData<EBFluxFAB> > >        > m_lambdaMG;
  Vector< Vector<RefCountedPtr<LevelData<BaseIVFAB<Real> > > > > m_etaIrregMG;
  Vector< Vector<RefCountedPtr<LevelData<BaseIVFAB<Real> > > > > m_lambdaIrregMG;
  Vector< Vector<NWOEBViscousTensorOp*> > m_MGops;
  std::vector< bool  >                                 m_hasMGObjects;

  Vector<EBLevelGrid>                                  m_eblgs;
  Real                                                 m_alpha;
  Real                                                 m_beta;

  //! Time-independent a coefficients.
  Vector<RefCountedPtr<LevelData<EBCellFAB> > >        m_acoef;


  Vector<RefCountedPtr<LevelData<EBFluxFAB> > >        m_eta;
  Vector<RefCountedPtr<LevelData<EBFluxFAB> > >        m_lambda;
  Vector<RefCountedPtr<LevelData<BaseIVFAB<Real> > > > m_etaIrreg;
  Vector<RefCountedPtr<LevelData<BaseIVFAB<Real> > > > m_lambdaIrreg;
  Real                                                 m_dxCoarse;
  Vector<int>                                          m_refRatio;
  RefCountedPtr<BaseDomainBCFactory>                   m_domainBCFactory;
  RefCountedPtr<BaseEBBCFactory>                       m_ebBCFactory;
  IntVect                                              m_ghostCellsPhi;
  IntVect                                              m_ghostCellsRhs;
  int                                                  m_numLevels;
  Vector<Real>                                         m_dx;
  bool m_noMG;
  bool m_forTimingOnly;
private:
  ///weak construction bad
  NWOEBViscousTensorOpFactory()
  {
    MayDay::Error("invalid operator");
  }

  //copy constructor and operator= disallowed for all the usual reasons
  NWOEBViscousTensorOpFactory(const NWOEBViscousTensorOpFactory& a_opin)
  {
    MayDay::Error("invalid operator");
  }

  //copy constructor and operator= disallowed for all the usual reasons
  void operator=(const NWOEBViscousTensorOpFactory& a_opin)
  {
    MayDay::Error("invalid operator");
  }
};

#include "NamespaceFooter.H"
#endif
