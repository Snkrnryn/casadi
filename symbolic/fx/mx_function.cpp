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

#include "mx_function_internal.hpp"
#include "../mx/mx_node.hpp"
#include "../stl_vector_tools.hpp"
#include "../mx/mx_tools.hpp"

#include <stack>
#include <typeinfo>
#include <cassert>

using namespace std;

namespace CasADi{

bool MXFunction::checkNode() const{
  return dynamic_cast<const MXFunctionInternal*>(get())!=0;
}

MXFunction::MXFunction(){
}

MXFunction::MXFunction(const FX& fx) {
  const MXFunctionInternal* temp = dynamic_cast<const MXFunctionInternal*>(fx.get());
  if (!temp) casadi_error("MXFunction(FX)::input FX cannot be cast into MXFunction");
  assignNode(temp->clone());
}
  
MXFunction::MXFunction(const MX& inputm, const MX& outputm){
  vector<MX> inputv(1);
  inputv[0] = inputm;
  vector<MX> outputv(1);
  outputv[0] = outputm;
  assignNode(new MXFunctionInternal(inputv,outputv));
}

MXFunction::MXFunction(const MX& inputm, const std::vector<MX>& outputv){
  vector<MX> inputv(1);
  inputv[0] = inputm;
  assignNode(new MXFunctionInternal(inputv,outputv));
}

MXFunction::MXFunction(const std::vector<MX>& inputv, const MX& outputm){
  vector<MX> outputv(1);
  outputv[0] = outputm;
  assignNode(new MXFunctionInternal(inputv,outputv));
}

MXFunction::MXFunction(const std::vector<MX>& inputv, const std::vector<MX>& outputv){
  assignNode(new MXFunctionInternal(inputv,outputv));
}

MXFunction::MXFunction(const std::vector<MX>& inputv, const IOSchemeVector< MX >& outputv){
  assignNode(new MXFunctionInternal(inputv,outputv));
  setOutputScheme(outputv.io_scheme());
}

MXFunction::MXFunction(const IOSchemeVector< MX >& inputv, const std::vector<MX>& outputv){
  assignNode(new MXFunctionInternal(inputv,outputv));
  setInputScheme(inputv.io_scheme());
}


MXFunction::MXFunction(const IOSchemeVector< MX >& inputv, const IOSchemeVector< MX >& outputv){
  assignNode(new MXFunctionInternal(inputv,outputv));
  setInputScheme(inputv.io_scheme());
  setOutputScheme(outputv.io_scheme());
}

const MXFunctionInternal* MXFunction::operator->() const{
  return (const MXFunctionInternal*)FX::operator->();
}

MXFunctionInternal* MXFunction::operator->(){
  return (MXFunctionInternal*)FX::operator->();
}

const MX& MXFunction::inputExpr(int ind) const{
  return (*this)->inputv_.at(ind);
}

const MX& MXFunction::outputExpr(int ind) const{
  return (*this)->outputv_.at(ind);
}
  
const std::vector<MX>& MXFunction::inputExpr() const{
  return (*this)->inputv_;
}
  
const std::vector<MX> & MXFunction::outputExpr() const{
  return (*this)->outputv_;
}

const std::vector<MXAlgEl>& MXFunction::algorithm() const{
  return (*this)->algorithm_;
}

int MXFunction::countNodes() const{
  assertInit();
  return algorithm().size();
}

MX MXFunction::jac(int iind, int oind, bool compact, bool symmetric){
  return (*this)->jac(iind,oind,compact,symmetric);
}

MX MXFunction::grad(int iind, int oind){
  return (*this)->grad(iind,oind);
}

SXFunction MXFunction::expand(const std::vector<SXMatrix>& inputv){
  return (*this)->expand(inputv);
}

std::vector<MX> MXFunction::getFree() const{
  return (*this)->free_vars_;
}

int MXFunction::getWorkSize() const{
  return (*this)->work_.size();
}

  void MXFunction::generateLiftingFunctions(MXFunction& vdef_fcn, MXFunction& vinit_fcn){
    (*this)->generateLiftingFunctions(vdef_fcn,vinit_fcn);
  }


} // namespace CasADi

