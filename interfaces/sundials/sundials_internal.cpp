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

#include "sundials_internal.hpp"
#include "casadi/stl_vector_tools.hpp"
#include "casadi/matrix/matrix_tools.hpp"
#include "casadi/mx/mx_tools.hpp"
#include "casadi/sx/sx_tools.hpp"
#include "casadi/fx/mx_function.hpp"
#include "casadi/fx/sx_function.hpp"

INPUTSCHEME(IntegratorInput)
OUTPUTSCHEME(IntegratorOutput)

using namespace std;
namespace CasADi{
namespace Sundials{
  
SundialsInternal::SundialsInternal(const FX& fd, const FX& fq) : IntegratorInternal(fd,fq){
  addOption("max_num_steps",               OT_INTEGER, 10000); // maximum number of steps
  addOption("reltol",                      OT_REAL,    1e-6); // relative tolerence for the IVP solution
  addOption("abstol",                      OT_REAL,    1e-8); // absolute tolerence  for the IVP solution
  addOption("exact_jacobian",              OT_BOOLEAN,  false);
  addOption("upper_bandwidth",             OT_INTEGER); // upper band-width of banded jacobians
  addOption("lower_bandwidth",             OT_INTEGER); // lower band-width of banded jacobians
  addOption("linear_solver",               OT_STRING, "dense","","user_defined|dense|banded|iterative");
  addOption("iterative_solver",            OT_STRING, "gmres","","gmres|bcgstab|tfqmr");
  addOption("pretype",                     OT_STRING, "none","","none|left|right|both");
  addOption("max_krylov",                  OT_INTEGER,  10);        // maximum krylov subspace size
  addOption("is_differential",             OT_INTEGERVECTOR, GenericType(), "A vector with a boolean describing the nature for each state.");
  addOption("sensitivity_method",          OT_STRING,  "simultaneous","","simultaneous|staggered");
  addOption("max_multistep_order",         OT_INTEGER, 5);
  addOption("use_preconditioner",          OT_BOOLEAN, false); // precondition an iterative solver
  addOption("stop_at_end",                 OT_BOOLEAN, false); // Stop the integrator at the end of the interval
  
  // Quadratures
  addOption("quad_err_con",                OT_BOOLEAN,false); // should the quadratures affect the step size control

  // Forward sensitivity problem
  addOption("fsens_err_con",               OT_BOOLEAN,true); // include the forward sensitivities in all error controls
  addOption("finite_difference_fsens",     OT_BOOLEAN, false); // use finite differences to approximate the forward sensitivity equations (if AD is not available)
  addOption("fsens_reltol",                OT_REAL); // relative tolerence for the forward sensitivity solution [default: equal to reltol]
  addOption("fsens_abstol",                OT_REAL); // absolute tolerence for the forward sensitivity solution [default: equal to abstol]
  addOption("fsens_scaling_factors",       OT_REALVECTOR); // scaling factor for the components if finite differences is used
  addOption("fsens_sensitiviy_parameters", OT_INTEGERVECTOR); // specifies which components will be used when estimating the sensitivity equations

  // Adjoint sensivity problem
  addOption("steps_per_checkpoint",        OT_INTEGER,20); // number of steps between two consecutive checkpoints
  addOption("interpolation_type",          OT_STRING,"hermite","type of interpolation for the adjoint sensitivities","hermite|polynomial");
  addOption("asens_upper_bandwidth",       OT_INTEGER); // upper band-width of banded jacobians
  addOption("asens_lower_bandwidth",       OT_INTEGER); // lower band-width of banded jacobians
  addOption("asens_linear_solver",         OT_STRING, "dense","","dense|banded|iterative");
  addOption("asens_iterative_solver",      OT_STRING, "gmres","","gmres|bcgstab|tfqmr");
  addOption("asens_pretype",               OT_STRING, "none","","none|left|right|both");
  addOption("asens_max_krylov",            OT_INTEGER,  10);        // maximum krylov subspace size
  addOption("asens_reltol",                OT_REAL); // relative tolerence for the adjoint sensitivity solution [default: equal to reltol]
  addOption("asens_abstol",                OT_REAL); // absolute tolerence for the adjoint sensitivity solution [default: equal to abstol]
  addOption("linear_solver_creator",       OT_LINEARSOLVER, GenericType(), "An linear solver creator function");
  addOption("linear_solver_options",       OT_DICTIONARY, GenericType(), "Options to be passed to the linear solver");
}

SundialsInternal::~SundialsInternal(){ 
}

void SundialsInternal::init(){
  // Call the base class method
  IntegratorInternal::init();
  
  // Read options
  abstol_ = getOption("abstol");
  reltol_ = getOption("reltol");
  exact_jacobian_ = getOption("exact_jacobian");
  max_num_steps_ = getOption("max_num_steps");
  finite_difference_fsens_ = getOption("finite_difference_fsens");
  fsens_abstol_ = hasSetOption("fsens_abstol") ? double(getOption("fsens_abstol")) : abstol_;
  fsens_reltol_ = hasSetOption("fsens_reltol") ? double(getOption("fsens_reltol")) : reltol_;
  asens_abstol_ = hasSetOption("asens_abstol") ? double(getOption("asens_abstol")) : abstol_;
  asens_reltol_ = hasSetOption("asens_reltol") ? double(getOption("asens_reltol")) : reltol_;
  stop_at_end_ = getOption("stop_at_end");
  
  // If time was not specified, initialise it.
  if(fd_.input(DAE_T).numel()==0) {
    std::vector<MX> in1(DAE_NUM_IN);
    in1[DAE_T] = MX("T");
    in1[DAE_Y] = MX("Y",fd_.input(DAE_Y).size1(),fd_.input(DAE_Y).size2());
    in1[DAE_YDOT] = MX("YDOT",fd_.input(DAE_YDOT).size1(),fd_.input(DAE_YDOT).size2());
    in1[DAE_P] = MX("P",fd_.input(DAE_P).size1(),fd_.input(DAE_P).size2());
    std::vector<MX> in2(in1);
    in2[DAE_T] = MX();
    fd_ = MXFunction(in1,fd_.call(in2));
    fd_.init();
  }
  
  // We only allow for 0-D time
  casadi_assert_message(fd_.input(DAE_T).numel()==1, "IntegratorInternal: time must be zero-dimensional, not (" <<  fd_.input(DAE_T).size1() << 'x' << fd_.input(DAE_T).size2() << ")");
  
  // Get the linear solver creator function
  if(linsol_.isNull() && hasSetOption("linear_solver_creator")){
    linearSolverCreator linear_solver_creator = getOption("linear_solver_creator");
  
    // Allocate an NLP solver
    linsol_ = linear_solver_creator(CRSSparsity());
  
    // Pass options
    if(hasSetOption("linear_solver_options")){
      const Dictionary& linear_solver_options = getOption("linear_solver_options");
      linsol_.setOption(linear_solver_options);
    }
  }
}

void SundialsInternal::deepCopyMembers(std::map<SharedObjectNode*,SharedObject>& already_copied){
  IntegratorInternal::deepCopyMembers(already_copied);
  jac_ = deepcopy(jac_,already_copied);
  linsol_ = deepcopy(linsol_,already_copied);
}

SundialsIntegrator SundialsInternal::jac(bool with_x, bool with_p){
  // Make sure that we need sensitivities w.r.t. X0 or P (or both)
  casadi_assert(with_x || with_p);
  
  // Type cast to SXMatrix (currently supported)
  SXFunction f = shared_cast<SXFunction>(fd_);
  if(f.isNull() != fd_.isNull()) return SundialsIntegrator();
  
  SXFunction q = shared_cast<SXFunction>(fq_);
  if(q.isNull() != fq_.isNull()) return SundialsIntegrator();
    
  // Number of state derivatives
  int nyp = fd_.input(DAE_YDOT).numel();
  
  // Number of sensitivities
  int ns_x = with_x*nx_;
  int ns_p = with_p*np_;
  int ns = ns_x + ns_p;

  // Sensitivities and derivatives of sensitivities
  SXMatrix ysens = ssym("ysens",ny_,ns);
  SXMatrix ypsens = ssym("ypsens",nyp,ns);
    
  // Sensitivity equation
  SXMatrix res_s = mul(f.jac(DAE_Y,DAE_RES),ysens);
  if(nyp>0) res_s += mul(f.jac(DAE_YDOT,DAE_RES),ypsens);
  if(with_p) res_s += horzcat(SXMatrix(ny_,ns_x),f.jac(DAE_P,DAE_RES));

  // Augmented DAE
  SXMatrix faug = vec(horzcat(f.outputSX(INTEGRATOR_XF),res_s));
  makeDense(faug); // NOTE: possible alternative: skip structural zeros (messes up the sparsity pattern of the augmented system)

  // Input arguments for the augmented DAE
  vector<SXMatrix> faug_in(DAE_NUM_IN);
  faug_in[DAE_T] = f.inputSX(DAE_T);
  faug_in[DAE_Y] = vec(horzcat(f.inputSX(DAE_Y),ysens));
  if(nyp>0) faug_in[DAE_YDOT] = vec(horzcat(f.inputSX(DAE_YDOT),ypsens));
  faug_in[DAE_P] = f.inputSX(DAE_P);
  
  // Create augmented DAE function
  SXFunction ffcn_aug(faug_in,faug);
  
  // Augmented quadratures
  SXFunction qfcn_aug;
  
  // Now lets do the same for the quadrature states
  if(!q.isNull()){
    
    // Sensitivity quadratures
    SXMatrix q_s = mul(q.jac(DAE_Y,DAE_RES),ysens);
    if(nyp>0) q_s += mul(q.jac(DAE_YDOT,DAE_RES),ypsens);
    if(with_p) q_s += horzcat(SXMatrix(nxq_,ns_x),q.jac(DAE_P,DAE_RES));

    // Augmented quadratures
    SXMatrix qaug = vec(horzcat(q.outputSX(INTEGRATOR_XF),q_s));
    makeDense(qaug); // NOTE: se above

    // Input to the augmented DAE (start with old)
    vector<SXMatrix> qaug_in(DAE_NUM_IN);
    qaug_in[DAE_T] = q.inputSX(DAE_T);
    qaug_in[DAE_Y] = vec(horzcat(q.inputSX(DAE_Y),ysens));
    if(nyp>0) qaug_in[DAE_YDOT] = vec(horzcat(q.inputSX(DAE_YDOT),ypsens));
    qaug_in[DAE_P] = q.inputSX(DAE_P);

    // Create augmented DAE function
    qfcn_aug = SXFunction(qaug_in,qaug);
  }
  
  // Create integrator instance
  SundialsIntegrator integrator;
  integrator.assignNode(create(ffcn_aug,qfcn_aug));

  // Set options
  integrator.setOption(dictionary());
  integrator.setOption("nrhs",1+ns);
  
  // Transmit information on derivative states
  if(hasSetOption("is_differential")){
    vector<int> is_diff = getOption("is_differential");
    casadi_assert_message(is_diff.size()==ny_,"is_differential has incorrect length");
    vector<int> is_diff_aug(ny_*(1+ns));
    for(int i=0; i<1+ns; ++i)
      for(int j=0; j<ny_; ++j)
        is_diff_aug[j+i*ny_] = is_diff[j];
    integrator.setOption("is_differential",is_diff_aug);
  }
  
  // Pass linear solver
  if(!linsol_.isNull()){
    LinearSolver linsol_aug = shared_cast<LinearSolver>(linsol_.clone());
//    linsol_aug->nrhs_ = 1+ns; // FIXME!!!
//    integrator.setLinearSolver(linsol_aug,jac_); // FIXME!!!
    integrator.setLinearSolver(linsol_aug);
  }
  
  return integrator;
}

CRSSparsity SundialsInternal::getJacSparsity(int iind, int oind){
  if(iind==INTEGRATOR_XP0){
    // Function value does not depend on the state derivative initial guess
    return CRSSparsity();
  } else {
    // Default (dense) sparsity
    return FXInternal::getJacSparsity(iind,oind);
  }
}

FX SundialsInternal::jacobian(const std::vector<std::pair<int,int> >& jblocks){
  bool with_x = false, with_p = false;
  for(int i=0; i<jblocks.size(); ++i){
    if(jblocks[i].second == INTEGRATOR_P){
      casadi_assert_message(jblocks[i].first == INTEGRATOR_XF,"IntegratorInternal::jacobian: Not derivative of state"); // can be removed?
      with_p = true;
    } else if(jblocks[i].second == INTEGRATOR_X0){
      casadi_assert_message(jblocks[i].first == INTEGRATOR_XF,"IntegratorInternal::jacobian: Not derivative of state"); // can be removed?
      with_x = true;
    }
  }
  
  // Create a new integrator for the forward sensitivity equations
  SundialsIntegrator fwdint = jac(with_x,with_p);
  
  // If creation was successful
  if(!fwdint.isNull()){
    fwdint.init();

    // Number of sensitivities
    int ns_x = with_x*nx_;
    int ns_p = with_p*np_;
    int ns = ns_x + ns_p;
    
    // Symbolic input of the Jacobian
    vector<MX> jac_in = symbolicInput();
    
    // Input to the augmented integrator
    vector<MX> fwdint_in(INTEGRATOR_NUM_IN);
    
    // Pass parameters without change
    fwdint_in[INTEGRATOR_P] = jac_in[INTEGRATOR_P];
    
    // Get the state
    MX x0 = jac_in[INTEGRATOR_X0];
    MX xp0 = jac_in[INTEGRATOR_XP0];
    
    // Separate the quadrature states from the rest of the states
    MX y0 = x0[range(ny_)];
    MX q0 = x0[range(ny_,nx_)];
    MX yp0 = xp0[range(ny_)];
    MX qp0 = xp0[range(ny_,nx_)];
    
    // Initial condition for the sensitivitiy equations
    DMatrix y0_sens(ns*ny_,1,0);
    DMatrix q0_sens(ns*nxq_,1,0);
    
    if(with_x){
        for(int i=0; i<ny_; ++i)
          y0_sens.data()[i + i*ns_x] = 1;
      
      for(int i=0; i<nxq_; ++i)
        q0_sens.data()[ny_ + i + i*ns_x] = 1;
    }
    
    // Augmented initial condition
    MX y0_aug = vertcat(y0,MX(y0_sens));
    MX q0_aug = vertcat(q0,MX(q0_sens));
    MX yp0_aug = vertcat(yp0,MX(y0_sens.sparsity(),0));
    MX qp0_aug = vertcat(qp0,MX(q0_sens.sparsity(),0));

    // Finally, we are ready to pass the initial condition for the state and state derivative
    fwdint_in[INTEGRATOR_X0] = vertcat(y0_aug,q0_aug);
    fwdint_in[INTEGRATOR_XP0] = vertcat(yp0_aug,qp0_aug);
    
    // Call the integrator with the constructed input (in fact, create a call node)
    vector<MX> fwdint_out = fwdint.call(fwdint_in);
    MX xf_aug = fwdint_out[INTEGRATOR_XF];
    MX xpf_aug = fwdint_out[INTEGRATOR_XPF];
    
    // Separate the quadrature states from the rest of the states
    MX yf_aug = xf_aug[range((ns+1)*ny_)];
    MX qf_aug = xf_aug[range((ns+1)*ny_,(ns+1)*nx_)];
    MX ypf_aug = xpf_aug[range((ns+1)*ny_)];
    MX qpf_aug = xpf_aug[range((ns+1)*ny_,(ns+1)*nx_)];
    
    // Get the state and state derivative at the final time
    MX yf = yf_aug[range(ny_)];
    MX qf = qf_aug[range(nxq_)];
    MX xf = vertcat(yf,qf);
    MX ypf = ypf_aug[range(ny_)];
    MX qpf = qpf_aug[range(nxq_)];
    MX xpf = vertcat(ypf,qpf);
    
    // Get the sensitivitiy equations state at the final time
    MX yf_sens = yf_aug[range(ny_,(ns+1)*ny_)];
    MX qf_sens = qf_aug[range(nxq_,(ns+1)*nxq_)];
    MX ypf_sens = yf_aug[range(ny_,(ns+1)*ny_)];
    MX qpf_sens = qf_aug[range(nxq_,(ns+1)*nxq_)];

    // Reshape the sensitivity state and state derivatives
    yf_sens = trans(reshape(yf_sens,ns,ny_));
    ypf_sens = trans(reshape(ypf_sens,ns,ny_));
    qf_sens = trans(reshape(qf_sens,ns,nxq_));
    qpf_sens = trans(reshape(qpf_sens,ns,nxq_));
    
    // We are now able to get the Jacobian
    MX J_xf = vertcat(yf_sens,qf_sens);
    MX J_xpf = vertcat(ypf_sens,qpf_sens);

    // Split up the Jacobians in parts for x0 and p
    MX J_xf_x0 = J_xf(range(J_xf.size1()),range(ns_x));
    MX J_xpf_x0 = J_xpf(range(J_xpf.size1()),range(ns_x));
    MX J_xf_p = J_xf(range(J_xf.size1()),range(ns_x,ns));
    MX J_xpf_p = J_xpf(range(J_xpf.size1()),range(ns_x,ns));
    
    // Output of the Jacobian
    vector<MX> jac_out(jblocks.size());
    for(int i=0; i<jblocks.size(); ++i){
      bool is_jac = jblocks[i].second >=0;
      bool is_x0 = jblocks[i].second==INTEGRATOR_X0;
      bool is_xf = jblocks[i].first==INTEGRATOR_XF;
      if(is_jac){
        if(is_x0){
          jac_out[i] = is_xf ? J_xf_x0 : J_xpf_x0;
        } else {
          jac_out[i] = is_xf ? J_xf_p : J_xpf_p;
        }
      } else {
        jac_out[i] = is_xf ? xf : xpf;
      }
    }
    MXFunction intjac(jac_in,jac_out);

    return intjac;
  } else {
    return FXInternal::jacobian(jblocks);
  }
}

void SundialsInternal::setInitialTime(double t0){
  t0_ = t0;
}

void SundialsInternal::setFinalTime(double tf){
  tf_ = tf;
}

} // namespace Sundials
} // namespace CasADi

