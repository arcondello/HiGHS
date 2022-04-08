import highspy
import numpy as np
inf = highspy.kHighsInf
h = highspy.Highs()
h.addVar(-inf, inf)
h.addVar(-inf, inf)
h.changeColsCost(2, np.array([0, 1]), np.array([0, 1], dtype=np.double))
num_cons = 2
lower = np.array([2, 0], dtype=np.double)
upper = np.array([inf, inf], dtype=np.double)
num_new_nz = 4
starts = np.array([0, 2])
indices = np.array([0, 1, 0, 1])
values = np.array([-1, 1, 1, 1], dtype=np.double)
h.addRows(num_cons, lower, upper, num_new_nz, starts, indices, values)
h.setOptionValue('log_to_console', True)
h.run()
num_var = h.getNumCol()
solution = h.getSolution()
basis = h.getBasis()
info = h.getInfo()
# ToDo Avoid having to pass "False"
model_status = h.getModelStatus(False)
print('Model status = ', h.modelStatusToString(model_status))
print('Optimal objective = ', info.objective_function_value)
print('Iteration count = ', info.simplex_iteration_count)
print('Primal solution status = ', h.solutionStatusToString(info.primal_solution_status))
print('Dual solution status = ', h.solutionStatusToString(info.dual_solution_status))
print('Basis validity = ', h.basisValidityToString(info.basis_validity))
for icol in range(num_var):
    print(icol, solution.col_value[icol], h.basisStatusToString(basis.col_status[icol]))

# Read in and solve avgas
h.readModel("check/instances/avgas.mps")
#h.writeModel("ml.mps")
h.run()
lp = h.getLp()
num_nz = h.getNumNz()
print('LP has ', lp.num_col_, ' columns', lp.num_row_, ' rows and ', num_nz, ' nonzeros')

# Clear so that incumbent model is empty
h.clear()

# Now define the blending model as a HighsLp instance
#
# ToDo Determine whether an empty HighsLp instance can be created as
# in C++, rather than pulling it from an emty Highs instance like this
lp = h.getLp()
num_nz = h.getNumNz()
print('LP has ', lp.num_col_, ' columns', lp.num_row_, ' rows and ', num_nz, ' nonzeros')
lp.num_col_ = 2
lp.num_row_ = 2
lp.sense_ = highspy.ObjSense.kMaximize
lp.col_cost_ = np.array([8, 10], dtype=np.double)
lp.col_lower_ = np.array([0, 0], dtype=np.double)
lp.col_upper_ = np.array([inf, inf], dtype=np.double)
lp.row_lower_ = np.array([-inf, -inf], dtype=np.double)
lp.row_upper_ = np.array([120, 210], dtype=np.double)
lp.a_matrix_.start_ = np.array([0, 2, 4])
lp.a_matrix_.index_ = np.array([0, 1, 0, 1])
lp.a_matrix_.value_ = np.array([0.3, 0.7, 0.5, 0.5], dtype=np.double)
h.passLp(lp)
h.run()
solution = h.getSolution()
basis = h.getBasis()
info = h.getInfo()
model_status = h.getModelStatus(False)
print('Model status = ', h.modelStatusToString(model_status))
print('Optimal objective = ', info.objective_function_value)
print('Iteration count = ', info.simplex_iteration_count)
print('Primal solution status = ', h.solutionStatusToString(info.primal_solution_status))
print('Dual solution status = ', h.solutionStatusToString(info.dual_solution_status))
print('Basis validity = ', h.basisValidityToString(info.basis_validity))
num_var = h.getNumCol()
num_row = h.getNumRow()
print('Variables')
for icol in range(num_var):
    print(icol, solution.col_value[icol], h.basisStatusToString(basis.col_status[icol]))
print('Constraints')
for irow in range(num_row):
    print(irow, solution.row_value[irow], h.basisStatusToString(basis.row_status[irow]))
