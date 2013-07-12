/*
 *    This file is part of CasADi.
 *
 *    CasADi -- A symbolic framework for dynamic optimization.
 *    Copyright (C) 2010 by Joel Andersson, Moritz Diehl, K.U.Leuven. All rights reserved.
 *
 *    CasADi is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU Lesser General Public
 *    License as published by the Free Software Foundation; either
 *    version 3 of the License, or (at your option) any later version.
 *
 *    CasADi is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *    Lesser General Public License for more details.
 *
 *    You should have received a copy of the GNU Lesser General Public
 *    License along with CasADi; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef SUNDIALS_INTERNAL_HPP
#define SUNDIALS_INTERNAL_HPP

#include "sundials_integrator.hpp"
#include "symbolic/fx/integrator_internal.hpp"

#include <nvector/nvector_serial.h>
#include <sundials/sundials_dense.h>
#include <sundials/sundials_iterative.h>
#include <sundials/sundials_types.h>

namespace CasADi{

  class SundialsInternal : public IntegratorInternal{
  public:
    /** \brief  Constructor */
    explicit SundialsInternal(const FX& dae, int nfwd, int nadj);

    /** \brief  Destructor */
    virtual ~SundialsInternal()=0;
    
    /** \brief  Initialize */
    virtual void init();

    /** \brief  Reset the forward problem and bring the time back to t0 */
    virtual void reset(int nsens, int nsensB, int nsensB_store) = 0;
  
    /** \brief  Deep copy data members */
    virtual void deepCopyMembers(std::map<SharedObjectNode*,SharedObject>& already_copied);
  
    /** \brief  Set stop time for the integration */
    virtual void setStopTime(double tf) = 0;
  
    /// Linear solver forward, backward
    LinearSolver linsol_, linsolB_;
  
    //@{
    /// options
    bool exact_jacobian_, exact_jacobianB_;
    double abstol_, reltol_;
    double fsens_abstol_, fsens_reltol_;
    double abstolB_, reltolB_;
    int max_num_steps_;
    bool finite_difference_fsens_;  
    bool stop_at_end_;
    //@}
  
    /// Current time (to be removed)
    double t_;
  
    /// number of checkpoints stored so far
    int ncheck_; 
  
    /// Supported linear solvers in Sundials
    enum LinearSolverType{SD_USER_DEFINED, SD_DENSE, SD_BANDED, SD_ITERATIVE};

    /// Supported iterative solvers in Sundials
    enum IterativeSolverType{SD_GMRES,SD_BCGSTAB,SD_TFQMR};

    /// Linear solver data (dense)
    struct LinSolDataDense{};
  
    /// Linear solver
    LinearSolverType linsol_f_, linsol_g_;
  
    /// Iterative solver
    IterativeSolverType itsol_f_, itsol_g_;
  
    /// Preconditioning
    int pretype_f_, pretype_g_;
  
    /// Max krylov size
    int max_krylov_, max_krylovB_;
  
    /// Use preconditioning
    bool use_preconditioner_, use_preconditionerB_;
  
    // Jacobian of the DAE with respect to the state and state derivatives
    FX jac_, jacB_;
  
    /** \brief  Get the integrator Jacobian for the forward problem */
    virtual FX getJacobian()=0;
  
    /** \brief  Get the integrator Jacobian for the backward problem */
    virtual FX getJacobianB()=0;

    void getX0(DMatrix& p, int dir = -1){
      casadi_assert(p.size()==(1+nfwd_)*nx_);
      double* v = p.ptr();
      getX0(v,dir);
    }

    void getX0(N_Vector p, int dir = -1){
      casadi_assert(NV_LENGTH_S(p)==(1+nfwd_)*nx_);
      double* v = NV_DATA_S(p);
      getX0(v,dir);
    }

    void getX0(double* v, int dir = -1){
      for(int d=-1; d<nfwd_; ++d){
        int ind = NEW_INTEGRATOR_NUM_IN*(1+d)+NEW_INTEGRATOR_X0;
        if(dir<0){
          input(ind).get(v);
        } else {
          fwdSeed(ind,dir).get(v);
        }
        v += nx_;
      }        
    }

   void setQF(DMatrix& p, int dir = -1){
      casadi_assert(p.size()==(1+nfwd_)*nq_);
      double* v = p.ptr();
      setQF(v,dir);
    }

    void setQF(N_Vector p, int dir = -1){
      casadi_assert(NV_LENGTH_S(p)==(1+nfwd_)*nq_);
      double* v = NV_DATA_S(p);
      setQF(v,dir);
    }

    void setQF(double* v, int dir = -1){
      for(int d=-1; d<nfwd_; ++d){
        int ind = NEW_INTEGRATOR_NUM_OUT*(1+d)+NEW_INTEGRATOR_QF;
        if(dir<0){
          output(ind).set(v);
        } else {
          fwdSens(ind,dir).set(v);
        }
        v += nq_;
      }        
    }

   void setXF(DMatrix& p, int dir = -1){
      casadi_assert(p.size()==(1+nfwd_)*nx_);
      double* v = p.ptr();
      setXF(v,dir);
    }

    void setXF(N_Vector p, int dir = -1){
      casadi_assert(NV_LENGTH_S(p)==(1+nfwd_)*nx_);
      double* v = NV_DATA_S(p);
      setXF(v,dir);
    }

    void setXF(double* v, int dir = -1){
      for(int d=-1; d<nfwd_; ++d){
        int ind = NEW_INTEGRATOR_NUM_OUT*(1+d)+NEW_INTEGRATOR_XF;
        if(dir<0){
          output(ind).set(v);
        } else {
          fwdSens(ind,dir).set(v);
        }
        v += nx_;
      }        
    }

    void getP(DMatrix& p, int dir = -1){
      casadi_assert(p.size()==(1+nfwd_)*np_);

      double* v = p.ptr();
      for(int d=-1; d<nfwd_; ++d){
        int ind = NEW_INTEGRATOR_NUM_IN*(1+d)+NEW_INTEGRATOR_P;
        if(dir<0){
          input(ind).get(v);
        } else {
          fwdSeed(ind,dir).get(v);
        }
        v += np_;
      }        
    }

    void getRX0(DMatrix& p, int dir = -1){
      casadi_assert(p.size()==nadj_*nx_);
      double* v = p.ptr();
      getRX0(v,dir);
    }

    void getRX0(N_Vector p, int dir = -1){
      casadi_assert(NV_LENGTH_S(p)==nadj_*nx_);
      double* v = NV_DATA_S(p);
      getRX0(v,dir);
    }

    void getRX0(double* v, int dir = -1){
      for(int d=0; d<nadj_; ++d){
        int ind = NEW_INTEGRATOR_NUM_IN*(1+nfwd_) + NEW_INTEGRATOR_NUM_OUT*d + NEW_INTEGRATOR_XF;
        if(dir<0){
          input(ind).get(v);
        } else {
          fwdSeed(ind,dir).get(v);
        }
        v += nx_;
      }        
    }

    void setRXF(DMatrix& p, int dir = -1){
      casadi_assert(p.size()==nadj_*nx_);
      double* v = p.ptr();
      setRXF(v,dir);
    }

    void setRXF(N_Vector p, int dir = -1){
      casadi_assert(NV_LENGTH_S(p)==nadj_*nx_);
      double* v = NV_DATA_S(p);
      setRXF(v,dir);
    }

    void setRXF(double* v, int dir = -1){
      for(int d=0; d<nadj_; ++d){
        int ind = NEW_INTEGRATOR_NUM_OUT*(1+nfwd_) + NEW_INTEGRATOR_NUM_IN*d + NEW_INTEGRATOR_X0;
        if(dir<0){
          output(ind).set(v);
        } else {
          fwdSens(ind,dir).set(v);
        }
        v += nx_;
      }        
    }

    void setRQF(DMatrix& p, int dir = -1){
      casadi_assert(p.size()==nadj_*np_);
      double* v = p.ptr();
      setRQF(v,dir);
    }

    void setRQF(N_Vector p, int dir = -1){
      casadi_assert(NV_LENGTH_S(p)==nadj_*np_);
      double* v = NV_DATA_S(p);
      setRQF(v,dir);
    }

    void setRQF(double* v, int dir = -1){
      for(int d=0; d<nadj_; ++d){
        int ind = NEW_INTEGRATOR_NUM_OUT*(1+nfwd_) + NEW_INTEGRATOR_NUM_IN*d + NEW_INTEGRATOR_P;
        if(dir<0){
          output(ind).set(v);
        } else {
          fwdSens(ind,dir).set(v);
        }
        v += np_;
      }        
    }

    void getRP(DMatrix& p, int dir = -1){
      casadi_assert(p.size()==nadj_*nq_);

      double* v = p.ptr();
      for(int d=0; d<nadj_; ++d){
        int ind = NEW_INTEGRATOR_NUM_IN*(1+nfwd_) + NEW_INTEGRATOR_NUM_OUT*d + NEW_INTEGRATOR_QF;
        if(dir<0){
          input(ind).get(v);
        } else {
          fwdSeed(ind,dir).get(v);
        }
        v += nq_;
      }        
    }
  
  };
  
} // namespace CasADi

#endif // SUNDIALS_INTERNAL_HPP
