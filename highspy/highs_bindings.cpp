#include "Highs.h"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>


namespace py = pybind11;
using namespace pybind11::literals;


void highs_passModel(Highs* h, HighsModel& model)
{
  HighsStatus status = h->passModel(model);
  if (status != HighsStatus::kOk)
    throw py::value_error("Error when passing model");
}
 
void highs_passLp(Highs* h, HighsLp& lp)
{
  HighsStatus status = h->passModel(lp);
  if (status != HighsStatus::kOk)
    throw py::value_error("Error when passing LP");
}
 
void highs_addRow(Highs* h, double lower, double upper, int num_new_nz, py::array_t<int> indices, py::array_t<double> values)
{
  py::buffer_info indices_info = indices.request();
  py::buffer_info values_info = values.request();

  int* indices_ptr = static_cast<int*>(indices_info.ptr);
  double* values_ptr = static_cast<double*>(values_info.ptr);

  HighsStatus status = h->addRow(lower, upper, num_new_nz, indices_ptr, values_ptr);

  if (status != HighsStatus::kOk)
    throw py::value_error("Error when adding row");
}


void highs_addRows(Highs* h, int num_cons, py::array_t<double> lower, py::array_t<double> upper, int num_new_nz,
		   py::array_t<int> starts, py::array_t<int> indices, py::array_t<double> values)
{
  py::buffer_info lower_info = lower.request();
  py::buffer_info upper_info = upper.request();
  py::buffer_info starts_info = starts.request();
  py::buffer_info indices_info = indices.request();
  py::buffer_info values_info = values.request();

  double* lower_ptr = static_cast<double*>(lower_info.ptr);
  double* upper_ptr = static_cast<double*>(upper_info.ptr);
  int* starts_ptr = static_cast<int*>(starts_info.ptr);
  int* indices_ptr = static_cast<int*>(indices_info.ptr);
  double* values_ptr = static_cast<double*>(values_info.ptr);

  HighsStatus status = h->addRows(num_cons, lower_ptr, upper_ptr, num_new_nz, starts_ptr, indices_ptr, values_ptr);

  if (status != HighsStatus::kOk)
    throw py::value_error("Error when adding rows");
}


void highs_addCol(Highs* h, double cost, double lower, double upper, int num_new_nz, py::array_t<int> indices, py::array_t<double> values)
{
  py::buffer_info indices_info = indices.request();
  py::buffer_info values_info = values.request();

  int* indices_ptr = static_cast<int*>(indices_info.ptr);
  double* values_ptr = static_cast<double*>(values_info.ptr);

  HighsStatus status = h->addCol(cost, lower, upper, num_new_nz, indices_ptr, values_ptr);

  if (status != HighsStatus::kOk)
    throw py::value_error("Error when adding col");
}


void highs_addVar(Highs* h, double lower, double upper)
{
  HighsStatus status = h->addVar(lower, upper);

  if (status != HighsStatus::kOk)
    throw py::value_error("Error when adding var");  
}


void highs_addVars(Highs* h, int num_vars, py::array_t<double> lower, py::array_t<double> upper)
{
  py::buffer_info lower_info = lower.request();
  py::buffer_info upper_info = upper.request();

  double* lower_ptr = static_cast<double*>(lower_info.ptr);
  double* upper_ptr = static_cast<double*>(upper_info.ptr);

  HighsStatus status = h->addVars(num_vars, lower_ptr, upper_ptr);

  if (status != HighsStatus::kOk)
    throw py::value_error("Error when adding vars");  
}


void highs_changeColsCost(Highs* h, int num_set_entries, py::array_t<int> indices, py::array_t<double> cost)
{
  py::buffer_info indices_info = indices.request();
  py::buffer_info cost_info = cost.request();

  int* indices_ptr = static_cast<int*>(indices_info.ptr);
  double* cost_ptr = static_cast<double*>(cost_info.ptr);

  HighsStatus status = h->changeColsCost(num_set_entries, indices_ptr, cost_ptr);

  if (status != HighsStatus::kOk)
    throw py::value_error("Error when changing objective coefficients");  
}


void highs_changeColsBounds(Highs* h, int num_set_entries, py::array_t<int> indices, py::array_t<double> lower, py::array_t<double> upper)
{
  py::buffer_info indices_info = indices.request();
  py::buffer_info lower_info = lower.request();
  py::buffer_info upper_info = upper.request();

  int* indices_ptr = static_cast<int*>(indices_info.ptr);
  double* lower_ptr = static_cast<double*>(lower_info.ptr);
  double* upper_ptr = static_cast<double*>(upper_info.ptr);

  HighsStatus status = h->changeColsBounds(num_set_entries, indices_ptr, lower_ptr, upper_ptr);

  if (status != HighsStatus::kOk)
    throw py::value_error("Error when changing variable bounds");  
}


void highs_changeColsIntegrality(Highs* h, int num_set_entries, py::array_t<int> indices, py::array_t<HighsVarType> integrality)
{
  py::buffer_info indices_info = indices.request();
  py::buffer_info integrality_info = integrality.request();

  int* indices_ptr = static_cast<int*>(indices_info.ptr);
  HighsVarType* integrality_ptr = static_cast<HighsVarType*>(integrality_info.ptr);

  HighsStatus status = h->changeColsIntegrality(num_set_entries, indices_ptr, integrality_ptr);

  if (status != HighsStatus::kOk)
    throw py::value_error("Error when changing variable integrality");  
}


void highs_deleteVars(Highs* h, int num_set_entries, py::array_t<int> indices)
{
  py::buffer_info indices_info = indices.request();

  int* indices_ptr = static_cast<int*>(indices_info.ptr);

  HighsStatus status = h->deleteVars(num_set_entries, indices_ptr);

  if (status != HighsStatus::kOk)
    throw py::value_error("Error when deleting columns");  
}


void highs_deleteRows(Highs* h, int num_set_entries, py::array_t<int> indices)
{
  py::buffer_info indices_info = indices.request();

  int* indices_ptr = static_cast<int*>(indices_info.ptr);

  HighsStatus status = h->deleteRows(num_set_entries, indices_ptr);

  if (status != HighsStatus::kOk)
    throw py::value_error("Error when deleting rows");  
}


bool highs_getBoolOption(Highs* h, const std::string& option)
{
  bool res;
  HighsStatus status = h->getOptionValue(option, res);

  if (status != HighsStatus::kOk)
    throw py::value_error("Error while getting option " + option);

  return res;
}


int highs_getIntOption(Highs* h, const std::string& option)
{
  int res;
  HighsStatus status = h->getOptionValue(option, res);

  if (status != HighsStatus::kOk)
    throw py::value_error("Error while getting option " + option);

  return res;
}


double highs_getDoubleOption(Highs* h, const std::string& option)
{
  double res;
  HighsStatus status = h->getOptionValue(option, res);

  if (status != HighsStatus::kOk)
    throw py::value_error("Error while getting option " + option);

  return res;
}


std::string highs_getStringOption(Highs* h, const std::string& option)
{
  std::string res;
  HighsStatus status = h->getOptionValue(option, res);

  if (status != HighsStatus::kOk)
    throw py::value_error("Error while getting option " + option);

  return res;
}


py::object highs_getOptionValue(Highs* h, const std::string& option)
{
  HighsOptionType option_type;
  HighsStatus status = h->getOptionType(option, option_type);

  if (status != HighsStatus::kOk)
    throw py::value_error("Error while getting option " + option);

  if (option_type == HighsOptionType::kBool)
    return py::cast(highs_getBoolOption(h, option));
  else if (option_type == HighsOptionType::kInt)
    return py::cast(highs_getIntOption(h, option));
  else if (option_type == HighsOptionType::kDouble)
    return py::cast(highs_getDoubleOption(h, option));
  else if (option_type == HighsOptionType::kString)
    return py::cast(highs_getStringOption(h, option));
  else
    throw py::value_error("Unrecognized option type");
}


ObjSense highs_getObjectiveSense(Highs* h)
{
  ObjSense obj_sense;
  HighsStatus status = h->getObjectiveSense(obj_sense);

  if (status != HighsStatus::kOk)
    throw py::value_error("Error while getting objective sense");

  return obj_sense;
}


double highs_getObjectiveOffset(Highs* h)
{
  double obj_offset;
  HighsStatus status = h->getObjectiveOffset(obj_offset);

  if (status != HighsStatus::kOk)
    throw py::value_error("Error while getting objective offset");

  return obj_offset;
}


class CallbackTuple {
public:
  CallbackTuple() = default;
  CallbackTuple(py::object _callback, py::object _cb_data) : callback(_callback), callback_data(_cb_data) {}
  ~CallbackTuple() = default;
  py::object callback;
  py::object callback_data;
};


void py_log_callback(HighsLogType log_type, const char* msgbuffer, void* callback_data)
{
  CallbackTuple* callback_tuple = static_cast<CallbackTuple*>(callback_data);
  std::string msg(msgbuffer);
  callback_tuple->callback(log_type, msg, callback_tuple->callback_data);
}


HighsStatus highs_setLogCallback(Highs* h, CallbackTuple* callback_tuple)
{
  void (*_log_callback)(HighsLogType, const char*, void*) = &py_log_callback;
  HighsStatus status = h->setLogCallback(_log_callback, callback_tuple);
  return status;
}


PYBIND11_MODULE(highs_bindings, m)
{
  py::enum_<ObjSense>(m, "ObjSense")
    .value("kMinimize", ObjSense::kMinimize)
    .value("kMaximize", ObjSense::kMaximize);
  py::enum_<MatrixFormat>(m, "MatrixFormat")
    .value("kColwise", MatrixFormat::kColwise)
    .value("kRowwise", MatrixFormat::kRowwise)
    .value("kRowwisePartitioned", MatrixFormat::kRowwisePartitioned);
  py::enum_<HessianFormat>(m, "HessianFormat")
    .value("kTriangular", HessianFormat::kTriangular)
    .value("kSquare", HessianFormat::kSquare);
  py::enum_<SolutionStatus>(m, "SolutionStatus")
    .value("kSolutionStatusNone", SolutionStatus::kSolutionStatusNone)
    .value("kSolutionStatusInfeasible", SolutionStatus::kSolutionStatusInfeasible)
    .value("kSolutionStatusFeasible", SolutionStatus::kSolutionStatusFeasible);
  py::enum_<BasisValidity>(m, "BasisValidity")
    .value("kBasisValidityInvalid", BasisValidity::kBasisValidityInvalid)
    .value("kBasisValidityValid", BasisValidity::kBasisValidityValid);
  py::enum_<HighsModelStatus>(m, "HighsModelStatus")
    .value("kNotset", HighsModelStatus::kNotset)
    .value("kLoadError", HighsModelStatus::kLoadError)
    .value("kModelError", HighsModelStatus::kModelError)
    .value("kPresolveError", HighsModelStatus::kPresolveError)
    .value("kSolveError", HighsModelStatus::kSolveError)
    .value("kPostsolveError", HighsModelStatus::kPostsolveError)
    .value("kModelEmpty", HighsModelStatus::kModelEmpty)
    .value("kOptimal", HighsModelStatus::kOptimal)
    .value("kInfeasible", HighsModelStatus::kInfeasible)
    .value("kUnboundedOrInfeasible", HighsModelStatus::kUnboundedOrInfeasible)
    .value("kUnbounded", HighsModelStatus::kUnbounded)
    .value("kObjectiveBound", HighsModelStatus::kObjectiveBound)
    .value("kObjectiveTarget", HighsModelStatus::kObjectiveTarget)
    .value("kTimeLimit", HighsModelStatus::kTimeLimit)
    .value("kIterationLimit", HighsModelStatus::kIterationLimit)
    .value("kUnknown", HighsModelStatus::kUnknown);
  py::enum_<HighsBasisStatus>(m, "HighsBasisStatus")
    .value("kLower", HighsBasisStatus::kLower)
    .value("kBasic", HighsBasisStatus::kBasic)
    .value("kUpper", HighsBasisStatus::kUpper)
    .value("kZero", HighsBasisStatus::kZero)
    .value("kNonbasic", HighsBasisStatus::kNonbasic);
  py::enum_<HighsVarType>(m, "HighsVarType")
    .value("kContinuous", HighsVarType::kContinuous)
    .value("kInteger", HighsVarType::kInteger)
    .value("kSemiContinuous", HighsVarType::kSemiContinuous)
    .value("kSemiInteger", HighsVarType::kSemiInteger);
  py::enum_<HighsStatus>(m, "HighsStatus")
    .value("kError", HighsStatus::kError)
    .value("kOk", HighsStatus::kOk)
    .value("kWarning", HighsStatus::kWarning);
  py::enum_<HighsLogType>(m, "HighsLogType")
    .value("kInfo", HighsLogType::kInfo)
    .value("kDetailed", HighsLogType::kDetailed)
    .value("kVerbose", HighsLogType::kVerbose)
    .value("kWarning", HighsLogType::kWarning)
    .value("kError", HighsLogType::kError);
  py::class_<CallbackTuple>(m, "CallbackTuple")
    .def(py::init<>())
    .def(py::init<py::object, py::object>())
    .def_readwrite("callback", &CallbackTuple::callback)
    .def_readwrite("callback_data", &CallbackTuple::callback_data);
  py::class_<HighsSparseMatrix>(m, "HighsSparseMatrix")
    .def_readwrite("format_", &HighsSparseMatrix::format_)
    .def_readwrite("num_col_", &HighsSparseMatrix::num_col_)
    .def_readwrite("num_row_", &HighsSparseMatrix::num_row_)
    .def_readwrite("start_", &HighsSparseMatrix::start_)
    .def_readwrite("p_end_", &HighsSparseMatrix::p_end_)
    .def_readwrite("index_", &HighsSparseMatrix::index_)
    .def_readwrite("value_", &HighsSparseMatrix::value_);
 py::class_<HighsLp>(m, "HighsLp")
   .def_readwrite("num_col_", &HighsLp::num_col_)
   .def_readwrite("num_row_", &HighsLp::num_row_)
   .def_readwrite("col_cost_", &HighsLp::col_cost_)
   .def_readwrite("col_lower_", &HighsLp::col_lower_)
   .def_readwrite("col_upper_", &HighsLp::col_upper_)
   .def_readwrite("row_lower_", &HighsLp::row_lower_)
   .def_readwrite("row_upper_", &HighsLp::row_upper_)
   .def_readwrite("a_matrix_", &HighsLp::a_matrix_)
   .def_readwrite("sense_", &HighsLp::sense_)
   .def_readwrite("offset_", &HighsLp::offset_)
   .def_readwrite("model_name_", &HighsLp::model_name_)
   .def_readwrite("col_names_", &HighsLp::col_names_)
   .def_readwrite("row_names_", &HighsLp::row_names_)
   .def_readwrite("integrality_", &HighsLp::integrality_)
   .def_readwrite("scale_", &HighsLp::scale_)
   .def_readwrite("is_scaled_", &HighsLp::is_scaled_)
   .def_readwrite("is_moved_", &HighsLp::is_moved_)
   .def_readwrite("mods_", &HighsLp::mods_);
  py::class_<HighsSolution>(m, "HighsSolution")
    .def_readwrite("value_valid", &HighsSolution::value_valid)
    .def_readwrite("dual_valid", &HighsSolution::dual_valid)
    .def_readwrite("col_value", &HighsSolution::col_value)
    .def_readwrite("col_dual", &HighsSolution::col_dual)
    .def_readwrite("row_value", &HighsSolution::row_value)
    .def_readwrite("row_dual", &HighsSolution::row_dual);
  py::class_<HighsBasis>(m, "HighsBasis")
    .def_readwrite("valid", &HighsBasis::valid)
    .def_readwrite("alien", &HighsBasis::alien)
    .def_readwrite("was_alien", &HighsBasis::was_alien)
    .def_readwrite("debug_id", &HighsBasis::debug_id)
    .def_readwrite("debug_update_count", &HighsBasis::debug_update_count)
    .def_readwrite("debug_origin_name", &HighsBasis::debug_origin_name)
    .def_readwrite("col_status", &HighsBasis::col_status)
    .def_readwrite("row_status", &HighsBasis::row_status);
  py::class_<HighsInfo>(m, "HighsInfo")
    .def_readwrite("valid", &HighsInfo::valid)
    .def_readwrite("mip_node_count", &HighsInfo::mip_node_count)
    .def_readwrite("simplex_iteration_count", &HighsInfo::simplex_iteration_count)
    .def_readwrite("ipm_iteration_count", &HighsInfo::ipm_iteration_count)
    .def_readwrite("qp_iteration_count", &HighsInfo::qp_iteration_count)
    .def_readwrite("crossover_iteration_count", &HighsInfo::crossover_iteration_count)
    .def_readwrite("primal_solution_status", &HighsInfo::primal_solution_status)
    .def_readwrite("dual_solution_status", &HighsInfo::dual_solution_status)
    .def_readwrite("basis_validity", &HighsInfo::basis_validity)
    .def_readwrite("objective_function_value", &HighsInfo::objective_function_value)
    .def_readwrite("mip_dual_bound", &HighsInfo::mip_dual_bound)
    .def_readwrite("mip_gap", &HighsInfo::mip_gap)
    .def_readwrite("max_integrality_violation", &HighsInfo::max_integrality_violation)
    .def_readwrite("num_primal_infeasibilities", &HighsInfo::num_primal_infeasibilities)
    .def_readwrite("max_primal_infeasibility", &HighsInfo::max_primal_infeasibility)
    .def_readwrite("sum_primal_infeasibilities", &HighsInfo::sum_primal_infeasibilities)
    .def_readwrite("num_dual_infeasibilities", &HighsInfo::num_dual_infeasibilities)
    .def_readwrite("max_dual_infeasibility", &HighsInfo::max_dual_infeasibility)
    .def_readwrite("sum_dual_infeasibilities", &HighsInfo::sum_dual_infeasibilities);
  py::class_<Highs>(m, "_Highs")
    .def(py::init<>())
    .def("readModel", &Highs::readModel)
    .def("passModel", &highs_passModel)
    .def("passLp", &highs_passLp)
    .def("run", &Highs::run)
    .def("writeModel", &Highs::writeModel)
    .def("getLp", &Highs::getLp)
    .def("getSolution", &Highs::getSolution)
    .def("getBasis", &Highs::getBasis)
    .def("getInfo", &Highs::getInfo)
    .def("changeObjectiveSense", &Highs::changeObjectiveSense)
    .def("changeObjectiveOffset", &Highs::changeObjectiveOffset)
    .def("changeColIntegrality", &Highs::changeColIntegrality)
    .def("changeColCost", &Highs::changeColCost)
    .def("changeColBounds", &Highs::changeColBounds)
    .def("changeRowBounds", &Highs::changeRowBounds)
    .def("changeCoeff", &Highs::changeCoeff)
    .def("getObjectiveValue", &Highs::getObjectiveValue)
    .def("getObjectiveSense", &highs_getObjectiveSense)
    .def("getObjectiveOffset", &highs_getObjectiveOffset)
    .def("getRunTime", &Highs::getRunTime)
    .def("getModelStatus", &Highs::getModelStatus)
    .def("addRows", &highs_addRows)
    .def("addRow", &highs_addRow)
    .def("addCol", &highs_addCol)
    .def("addVar", &highs_addVar)
    .def("addVars", &highs_addVars)
    .def("changeColsCost", &highs_changeColsCost)
    .def("changeColsBounds", &highs_changeColsBounds)
    .def("changeColsIntegrality", &highs_changeColsIntegrality)
    .def("setLogCallback", &highs_setLogCallback)
    .def("deleteVars", &highs_deleteVars)
    .def("deleteRows", &highs_deleteRows)
    .def("clear", &Highs::clear)
    .def("clearModel", &Highs::clearModel)
    .def("clearSolver", &Highs::clearSolver)
    .def("checkSolutionFeasibility", &Highs::checkSolutionFeasibility)
    .def("getNumCol", &Highs::getNumCol)
    .def("getNumRow", &Highs::getNumRow)
    .def("getNumNz", &Highs::getNumNz)
    .def("getHessianNumNz", &Highs::getHessianNumNz)
    .def("resetOptions", &Highs::resetOptions)
    .def("getOptionValue", &highs_getOptionValue)
    .def("setOptionValue", static_cast<HighsStatus (Highs::*)(const std::string&, const bool)>(&Highs::setOptionValue))
    .def("setOptionValue", static_cast<HighsStatus (Highs::*)(const std::string&, const int)>(&Highs::setOptionValue))
    .def("setOptionValue", static_cast<HighsStatus (Highs::*)(const std::string&, const double)>(&Highs::setOptionValue))
    .def("setOptionValue", static_cast<HighsStatus (Highs::*)(const std::string&, const std::string)>(&Highs::setOptionValue))
    .def("modelStatusToString", &Highs::modelStatusToString)
    .def("solutionStatusToString", &Highs::solutionStatusToString)
    .def("basisStatusToString", &Highs::basisStatusToString)
    .def("basisValidityToString", &Highs::basisValidityToString);
  
  m.attr("kHighsInf") = kHighsInf;
  m.attr("HIGHS_VERSION_MAJOR") = HIGHS_VERSION_MAJOR;
  m.attr("HIGHS_VERSION_MINOR") = HIGHS_VERSION_MINOR;
  m.attr("HIGHS_VERSION_PATCH") = HIGHS_VERSION_PATCH;
}
