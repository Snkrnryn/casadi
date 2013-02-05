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
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSefcn.  See the GNU
 *    Lesser General Public License for more details.
 *
 *    You should have received a copy of the GNU Lesser General Public
 *    License along with CasADi; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <symbolic/casadi.hpp>
#include <interfaces/qpoases/qpoases_solver.hpp>

#include <iomanip>
#include <ctime>

using namespace CasADi;
using namespace std;


class Tester{
public:
  // Constructor
  Tester(int n_boxes, int n_euler, int n_meas) : n_boxes_(n_boxes), n_euler_(n_euler), n_meas_(n_meas){}

  // Perform the modelling
  void model();

  // Simulate to generae measurements
  void simulate(double drag_true, double depth_true);

  // Transscribe as an NLP
  void transcribe(bool single_shooting);

  // Prepare NLP
  void prepare();
  void prepareNew();

  // Solve NLP
  void solve(int& iter_count);

  // Perform the parameter estimation
  void optimize(double drag_guess, double depth_guess, int& iter_count, double& sol_time, double& drag_est, double& depth_est);

  // Dimensions
  int n_boxes_;
  int n_euler_;
  int n_meas_;

  // Initial conditions
  DMatrix u0_;
  DMatrix v0_;
  DMatrix h0_;

  // Discrete time dynamics
  FX f_;
  
  // Generated measurements
  vector<DMatrix> H_meas_;

  // NLP solver
  bool single_shooting_;

  // NLP
  SXFunction fg_sx_;
  MXFunction fg_mx_;

  /// QP solver for the subproblems
  QPSolver qp_solver_;

  /// maximum number of sqp iterations
  int maxiter_; 

  /// stopping criterion for the stepsize
  double toldx_;
  
  /// stopping criterion for the lagrangian gradient
  double tolgl_;

  /// Residual
  vector<double> d_k_;
   
  /// Primal step
  vector<double> dx_k_;
   
  /// Dual step
  vector<double> dlambda_u_, dlambda_g_;
  
  /// Indices
  enum GIn{G_U,G_V,G_LAM_X,G_LAM_G,G_NUM_IN};
  enum GOut{G_D,G_G,G_F,G_NUM_OUT};
  
  enum LinIn{LIN_U,LIN_V,LIN_LAM_X,LIN_LAM_G,LIN_D,LIN_NUM_IN};
  enum LinOut{LIN_F1,LIN_J1,LIN_F2,LIN_J2,LIN_NUM_OUT};
  
  enum ExpIn{EXP_U,EXP,V,EXP_EXP_X,EXP_LAM_G,EXP_D,EXP_DU,EXP_DLAM_F2,EXP_NUM_IN};
  enum ExpOut{EXP_E,EXP_NUM_OUT};
  
  /// Residual function
  FX rfcn_;
  
  /// Quadratic approximation
  FX lfcn_;
  
  /// Step expansion
  FX efcn_;
  
  /// Dimensions
  int nu_, nv_, nx_;
  
  vector<double> u_init_, lbu_, ubu_, u_opt_, lambda_u_, g_, lbg_, ubg_, lambda_g_;
  vector<double> v_init_, lbv_, ubv_, v_opt_, lambda_v_, h_, lbh_, ubh_, lambda_h_;

};

void Tester::model(){
  // Physical parameters
  double g = 9.81; // gravity
  double poolwidth = 0.2;
  double sprad = 0.03;
  double spheight = 0.01;
  double endtime = 1.0;
    
  // Discretization
  int ntimesteps = n_euler_*n_meas_;
  double dt = endtime/ntimesteps;
  double dx = poolwidth/n_boxes_;
  double dy = poolwidth/n_boxes_;
  vector<double> x(n_boxes_), y(n_boxes_);
  for(int i=0; i<n_boxes_; ++i){
    x[i] = (i+0.5)*dx;
    y[i] = (i+0.5)*dy;
  }
  
  // Initial conditions
  u0_ = DMatrix::zeros(n_boxes_+1,n_boxes_  );
  v0_ = DMatrix::zeros(n_boxes_  ,n_boxes_+1);
  h0_ = DMatrix::zeros(n_boxes_  ,n_boxes_  ); 
  for(int i=0; i<n_boxes_; ++i){
    for(int j=0; j<n_boxes_; ++j){
      double spdist = sqrt(pow((x[i]-0.04),2.) + pow((y[j]-0.04),2.));
      if(spdist<sprad/3.0){
	h0_.elem(i,j) = spheight * cos(3.0*M_PI*spdist/(2.0*sprad));
      }
    }
  }
  
  // Free parameters
  SX drag("b");
  SX depth("H");
  vector<SX> p(2); p[0]=drag; p[1]=depth;
  
  // The state at a measurement
  SXMatrix uk = ssym("uk",n_boxes_+1, n_boxes_);
  SXMatrix vk = ssym("vk",n_boxes_  , n_boxes_+1);
  SXMatrix hk = ssym("hk",n_boxes_  , n_boxes_);
  
  // Take one step of the integrator
  SXMatrix u = uk;
  SXMatrix v = vk;
  SXMatrix h = hk;
  
  // Temporaries
  SX d1 = -dt*g/dx;
  SX d2 = dt*drag;
  
  // Update u
  for(int i=0; i<n_boxes_-1; ++i){
    for(int j=0; j<n_boxes_; ++j){
      u.elem(1+i,j) += d1*(h.elem(1+i,j)-h.elem(i,j))- d2*u.elem(1+i,j);
    }
  }
  
  // Update v
  d1 = -dt*g/dy;
  for(int i=0; i<n_boxes_; ++i){
    for(int j=0; j<n_boxes_-1; ++j){
      v.elem(i,j+1) += d1*(h.elem(i,j+1)-h.elem(i,j))- d2*v.elem(i,j+1);
    }
  }
  
  // Update h
  d1 = (-depth*dt)*(1.0/dx);
  d2 = (-depth*dt)*(1.0/dy);
  for(int i=0; i<n_boxes_; ++i){
    for(int j=0; j<n_boxes_; ++j){
      h.elem(i,j) += d1*(u.elem(1+i,j)-u.elem(i,j)) + d2*(v.elem(i,j+1)-v.elem(i,j));
    }
  }
  
  // Create an integrator function
  vector<SXMatrix> f_step_in(4);
  f_step_in[0] = p;
  f_step_in[1] = uk;
  f_step_in[2] = vk;
  f_step_in[3] = hk;
  vector<SXMatrix> f_step_out(3);
  f_step_out[0] = u;
  f_step_out[1] = v;
  f_step_out[2] = h;
  SXFunction f_step(f_step_in,f_step_out);
  f_step.init();
  cout << "generated single step dynamics (" << f_step.getAlgorithmSize() << " nodes)" << endl;
  
  // Integrate over one interval
  vector<MX> f_in(4);
  MX P = msym("P",2);
  MX Uk = msym("Uk",n_boxes_+1, n_boxes_);
  MX Vk = msym("Vk",n_boxes_  , n_boxes_+1);
  MX Hk = msym("Hk",n_boxes_  , n_boxes_);
  f_in[0] = P;
  f_in[1] = Uk;
  f_in[2] = Vk;
  f_in[3] = Hk;
  vector<MX> f_inter = f_in;
  vector<MX> f_out;
  for(int j=0; j<n_euler_; ++j){
    // Create a call node
    f_out = f_step.call(f_inter);
    
    // Save intermediate state
    f_inter[1] = f_out[0];
    f_inter[2] = f_out[1];
    f_inter[3] = f_out[2];
  }
  
  // Create an integrator function
  MXFunction f_mx(f_in,f_out);
  f_mx.init();
  cout << "generated discrete dynamics, MX (" << f_mx.countNodes() << " nodes)" << endl;
  
  // Expand the discrete dynamics
  if(false){
    SXFunction f_sx(f_mx);
    f_sx.init();
    cout << "generated discrete dynamics, SX (" << f_sx.getAlgorithmSize() << " nodes)" << endl;
    f_ = f_sx;
  } else {
    f_ = f_mx;
  }
}

void Tester::simulate(double drag_true, double depth_true){
  
  // Measurements
  H_meas_.reserve(n_meas_);
  
  // Simulate once to generate "measurements"
  vector<double> p_true(2); p_true[0]=drag_true; p_true[1]=depth_true;
  f_.setInput(p_true,0);
  f_.setInput(u0_,1);
  f_.setInput(v0_,2);
  f_.setInput(h0_,3);
  clock_t time1 = clock();
  for(int k=0; k<n_meas_; ++k){
    f_.evaluate();
    const DMatrix& u = f_.output(0);
    const DMatrix& v = f_.output(1);
    const DMatrix& h = f_.output(2);
    f_.setInput(u,1);
    f_.setInput(v,2);
    f_.setInput(h,3);
    
    // Save a copy of h
    H_meas_.push_back(h);
  }
  clock_t time2 = clock();
  double t_elapsed = double(time2-time1)/CLOCKS_PER_SEC;
  cout << "measurements generated in " << t_elapsed << " seconds." << endl;
}


void Tester::transcribe(bool single_shooting){
  single_shooting_ = single_shooting;
  
  // NLP variables
  MX nlp_u = msym("u",2);
  MX nlp_v = msym("v",single_shooting ? 0 : n_boxes_*n_boxes_*n_meas_);

  // Variables in the lifted NLP
  MX P = nlp_u;
  int v_offset = 0;
  
  // Least-squares objective function
  MX F;
  
  // Constraint function
  MX G = MX::sparse(0,1);
  
  // Generate full-space NLP
  MX U = u0_;  MX V = v0_;  MX H = h0_;
  for(int k=0; k<n_meas_; ++k){
    // Take a step
    MX f_arg[4] = {P,U,V,H};
    vector<MX> f_res = f_.call(vector<MX>(f_arg,f_arg+4));
    U = f_res[0];    V = f_res[1];    H = f_res[2];
    
    if(!single_shooting){
      // Lift the variable
      MX H_def = H;
      H = nlp_v[Slice(v_offset,v_offset+n_boxes_*n_boxes_)];
      H = reshape(H,h0_.sparsity());
      v_offset += H.size();
      
      // Constraint function term
      G.append(flatten(H_def-H));
    } else {
      H = lift(H);
    }
    
    // Objective function term
    F.append(flatten(H-H_meas_[k]));
  }
 
  // Function which calculates the objective terms and constraints
  vector<MX> fg_in;
  fg_in.push_back(nlp_u);
  fg_in.push_back(nlp_v);

  vector<MX> fg_out;
  fg_out.push_back(F);
  fg_out.push_back(G);

  fg_mx_ = MXFunction(fg_in,fg_out);
  fg_mx_.init();
  cout << "Generated lifted NLP (" << fg_mx_.countNodes() << " nodes)" << endl;
  
  // Expand NLP
  fg_sx_ = SXFunction(fg_mx_);
  fg_sx_.init();
  cout << "expanded lifted NLP (" << fg_sx_.getAlgorithmSize() << " nodes)" << endl;
  
  nu_ = nlp_u.size();
  nv_ = nlp_v.size();
  nx_ = nu_ + nv_;


  u_init_.resize(nu_,0);
  u_opt_.resize(nu_,0);
  lbu_.resize(nu_,-numeric_limits<double>::infinity());
  ubu_.resize(nu_, numeric_limits<double>::infinity());
  lambda_u_.resize(nx_,0);

  v_init_.resize(nv_,0);
  v_opt_.resize(nv_,0);
  lbv_.resize(nv_,-numeric_limits<double>::infinity());
  ubv_.resize(nv_, numeric_limits<double>::infinity());
  lambda_v_.resize(nv_,0);

  g_.resize(G.size());
  lbg_.resize(G.size(),-numeric_limits<double>::infinity());
  ubg_.resize(G.size(), numeric_limits<double>::infinity());
  lambda_g_.resize(G.size(),0);

  // Prepare the NLP solver
  prepare();
}

void Tester::prepareNew(){
  bool verbose_ = false;
  bool gauss_newton_ = true;

  if(!single_shooting_) return;
  return;

  // Extract the expressions
  MX x = fg_mx_.inputExpr(0);
  MX f = fg_mx_.outputExpr(0);
  MX g = fg_mx_.outputExpr(1);

  // Generate lifting functions
  MXFunction F,G,Z;
  fg_mx_.generateLiftingFunctions(F,G,Z);
  F.init();
  G.init();



  return;

  // Residual function G
  vector<MX> G_in(G_NUM_IN);
  G_in[G_U] = veccat(G.inputExpr());
  //    G_in[G_LAM_X] = lam_x;
  //    G_in[G_LAM_G] = lam_g;
  vector<MX> G_out(G_NUM_OUT);
  
  vector<MX> v_eq = G.outputExpr();
  MX f1 = v_eq.at(v_eq.size()-2);
  f = inner_prod(f1,f1)/2;
  g = v_eq.at(v_eq.size()-1);
  v_eq.resize(v_eq.size()-2);
  
  G_out[G_D] = veccat(v_eq);
  G_out[G_G] = g;
  G_out[G_F] = f;
  
  rfcn_ = MXFunction(G_in,G_out);
  rfcn_.setOption("name","rfcn");
  
  G_in = shared_cast<MXFunction>(rfcn_).inputExpr();
  G_out = shared_cast<MXFunction>(rfcn_).outputExpr();
  
  rfcn_.setOption("number_of_fwd_dir",0);
  rfcn_.setOption("number_of_adj_dir",0);
  rfcn_.init();
  if(verbose_){
    cout << "Generated residual function ( " << shared_cast<MXFunction>(rfcn_).getAlgorithmSize() << " nodes)." << endl;
  }
  
  // Modified function Z
  enum ZIn{Z_U,Z_D,Z_LAM_X,Z_LAM_F2,Z_NUM_IN};
  vector<MX> zfcn_in(Z_NUM_IN);
    
  vector<MX> d = Z.inputExpr();
  MX u = d.front();
  d.erase(d.begin());

  zfcn_in[Z_U] = u;
  zfcn_in[Z_D] = veccat(d);
  //  zfcn_in[Z_LAM_X] = lam_x;
  //  zfcn_in[Z_LAM_F2] = lam_f2;

  enum ZOut{Z_D_DEF,Z_F12,Z_NUM_OUT};
  vector<MX> zfcn_out(Z_NUM_OUT);

  vector<MX> d_def = Z.outputExpr();
  MX f1_z = d_def.at(d_def.size()-2);
  MX f2_z = d_def.at(d_def.size()-1);

  int nf1 = f1_z.numel();
  int nf2 = f2_z.numel();

  cout << "nf1 = " << nf1 << endl;
  cout << "nf2 = " << nf2 << endl;

  d_def.resize(d_def.size()-2);

  zfcn_out[Z_D_DEF] = veccat(d_def);
  zfcn_out[Z_F12] = vertcat(f1_z,f2_z);
  
  MXFunction zfcn(zfcn_in,zfcn_out);
  zfcn.init();
  if(verbose_){
    cout << "Generated reconstruction function ( " << zfcn.getAlgorithmSize() << " nodes)." << endl;
  }
  zfcn_in = zfcn.inputExpr();
  zfcn_out = zfcn.outputExpr();

  // Matrix A and B in lifted Newton
  MX B = zfcn.jac(Z_U,Z_F12);
  MX B1 = B(Slice(0,nf1),Slice(0,B.size2()));
  MX B2 = B(Slice(nf1,B.size1()),Slice(0,B.size2()));
  if(verbose_){
    cout << "Formed B1 (dimension " << B1.size1() << "-by-" << B1.size2() << ", "<< B1.size() << " nonzeros) " <<
    "and B2 (dimension " << B2.size1() << "-by-" << B2.size2() << ", "<< B2.size() << " nonzeros)." << endl;
  }
  
  int nu = u.numel();

  MX lam_f2 = msym("lam_f2",0);


  // Step in u
  MX du = msym("du",nu);
  MX dlam_f2 = msym("dlam_f2",lam_f2.sparsity());

  
  MX b1 = f1_z;
  MX b2 = f2_z;
  MX e;
  

  //  if(nv > 0){
  if(true){
    
    // Directional derivative of Z
    vector<vector<MX> > Z_fwdSeed(2,zfcn_in);
    vector<vector<MX> > Z_fwdSens(2,zfcn_out);
    vector<vector<MX> > Z_adjSeed;
    vector<vector<MX> > Z_adjSens;
    
    Z_fwdSeed[0][Z_U] = MX(zfcn_in[Z_U].sparsity());
    Z_fwdSeed[0][Z_D] = -zfcn_in[Z_D];
    Z_fwdSeed[0][Z_LAM_X] = MX();
    Z_fwdSeed[0][Z_LAM_F2] = MX();
    
    Z_fwdSeed[1][Z_U] = du;
    Z_fwdSeed[1][Z_D] = -zfcn_in[Z_D];
    Z_fwdSeed[1][Z_LAM_X] = MX();
    Z_fwdSeed[1][Z_LAM_F2] = MX();
    
    zfcn.eval(zfcn_in,zfcn_out,Z_fwdSeed,Z_fwdSens,Z_adjSeed,Z_adjSens,true);
    
    b1 += Z_fwdSens[0][Z_F12](Slice(0,nf1));
    b2 += Z_fwdSens[0][Z_F12](Slice(nf1,B.size1()));
    e = Z_fwdSens[1][Z_D_DEF];
  }
  if(verbose_){
    cout << "Formed b1 (dimension " << b1.size1() << "-by-" << b1.size2() << ", "<< b1.size() << " nonzeros) " <<
    "and b2 (dimension " << b2.size1() << "-by-" << b2.size2() << ", "<< b2.size() << " nonzeros)." << endl;
  }

  // Generate Gauss-Newton Hessian
  if(gauss_newton_){
    b1 = mul(trans(B1),b1);
    B1 = mul(trans(B1),B1);
    if(verbose_){
      cout << "Gauss Newton Hessian (dimension " << B1.size1() << "-by-" << B1.size2() << ", "<< B1.size() << " nonzeros)." << endl;
    }
  }

  // Make sure b1 and b2 are dense vectors
  makeDense(b1);
  makeDense(b2);

  // Quadratic approximation
  vector<MX> lfcn_in(LIN_NUM_IN);
  lfcn_in[LIN_U] =     veccat(G.inputExpr());
  //u; // FIXME?
  lfcn_in[LIN_D] = zfcn_in[Z_D];
  lfcn_in[LIN_LAM_X] = MX(0,1); // lam_x;
  lfcn_in[LIN_LAM_G] = MX(0,1); // lam_g;

  
  vector<MX> lfcn_out(LIN_NUM_OUT);
  lfcn_out[LIN_F1] = b1;
  lfcn_out[LIN_J1] = B1;
  lfcn_out[LIN_F2] = b2;
  lfcn_out[LIN_J2] = B2;
  lfcn_ = MXFunction(lfcn_in,lfcn_out);
  lfcn_.setOption("number_of_fwd_dir",0);
  lfcn_.setOption("number_of_adj_dir",0);
  lfcn_.setOption("name","lfcn");
  lfcn_.init();
  lfcn_in = shared_cast<MXFunction>(lfcn_).inputExpr();
  lfcn_out = shared_cast<MXFunction>(lfcn_).outputExpr();


  if(verbose_){
    cout << "Generated linearization function ( " << shared_cast<MXFunction>(lfcn_).getAlgorithmSize() << " nodes)." << endl;
  }

  // Step expansion
  vector<MX> efcn_in(EXP_NUM_IN);
  copy(lfcn_in.begin(),lfcn_in.end(),efcn_in.begin());
  efcn_in[EXP_DU] = du;
  efcn_in[EXP_DLAM_F2] = MX(); //dlam_f2;
  efcn_ = MXFunction(efcn_in,e);
  efcn_.setOption("number_of_fwd_dir",0);
  efcn_.setOption("number_of_adj_dir",0);
  efcn_.setOption("name","efcn");
  efcn_.init();
  if(verbose_){
    cout << "Generated step expansion function ( " << shared_cast<MXFunction>(efcn_).getAlgorithmSize() << " nodes)." << endl;
  }    
}


void Tester::prepare(){
  prepareNew();

  bool verbose_ = false;
  bool gauss_newton_ = true;
    
  // Extract the free variables and split into independent and dependent variables
  SXMatrix u = fg_sx_.inputExpr(0);
  SXMatrix v = fg_sx_.inputExpr(1);
  SXMatrix x = vertcat(u,v);

  // Extract the constraint equations and split into constraints and definitions of dependent variables
  SXMatrix f1 = fg_sx_.outputExpr(0);
  int nf1 = f1.numel();
  SXMatrix g = fg_sx_.outputExpr(1);
  int nf2 = g.numel()-nv_;
  SXMatrix v_eq = g(Slice(0,nv_));
  SXMatrix f2 = g(Slice(nv_,nv_+nf2));
  
  // Definition of v
  SXMatrix v_def = v_eq + v;

  // Objective function
  SXMatrix f;
  
  // Multipliers
  SXMatrix lam_x, lam_g, lam_f2;
  if(gauss_newton_){
    
    // Least square objective
    f = inner_prod(f1,f1)/2;
    
  } else {
    
    // Scalar objective function
    f = f1;
    
    // Lagrange multipliers for the simple bounds on u
    SXMatrix lam_u = ssym("lam_u",nu_);
    
    // Lagrange multipliers for the simple bounds on v
    SXMatrix lam_v = ssym("lam_v",nv_);
    
    // Lagrange multipliers for the simple bounds on x
    lam_x = vertcat(lam_u,lam_v);

    // Lagrange multipliers corresponding to the definition of the dependent variables
    SXMatrix lam_v_eq = ssym("lam_v_eq",nv_);

    // Lagrange multipliers for the nonlinear constraints that aren't eliminated
    lam_f2 = ssym("lam_f2",nf2);

    if(verbose_){
      cout << "Allocated intermediate variables." << endl;
    }
    
    // Lagrange multipliers for constraints
    lam_g = vertcat(lam_v_eq,lam_f2);
    
    // Lagrangian function
    SXMatrix lag = f + inner_prod(lam_x,x);
    if(!f2.empty()) lag += inner_prod(lam_f2,f2);
    if(!v.empty()) lag += inner_prod(lam_v_eq,v_def);
    
    // Gradient of the Lagrangian
    SXMatrix lgrad = CasADi::gradient(lag,x);
    if(!v.empty()) lgrad -= vertcat(SXMatrix::zeros(nu_),lam_v_eq); // Put here to ensure that lgrad is of the form "h_extended -v_extended"
    makeDense(lgrad);
    if(verbose_){
      cout << "Generated the gradient of the Lagrangian." << endl;
    }

    // Condensed gradient of the Lagrangian
    f1 = lgrad[Slice(0,nu_)];
    nf1 = nu_;
    
    // Gradient of h
    SXMatrix v_eq_grad = lgrad[Slice(nu_,nx_)];
    
    // Reverse lam_v_eq and v_eq_grad
    SXMatrix v_eq_grad_reversed = v_eq_grad;
    copy(v_eq_grad.rbegin(),v_eq_grad.rend(),v_eq_grad_reversed.begin());
    SXMatrix lam_v_eq_reversed = lam_v_eq;
    copy(lam_v_eq.rbegin(),lam_v_eq.rend(),lam_v_eq_reversed.begin());
    
    // Augment h and lam_v_eq
    v_eq.append(v_eq_grad_reversed);
    v.append(lam_v_eq_reversed);
  }

  // Residual function G
  SXMatrixVector G_in(G_NUM_IN);
  G_in[G_U] = u;
  G_in[G_V] = v;
  G_in[G_LAM_X] = lam_x;
  G_in[G_LAM_G] = lam_g;

  SXMatrixVector G_out(G_NUM_OUT);
  G_out[G_D] = v_eq;
  G_out[G_G] = g;
  G_out[G_F] = f;

  rfcn_ = SXFunction(G_in,G_out);
  rfcn_.setOption("number_of_fwd_dir",0);
  rfcn_.setOption("number_of_adj_dir",0);
  rfcn_.init();
  if(verbose_){
    cout << "Generated residual function ( " << shared_cast<SXFunction>(rfcn_).getAlgorithmSize() << " nodes)." << endl;
  }
  
  // Difference vector d
  SXMatrix d = ssym("d",nv_);
  if(!gauss_newton_){
    vector<SX> dg = ssym("dg",nv_).data();
    reverse(dg.begin(),dg.end());
    d.append(dg);
  }

  // Substitute out the v from the h
  SXMatrix d_def = (v_eq + v)-d;
  SXMatrixVector ex(3);
  ex[0] = f1;
  ex[1] = f2;
  ex[2] = f;
  substituteInPlace(v, d_def, ex, false);
  SXMatrix f1_z = ex[0];
  SXMatrix f2_z = ex[1];
  SXMatrix f_z = ex[2];
  
  // Modified function Z
  enum ZIn{Z_U,Z_D,Z_LAM_X,Z_LAM_F2,Z_NUM_IN};
  SXMatrixVector zfcn_in(Z_NUM_IN);
  zfcn_in[Z_U] = u;
  zfcn_in[Z_D] = d;
  zfcn_in[Z_LAM_X] = lam_x;
  zfcn_in[Z_LAM_F2] = lam_f2;
  
  enum ZOut{Z_D_DEF,Z_F12,Z_NUM_OUT};
  SXMatrixVector zfcn_out(Z_NUM_OUT);
  zfcn_out[Z_D_DEF] = d_def;
  zfcn_out[Z_F12] = vertcat(f1_z,f2_z);
  
  SXFunction zfcn(zfcn_in,zfcn_out);
  zfcn.init();
  if(verbose_){
    cout << "Generated reconstruction function ( " << zfcn.getAlgorithmSize() << " nodes)." << endl;
  }

  // Matrix A and B in lifted Newton
  SXMatrix B = zfcn.jac(Z_U,Z_F12);
  SXMatrix B1 = B(Slice(0,nf1),Slice(0,B.size2()));
  SXMatrix B2 = B(Slice(nf1,B.size1()),Slice(0,B.size2()));
  if(verbose_){
    cout << "Formed B1 (dimension " << B1.size1() << "-by-" << B1.size2() << ", "<< B1.size() << " nonzeros) " <<
    "and B2 (dimension " << B2.size1() << "-by-" << B2.size2() << ", "<< B2.size() << " nonzeros)." << endl;
  }
  
  // Step in u
  SXMatrix du = ssym("du",nu_);
  SXMatrix dlam_f2 = ssym("dlam_f2",lam_f2.sparsity());
  
  SXMatrix b1 = f1_z;
  SXMatrix b2 = f2_z;
  SXMatrix e;
  if(nv_ > 0){
    
    // Directional derivative of Z
    vector<vector<SXMatrix> > Z_fwdSeed(2,zfcn_in);
    vector<vector<SXMatrix> > Z_fwdSens(2,zfcn_out);
    vector<vector<SXMatrix> > Z_adjSeed;
    vector<vector<SXMatrix> > Z_adjSens;
    
    Z_fwdSeed[0][Z_U].setZero();
    Z_fwdSeed[0][Z_D] = -d;
    Z_fwdSeed[0][Z_LAM_X].setZero();
    Z_fwdSeed[0][Z_LAM_F2].setZero();
    
    Z_fwdSeed[1][Z_U] = du;
    Z_fwdSeed[1][Z_D] = -d;
    Z_fwdSeed[1][Z_LAM_X].setZero();
    Z_fwdSeed[1][Z_LAM_F2] = dlam_f2;
    
    zfcn.eval(zfcn_in,zfcn_out,Z_fwdSeed,Z_fwdSens,Z_adjSeed,Z_adjSens,true);
    
    b1 += Z_fwdSens[0][Z_F12](Slice(0,nf1));
    b2 += Z_fwdSens[0][Z_F12](Slice(nf1,B.size1()));
    e = Z_fwdSens[1][Z_D_DEF];
  }
  if(verbose_){
    cout << "Formed b1 (dimension " << b1.size1() << "-by-" << b1.size2() << ", "<< b1.size() << " nonzeros) " <<
    "and b2 (dimension " << b2.size1() << "-by-" << b2.size2() << ", "<< b2.size() << " nonzeros)." << endl;
  }
  
  // Generate Gauss-Newton Hessian
  if(gauss_newton_){
    b1 = mul(trans(B1),b1);
    B1 = mul(trans(B1),B1);
    if(verbose_){
      cout << "Gauss Newton Hessian (dimension " << B1.size1() << "-by-" << B1.size2() << ", "<< B1.size() << " nonzeros)." << endl;
    }
  }
  
  // Make sure b1 and b2 are dense vectors
  makeDense(b1);
  makeDense(b2);
  
  // Quadratic approximation
  SXMatrixVector lfcn_in(LIN_NUM_IN);
  lfcn_in[LIN_U] = u;
  lfcn_in[LIN_V] = v;
  lfcn_in[LIN_D] = d;
  lfcn_in[LIN_LAM_X] = lam_x;
  lfcn_in[LIN_LAM_G] = lam_g;
  
  SXMatrixVector lfcn_out(LIN_NUM_OUT);
  lfcn_out[LIN_F1] = b1;
  lfcn_out[LIN_J1] = B1;
  lfcn_out[LIN_F2] = b2;
  lfcn_out[LIN_J2] = B2;
  lfcn_ = SXFunction(lfcn_in,lfcn_out);
//   lfcn_.setOption("verbose",true);
  lfcn_.setOption("number_of_fwd_dir",0);
  lfcn_.setOption("number_of_adj_dir",0);
  lfcn_.setOption("live_variables",true);
  lfcn_.init();
  if(verbose_){
    cout << "Generated linearization function ( " << shared_cast<SXFunction>(lfcn_).getAlgorithmSize() << " nodes)." << endl;
  }
    
  // Step expansion
  SXMatrixVector efcn_in(EXP_NUM_IN);
  copy(lfcn_in.begin(),lfcn_in.end(),efcn_in.begin());
  efcn_in[EXP_DU] = du;
  efcn_in[EXP_DLAM_F2] = dlam_f2;
  efcn_ = SXFunction(efcn_in,e);
  efcn_.setOption("number_of_fwd_dir",0);
  efcn_.setOption("number_of_adj_dir",0);
  efcn_.setOption("live_variables",true);
  efcn_.init();
  if(verbose_){
    cout << "Generated step expansion function ( " << shared_cast<SXFunction>(efcn_).getAlgorithmSize() << " nodes)." << endl;
  }
  
  // Allocate a QP solver
  qp_solver_ = QPOasesSolver(B1.sparsity(),B2.sparsity());
  qp_solver_.setOption("printLevel","none");
  
  // Initialize the QP solver
  qp_solver_.init();
  if(verbose_){
    cout << "Allocated QP solver." << endl;
  }

  // Residual
  d_k_.resize(d.size(),0);
  
  // Primal step
  dx_k_.resize(nx_);

  // Dual step
  dlambda_u_.resize(lambda_u_.size());
  dlambda_g_.resize(lambda_g_.size());
}

void Tester::solve(int& iter_count){
  bool gauss_newton_ = true;
  int maxiter_ = 100;
  double toldx_ = 1e-9;
 


  // Objective value
  double f_k = numeric_limits<double>::quiet_NaN();
  
  // Current guess for the primal solution
  copy(u_init_.begin(),u_init_.end(),u_opt_.begin());
  copy(v_init_.begin(),v_init_.end(),v_opt_.begin());
  
  int k=0;
  
  // Does G depend on the multipliers?
  bool has_lam_x =  !rfcn_.input(G_LAM_X).empty();
  bool has_lam_g =  !rfcn_.input(G_LAM_G).empty();
  bool has_lam_f2 = !efcn_.input(EXP_DLAM_F2).empty();
  
  while(true){
    // Evaluate residual
    rfcn_.setInput(u_opt_,G_U);
    rfcn_.setInput(v_opt_,G_V);
    if(has_lam_x) rfcn_.setInput(lambda_u_,G_LAM_X);
    if(has_lam_g) rfcn_.setInput(lambda_g_,G_LAM_G);
    rfcn_.evaluate();
    rfcn_.getOutput(d_k_,G_D);
    f_k = rfcn_.output(G_F).toScalar();
    const DMatrix& g_k = rfcn_.output(G_G);
    
    // Construct the QP
    lfcn_.setInput(u_opt_,LIN_U);
    lfcn_.setInput(v_opt_,LIN_V);
    
    if(has_lam_x) lfcn_.setInput(lambda_u_,LIN_LAM_X);
    if(has_lam_g) lfcn_.setInput(lambda_g_,LIN_LAM_G);
    lfcn_.setInput(d_k_,LIN_D);
    lfcn_.evaluate();
    DMatrix& B1_k = lfcn_.output(LIN_J1);
    const DMatrix& b1_k = lfcn_.output(LIN_F1);
    const DMatrix& B2_k = lfcn_.output(LIN_J2);
    const DMatrix& b2_k = lfcn_.output(LIN_F2);

    // Regularization
    double reg = 0;
    bool regularization = true;
    
    // Check the smallest eigenvalue of the Hessian
    if(regularization && nu_==2){
      double a = B1_k.elem(0,0);
      double b = B1_k.elem(0,1);
      double c = B1_k.elem(1,0);
      double d = B1_k.elem(1,1);
      
      // Make sure no not a numbers
      casadi_assert(a==a && b==b && c==c &&  d==d);
      
      // Make sure symmetric
      if(b!=c){
	casadi_assert_warning(fabs(b-c)<1e-10,"Hessian is not symmetric: " << b << " != " << c);
	B1_k.elem(1,0) = c = b;
      }
      
      double eig_smallest = (a+d)/2 - std::sqrt(4*b*c + (a-d)*(a-d))/2;
      double threshold = 1e-8;
      if(eig_smallest<threshold){
	// Regularization
	reg = threshold-eig_smallest;
	std::cerr << "Regularization with " << reg << " to ensure positive definite Hessian." << endl;
	B1_k(0,0) += reg;
	B1_k(1,1) += reg;
      }
    }
    
    
    // Solve the QP
    qp_solver_.setInput(B1_k,QP_H);
    qp_solver_.setInput(b1_k,QP_G);
    qp_solver_.setInput(B2_k,QP_A);
    std::transform(lbu_.begin(),lbu_.end(),u_opt_.begin(),qp_solver_.input(QP_LBX).begin(),std::minus<double>());
    std::transform(ubu_.begin(),ubu_.end(),u_opt_.begin(),qp_solver_.input(QP_UBX).begin(),std::minus<double>());
    std::transform(lbg_.begin()+nv_,lbg_.end(), b2_k.begin(),qp_solver_.input(QP_LBA).begin(),std::minus<double>());
    std::transform(ubg_.begin()+nv_,ubg_.end(), b2_k.begin(),qp_solver_.input(QP_UBA).begin(),std::minus<double>());
    qp_solver_.evaluate();
    const DMatrix& du_k = qp_solver_.output(QP_PRIMAL);
    const DMatrix& dlam_u_k = qp_solver_.output(QP_LAMBDA_X);
    const DMatrix& dlam_f2_k = qp_solver_.output(QP_LAMBDA_A);    
    
    // Expand the step
    for(int i=0; i<LIN_NUM_IN; ++i){
      efcn_.setInput(lfcn_.input(i),i);
    }
    efcn_.setInput(du_k,EXP_DU);
    if(has_lam_f2) efcn_.setInput(dlam_f2_k,EXP_DLAM_F2);
    efcn_.evaluate();
    const DMatrix& dv_k = efcn_.output();
    
    // Expanded primal step
    copy(du_k.begin(),du_k.end(),dx_k_.begin());
    copy(dv_k.begin(),dv_k.begin()+nv_,dx_k_.begin()+nu_);

    // Expanded dual step
    copy(dlam_u_k.begin(),dlam_u_k.end(),dlambda_u_.begin());
    copy(dlam_f2_k.begin(),dlam_f2_k.end(),dlambda_g_.begin()+nv_);
    copy(dv_k.rbegin(),dv_k.rbegin()+nv_,dlambda_g_.begin());
    
    // Take a full step
    transform(dx_k_.begin(),dx_k_.begin()+nu_,u_opt_.begin(),u_opt_.begin(),plus<double>());
    transform(dx_k_.begin()+nu_,dx_k_.end(),v_opt_.begin(),v_opt_.begin(),plus<double>());

    copy(dlambda_u_.begin(),dlambda_u_.end(),lambda_u_.begin());
    transform(dlambda_g_.begin(),dlambda_g_.end(),lambda_g_.begin(),lambda_g_.begin(),plus<double>());

    // Step size
    double norm_step=0;
    for(vector<double>::const_iterator it=dx_k_.begin(); it!=dx_k_.end(); ++it)  norm_step += *it**it;
    if(!gauss_newton_){
      for(vector<double>::const_iterator it=dlambda_g_.begin(); it!=dlambda_g_.end(); ++it) norm_step += *it**it;
    }
    norm_step = sqrt(norm_step);
    
    // Constraint violation
    double norm_viol = 0;
    for(int i=0; i<nu_; ++i){
      double d = ::fmax(u_opt_.at(i)-ubu_.at(i),0.) + ::fmax(lbu_.at(i)-u_opt_.at(i),0.);
      norm_viol += d*d;
    }
    for(int i=0; i<nv_; ++i){
      double d = ::fmax(v_opt_.at(i)-ubv_.at(i),0.) + ::fmax(lbv_.at(i)-v_opt_.at(i),0.);
      norm_viol += d*d;
    }
    for(int i=0; i<g_k.size(); ++i){
      double d = ::fmax(g_k.at(i)-ubg_.at(i),0.) + ::fmax(lbg_.at(i)-g_k.at(i),0.);
      norm_viol += d*d;
    }
    norm_viol = sqrt(norm_viol);
    
    // Print progress (including the header every 10 rows)
    if(k % 10 == 0){
      cout << setw(4) << "iter" << setw(20) << "objective" << setw(20) << "norm_step" << setw(20) << "norm_viol" << endl;
    }
    cout   << setw(4) <<     k  << setw(20) <<  f_k        << setw(20) <<  norm_step  << setw(20) <<  norm_viol  << endl;
    
    
    // Check if stopping criteria is satisfied
    if(norm_viol + norm_step < toldx_){
      cout << "Convergence achieved!" << endl;
      break;
    }
    
    // Increase iteration count
    k = k+1;
    
    // Check if number of iterations have been reached
    if(k >= maxiter_){
      cout << "Maximum number of iterations (" << maxiter_ << ") reached" << endl;
      break;
    }
  }
  
  // Store optimal value
  cout << "optimal cost = " << f_k << endl;
  iter_count = k;
}

void Tester::optimize(double drag_guess, double depth_guess, int& iter_count, double& sol_time, double& drag_est, double& depth_est){
  // Initial guess for the parameters
  u_init_[0] = drag_guess;
  u_init_[1] = depth_guess;
  fill(v_init_.begin(),v_init_.end(),0);

  // Initial guess for the heights
  if(!single_shooting_){
    vector<double>::iterator it=v_init_.begin();
    for(int k=0; k<n_meas_; ++k){
      copy(H_meas_[k].begin(),H_meas_[k].end(),it);
      it += n_boxes_*n_boxes_;
    }
  }
  
  fill(lbg_.begin(),lbg_.end(),0);
  fill(ubg_.begin(),ubg_.end(),0);
  // 	ubg_[nv] = numeric_limits<double>::infinity();
  // 	ubg_[nv+1] = numeric_limits<double>::infinity();
  // 	lbg_[nv] = -numeric_limits<double>::infinity();
  // 	lbg_[nv+1] = -numeric_limits<double>::infinity();
	
  //   lbu_.setAll(-1000.);
  //   ubu_.setAll( 1000.);
  lbu_[0] = 0;
  lbu_[1] = 0;

  clock_t time1 = clock();
  solve(iter_count);
  clock_t time2 = clock();
  
  // Solution statistics  
  sol_time = double(time2-time1)/CLOCKS_PER_SEC;
  drag_est = u_opt_.at(0);
  depth_est = u_opt_.at(1);
}

int main(){

  double drag_true = 2.0; // => u(0)
  double depth_true = 0.01; // => u(1)
 

  // Initial guesses
  vector<double> drag_guess, depth_guess;
  drag_guess.push_back( 0.5); depth_guess.push_back(0.01);
  drag_guess.push_back( 5.0); depth_guess.push_back(0.01);
  drag_guess.push_back(15.0); depth_guess.push_back(0.01);
  drag_guess.push_back(30.0); depth_guess.push_back(0.01);
  drag_guess.push_back( 2.0); depth_guess.push_back(0.005);
  drag_guess.push_back( 2.0); depth_guess.push_back(0.02);
  drag_guess.push_back( 2.0); depth_guess.push_back(0.1);
  drag_guess.push_back( 0.2); depth_guess.push_back(0.001);
  drag_guess.push_back( 1.0); depth_guess.push_back(0.005);
  drag_guess.push_back( 4.0); depth_guess.push_back(0.02);
  drag_guess.push_back( 1.0); depth_guess.push_back(0.02);
  drag_guess.push_back(20.0); depth_guess.push_back(0.001);
  
  // Number of tests
  const int n_tests = drag_guess.size();

  // Number of iterations
  vector<int> iter_count_gn(n_tests,-1);
  vector<int> iter_count_eh(n_tests,-1);
  
  // Solution time
  vector<double> sol_time_gn(n_tests,-1);
  vector<double> sol_time_eh(n_tests,-1);
  
  // Estimated drag and depth
  vector<double> drag_est_gn(n_tests,-1);
  vector<double> depth_est_gn(n_tests,-1);
  vector<double> drag_est_eh(n_tests,-1);
  vector<double> depth_est_eh(n_tests,-1);
  
  // Create a tester object
  // Tester t(3,20,20); // The largest dimensions which work with SX and IPOPT
  Tester t(15,10,10); // The largest dimensions which work with SX and exact Hessian
  // Tester t(20,10,50); // The largest dimensions which work with SX and Gauss-Newton Hessian
    
  // Perform the modelling
  t.model();

  // Optimization parameters
  t.simulate(drag_true, depth_true);
  
  // For both single and multiple shooting
  for(int sol=0; sol<2; ++sol){

    // Transcribe as an NLP
    bool single_shooting = sol==0;
    t.transcribe(single_shooting);
  
    //    continue;
  
    // Run tests
    for(int test=0; test<n_tests; ++test){
      // Print progress
      cout << "test " << test << endl;
      try{
	t.optimize(drag_guess[test],depth_guess[test],
		   sol==0 ? iter_count_gn[test] : iter_count_eh[test],
		   sol==0 ? sol_time_gn[test] : sol_time_eh[test],
		   sol==0 ? drag_est_gn[test] : drag_est_eh[test],
		   sol==0 ? depth_est_gn[test] : depth_est_eh[test]);

	// Estimated drag
      } catch(exception& ex){
	cout << "Test " << test << " failed: " << ex.what() << endl;
      }
    }
  }

  // Tolerance 
  double tol=1e-3;
  
  cout << 
  setw(10) << "drag" <<  "  &" <<
  setw(10) << "depth" << "  &" << 
  setw(10) << "iter_ss" << "  &" << 
  setw(10) << "time_ss" << "  &" <<
  setw(10) << "iter_ms" << "  &" << 
  setw(10) << "time_ms" << "  \\\\ \%" <<
  setw(10) << "edrag_ss" << 
  setw(10) << "edepth_ss" <<
  setw(10) << "edrag_ms" << 
  setw(10) << "edepth_ms" << endl;
  for(int test=0; test<n_tests; ++test){
    cout << setw(10) << drag_guess[test] << "  &";
    cout << setw(10) << depth_guess[test] << "  &";
    if(fabs(drag_est_gn[test]-drag_true) + fabs(depth_est_gn[test]-depth_true)<tol){
      cout << setw(10) << iter_count_gn[test] << "  &";
      cout << setw(10) << sol_time_gn[test] << "  &";
    } else {
      cout << setw(10) << "$\\infty$" << "  &";
      cout << setw(10) << "$\\infty$" << "  &";
    }
    if(fabs(drag_est_eh[test]-drag_true) + fabs(depth_est_eh[test]-depth_true)<tol){
      cout << setw(10) << iter_count_eh[test] << "  &";
      cout << setw(10) << sol_time_eh[test] << "  \\\\ \%";
    } else {
      cout << setw(10) << "$\\infty$" << "  &";
      cout << setw(10) << "$\\infty$" << "  \\\\ \%";
    }
    cout << setw(10) << drag_est_gn[test];
    cout << setw(10) << depth_est_gn[test];
    cout << setw(10) << drag_est_eh[test];
    cout << setw(10) << depth_est_eh[test] << endl;
  }
  
  return 0;
}
