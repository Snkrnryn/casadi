from casadi import *
from copy import deepcopy

# Time 
t = ssym("t")

# Differential states
s = ssym("s"); v = ssym("v"); m = ssym("m")
x = vertcat([s,v,m])

# State derivatives
sdot = ssym("sdot"); vdot = ssym("vdot"); mdot = ssym("mdot")
xdot = vertcat([sdot,vdot,mdot])

# Control
u = ssym("u")

# Constants
alpha = 0.05 # friction
beta = 0.1   # fuel consumption rate
  
# Differential equation
ode = vertcat([
  v-sdot,
  (u-alpha*v*v)/m - vdot,
  -beta*u*u       - mdot])
  
# Quadrature
quad = v**3 + ((3-sin(t)) - u)**2

# DAE callback function
ffcn = SXFunction(daeIn(t=t,x=x,xdot=xdot,p=u),daeOut(ode=ode,quad=quad))

# Time length
tf = 0.5

# Initial position
x0 = [0.,0.,1.]

# Parameter guess
u0 = 0.4

# Integrator
I = CVodesIntegrator(ffcn)
I.setOption("tf",tf)
I.init()

# Calculate once, adjoint
I_adj = I.derivative(0,1)
I_adj.setInput(x0,"x0")
I_adj.setInput(u0,"p")
I_adj.setInput([0,0,0],INTEGRATOR_NUM_IN+INTEGRATOR_XF)
I_adj.setInput(1.0,INTEGRATOR_NUM_IN+INTEGRATOR_QF)
I_adj.evaluate()
adj_x0 = deepcopy(I_adj.getOutput(INTEGRATOR_NUM_OUT+INTEGRATOR_X0))
adj_p = deepcopy(I_adj.getOutput(INTEGRATOR_NUM_OUT+INTEGRATOR_P))
print "%50s" % "Adjoint sensitivities:", "d(qf)/d(x0) = ", adj_x0, ", d(qf)/d(p) = ", adj_p

# Perturb adjoint solution to get a finite difference approximation of the second order sensitivities
h = 0.001
I_adj.setInput(x0,"x0")
I_adj.setInput(u0+h,"p")
I_adj.setInput([0,0,0],INTEGRATOR_NUM_IN+INTEGRATOR_XF)
I_adj.setInput(1.0,INTEGRATOR_NUM_IN+INTEGRATOR_QF)
I_adj.evaluate()
adj_x0_pert = deepcopy(I_adj.getOutput(INTEGRATOR_NUM_OUT+INTEGRATOR_X0))
adj_p_pert = deepcopy(I_adj.getOutput(INTEGRATOR_NUM_OUT+INTEGRATOR_P))
print "%50s" % "FD of adjoint sensitivities:", "d2(qf)/d(x0)d(p) = ", (adj_x0_pert-adj_x0)/h, ", d2(qf)/d(p)d(p) = ", (adj_p_pert-adj_p)/h
