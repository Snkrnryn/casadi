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

#ifndef RK_INTEGRATOR_HPP
#define RK_INTEGRATOR_HPP

#include "symbolic/fx/integrator.hpp"

namespace CasADi{
  
  class RKIntegratorInternal;
  
  /**
     \brief Fixed step Runge-Kutta integrator
     ODE integrator based on explicit Runge-Kutta methods
  
     The method is still under development
  
     \author Joel Andersson
     \date 2011
  */
  class RKIntegrator : public Integrator {
  public:
    /** \brief  Default constructor */
    RKIntegrator();
    
    /** \brief  Create an integrator for explicit ODEs
     *   \param f dynamical system
     * \copydoc scheme_DAEInput
     * \copydoc scheme_DAEOutput
     *
     */
    explicit RKIntegrator(const FX& f);

    /// Access functions of the node
    RKIntegratorInternal* operator->();
    const RKIntegratorInternal* operator->() const;

    /// Check if the node is pointing to the right type of object
    virtual bool checkNode() const;

    /// Static creator function
#ifdef SWIG
    %callback("%s_cb");
#endif
    static Integrator creator(const FX& f, int nfwd, int nadj){ return RKIntegrator(f);}
#ifdef SWIG
    %nocallback;
#endif
    
  };

} // namespace CasADi

#endif //RK_INTEGRATOR_HPP
