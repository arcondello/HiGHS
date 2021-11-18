/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                       */
/*    This file is part of the HiGHS linear optimization suite           */
/*                                                                       */
/*    Written and engineered 2008-2021 at the University of Edinburgh    */
/*                                                                       */
/*    Available as open-source under the MIT License                     */
/*                                                                       */
/*    Authors: Julian Hall, Ivet Galabova, Qi Huangfu, Leona Gottwald    */
/*    and Michael Feldmeier                                              */
/*                                                                       */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/**@file util/HFactor.cpp
 * @brief Types of solution classes
 */
#include "util/HFactor.h"

#include <cassert>
#include <iostream>
#include <sstream>

#include "lp_data/HConst.h"
#include "pdqsort/pdqsort.h"
#include "util/FactorTimer.h"
#include "util/HFactorDebug.h"
#include "util/HVector.h"
#include "util/HVectorBase.h"
#include "util/HighsTimer.h"

// std::vector, std::max and std::min used in HFactor.h for local
// in-line functions, so HFactor.h has #include <algorithm>
using std::fabs;

using std::copy;
using std::fill_n;
using std::make_pair;
using std::min;
using std::pair;

void solveMatrixT(const HighsInt X_Start, const HighsInt x_end,
                  const HighsInt y_start, const HighsInt y_end,
                  const HighsInt* t_index, const double* t_value,
                  const double t_pivot, HighsInt* rhs_count,
                  HighsInt* rhs_index, double* rhs_array) {
  // Collect by X
  double pivot_multiplier = 0;
  for (HighsInt k = X_Start; k < x_end; k++)
    pivot_multiplier += t_value[k] * rhs_array[t_index[k]];

  // Scatter by Y
  if (fabs(pivot_multiplier) > kHighsTiny) {
    HighsInt work_count = *rhs_count;

    pivot_multiplier /= t_pivot;
    for (HighsInt k = y_start; k < y_end; k++) {
      const HighsInt index = t_index[k];
      const double value0 = rhs_array[index];
      const double value1 = value0 - pivot_multiplier * t_value[k];
      if (value0 == 0) rhs_index[work_count++] = index;
      rhs_array[index] = (fabs(value1) < kHighsTiny) ? kHighsZero : value1;
    }

    *rhs_count = work_count;
  }
}

void solveHyper(const HighsInt h_size, const HighsInt* h_lookup,
                const HighsInt* h_pivot_index, const double* h_pivot_value,
                const HighsInt* h_start, const HighsInt* h_end,
                const HighsInt* h_index, const double* h_value, HVector* rhs) {
  HighsInt rhs_count = rhs->count;
  HighsInt* rhs_index = &rhs->index[0];
  double* rhs_array = &rhs->array[0];

  // Take count

  // Build list
  char* list_mark = &rhs->cwork[0];
  HighsInt* list_index = &rhs->iwork[0];
  HighsInt* list_stack = &rhs->iwork[h_size];
  HighsInt list_count = 0;

  HighsInt count_pivot = 0;
  HighsInt count_entry = 0;

  for (HighsInt i = 0; i < rhs_count; i++) {
    // Skip touched index
    HighsInt i_trans =
        h_lookup[rhs_index[i]];  // XXX: this contains a bug iTran
    if (list_mark[i_trans])      // XXX bug here
      continue;

    HighsInt Hi = i_trans;      // H matrix pivot index
    HighsInt Hk = h_start[Hi];  // H matrix non zero position
    HighsInt n_stack = -1;      // Usage of the stack (-1 not used)

    list_mark[Hi] = 1;  // Mark this as touched

    for (;;) {
      if (Hk < h_end[Hi]) {
        HighsInt Hi_sub = h_lookup[h_index[Hk++]];
        if (list_mark[Hi_sub] == 0) {  // Go to a child
          list_mark[Hi_sub] = 1;       // Mark as touched
          list_stack[++n_stack] = Hi;  // Store current into stack
          list_stack[++n_stack] = Hk;
          Hi = Hi_sub;  // Replace current with child
          Hk = h_start[Hi];
          if (Hi >= h_size) {
            count_pivot++;
            count_entry += h_end[Hi] - h_start[Hi];
          }
        }
      } else {
        list_index[list_count++] = Hi;
        if (n_stack == -1)  // Quit on empty stack
          break;
        Hk = list_stack[n_stack--];  // Back to last in stack
        Hi = list_stack[n_stack--];
      }
    }
  }

  rhs->synthetic_tick += count_pivot * 20 + count_entry * 10;

  // Solve with list
  if (h_pivot_value == 0) {
    rhs_count = 0;
    for (HighsInt iList = list_count - 1; iList >= 0; iList--) {
      HighsInt i = list_index[iList];
      list_mark[i] = 0;
      HighsInt pivotRow = h_pivot_index[i];
      double pivot_multiplier = rhs_array[pivotRow];
      if (fabs(pivot_multiplier) > kHighsTiny) {
        rhs_index[rhs_count++] = pivotRow;
        const HighsInt start = h_start[i];
        const HighsInt end = h_end[i];
        for (HighsInt k = start; k < end; k++)
          rhs_array[h_index[k]] -= pivot_multiplier * h_value[k];
      } else
        rhs_array[pivotRow] = 0;
    }
    rhs->count = rhs_count;
  } else {
    rhs_count = 0;
    for (HighsInt iList = list_count - 1; iList >= 0; iList--) {
      HighsInt i = list_index[iList];
      list_mark[i] = 0;
      HighsInt pivotRow = h_pivot_index[i];
      double pivot_multiplier = rhs_array[pivotRow];
      if (fabs(pivot_multiplier) > kHighsTiny) {
        pivot_multiplier /= h_pivot_value[i];
        rhs_array[pivotRow] = pivot_multiplier;
        rhs_index[rhs_count++] = pivotRow;
        const HighsInt start = h_start[i];
        const HighsInt end = h_end[i];
        for (HighsInt k = start; k < end; k++)
          rhs_array[h_index[k]] -= pivot_multiplier * h_value[k];
      } else
        rhs_array[pivotRow] = 0;
    }
    rhs->count = rhs_count;
  }
}

void HFactor::setup(const HighsSparseMatrix& a_matrix,
                    std::vector<HighsInt>& basic_index,
                    const double pivot_threshold, const double pivot_tolerance,
                    const HighsInt highs_debug_level,
                    const HighsLogOptions* log_options) {
  HighsInt basic_index_size = basic_index.size();
  // Nothing to do if basic index has no entries, and mustn't try to
  // pass the pointer to entry 0 of a vector of size 0.
  if (basic_index_size <= 0) return;
  this->setupGeneral(&a_matrix, basic_index_size, &basic_index[0],
                     pivot_threshold, pivot_tolerance, highs_debug_level,
                     log_options);
  return;
}

void HFactor::setupGeneral(const HighsSparseMatrix* a_matrix,
                           HighsInt num_basic, HighsInt* basic_index,
                           const double pivot_threshold,
                           const double pivot_tolerance,
                           const HighsInt highs_debug_level,
                           const HighsLogOptions* log_options) {
  this->setupGeneral(
      a_matrix->num_col_, a_matrix->num_row_, num_basic, &a_matrix->start_[0],
      &a_matrix->index_[0], &a_matrix->value_[0], basic_index, pivot_threshold,
      pivot_tolerance, highs_debug_level, log_options, true, kUpdateMethodFt);
}

void HFactor::setup(
    const HighsInt num_col_, const HighsInt num_row_, const HighsInt* a_start_,
    const HighsInt* a_index_, const double* a_value_, HighsInt* basic_index_,
    const double pivot_threshold_, const double pivot_tolerance_,
    const HighsInt highs_debug_level_, const HighsLogOptions* log_options_,
    const bool use_original_HFactor_logic_, const HighsInt update_method_) {
  setupGeneral(num_col_, num_row_, num_row_, a_start_, a_index_, a_value_,
               basic_index_, pivot_threshold_, pivot_tolerance_,
               highs_debug_level_, log_options_, use_original_HFactor_logic_,
               update_method_);
}

void HFactor::setupGeneral(
    const HighsInt num_col_, const HighsInt num_row_, HighsInt num_basic_,
    const HighsInt* a_start_, const HighsInt* a_index_, const double* a_value_,
    HighsInt* basic_index_, const double pivot_threshold_,
    const double pivot_tolerance_, const HighsInt highs_debug_level_,
    const HighsLogOptions* log_options_, const bool use_original_HFactor_logic_,
    const HighsInt update_method_) {
  // Copy Problem size and (pointer to) coefficient matrix
  num_row = num_row_;
  num_col = num_col_;
  num_basic = num_basic_;
  this->a_matrix_valid = true;
  a_start = a_start_;
  a_index = a_index_;
  a_value = a_value_;
  basic_index = basic_index_;
  pivot_threshold =
      max(kMinPivotThreshold, min(pivot_threshold_, kMaxPivotThreshold));
  pivot_tolerance =
      max(kMinPivotTolerance, min(pivot_tolerance_, kMaxPivotTolerance));
  highs_debug_level = highs_debug_level_;
  log_data = decltype(log_data)(new LogData());
  log_options.output_flag = &log_data->output_flag;
  log_options.log_to_console = &log_data->log_to_console;
  log_options.log_dev_level = &log_data->log_dev_level;

  if (!log_options_) {
    log_data->output_flag = false;
    log_data->log_to_console = true;
    log_data->log_dev_level = 0;
    log_options.log_file_stream = nullptr;
  } else {
    log_data->output_flag = *(log_options_->output_flag);
    log_data->log_to_console = *(log_options_->log_to_console);
    log_data->log_dev_level = *(log_options_->log_dev_level);
    log_options.log_file_stream = log_options_->log_file_stream;
  }

  use_original_HFactor_logic = use_original_HFactor_logic_;
  update_method = update_method_;

  // Allocate for working buffer
  iwork.reserve(num_row * 2);
  dwork.assign(num_row, 0);

  // Find Basis matrix limit size
  basis_matrix_limit_size = 0;

  iwork.assign(num_row + 1, 0);
  for (HighsInt i = 0; i < num_col; i++) iwork[a_start[i + 1] - a_start[i]]++;
  for (HighsInt i = num_row, counted = 0; i >= 0 && counted < num_row; i--)
    basis_matrix_limit_size += i * iwork[i], counted += iwork[i];
  basis_matrix_limit_size += num_row;

  // Allocate space for basis matrix, L, U factor and Update buffer
  const HighsInt b_max_dim = max(num_row, num_basic);
  b_var.resize(b_max_dim);
  b_start.resize(b_max_dim + 1, 0);
  b_index.resize(basis_matrix_limit_size);
  b_value.resize(basis_matrix_limit_size);

  // Allocate space for pivot records
  const HighsInt permute_max_dim = max(num_row, num_basic);
  permute.resize(permute_max_dim);

  // Allocate space for Markowitz matrices
  const HighsInt mc_dim = num_basic;
  mc_var.resize(mc_dim);
  mc_start.resize(mc_dim);
  mc_count_a.resize(mc_dim);
  mc_count_n.resize(mc_dim);
  mc_space.resize(mc_dim);
  mc_min_pivot.resize(mc_dim);
  mc_index.resize(basis_matrix_limit_size * kMCExtraEntriesMultiplier);
  mc_value.resize(basis_matrix_limit_size * kMCExtraEntriesMultiplier);

  mr_start.resize(num_row);
  mr_count.resize(num_row);
  mr_space.resize(num_row);
  mr_count_before.resize(num_row);
  mr_index.resize(basis_matrix_limit_size * kMRExtraEntriesMultiplier);

  mwz_column_mark.assign(num_row, 0);
  mwz_column_index.resize(num_row);
  mwz_column_array.assign(num_row, 0);

  // Allocate space for count-link-list
  const HighsInt col_link_max_count = num_row;
  const HighsInt col_link_dim = num_basic;
  col_link_first.assign(col_link_max_count + 1, -1);
  col_link_next.resize(col_link_dim);
  col_link_last.resize(col_link_dim);

  const HighsInt row_link_max_count = num_basic;
  const HighsInt row_link_dim = num_row;
  // Strictly not necessary, but nice if it's the right size
  row_link_first.resize(row_link_max_count + 1);
  row_link_first.assign(row_link_max_count + 1, -1);
  row_link_next.resize(row_link_dim);
  row_link_last.resize(row_link_dim);

  // Allocate space for L factor
  l_pivot_lookup.resize(num_row);
  l_pivot_index.reserve(num_row);
  l_start.reserve(num_row + 1);
  l_index.reserve(basis_matrix_limit_size * kLFactorExtraEntriesMultiplier);
  l_value.reserve(basis_matrix_limit_size * kLFactorExtraEntriesMultiplier);

  lr_start.reserve(num_row + 1);
  lr_index.reserve(basis_matrix_limit_size * kLFactorExtraEntriesMultiplier);
  lr_value.reserve(basis_matrix_limit_size * kLFactorExtraEntriesMultiplier);

  // Allocate space for U factor
  u_pivot_lookup.resize(num_row);
  u_pivot_index.reserve(num_row + kUFactorExtraVectors);
  u_pivot_value.reserve(num_row + kUFactorExtraVectors);

  u_start.reserve(num_row + kUFactorExtraVectors + 1);
  u_last_p.reserve(num_row + kUFactorExtraVectors);
  u_index.reserve(basis_matrix_limit_size * kUFactorExtraEntriesMultiplier);
  u_value.reserve(basis_matrix_limit_size * kUFactorExtraEntriesMultiplier);

  ur_start.reserve(num_row + kUFactorExtraVectors + 1);
  ur_lastp.reserve(num_row + kUFactorExtraVectors);
  ur_space.reserve(num_row + kUFactorExtraVectors);
  ur_index.reserve(basis_matrix_limit_size * kUFactorExtraEntriesMultiplier);
  ur_value.reserve(basis_matrix_limit_size * kUFactorExtraEntriesMultiplier);

  // Allocate spaces for Update buffer
  pf_pivot_value.reserve(kPFFPivotEntries);
  pf_pivot_index.reserve(kPFFPivotEntries);
  pf_start.reserve(kPFVectors + 1);
  pf_index.reserve(basis_matrix_limit_size * kPFEntriesMultiplier);
  pf_value.reserve(basis_matrix_limit_size * kPFEntriesMultiplier);
}

void HFactor::setupMatrix(const HighsInt* a_start_, const HighsInt* a_index_,
                          const double* a_value_) {
  a_start = a_start_;
  a_index = a_index_;
  a_value = a_value_;
  this->a_matrix_valid = true;
}

void HFactor::setupMatrix(const HighsSparseMatrix* a_matrix) {
  setupMatrix(&a_matrix->start_[0], &a_matrix->index_[0], &a_matrix->value_[0]);
}

HighsInt HFactor::build(HighsTimerClock* factor_timer_clock_pointer) {
  const bool report_lu = false;
  // Ensure that the A matrix is valid for factorization
  assert(this->a_matrix_valid);
  FactorTimer factor_timer;
  // Possibly use the refactorization information!
  if (refactor_info_.use) {
    factor_timer.start(FactorReinvert, factor_timer_clock_pointer);
    rank_deficiency = rebuild(factor_timer_clock_pointer);
    factor_timer.stop(FactorReinvert, factor_timer_clock_pointer);
    if (!rank_deficiency) return 0;
  }
  // Refactoring from just the list of basic variables. Initialise the
  // refactorization information.
  refactor_info_.clear();
  // Start the timer
  factor_timer.start(FactorInvert, factor_timer_clock_pointer);
  build_synthetic_tick = 0;
  factor_timer.start(FactorInvertSimple, factor_timer_clock_pointer);
  // Build the L, U factor
  buildSimple();
  if (report_lu) {
    printf("\nAfter units and singletons\n");
    reportLu(kReportLuBoth, false);
  }
  factor_timer.stop(FactorInvertSimple, factor_timer_clock_pointer);
  factor_timer.start(FactorInvertKernel, factor_timer_clock_pointer);
  rank_deficiency = buildKernel();
  factor_timer.stop(FactorInvertKernel, factor_timer_clock_pointer);
  // rank_deficiency is the deficiency of the basic variables. If
  // num_basic < num_row, then have to identify the logicals required
  // to complete the basis by continuing as if a full-dimension set of
  // basic variables was rank deficient.
  const bool incomplete_basis = num_basic < num_row;
  if (rank_deficiency || incomplete_basis) {
    factor_timer.start(FactorInvertDeficient, factor_timer_clock_pointer);
    if (num_basic == num_row)
      highsLogDev(log_options, HighsLogType::kWarning,
                  "Rank deficiency of %" HIGHSINT_FORMAT
                  " identified in basis matrix\n",
                  rank_deficiency);
    // Singular matrix B: reorder the basic variables so that the
    // singular columns are in the position corresponding to the
    // logical which replaces them
    buildHandleRankDeficiency();
    buildMarkSingC();
    factor_timer.stop(FactorInvertDeficient, factor_timer_clock_pointer);
  }
  if (incomplete_basis) {
    // Completing the factorization is not relevant if the basis
    // matrix is incomplete, so clear any refactorization information
    // and return
    this->refactor_info_.clear();
    assert(!this->refactor_info_.use);
    const HighsInt basic_index_rank_deficiency =
        rank_deficiency - (num_row - num_basic);
    return basic_index_rank_deficiency;
  }
  // Complete INVERT
  if (report_lu) {
    printf("\nFactored INVERT\n");
    reportLu(kReportLuBoth, false);
  }
  factor_timer.start(FactorInvertFinish, factor_timer_clock_pointer);
  buildFinish();
  factor_timer.stop(FactorInvertFinish, factor_timer_clock_pointer);
  //
  // Indicate that the refactorization information is known unless the basis was
  // rank deficient
  if (rank_deficiency) {
    this->refactor_info_.clear();
  } else {
    // Check that the refactorization information is not (yet) flagged
    // to be used in a future call
    assert(!this->refactor_info_.use);
    // Record build_synthetic_tick to use as the value of
    // build_synthetic_tick if refactorization is performed. Not only
    // is there no build_synthetic_tick measure for refactorization,
    // if there were it would give an unrealistic underestimate of the
    // cost of factorization from scratch
    this->refactor_info_.build_synthetic_tick = this->build_synthetic_tick;
  }

  // Record the number of entries in the INVERT
  invert_num_el = l_start[num_row] + u_last_p[num_row - 1] + num_row;

  kernel_dim -= rank_deficiency;
  debugLogRankDeficiency(highs_debug_level, log_options, rank_deficiency,
                         basis_matrix_num_el, invert_num_el, kernel_dim,
                         kernel_num_el, nwork);
  factor_timer.stop(FactorInvert, factor_timer_clock_pointer);
  return rank_deficiency;
}

void HFactor::ftranCall(HVector& vector, const double expected_density,
                        HighsTimerClock* factor_timer_clock_pointer) const {
  FactorTimer factor_timer;
  factor_timer.start(FactorFtran, factor_timer_clock_pointer);
  ftranL(vector, expected_density, factor_timer_clock_pointer);
  ftranU(vector, expected_density, factor_timer_clock_pointer);
  factor_timer.stop(FactorFtran, factor_timer_clock_pointer);
}

void HFactor::btranCall(HVector& vector, const double expected_density,
                        HighsTimerClock* factor_timer_clock_pointer) const {
  FactorTimer factor_timer;
  factor_timer.start(FactorBtran, factor_timer_clock_pointer);
  btranU(vector, expected_density, factor_timer_clock_pointer);
  btranL(vector, expected_density, factor_timer_clock_pointer);
  factor_timer.stop(FactorBtran, factor_timer_clock_pointer);
}

void HFactor::update(HVector* aq, HVector* ep, HighsInt* iRow, HighsInt* hint) {
  // Updating implies a change of basis. Since the refactorizaion info
  // no longer corresponds to the current basis, it must be
  // invalidated
  this->refactor_info_.clear();
  // Special case
  if (aq->next) {
    updateCFT(aq, ep, iRow);
    return;
  }

  if (update_method == kUpdateMethodFt) updateFT(aq, ep, *iRow);
  if (update_method == kUpdateMethodPf) updatePF(aq, *iRow, hint);
  if (update_method == kUpdateMethodMpf) updateMPF(aq, ep, *iRow, hint);
  if (update_method == kUpdateMethodApf) updateAPF(aq, ep, *iRow);
}

bool HFactor::setPivotThreshold(const double new_pivot_threshold) {
  if (new_pivot_threshold < kMinPivotThreshold) return false;
  if (new_pivot_threshold > kMaxPivotThreshold) return false;
  pivot_threshold = new_pivot_threshold;
  return true;
}

void HFactor::luClear() {
  l_start.clear();
  l_start.push_back(0);
  l_index.clear();
  l_value.clear();

  u_pivot_index.clear();
  u_pivot_value.clear();
  u_start.clear();
  u_start.push_back(0);
  u_index.clear();
  u_value.clear();
}

void HFactor::buildSimple() {
  /**
   * 0. Clear L and U factor
   */
  luClear();

  const bool progress_report = num_basic != num_row;
  const HighsInt progress_frequency = 100000;

  // Set all values of permute to -1 so that unpermuted (rank
  // deficient) columns can be identified
  const HighsInt permute_dim = num_basic;
  // Strictly not necessary, but nice if it's the right size
  permute.resize(permute_dim);
  permute.assign(permute_dim, -1);

  /**
   * 1. Prepare basis matrix and deal with unit columns
   */

  const bool report_unit = false;
  const bool report_singletons = false;
  const bool report_markowitz = false;
  const bool report_anything =
      report_unit || report_singletons || report_markowitz;
  HighsInt BcountX = 0;
  fill_n(&mr_count_before[0], num_row, 0);
  nwork = 0;
  if (report_anything) printf("\nFactor\n");
  // Compile a vector iwork of the indices within basic_index of the
  // its nwork non-unit structural columns: they will be formed into
  // the B matrix as the kernel
  const HighsInt iwork_dim = num_basic;
  // Strictly not necessary, but nice if it's the right size
  iwork.resize(iwork_dim + 1, 0);
  iwork.assign(iwork_dim + 1, 0);
  for (HighsInt iCol = 0; iCol < num_basic; iCol++) {
    if (progress_report && iCol) {
      if (iCol % progress_frequency == 0)
        printf("HFactor::buildSimple stage = %6d\n", (int)iCol);
    }

    HighsInt iMat = basic_index[iCol];
    HighsInt iRow = -1;
    int8_t pivot_type = kPivotIllegal;
    if (iMat >= num_col) {
      if (report_unit) printf("Stage %d: Logical\n", (int)(l_start.size() - 1));
      // 1.1 Logical column
      pivot_type = kPivotLogical;
      // Check for double pivot
      HighsInt lc_iRow = iMat - num_col;
      if (mr_count_before[lc_iRow] >= 0) {
        iRow = lc_iRow;
      } else {
        highsLogDev(log_options, HighsLogType::kError,
                    "INVERT Error: Found a logical column with pivot "
                    "already in row %" HIGHSINT_FORMAT "\n",
                    lc_iRow);
        // Treat this as a column to be handled in the kernel, so that
        // the rank deficiency is detected as late as possible.
        mr_count_before[lc_iRow]++;
        b_index[BcountX] = lc_iRow;
        b_value[BcountX++] = 1.0;
        iwork[nwork++] = iCol;
      }
    } else {
      // 1.2 Structural column
      HighsInt start = a_start[iMat];
      HighsInt count = a_start[iMat + 1] - start;
      HighsInt lc_iRow = a_index[start];
      // Check for unit column with double pivot
      bool unit_col = count == 1 && a_value[start] == 1;
      if (unit_col && mr_count_before[lc_iRow] >= 0) {
        if (report_unit) printf("Stage %d: Unit\n", (int)(l_start.size() - 1));
        // Don't exploit this special case in case the matrix is
        // re-factorized after scaling has been applied, making this
        // column non-unit.
        pivot_type = kPivotColSingleton;  //;kPivotUnit;//
        iRow = lc_iRow;
      } else {
        if (unit_col)
          highsLogDev(log_options, HighsLogType::kError,
                      "INVERT Error: Found a second unit column with pivot in "
                      "row %" HIGHSINT_FORMAT "\n",
                      lc_iRow);
        for (HighsInt k = start; k < start + count; k++) {
          mr_count_before[a_index[k]]++;
          b_index[BcountX] = a_index[k];
          b_value[BcountX++] = a_value[k];
        }
        iwork[nwork++] = iCol;
      }
    }

    if (iRow >= 0) {
      // 1.3 Record unit column
      permute[iCol] = iRow;
      l_start.push_back(l_index.size());
      u_pivot_index.push_back(iRow);
      u_pivot_value.push_back(1);
      u_start.push_back(u_index.size());
      mr_count_before[iRow] = -num_row;
      assert(pivot_type != kPivotIllegal);
      this->refactor_info_.pivot_row.push_back(iRow);
      this->refactor_info_.pivot_var.push_back(iMat);
      this->refactor_info_.pivot_type.push_back(pivot_type);
    }
    b_start[iCol + 1] = BcountX;
    b_var[iCol] = iMat;
  }
  // Record the number of elements in the basis matrix
  basis_matrix_num_el = num_row - nwork + BcountX;

  // count1 = 0;
  // Comments: for pds-20, dfl001: 60 / 80
  // Comments: when system is large: enlarge
  // Comments: when system is small: decrease
  build_synthetic_tick += BcountX * 60 + (num_row - nwork) * 80;

  /**
   * 2. Search for and deal with singletons
   */
  double t2_search = 0;
  double t2_store_l = l_index.size();
  double t2_store_u = u_index.size();
  double t2_store_p = nwork;
  while (nwork > 0) {
    HighsInt nworkLast = nwork;
    nwork = 0;
    for (HighsInt i = 0; i < nworkLast; i++) {
      const HighsInt iCol = iwork[i];
      const HighsInt start = b_start[iCol];
      const HighsInt end = b_start[iCol + 1];
      HighsInt pivot_k = -1;
      HighsInt found_row_singleton = 0;
      HighsInt count = 0;

      // 2.1 Search for singleton
      t2_search += end - start;
      for (HighsInt k = start; k < end; k++) {
        const HighsInt iRow = b_index[k];
        if (mr_count_before[iRow] == 1) {
          pivot_k = k;
          found_row_singleton = 1;
          break;
        }
        if (mr_count_before[iRow] > 1) {
          pivot_k = k;
          count++;
        }
      }

      if (found_row_singleton) {
        // 2.2 Deal with row singleton
        const double pivot_multiplier = 1 / b_value[pivot_k];
        if (report_singletons)
          printf("Stage %d: Row singleton (%4d, %g)\n",
                 (int)(l_start.size() - 1), (int)pivot_k, pivot_multiplier);
        for (HighsInt section = 0; section < 2; section++) {
          HighsInt p0 = section == 0 ? start : pivot_k + 1;
          HighsInt p1 = section == 0 ? pivot_k : end;
          for (HighsInt k = p0; k < p1; k++) {
            HighsInt iRow = b_index[k];
            if (mr_count_before[iRow] > 0) {
              if (report_singletons)
                printf("Row singleton: L En (%4d, %11.4g)\n", (int)iRow,
                       b_value[k] * pivot_multiplier);
              l_index.push_back(iRow);
              l_value.push_back(b_value[k] * pivot_multiplier);
            } else {
              if (report_singletons)
                printf("Row singleton: U En (%4d, %11.4g)\n", (int)iRow,
                       b_value[k]);
              u_index.push_back(iRow);
              u_value.push_back(b_value[k]);
            }
            mr_count_before[iRow]--;
          }
        }
        HighsInt iRow = b_index[pivot_k];
        mr_count_before[iRow] = 0;
        permute[iCol] = iRow;
        l_start.push_back(l_index.size());

        if (report_singletons)
          printf("Row singleton: U Pv (%4d, %11.4g)\n", (int)iRow,
                 b_value[pivot_k]);
        u_pivot_index.push_back(iRow);
        u_pivot_value.push_back(b_value[pivot_k]);
        u_start.push_back(u_index.size());
        assert(b_var[iCol] == basic_index[iCol]);

        this->refactor_info_.pivot_row.push_back(iRow);
        this->refactor_info_.pivot_var.push_back(basic_index[iCol]);
        this->refactor_info_.pivot_type.push_back(kPivotRowSingleton);
      } else if (count == 1) {
        if (report_singletons)
          printf("Stage %d: Col singleton \n", (int)(l_start.size() - 1));
        // 2.3 Deal with column singleton
        for (HighsInt k = start; k < pivot_k; k++) {
          if (report_singletons)
            printf("Col singleton: U En (%4d, %11.4g)\n", (int)b_index[k],
                   b_value[k]);
          u_index.push_back(b_index[k]);
          u_value.push_back(b_value[k]);
        }
        for (HighsInt k = pivot_k + 1; k < end; k++) {
          if (report_singletons)
            printf("Col singleton: U En (%4d, %11.4g)\n", (int)b_index[k],
                   b_value[k]);
          u_index.push_back(b_index[k]);
          u_value.push_back(b_value[k]);
        }

        HighsInt iRow = b_index[pivot_k];
        mr_count_before[iRow] = 0;
        permute[iCol] = iRow;
        l_start.push_back(l_index.size());

        if (report_singletons)
          printf("Col singleton: U Pv (%4d, %11.4g)\n", (int)iRow,
                 b_value[pivot_k]);
        u_pivot_index.push_back(iRow);
        u_pivot_value.push_back(b_value[pivot_k]);
        u_start.push_back(u_index.size());
        assert(b_var[iCol] == basic_index[iCol]);
        this->refactor_info_.pivot_row.push_back(iRow);
        this->refactor_info_.pivot_var.push_back(basic_index[iCol]);
        this->refactor_info_.pivot_type.push_back(kPivotColSingleton);
      } else {
        iwork[nwork++] = iCol;
      }
    }

    // No singleton found in the last pass
    if (nworkLast == nwork) break;
  }
  if (report_anything) reportLu(kReportLuBoth, false);
  t2_store_l = l_index.size() - t2_store_l;
  t2_store_u = u_index.size() - t2_store_u;
  t2_store_p = t2_store_p - nwork;

  build_synthetic_tick +=
      t2_search * 20 + (t2_store_p + t2_store_l + t2_store_u) * 80;

  /**
   * 3. Prepare the kernel parts
   */
  // 3.1 Prepare row links, row matrix spaces
  row_link_first.assign(num_basic + 1, -1);
  mr_count.assign(num_row, 0);
  HighsInt mr_countX = 0;
  // Determine the number of entries in the kernel
  kernel_num_el = 0;
  for (HighsInt iRow = 0; iRow < num_row; iRow++) {
    HighsInt count = mr_count_before[iRow];
    if (count > 0) {
      mr_start[iRow] = mr_countX;
      mr_space[iRow] = count * 2;
      mr_countX += count * 2;
      rlinkAdd(iRow, count);
      kernel_num_el += count + 1;
    }
  }
  mr_index.resize(mr_countX);

  // 3.2 Prepare column links, kernel matrix
  //
  // MC matrix is the column-wise kernel
  // 
  // mc_start exists for all columns in the original matrix, but its
  // entries are zero for all columns not in the kernel
  //
  // mc_count_a is the number of entries in a column that are active -
  // all the kernel initially
  //
  // mc_count_n is the number of entries in a column that are
  // non-active - all zero initially
  //
  // mc_space is the interval between the start of one column and the
  // next - twice mc_count_a initially
  //
  col_link_first.assign(num_row + 1, -1);
  const HighsInt mc_dim = num_basic;
  mc_index.clear();
  mc_value.clear();
  mc_count_a.assign(mc_dim, 0);
  mc_count_n.assign(mc_dim, 0);
  HighsInt MCcountX = 0;
  for (HighsInt i = 0; i < nwork; i++) {
    HighsInt iCol = iwork[i];
    mc_var[iCol] = b_var[iCol];
    mc_start[iCol] = MCcountX;
    mc_space[iCol] = (b_start[iCol + 1] - b_start[iCol]) * 2;
    MCcountX += mc_space[iCol];
    mc_index.resize(MCcountX);
    mc_value.resize(MCcountX);
    for (HighsInt k = b_start[iCol]; k < b_start[iCol + 1]; k++) {
      const HighsInt iRow = b_index[k];
      const double value = b_value[k];
      if (mr_count_before[iRow] > 0) {
        colInsert(iCol, iRow, value);
        rowInsert(iCol, iRow);
      } else {
        colStoreN(iCol, iRow, value);
      }
    }
    colFixMax(iCol);
    clinkAdd(iCol, mc_count_a[iCol]);
  }
  build_synthetic_tick += (num_row + nwork + MCcountX) * 40 + mr_countX * 20;
  // Record the kernel dimension
  kernel_dim = nwork;
  assert((HighsInt)this->refactor_info_.pivot_row.size() == num_basic - nwork);
}

HighsInt HFactor::buildKernel() {
  // Deal with the kernel part by 'n-work' pivoting

  fake_search = 0;
  double fake_fill = 0;
  double fake_eliminate = 0;

  const bool progress_report = num_basic != num_row;
  const HighsInt progress_frequency = 10000;
  HighsInt report_from_GE_stage = 109313;//2075;//5840;//kHighsIInf;
  HighsInt report_to_GE_stage = report_from_GE_stage+5;
  const bool report_GE_search = true;
  const bool report_GE_elimination = false;
  bool report_GE_stage = false;
  bool report_search = false;
  bool report_elimination = false;
  std::string GE_stage_name = "";
  std::stringstream ss;
  const double query_pivot_value = 1e-8;//-1;
  const HighsInt track_iRow = -69015;
  const HighsInt track_iCol = -79920;
  const HighsInt track_iEl = -11557924;
  HighsInt mc_index_size = mc_index.size();
  //  printf("HFactor::buildKernel dim = %d; mc_index.size = %d\n", (int)kernel_dim, (int)mc_index_size);
  HighsInt track_iEl_index = kHighsIInf;
  double track_iEl_value = kHighsInf;
  double track_value = kHighsInf;
  bool track_pivoted = false;
  reportKernelValueChange(GE_stage_name, track_iRow, track_iCol, track_value);
  if (progress_report) {
    log_data->output_flag = true;
    log_data->log_dev_level = 2;
    log_options.log_file_stream = stdout;
  }
  HighsIntValueDistribution pivot_merit;
  HighsValueDistribution pivot_value;
  if (progress_report) {
    initialiseValueDistribution("Kernel pivot merit", "", 1, 1024*1024, 2, pivot_merit);
    initialiseValueDistribution("Kernel pivot value", "", 1e-8, 1e16, 10.0, pivot_value);
  }
  const HighsInt num_simple_pivot = num_basic - nwork;
  HighsInt num_pivot = num_simple_pivot;
  HighsInt num_kernel_pivot = 0;
  min_row_count = kHighsIInf;
  min_col_count = kHighsIInf;
  // Row count can be more than the number of rows if num_basic >
  // num_row
  const HighsInt max_count = max(num_row, num_basic);
  const HighsInt kMarkowitzSearchStrategyOg = 0;
  const HighsInt kMarkowitzSearchStrategySwitchedOg = 1;
  const HighsInt kMarkowitzSearchStrategyAlternateBest = 2;
  const HighsInt markowitz_search_strategy = kMarkowitzSearchStrategyOg;
  
  while (nwork-- > 0) {
    /**
     * 1. Search for the pivot
     */
    //    if (num_pivot>100000) reportMcColumn(num_pivot, 151955);
    if (track_iEl >=0 && track_iEl < (int)mc_index.size()) {
      if (mc_index[track_iEl] != track_iEl_index ||
	  mc_value[track_iEl] != track_iEl_value) {
	printf("Pass %6d: For k = %7d, MC(%7d; %11.4g) changed to MC(%7d; %11.4g) \n",
	       (int)num_pivot, (int)track_iEl,
	       (int)track_iEl_index, track_iEl_value,
	       (int)mc_index[track_iEl], mc_value[track_iEl]);
	track_iEl_index = mc_index[track_iEl];
	track_iEl_value = mc_value[track_iEl];
      }
    }
    HighsInt jColPivot = -1;
    HighsInt iRowPivot = -1;
    //    int8_t pivot_type = kPivotIllegal;
    // 1.1. Setup search merits
    search_limit = min(nwork, kMaxKernelSearch);
    search_count = 0;

    merit_limit = 1.0 * num_basic * num_row;
    merit_pivot = merit_limit;
    merit_ideal = 0.0;

    if (progress_report) {
      if (num_pivot % progress_frequency == 0 || num_kernel_pivot == 0) {
        HighsInt local_min_col_count = kHighsIInf;
        HighsInt local_min_row_count = kHighsIInf;
        for (HighsInt count = 1; count < num_row; count++) {
          if (col_link_first[count] >= 0) {
            local_min_col_count = count;
            break;
          }
        }
        for (HighsInt count = 1; count < num_basic; count++) {
          if (row_link_first[count] >= 0) {
            local_min_row_count = count;
            break;
          }
        }
        printf(
            "\nHFactor::buildKernel stage = %6d: min_col_count = %3d; "
            "min_row_count = %3d\n",
            (int)num_pivot, (int)local_min_col_count, (int)local_min_row_count);
	ss.str(std::string());
	ss << "Kernel " << num_pivot << " report ";
	analyseActiveSubmatrix(ss.str());
      }
    }
    num_pivot++;
    num_kernel_pivot++;
    if (progress_report) {
      report_GE_stage =
	num_pivot >= report_from_GE_stage &&
	num_pivot <= report_to_GE_stage;
      report_search = report_GE_stage && report_GE_search;
      report_elimination = report_GE_stage && report_GE_elimination;
    }
    
    ss.str(std::string());
    ss << "GE stage " << num_pivot;
    GE_stage_name = ss.str();
    if (report_GE_stage) {
      analyseActiveSubmatrix(GE_stage_name);
      printf("\n");
    }
    if (!track_pivoted) reportKernelValueChange(GE_stage_name, track_iRow, track_iCol, track_value);

    // 1.2. Search for local singletons
    bool found_pivot = false;
    if (!found_pivot && col_link_first[1] != -1) {
      jColPivot = col_link_first[1];
      iRowPivot = mc_index[mc_start[jColPivot]];
      merit_pivot = 0;
      found_pivot = true;
    }
    if (!found_pivot && row_link_first[1] != -1) {
      iRowPivot = row_link_first[1];
      jColPivot = mr_index[mr_start[iRowPivot]];
      merit_pivot = 0;
      found_pivot = true;
    }
    const bool singleton_pivot = found_pivot;
    // 1.3. Major search loop: skipped if found_pivot is true
    //
    // Markowitz merit of the candidate pivot is merit_pivot,
    // initialised to merit_limit, since any pivot will have better
    // merit than that
    //
    if (!found_pivot) {
      //
      // Find the smallest row and column counts exceeding 1 in the
      // active sub-matrix
      min_row_count = kHighsIInf;
      for (HighsInt count = 2; count <= num_row; count++) {
	if (col_link_first[count] >= 0) {
	  min_row_count = count;
	  break;
	}
      }
      min_col_count = kHighsIInf;
      for (HighsInt count = 2; count <= num_basic; count++) {
	if (row_link_first[count] >= 0) {
	  min_col_count = count;
	  break;
	}
      }
      bool no_row_count = min_row_count == kHighsIInf;
      bool no_col_count = min_col_count == kHighsIInf;
      assert(no_row_count == no_col_count);
      if (!no_row_count) {
	// Record the best possible merit: search should stop if an
	// pivot of this merit is found
	merit_ideal = 1.0 * (min_row_count-1) * (min_col_count-1);
	// Now to search for a pivot
	if (markowitz_search_strategy == kMarkowitzSearchStrategyOg) {
	  for (HighsInt count = 2; count <= max_count; count++) {
	    other_count_ideal = count-1;
	    findPivotColSearch(found_pivot, count, jColPivot, iRowPivot, GE_stage_name, report_search);
	    other_count_ideal = count;
	    findPivotRowSearch(found_pivot, count, jColPivot, iRowPivot, GE_stage_name, report_search);
	    if (found_pivot) break;
	  }
	} else if (markowitz_search_strategy == kMarkowitzSearchStrategySwitchedOg) {
	  for (HighsInt count = 2; count <= max_count; count++) {
	    findPivotRowSearch(found_pivot, count, jColPivot, iRowPivot, GE_stage_name, report_search);
	    findPivotColSearch(found_pivot, count, jColPivot, iRowPivot, GE_stage_name, report_search);
	    if (found_pivot) break;
	  }
	}
      }
    }
    // 1.4. If we found nothing: tell singular
    if (!found_pivot) {
      rank_deficiency = nwork + 1;
      if (progress_report) {
	logValueDistribution(log_options, pivot_merit);
	logValueDistribution(log_options, pivot_value);
      }
      return rank_deficiency;
    }

    if (jColPivot == track_iCol) track_pivoted = true;

    /**
     * 2. Elimination other elements by the pivot
     */
    // 2.1. Delete the pivot
    double pivot_multiplier = colDelete(jColPivot, iRowPivot);
    if (progress_report) {
      updateValueDistribution((HighsInt)merit_pivot, pivot_merit);
      updateValueDistribution(pivot_multiplier, pivot_value);
    }

    if (report_GE_stage) {
      printf("\n%s: Merit %6d: pivot = %11.4g for (%6d, %6d)\n",
	     GE_stage_name.c_str(),
	     (int)merit_pivot,
	     pivot_multiplier, (int)iRowPivot, (int)jColPivot);
    }
    const bool pivot_too_small = fabs(pivot_multiplier) < pivot_tolerance;
    if (pivot_too_small) {
      printf("\n%s: Merit %d: pivot = %11.4g for (%6d, %6d) is too small\n",
	     GE_stage_name.c_str(), (int)merit_pivot,
	     pivot_multiplier, (int)iRowPivot, (int)jColPivot);
      highsLogDev(log_options, HighsLogType::kWarning,
                  "Small |pivot| = %g when nwork = %" HIGHSINT_FORMAT "\n",
                  fabs(pivot_multiplier), nwork);
      rank_deficiency = nwork + 1;
      assert((HighsInt)this->refactor_info_.pivot_row.size() +
                 rank_deficiency ==
             num_basic);
      if (progress_report) {
	logValueDistribution(log_options, pivot_merit);
	logValueDistribution(log_options, pivot_value);
      }
      return rank_deficiency;
    }
    if (fabs(pivot_multiplier) < query_pivot_value) {
      printf("\n%s: Merit %d: pivot = %11.4g for (%6d, %6d) is less than %g, with tolerance = %g\n",
	     GE_stage_name.c_str(), (int)merit_pivot, 
	     pivot_multiplier, (int)iRowPivot, (int)jColPivot,
	     query_pivot_value, pivot_tolerance);
    }
    rowDelete(jColPivot, iRowPivot);
    clinkDel(jColPivot);
    rlinkDel(iRowPivot);
    permute[jColPivot] = iRowPivot;
    //    printf("Mwz pivot %3d; mc_var[%3d] = %d; basic_index[%3d] = %d\n",
    //    (int)(num_row-nwork),
    //	   (int)jColPivot, (int)mc_var[jColPivot],
    //	   (int)jColPivot, (int)basic_index[jColPivot]);
    assert(mc_var[jColPivot] == basic_index[jColPivot]);

    this->refactor_info_.pivot_row.push_back(iRowPivot);
    this->refactor_info_.pivot_var.push_back(basic_index[jColPivot]);
    this->refactor_info_.pivot_type.push_back(kPivotMarkowitz);

    // 2.2. Store active pivot column to L
    HighsInt start_A = mc_start[jColPivot];
    HighsInt end_A = start_A + mc_count_a[jColPivot];
    HighsInt mwz_column_count = 0;
    for (HighsInt k = start_A; k < end_A; k++) {
      const HighsInt iRow = mc_index[k];
      const double value = mc_value[k] / pivot_multiplier;
      mwz_column_index[mwz_column_count++] = iRow;
      mwz_column_array[iRow] = value;
      mwz_column_mark[iRow] = 1;
      l_index.push_back(iRow);
      l_value.push_back(value);
      mr_count_before[iRow] = mr_count[iRow];
      rowDelete(jColPivot, (int)iRow);
    }
    l_start.push_back(l_index.size());
    fake_fill += 2 * mc_count_a[jColPivot];

    // 2.3. Store non active pivot column to U
    HighsInt end_N = start_A + mc_space[jColPivot];
    HighsInt start_N = end_N - mc_count_n[jColPivot];
    for (HighsInt i = start_N; i < end_N; i++) {
      u_index.push_back(mc_index[i]);
      u_value.push_back(mc_value[i]);
    }
    u_pivot_index.push_back(iRowPivot);
    u_pivot_value.push_back(pivot_multiplier);
    u_start.push_back(u_index.size());
    fake_fill += end_N - start_N;

    // 2.4. Loop over pivot row to eliminate other column
    const HighsInt row_start = mr_start[iRowPivot];
    const HighsInt row_end = row_start + mr_count[iRowPivot];
    for (HighsInt row_k = row_start; row_k < row_end; row_k++) {
      // 2.4.1. My pointer
      HighsInt iCol = mr_index[row_k];
      const HighsInt my_count = mc_count_a[iCol];
      const HighsInt my_start = mc_start[iCol];
      const HighsInt my_end = my_start + my_count - 1;
      double my_pivot = colDelete(iCol, iRowPivot);
      colStoreN(iCol, iRowPivot, my_pivot);
      // 2.4.2. Elimination on the overlapping part
      //
      // The elimination is driven by mwz_column_*, that is set up
      // above
      //
      // mwz_column_count is the number of nonzeros in the pivotal
      // column
      //
      // mwz_column_index is the list of indices of nonzeros in the
      // pivotal column
      //
      // mwz_column_array is the scattered nonzeros in the pivotal
      // column
      //
      // mwz_column_mark is a scattered set of markers corresponding
      // to nonzeros in the pivotal column
      //
      // When considering the action on a non-pivotal column,
      // initially all operations are assumed to create fill-in
      //
      // A loop over the nonzeros in the non-pivotal column identifies
      // entries in the pivotal column that don't cause fill-in, and
      // this is recorded by setting the mwz_column_mark entry to
      // 0
      //
      // After completing the loop, cancellations and fill-in are then
      // handled
      //
      // If the net number of nonzeros exceeds the space available for
      // the column, it's copied to the end of mc_column_index/value
      //
      HighsInt nFillin = mwz_column_count;
      HighsInt nCancel = 0;
      for (HighsInt my_k = my_start; my_k < my_end; my_k++) {
        HighsInt iRow = mc_index[my_k];
        double value = mc_value[my_k];
        if (mwz_column_mark[iRow]) {
	  // The pivotal column has a nonzero in this row, so remove
	  // the mark and reduce the count of fill-in
          mwz_column_mark[iRow] = 0;
          nFillin--;
	  if ((iRow == track_iRow && iCol == track_iCol) || report_elimination) {
	    double new_value = value - my_pivot * mwz_column_array[iRow];
	    printf("%s: B(%6d, %6d) = %19.12g has %11.4g * %19.12g = %19.12g subtracted from it to give %11.4g\n",
		   GE_stage_name.c_str(),
		   (int)iRow, (int)iCol, value, my_pivot,
		   mwz_column_array[iRow],
		   my_pivot * mwz_column_array[iRow],
		   value - my_pivot * mwz_column_array[iRow]);
	  }
          value -= my_pivot * mwz_column_array[iRow];
          if (fabs(value) < kHighsTiny) {
            value = 0;
            nCancel++;
          }
          mc_value[my_k] = value;
        }
      }
      fake_eliminate += mwz_column_count;
      fake_eliminate += nFillin * 2;
      // 2.4.3. Remove cancellation gaps
      if (nCancel > 0) {
        HighsInt new_end = my_start;
        for (HighsInt my_k = my_start; my_k < my_end; my_k++) {
          if (mc_value[my_k] != 0) {
            mc_index[new_end] = mc_index[my_k];
            mc_value[new_end++] = mc_value[my_k];
          } else {
            rowDelete(iCol, mc_index[my_k]);
          }
        }
        mc_count_a[iCol] = new_end - my_start;
      }
      // 2.4.4. Insert fill-in
      if (nFillin > 0) {
	// 2.4.4.0 Avoid fill-in that's too small
	for (HighsInt i = 0; i < mwz_column_count; i++) {
	  HighsInt iRow = mwz_column_index[i];
	  if (mwz_column_mark[iRow]) {
	    double value = -my_pivot * mwz_column_array[iRow];
	    if (fabs(value) < kHighsTiny) {
	      // Too small to record as fill-in
	      mwz_column_mark[iRow] = 0;
	      value = 0;
	      nFillin--;
	    }
	  }
	}

        // 2.4.4.1 Copy column to the end of mc_column_index/value if
        // new number of nonzeros exceeds space available. Space is
        // (at least) doubled
        if (mc_count_a[iCol] + mc_count_n[iCol] + nFillin > mc_space[iCol]) {
          // p1&2=active, p3&4=non active, p5=new p1, p7=new p3
          HighsInt p1 = mc_start[iCol];
          HighsInt p2 = p1 + mc_count_a[iCol];
          HighsInt p3 = p1 + mc_space[iCol] - mc_count_n[iCol];
          HighsInt p4 = p1 + mc_space[iCol];
          mc_space[iCol] += max(mc_space[iCol], nFillin);
          HighsInt p5 = mc_start[iCol] = mc_index.size();
          HighsInt p7 = p5 + mc_space[iCol] - mc_count_n[iCol];
          mc_index.resize(p5 + mc_space[iCol]);
          mc_value.resize(p5 + mc_space[iCol]);
          copy(&mc_index[p1], &mc_index[p2], &mc_index[p5]);
          copy(&mc_value[p1], &mc_value[p2], &mc_value[p5]);
          copy(&mc_index[p3], &mc_index[p4], &mc_index[p7]);
          copy(&mc_value[p3], &mc_value[p4], &mc_value[p7]);
        }

        // 2.4.4.2 Fill into column copy
        for (HighsInt i = 0; i < mwz_column_count; i++) {
          HighsInt iRow = mwz_column_index[i];
          if (mwz_column_mark[iRow]) {
	    double value = -my_pivot * mwz_column_array[iRow];
            colInsert(iCol, iRow, -my_pivot * mwz_column_array[iRow]);
	  }
        }

        // 2.4.4.3 Fill into the row copy
        for (HighsInt i = 0; i < mwz_column_count; i++) {
          HighsInt iRow = mwz_column_index[i];
          if (mwz_column_mark[iRow]) {
            // Expand row space
            if (mr_count[iRow] == mr_space[iRow]) {
              HighsInt p1 = mr_start[iRow];
              HighsInt p2 = p1 + mr_count[iRow];
              HighsInt p3 = mr_start[iRow] = mr_index.size();
              mr_space[iRow] *= 2;
              mr_index.resize(p3 + mr_space[iRow]);
              copy(&mr_index[p1], &mr_index[p2], &mr_index[p3]);
            }
            rowInsert(iCol, iRow);
          }
        }
      }

      // 2.4.5. Reset pivot column mark
      for (HighsInt i = 0; i < mwz_column_count; i++)
        mwz_column_mark[mwz_column_index[i]] = 1;

      // 2.4.6. Fix max value and link list
      colFixMax(iCol);
      if (my_count != mc_count_a[iCol]) {
        clinkDel(iCol);
        clinkAdd(iCol, mc_count_a[iCol]);
      }
    }

    // 2.5. Clear pivot column buffer
    for (HighsInt i = 0; i < mwz_column_count; i++)
      mwz_column_mark[mwz_column_index[i]] = 0;

    // 2.6. Correct row links for the remain active part
    for (HighsInt i = start_A; i < end_A; i++) {
      HighsInt iRow = mc_index[i];
      if (mr_count_before[iRow] != mr_count[iRow]) {
        rlinkDel(iRow);
        rlinkAdd(iRow, mr_count[iRow]);
      }
    }
  }
  build_synthetic_tick +=
      fake_search * 20 + fake_fill * 160 + fake_eliminate * 80;
  rank_deficiency = 0;
  if (progress_report) {
    logValueDistribution(log_options, pivot_merit);
    logValueDistribution(log_options, pivot_value);
  }
  return rank_deficiency;
}

void HFactor::findPivotColSearch(bool& found_pivot,
				 const HighsInt count,
				 HighsInt& jColPivot,
				 HighsInt& iRowPivot,
				 const std::string GE_stage_name,
				 const bool report_search) {
  if (count > num_row) return;
  // 1.3.1 Search through any columns with row count = count
  for (HighsInt j = col_link_first[count]; j != -1;
       j = col_link_next[j]) {
    assert(count >= min_row_count);
    double min_pivot = mc_min_pivot[j];
    HighsInt start = mc_start[j];
    HighsInt end = start + mc_count_a[j];
    // Look for possible pivots in this column. 
    for (HighsInt k = start; k < end; k++) {
      HighsInt report_row_count = -1;
      HighsInt report_row_index = -1;
      bool report_new_candidate = false;
      double merit_local = -1;
      if (fabs(mc_value[k]) >= min_pivot) {
	// This entry is big enough to be a pivot, but what's
	// its merit?
	HighsInt i = mc_index[k];
	HighsInt row_count = mr_count[i];
	merit_local = 1.0 * (count - 1) * (row_count - 1);
	report_row_count = row_count;
	report_row_index = i;
	if (merit_pivot > merit_local) {
	  // Better than the current best merit
	  report_new_candidate = true;
	  merit_pivot = merit_local;
	  jColPivot = j;
	  iRowPivot = i;
	  found_pivot = found_pivot || (row_count < count);
	  assert((row_count < count) == (row_count <= other_count_ideal));
	  assert(row_count >= other_count_ideal);
	}
      }
      if (report_search) {
	printf("%s: Search %6d (Col) Count %3d; merit (local %10.4g; ideal %10.4g): "
	       "Col %6d searched   0 indices for Row %6d (count %3d; value %11.4g)",
	       GE_stage_name.c_str(), (int)search_count,
	       (int)count, merit_local, merit_ideal, 
	       (int)j, (int)report_row_index,
	       (int)report_row_count, mc_value[k]);
	if (report_new_candidate) printf(" new_cdd merit %10.4g", merit_pivot);
	printf("\n");
      }
      if (use_merit_ideal && merit_pivot <= merit_ideal) break;
    }
    
    if ((search_count++ >= search_limit && merit_pivot < merit_limit) ||
	(use_merit_ideal && merit_pivot <= merit_ideal)) found_pivot = true;
    if (found_pivot) break;
    
    fake_search += count;
  }
}

void HFactor::findPivotRowSearch(bool& found_pivot,
				 const HighsInt count,
				 HighsInt& jColPivot,
				 HighsInt& iRowPivot,
				 const std::string GE_stage_name,
				 const bool report_search) {
  if (count > num_basic) return;
  // 1.3.2 Search for rows
  for (HighsInt i = row_link_first[count]; i != -1;
       i = row_link_next[i]) {
    assert(count >= min_col_count);
    HighsInt start = mr_start[i];
    HighsInt end = start + mr_count[i];
    for (HighsInt k = start; k < end; k++) {
      HighsInt j = mr_index[k];
      HighsInt column_count = mc_count_a[j];
      double merit_local = 1.0 * (count - 1) * (column_count - 1);
      HighsInt num_index_searched = 0;
      double value_found = kHighsInf;
      double min_pivot_required = kHighsInf;
      bool report_new_candidate = false;
      if (merit_pivot > merit_local) {
	HighsInt ifind = mc_start[j];
	while (mc_index[ifind] != i) ifind++;
	num_index_searched = ifind-mc_start[j]+1;
	value_found = mc_value[ifind];
	min_pivot_required = mc_min_pivot[j];
	if (fabs(mc_value[ifind]) >= mc_min_pivot[j]) {
	  report_new_candidate = true;
	  merit_pivot = merit_local;
	  jColPivot = j;
	  iRowPivot = i;
	  found_pivot = found_pivot || (column_count <= count);
	  assert((column_count <= count) == (column_count <= other_count_ideal));
	  assert(column_count >= other_count_ideal);
	}
      }
      if (report_search) {
	printf("%s: Search %6d (Row) Count %3d; merit (local %10.4g; ideal %10.4g): "
	       "Row %6d searched %3d indices for Col %6d (count %3d; value %11.4g; rq %11.4g)",
	       GE_stage_name.c_str(), (int)search_count,
	       (int)count, merit_local, merit_ideal, 
	       (int)i, (int)num_index_searched,
	       (int)j, (int)column_count, value_found, min_pivot_required);
	if (report_new_candidate) printf(" new_cdd merit %10.4g", merit_pivot);
	printf("\n");
      }
      if (use_merit_ideal && merit_pivot <= merit_ideal) break;
    }
    if ((search_count++ >= search_limit && merit_pivot < merit_limit) ||
	(use_merit_ideal && merit_pivot <= merit_ideal)) found_pivot = true;
    if (found_pivot) break;
  }

  fake_search += count;
}

void HFactor::buildHandleRankDeficiency() {
  debugReportRankDeficiency(0, highs_debug_level, log_options, num_row, permute,
                            iwork, basic_index, rank_deficiency,
                            row_with_no_pivot, col_with_no_pivot);
  // iwork can now be used as workspace: use it to accumulate the new
  // basic_index. iwork is set to -1 and basic_index is permuted into it.
  // Indices of iwork corresponding to missing indices in permute
  // remain -1. Hence the -1's become markers for the logicals which
  // will replace singular columns. Once basic_index[i] is read, it can
  // be used to pack up the entries in basic_index which are not
  // permuted anywhere - and so will be singular columns.
  //
  // On entry, rank_deficiency is the rank deficiency of basic_index, which is
  //
  // * Less than the rank deficiency of the basis matrix if num_basic < num_row
  //
  //
  const HighsInt basic_index_rank_deficiency = rank_deficiency;
  if (num_basic < num_row) {
    rank_deficiency += num_row - num_basic;
  }
  row_with_no_pivot.resize(rank_deficiency);
  col_with_no_pivot.resize(rank_deficiency);
  HighsInt lc_rank_deficiency = 0;
  if (num_basic < num_row) {
    // iwork still has to be used to indicate the rows with no pivots,
    // so resize it
    iwork.resize(num_row);
  } else if (num_basic > num_row) {
    // iwork only has to be used to indicate the rows with no pivots,
    // so resize it
    iwork.resize(num_basic);
  }
  // ToDo: surely this is neater as iwork.assign(num_row, -1);
  for (HighsInt i = 0; i < num_row; i++) iwork[i] = -1;
  for (HighsInt i = 0; i < num_basic; i++) {
    HighsInt perm_i = permute[i];
    if (perm_i >= 0) {
      iwork[perm_i] = basic_index[i];
    } else {
      col_with_no_pivot[lc_rank_deficiency++] = i;
    }
  }
  if (num_basic < num_row) {
    // Resize permute and complete iwork and col_with_no_pivot with
    // fictitious indices and entries of basic_index
    permute.resize(num_row);
    for (HighsInt i = num_basic; i < num_row; i++) {
      col_with_no_pivot[lc_rank_deficiency++] = i;
      permute[i] = -1;
    }
  }
  assert(lc_rank_deficiency == rank_deficiency);
  lc_rank_deficiency = 0;
  for (HighsInt i = 0; i < num_row; i++) {
    if (iwork[i] < 0) {
      // Record the rows with no pivots in row_with_no_pivot and indicate them
      // within iwork by storing the negation of one more than their
      // rank deficiency counter [since we can't have -0].
      row_with_no_pivot[lc_rank_deficiency] = i;
      iwork[i] = -(lc_rank_deficiency + 1);
      lc_rank_deficiency++;
    }
  }
  if (num_row < num_basic) {
    // Record fictitious rows with no pivots for the excess basic
    // variables so that permute will be constructed as a permutation
    // of all entries in basic_index
    for (HighsInt i = num_row; i < num_basic; i++) {
      row_with_no_pivot[lc_rank_deficiency] = i;
      iwork[i] = -(lc_rank_deficiency + 1);
      lc_rank_deficiency++;
    }
  }
  assert(lc_rank_deficiency == rank_deficiency);
  debugReportRankDeficiency(1, highs_debug_level, log_options, num_row, permute,
                            iwork, basic_index, rank_deficiency,
                            row_with_no_pivot, col_with_no_pivot);
  const HighsInt row_rank_deficiency =
      rank_deficiency - max(num_basic - num_row, (HighsInt)0);
  // Complete the permutation using the indices of rows with no pivot,
  // the last max(num_basic-num_row, 0) of which will be fictitious
  for (HighsInt k = 0; k < rank_deficiency; k++) {
    HighsInt iRow = row_with_no_pivot[k];
    HighsInt iCol = col_with_no_pivot[k];
    assert(permute[iCol] == -1);
    permute[iCol] = iRow;
    if (k < row_rank_deficiency) {
      // Only correct the factorization for the true rows
      l_start.push_back(l_index.size());
      u_pivot_index.push_back(iRow);
      u_pivot_value.push_back(1);
      u_start.push_back(u_index.size());
    }
  }
  debugReportRankDeficiency(2, highs_debug_level, log_options, num_row, permute,
                            iwork, basic_index, rank_deficiency,
                            row_with_no_pivot, col_with_no_pivot);
  debugReportRankDeficientASM(
      highs_debug_level, log_options, num_row, mc_start, mc_count_a, mc_index,
      mc_value, iwork, rank_deficiency, col_with_no_pivot, row_with_no_pivot);
}

void HFactor::buildMarkSingC() {
  // Singular matrix B: reorder the basic variables so that the
  // singular columns are in the position corresponding to the
  // logical which replaces them
  debugReportMarkSingC(0, highs_debug_level, log_options, num_row, iwork,
                       basic_index);

  const HighsInt basic_index_rank_deficiency =
      rank_deficiency - max(num_row - num_basic, (HighsInt)0);
  var_with_no_pivot.resize(rank_deficiency);
  for (HighsInt k = 0; k < rank_deficiency; k++) {
    HighsInt ASMrow = row_with_no_pivot[k];
    HighsInt ASMcol = col_with_no_pivot[k];
    assert(ASMrow < (HighsInt)iwork.size());
    assert(-iwork[ASMrow] - 1 >= 0 && -iwork[ASMrow] - 1 < rank_deficiency);
    // Store negation of 1+ASMcol so that removing column 0 can be
    // identified!
    iwork[ASMrow] = -(ASMcol + 1);
    // Only update basic_index for the true entries
    if (ASMcol < num_basic) {
      assert(k < basic_index_rank_deficiency);
      // Record the variable in basic_index that had no pivot, and
      // replace it with the logical
      var_with_no_pivot[k] = basic_index[ASMcol];
      basic_index[ASMcol] = num_col + ASMrow;
    } else if (num_basic < num_row) {
      assert(ASMcol == num_basic + k - basic_index_rank_deficiency);
      // Record an illegal variable when there's no index to displace
      var_with_no_pivot[k] = -1;
    }
  }
  debugReportMarkSingC(1, highs_debug_level, log_options, num_row, iwork,
                       basic_index);
}

void HFactor::buildFinish() {
  // Must only be called in the case where there are at least as many basic
  // variables as rows
  assert(num_basic >= num_row);
  //  debugPivotValueAnalysis(highs_debug_level, log_options, num_row,
  //  u_pivot_value);
  // The look up table
  for (HighsInt i = 0; i < num_row; i++) u_pivot_lookup[u_pivot_index[i]] = i;
  l_pivot_index = u_pivot_index;
  l_pivot_lookup = u_pivot_lookup;

  // LR space
  HighsInt LcountX = l_index.size();
  lr_index.resize(LcountX);
  lr_value.resize(LcountX);

  // LR pointer
  iwork.assign(num_row, 0);
  for (HighsInt k = 0; k < LcountX; k++) iwork[l_pivot_lookup[l_index[k]]]++;

  lr_start.assign(num_row + 1, 0);
  for (HighsInt i = 1; i <= num_row; i++)
    lr_start[i] = lr_start[i - 1] + iwork[i - 1];

  // LR elements
  iwork.assign(&lr_start[0], &lr_start[num_row]);
  for (HighsInt i = 0; i < num_row; i++) {
    const HighsInt index = l_pivot_index[i];
    for (HighsInt k = l_start[i]; k < l_start[i + 1]; k++) {
      HighsInt iRow = l_pivot_lookup[l_index[k]];
      HighsInt i_put = iwork[iRow]++;
      lr_index[i_put] = index;
      lr_value[i_put] = l_value[k];
    }
  }

  // U pointer
  u_start.push_back(0);
  u_last_p.assign(&u_start[1], &u_start[num_row + 1]);
  u_start.resize(num_row);

  // UR space
  HighsInt u_countX = u_index.size();
  HighsInt ur_stuff_size = update_method == kUpdateMethodFt ? 5 : 0;
  HighsInt ur_count_size = u_countX + ur_stuff_size * num_row;
  ur_index.resize(ur_count_size);
  ur_value.resize(ur_count_size);

  // UR pointer
  //
  // NB ur_lastp just being used as temporary storage here
  ur_start.assign(num_row + 1, 0);
  ur_lastp.assign(num_row, 0);
  ur_space.assign(num_row, ur_stuff_size);
  for (HighsInt k = 0; k < u_countX; k++)
    ur_lastp[u_pivot_lookup[u_index[k]]]++;
  for (HighsInt i = 1; i <= num_row; i++)
    ur_start[i] = ur_start[i - 1] + ur_lastp[i - 1] + ur_stuff_size;
  ur_start.resize(num_row);

  // UR element
  //
  // NB ur_lastp initialised here!
  ur_lastp = ur_start;
  for (HighsInt i = 0; i < num_row; i++) {
    const HighsInt index = u_pivot_index[i];
    for (HighsInt k = u_start[i]; k < u_last_p[i]; k++) {
      HighsInt iRow = u_pivot_lookup[u_index[k]];
      HighsInt i_put = ur_lastp[iRow]++;
      ur_index[i_put] = index;
      ur_value[i_put] = u_value[k];
    }
  }

  // Re-factor merit
  u_merit_x = num_row + (LcountX + u_countX) * 1.5;
  u_total_x = u_countX;
  if (update_method == kUpdateMethodPf) u_merit_x = num_row + u_countX * 4;
  if (update_method == kUpdateMethodMpf) u_merit_x = num_row + u_countX * 3;

  // Clear update buffer
  pf_pivot_value.clear();
  pf_pivot_index.clear();
  pf_start.clear();
  pf_start.push_back(0);
  pf_index.clear();
  pf_value.clear();

  if (!this->refactor_info_.use) {
    // Finally, if not calling buildFinish after refactorizing,
    // permute the basic variables
    iwork.assign(basic_index, basic_index + num_basic);
    for (HighsInt i = 0; i < num_basic; i++) basic_index[permute[i]] = iwork[i];
    // If there are more basic variables than rows, basic_index
    // should have been permuted so that its last num_basic-num_row
    // entries are logicals
    for (HighsInt i = num_row; i < num_basic; i++)
      assert(basic_index[i] >= num_col);
    // Add cost of buildFinish to build_synthetic_tick
    build_synthetic_tick += num_row * 80 + (LcountX + u_countX) * 60;
  }
}

void HFactor::ftranL(HVector& rhs, const double expected_density,
                     HighsTimerClock* factor_timer_clock_pointer) const {
  FactorTimer factor_timer;
  factor_timer.start(FactorFtranLower, factor_timer_clock_pointer);
  if (update_method == kUpdateMethodApf) {
    factor_timer.start(FactorFtranLowerAPF, factor_timer_clock_pointer);
    rhs.tight();
    rhs.pack();
    ftranAPF(rhs);
    factor_timer.stop(FactorFtranLowerAPF, factor_timer_clock_pointer);
    rhs.tight();
  }

  double current_density = 1.0 * rhs.count / num_row;
  if (current_density > kHyperCancel || expected_density > kHyperFtranL) {
    factor_timer.start(FactorFtranLowerSps, factor_timer_clock_pointer);
    // Alias to RHS
    HighsInt rhs_count = 0;
    HighsInt* rhs_index = &rhs.index[0];
    double* rhs_array = &rhs.array[0];

    // Alias to factor L
    const HighsInt* l_start = &this->l_start[0];
    const HighsInt* l_index =
        this->l_index.size() > 0 ? &this->l_index[0] : NULL;
    const double* l_value = this->l_value.size() > 0 ? &this->l_value[0] : NULL;

    // Transform
    for (HighsInt i = 0; i < num_row; i++) {
      HighsInt pivotRow = l_pivot_index[i];
      const double pivot_multiplier = rhs_array[pivotRow];
      if (fabs(pivot_multiplier) > kHighsTiny) {
        rhs_index[rhs_count++] = pivotRow;
        const HighsInt start = l_start[i];
        const HighsInt end = l_start[i + 1];
        for (HighsInt k = start; k < end; k++)
          rhs_array[l_index[k]] -= pivot_multiplier * l_value[k];
      } else
        rhs_array[pivotRow] = 0;
    }

    // Save the count
    rhs.count = rhs_count;
    factor_timer.stop(FactorFtranLowerSps, factor_timer_clock_pointer);
  } else {
    factor_timer.start(FactorFtranLowerHyper, factor_timer_clock_pointer);
    const HighsInt* l_index =
        this->l_index.size() > 0 ? &this->l_index[0] : NULL;
    const double* l_value = this->l_value.size() > 0 ? &this->l_value[0] : NULL;
    solveHyper(num_row, &l_pivot_lookup[0], &l_pivot_index[0], 0, &l_start[0],
               &l_start[1], &l_index[0], &l_value[0], &rhs);
    factor_timer.stop(FactorFtranLowerHyper, factor_timer_clock_pointer);
  }
  factor_timer.stop(FactorFtranLower, factor_timer_clock_pointer);
}

void HFactor::btranL(HVector& rhs, const double expected_density,
                     HighsTimerClock* factor_timer_clock_pointer) const {
  FactorTimer factor_timer;
  factor_timer.start(FactorBtranLower, factor_timer_clock_pointer);
  double current_density = 1.0 * rhs.count / num_row;
  if (current_density > kHyperCancel || expected_density > kHyperBtranL) {
    // Alias to RHS
    factor_timer.start(FactorBtranLowerSps, factor_timer_clock_pointer);
    HighsInt rhs_count = 0;
    HighsInt* rhs_index = &rhs.index[0];
    double* rhs_array = &rhs.array[0];

    // Alias to factor L
    const HighsInt* lr_start = &this->lr_start[0];
    const HighsInt* lr_index =
        this->lr_index.size() > 0 ? &this->lr_index[0] : NULL;
    const double* lr_value =
        this->lr_value.size() > 0 ? &this->lr_value[0] : NULL;

    // Transform
    for (HighsInt i = num_row - 1; i >= 0; i--) {
      HighsInt pivotRow = l_pivot_index[i];
      const double pivot_multiplier = rhs_array[pivotRow];
      if (fabs(pivot_multiplier) > kHighsTiny) {
        rhs_index[rhs_count++] = pivotRow;
        rhs_array[pivotRow] = pivot_multiplier;
        const HighsInt start = lr_start[i];
        const HighsInt end = lr_start[i + 1];
        for (HighsInt k = start; k < end; k++)
          rhs_array[lr_index[k]] -= pivot_multiplier * lr_value[k];
      } else
        rhs_array[pivotRow] = 0;
    }

    // Save the count
    rhs.count = rhs_count;
    factor_timer.stop(FactorBtranLowerSps, factor_timer_clock_pointer);
  } else {
    factor_timer.start(FactorBtranLowerHyper, factor_timer_clock_pointer);
    const HighsInt* lr_index =
        this->lr_index.size() > 0 ? &this->lr_index[0] : NULL;
    const double* lr_value =
        this->lr_value.size() > 0 ? &this->lr_value[0] : NULL;
    solveHyper(num_row, &l_pivot_lookup[0], &l_pivot_index[0], 0, &lr_start[0],
               &lr_start[1], &lr_index[0], &lr_value[0], &rhs);
    factor_timer.stop(FactorBtranLowerHyper, factor_timer_clock_pointer);
  }

  if (update_method == kUpdateMethodApf) {
    factor_timer.start(FactorBtranLowerAPF, factor_timer_clock_pointer);
    btranAPF(rhs);
    rhs.tight();
    rhs.pack();
    factor_timer.stop(FactorBtranLowerAPF, factor_timer_clock_pointer);
  }
  factor_timer.stop(FactorBtranLower, factor_timer_clock_pointer);
}

void HFactor::ftranU(HVector& rhs, const double expected_density,
                     HighsTimerClock* factor_timer_clock_pointer) const {
  FactorTimer factor_timer;
  factor_timer.start(FactorFtranUpper, factor_timer_clock_pointer);
  // The update part
  if (update_method == kUpdateMethodFt) {
    factor_timer.start(FactorFtranUpperFT, factor_timer_clock_pointer);
    //    const double current_density = 1.0 * rhs.count / num_row;
    ftranFT(rhs);
    rhs.tight();
    rhs.pack();
    factor_timer.stop(FactorFtranUpperFT, factor_timer_clock_pointer);
  }
  if (update_method == kUpdateMethodMpf) {
    factor_timer.start(FactorFtranUpperMPF, factor_timer_clock_pointer);
    ftranMPF(rhs);
    rhs.tight();
    rhs.pack();
    factor_timer.stop(FactorFtranUpperMPF, factor_timer_clock_pointer);
  }

  // The regular part
  const double current_density = 1.0 * rhs.count / num_row;
  if (current_density > kHyperCancel || expected_density > kHyperFtranU) {
    const bool report_ftran_upper_sparse =
        false;  // current_density < kHyperCancel;
    HighsInt use_clock;
    if (current_density < 0.1)
      use_clock = FactorFtranUpperSps2;
    else if (current_density < 0.5)
      use_clock = FactorFtranUpperSps1;
    else
      use_clock = FactorFtranUpperSps0;
    factor_timer.start(use_clock, factor_timer_clock_pointer);
    // Alias to non constant
    double rhs_synthetic_tick = 0;
    HighsInt rhs_count = 0;
    HighsInt* rhs_index = &rhs.index[0];
    double* rhs_array = &rhs.array[0];

    // Alias to the factor
    const HighsInt* u_start = &this->u_start[0];
    const HighsInt* u_end = &this->u_last_p[0];
    const HighsInt* u_index =
        this->u_index.size() > 0 ? &this->u_index[0] : NULL;
    const double* u_value = this->u_value.size() > 0 ? &this->u_value[0] : NULL;

    // Transform
    HighsInt u_pivot_count = u_pivot_index.size();
    for (HighsInt i_logic = u_pivot_count - 1; i_logic >= 0; i_logic--) {
      // Skip void
      if (u_pivot_index[i_logic] == -1) continue;

      // Normal part
      const HighsInt pivotRow = u_pivot_index[i_logic];
      double pivot_multiplier = rhs_array[pivotRow];
      if (fabs(pivot_multiplier) > kHighsTiny) {
        pivot_multiplier /= u_pivot_value[i_logic];
        rhs_index[rhs_count++] = pivotRow;
        rhs_array[pivotRow] = pivot_multiplier;
        const HighsInt start = u_start[i_logic];
        const HighsInt end = u_end[i_logic];
        if (i_logic >= num_row) {
          rhs_synthetic_tick += (end - start);
        }
        for (HighsInt k = start; k < end; k++)
          rhs_array[u_index[k]] -= pivot_multiplier * u_value[k];
      } else
        rhs_array[pivotRow] = 0;
    }

    // Save the count
    rhs.count = rhs_count;
    rhs.synthetic_tick +=
        rhs_synthetic_tick * 15 + (u_pivot_count - num_row) * 10;
    factor_timer.stop(use_clock, factor_timer_clock_pointer);
    if (report_ftran_upper_sparse) {
      const double final_density = 1.0 * rhs.count / num_row;
      printf(
          "FactorFtranUpperSps: expected_density = %10.4g; current_density = "
          "%10.4g; final_density = %10.4g\n",
          expected_density, current_density, final_density);
    }
  } else {
    HighsInt use_clock = -1;
    if (current_density < 5e-6)
      use_clock = FactorFtranUpperHyper5;
    else if (current_density < 1e-5)
      use_clock = FactorFtranUpperHyper4;
    else if (current_density < 1e-4)
      use_clock = FactorFtranUpperHyper3;
    else if (current_density < 1e-3)
      use_clock = FactorFtranUpperHyper2;
    else if (current_density < 1e-2)
      use_clock = FactorFtranUpperHyper1;
    else
      use_clock = FactorFtranUpperHyper0;
    factor_timer.start(use_clock, factor_timer_clock_pointer);
    const HighsInt* u_index =
        this->u_index.size() > 0 ? &this->u_index[0] : NULL;
    const double* u_value = this->u_value.size() > 0 ? &this->u_value[0] : NULL;
    solveHyper(num_row, &u_pivot_lookup[0], &u_pivot_index[0],
               &u_pivot_value[0], &u_start[0], &u_last_p[0], &u_index[0],
               &u_value[0], &rhs);
    factor_timer.stop(use_clock, factor_timer_clock_pointer);
  }
  if (update_method == kUpdateMethodPf) {
    factor_timer.start(FactorFtranUpperPF, factor_timer_clock_pointer);
    ftranPF(rhs);
    rhs.tight();
    rhs.pack();
    factor_timer.stop(FactorFtranUpperPF, factor_timer_clock_pointer);
  }
  factor_timer.stop(FactorFtranUpper, factor_timer_clock_pointer);
}

void HFactor::btranU(HVector& rhs, const double expected_density,
                     HighsTimerClock* factor_timer_clock_pointer) const {
  FactorTimer factor_timer;
  factor_timer.start(FactorBtranUpper, factor_timer_clock_pointer);
  if (update_method == kUpdateMethodPf) {
    factor_timer.start(FactorBtranUpperPF, factor_timer_clock_pointer);
    btranPF(rhs);
    factor_timer.stop(FactorBtranUpperPF, factor_timer_clock_pointer);
  }

  // The regular part
  double current_density = 1.0 * rhs.count / num_row;
  if (current_density > kHyperCancel || expected_density > kHyperBtranU) {
    factor_timer.start(FactorBtranUpperSps, factor_timer_clock_pointer);
    // Alias to non constant
    double rhs_synthetic_tick = 0;
    HighsInt rhs_count = 0;
    HighsInt* rhs_index = &rhs.index[0];
    double* rhs_array = &rhs.array[0];

    // Alias to the factor
    const HighsInt* ur_start = &this->ur_start[0];
    const HighsInt* ur_end = &this->ur_lastp[0];
    const HighsInt* ur_index = &this->ur_index[0];
    const double* ur_value = &this->ur_value[0];

    // Transform
    HighsInt u_pivot_count = u_pivot_index.size();
    for (HighsInt i_logic = 0; i_logic < u_pivot_count; i_logic++) {
      // Skip void
      if (u_pivot_index[i_logic] == -1) continue;

      // Normal part
      const HighsInt pivotRow = u_pivot_index[i_logic];
      double pivot_multiplier = rhs_array[pivotRow];
      if (fabs(pivot_multiplier) > kHighsTiny) {
        pivot_multiplier /= u_pivot_value[i_logic];
        rhs_index[rhs_count++] = pivotRow;
        rhs_array[pivotRow] = pivot_multiplier;
        const HighsInt start = ur_start[i_logic];
        const HighsInt end = ur_end[i_logic];
        if (i_logic >= num_row) {
          rhs_synthetic_tick += (end - start);
        }
        for (HighsInt k = start; k < end; k++)
          rhs_array[ur_index[k]] -= pivot_multiplier * ur_value[k];
      } else
        rhs_array[pivotRow] = 0;
    }

    // Save the count
    rhs.count = rhs_count;
    rhs.synthetic_tick +=
        rhs_synthetic_tick * 15 + (u_pivot_count - num_row) * 10;
    factor_timer.stop(FactorBtranUpperSps, factor_timer_clock_pointer);
  } else {
    factor_timer.start(FactorBtranUpperHyper, factor_timer_clock_pointer);
    solveHyper(num_row, &u_pivot_lookup[0], &u_pivot_index[0],
               &u_pivot_value[0], &ur_start[0], &ur_lastp[0], &ur_index[0],
               &ur_value[0], &rhs);
    factor_timer.stop(FactorBtranUpperHyper, factor_timer_clock_pointer);
  }

  // The update part
  if (update_method == kUpdateMethodFt) {
    factor_timer.start(FactorBtranUpperFT, factor_timer_clock_pointer);
    rhs.tight();
    rhs.pack();
    //    const double current_density = 1.0 * rhs.count / num_row;
    btranFT(rhs);
    rhs.tight();
    factor_timer.stop(FactorBtranUpperFT, factor_timer_clock_pointer);
  }
  if (update_method == kUpdateMethodMpf) {
    factor_timer.start(FactorBtranUpperMPF, factor_timer_clock_pointer);
    rhs.tight();
    rhs.pack();
    btranMPF(rhs);
    rhs.tight();
    factor_timer.stop(FactorBtranUpperMPF, factor_timer_clock_pointer);
  }
  factor_timer.stop(FactorBtranUpper, factor_timer_clock_pointer);
}

void HFactor::ftranFT(HVector& vector) const {
  // Alias to PF buffer
  const HighsInt pf_pivot_count = pf_pivot_index.size();
  HighsInt* pf_pivot_index = NULL;
  if (this->pf_pivot_index.size() > 0)
    pf_pivot_index = (HighsInt*)&this->pf_pivot_index[0];

  const HighsInt* pf_start =
      this->pf_start.size() > 0 ? &this->pf_start[0] : NULL;
  const HighsInt* pf_index =
      this->pf_index.size() > 0 ? &this->pf_index[0] : NULL;
  const double* pf_value =
      this->pf_value.size() > 0 ? &this->pf_value[0] : NULL;

  // Alias to non constant
  HighsInt rhs_count = vector.count;
  HighsInt* rhs_index = &vector.index[0];
  double* rhs_array = &vector.array[0];

  // Forwardly apply row ETA
  for (HighsInt i = 0; i < pf_pivot_count; i++) {
    HighsInt iRow = pf_pivot_index[i];
    double value0 = rhs_array[iRow];
    double value1 = value0;
    const HighsInt start = pf_start[i];
    const HighsInt end = pf_start[i + 1];
    for (HighsInt k = start; k < end; k++)
      value1 -= rhs_array[pf_index[k]] * pf_value[k];
    // This would skip the situation where they are both zeros
    if (value0 || value1) {
      if (value0 == 0) rhs_index[rhs_count++] = iRow;
      rhs_array[iRow] = (fabs(value1) < kHighsTiny) ? kHighsZero : value1;
    }
  }

  // Save count back
  vector.count = rhs_count;
  vector.synthetic_tick += pf_pivot_count * 20 + pf_start[pf_pivot_count] * 5;
  if (pf_start[pf_pivot_count] / (pf_pivot_count + 1) < 5) {
    vector.synthetic_tick += pf_start[pf_pivot_count] * 5;
  }
}

void HFactor::btranFT(HVector& vector) const {
  // Alias to PF buffer
  const HighsInt pf_pivot_count = pf_pivot_index.size();
  const HighsInt* pf_pivot_index =
      this->pf_pivot_index.size() > 0 ? &this->pf_pivot_index[0] : NULL;
  const HighsInt* pf_start =
      this->pf_start.size() > 0 ? &this->pf_start[0] : NULL;
  const HighsInt* pf_index =
      this->pf_index.size() > 0 ? &this->pf_index[0] : NULL;
  const double* pf_value =
      this->pf_value.size() > 0 ? &this->pf_value[0] : NULL;

  // Alias to non constant
  double rhs_synthetic_tick = 0;
  HighsInt rhs_count = vector.count;
  HighsInt* rhs_index = &vector.index[0];
  double* rhs_array = &vector.array[0];

  // Backwardly apply row ETA
  for (HighsInt i = pf_pivot_count - 1; i >= 0; i--) {
    HighsInt pivotRow = pf_pivot_index[i];
    double pivot_multiplier = rhs_array[pivotRow];
    if (pivot_multiplier) {
      const HighsInt start = pf_start[i];
      const HighsInt end = pf_start[i + 1];
      rhs_synthetic_tick += (end - start);
      for (HighsInt k = start; k < end; k++) {
        HighsInt iRow = pf_index[k];
        double value0 = rhs_array[iRow];
        double value1 = value0 - pivot_multiplier * pf_value[k];
        if (value0 == 0) rhs_index[rhs_count++] = iRow;
        rhs_array[iRow] = (fabs(value1) < kHighsTiny) ? kHighsZero : value1;
      }
    }
  }

  vector.synthetic_tick += rhs_synthetic_tick * 15 + pf_pivot_count * 10;

  // Save count back
  vector.count = rhs_count;
}

void HFactor::ftranPF(HVector& vector) const {
  // Alias to PF buffer
  const HighsInt pf_pivot_count = pf_pivot_index.size();
  const HighsInt* pf_pivot_index = &this->pf_pivot_index[0];
  const double* pf_pivot_value = &this->pf_pivot_value[0];
  const HighsInt* pf_start = &this->pf_start[0];
  const HighsInt* pf_index = &this->pf_index[0];
  const double* pf_value = &this->pf_value[0];

  // Alias to non constant
  HighsInt rhs_count = vector.count;
  HighsInt* rhs_index = &vector.index[0];
  double* rhs_array = &vector.array[0];

  // Forwardly
  for (HighsInt i = 0; i < pf_pivot_count; i++) {
    HighsInt pivotRow = pf_pivot_index[i];
    double pivot_multiplier = rhs_array[pivotRow];
    if (fabs(pivot_multiplier) > kHighsTiny) {
      pivot_multiplier /= pf_pivot_value[i];
      rhs_array[pivotRow] = pivot_multiplier;
      for (HighsInt k = pf_start[i]; k < pf_start[i + 1]; k++) {
        const HighsInt index = pf_index[k];
        const double value0 = rhs_array[index];
        const double value1 = value0 - pivot_multiplier * pf_value[k];
        if (value0 == 0) rhs_index[rhs_count++] = index;
        rhs_array[index] = (fabs(value1) < kHighsTiny) ? kHighsZero : value1;
      }
    }
  }

  // Save count
  vector.count = rhs_count;
}

void HFactor::btranPF(HVector& vector) const {
  // Alias to PF buffer
  const HighsInt pf_pivot_count = pf_pivot_index.size();
  const HighsInt* pf_pivot_index = &this->pf_pivot_index[0];
  const double* pf_pivot_value = &this->pf_pivot_value[0];
  const HighsInt* pf_start = &this->pf_start[0];
  const HighsInt* pf_index = &this->pf_index[0];
  const double* pf_value = &this->pf_value[0];

  // Alias to non constant
  HighsInt rhs_count = vector.count;
  HighsInt* rhs_index = &vector.index[0];
  double* rhs_array = &vector.array[0];

  // Backwardly
  for (HighsInt i = pf_pivot_count - 1; i >= 0; i--) {
    HighsInt pivotRow = pf_pivot_index[i];
    double pivot_multiplier = rhs_array[pivotRow];
    for (HighsInt k = pf_start[i]; k < pf_start[i + 1]; k++)
      pivot_multiplier -= pf_value[k] * rhs_array[pf_index[k]];
    pivot_multiplier /= pf_pivot_value[i];

    if (rhs_array[pivotRow] == 0) rhs_index[rhs_count++] = pivotRow;
    rhs_array[pivotRow] =
        (fabs(pivot_multiplier) < kHighsTiny) ? 1e-100 : pivot_multiplier;
  }

  // Save count
  vector.count = rhs_count;
}

void HFactor::ftranMPF(HVector& vector) const {
  // Alias to non constant
  HighsInt rhs_count = vector.count;
  HighsInt* rhs_index = &vector.index[0];
  double* rhs_array = &vector.array[0];

  // Forwardly
  HighsInt pf_pivot_count = pf_pivot_value.size();
  for (HighsInt i = 0; i < pf_pivot_count; i++) {
    solveMatrixT(pf_start[i * 2 + 1], pf_start[i * 2 + 2], pf_start[i * 2],
                 pf_start[i * 2 + 1], &pf_index[0], &pf_value[0],
                 pf_pivot_value[i], &rhs_count, rhs_index, rhs_array);
  }

  // Remove cancellation
  vector.count = rhs_count;
}

void HFactor::btranMPF(HVector& vector) const {
  // Alias to non constant
  HighsInt rhs_count = vector.count;
  HighsInt* rhs_index = &vector.index[0];
  double* rhs_array = &vector.array[0];

  // Backwardly
  for (HighsInt i = pf_pivot_value.size() - 1; i >= 0; i--) {
    solveMatrixT(pf_start[i * 2], pf_start[i * 2 + 1], pf_start[i * 2 + 1],
                 pf_start[i * 2 + 2], &pf_index[0], &pf_value[0],
                 pf_pivot_value[i], &rhs_count, rhs_index, rhs_array);
  }

  // Remove cancellation
  vector.count = rhs_count;
}

void HFactor::ftranAPF(HVector& vector) const {
  // Alias to non constant
  HighsInt rhs_count = vector.count;
  HighsInt* rhs_index = &vector.index[0];
  double* rhs_array = &vector.array[0];

  // Backwardly
  HighsInt pf_pivot_count = pf_pivot_value.size();
  for (HighsInt i = pf_pivot_count - 1; i >= 0; i--) {
    solveMatrixT(pf_start[i * 2 + 1], pf_start[i * 2 + 2], pf_start[i * 2],
                 pf_start[i * 2 + 1], &pf_index[0], &pf_value[0],
                 pf_pivot_value[i], &rhs_count, rhs_index, rhs_array);
  }

  // Remove cancellation
  vector.count = rhs_count;
}

void HFactor::btranAPF(HVector& vector) const {
  // Alias to non constant
  HighsInt rhs_count = vector.count;
  HighsInt* rhs_index = &vector.index[0];
  double* rhs_array = &vector.array[0];

  // Forwardly
  HighsInt pf_pivot_count = pf_pivot_value.size();
  for (HighsInt i = 0; i < pf_pivot_count; i++) {
    solveMatrixT(pf_start[i * 2], pf_start[i * 2 + 1], pf_start[i * 2 + 1],
                 pf_start[i * 2 + 2], &pf_index[0], &pf_value[0],
                 pf_pivot_value[i], &rhs_count, rhs_index, rhs_array);
  }
  vector.count = rhs_count;
}

void HFactor::updateCFT(HVector* aq, HVector* ep, HighsInt* iRow
                        //, HighsInt* hint
) {
  /*
   * In the major update loop, the prefix
   *
   * c(p) = current working pivot
   * p(p) = previous pivot  (0 =< pp < cp)
   */

  HighsInt num_update = 0;
  for (HVector* vec = aq; vec != 0; vec = vec->next) num_update++;

  HVector** aq_work = new HVector*[num_update];
  HVector** ep_work = new HVector*[num_update];

  for (HighsInt i = 0; i < num_update; i++) {
    aq_work[i] = aq;
    ep_work[i] = ep;
    aq = aq->next;
    ep = ep->next;
  }

  // Pivot related buffers
  HighsInt pf_np0 = pf_pivot_index.size();
  HighsInt* p_logic = new HighsInt[num_update];
  double* p_value = new double[num_update];
  double* p_alpha = new double[num_update];
  for (HighsInt cp = 0; cp < num_update; cp++) {
    HighsInt c_row = iRow[cp];
    HighsInt i_logic = u_pivot_lookup[c_row];
    p_logic[cp] = i_logic;
    p_value[cp] = u_pivot_value[i_logic];
    p_alpha[cp] = aq_work[cp]->array[c_row];
  }

  // Temporary U pointers
  HighsInt* t_start = new HighsInt[num_update + 1];
  double* t_pivot = new double[num_update];
  t_start[0] = u_index.size();

  // Logically sorted previous row_ep
  vector<pair<HighsInt, int> > sorted_pp;

  // Major update loop
  for (HighsInt cp = 0; cp < num_update; cp++) {
    // 1. Expand partial FTRAN result to buffer
    iwork.clear();
    for (HighsInt i = 0; i < aq_work[cp]->packCount; i++) {
      HighsInt index = aq_work[cp]->packIndex[i];
      double value = aq_work[cp]->packValue[i];
      iwork.push_back(index);
      dwork[index] = value;
    }

    // 2. Update partial FTRAN result by recent FT matrix
    for (HighsInt pp = 0; pp < cp; pp++) {
      HighsInt p_row = iRow[pp];
      double value = dwork[p_row];
      HighsInt pf_pp = pp + pf_np0;
      for (HighsInt i = pf_start[pf_pp]; i < pf_start[pf_pp + 1]; i++)
        value -= dwork[pf_index[i]] * pf_value[i];
      iwork.push_back(p_row);  // OK to duplicate
      dwork[p_row] = value;
    }

    // 3. Store the partial FTRAN result to matirx U
    double ppaq = dwork[iRow[cp]];  // pivot of the partial aq
    dwork[iRow[cp]] = 0;
    HighsInt u_countX = t_start[cp];
    HighsInt u_startX = u_countX;
    for (unsigned i = 0; i < iwork.size(); i++) {
      HighsInt index = iwork[i];
      double value = dwork[index];
      dwork[index] = 0;  // This effectively removes all duplication
      if (fabs(value) > kHighsTiny) {
        u_index.push_back(index);
        u_value.push_back(value);
      }
    }
    u_countX = u_index.size();
    t_start[cp + 1] = u_countX;
    t_pivot[cp] = p_value[cp] * p_alpha[cp];

    // 4. Expand partial BTRAN result to buffer
    iwork.clear();
    for (HighsInt i = 0; i < ep_work[cp]->packCount; i++) {
      HighsInt index = ep_work[cp]->packIndex[i];
      double value = ep_work[cp]->packValue[i];
      iwork.push_back(index);
      dwork[index] = value;
    }

    // 5. Delete logical later rows (in logical order)
    for (HighsInt isort = 0; isort < cp; isort++) {
      HighsInt pp = sorted_pp[isort].second;
      HighsInt p_row = iRow[pp];
      double multiplier = -p_value[pp] * dwork[p_row];
      if (fabs(dwork[p_row]) > kHighsTiny) {
        for (HighsInt i = 0; i < ep_work[pp]->packCount; i++) {
          HighsInt index = ep_work[pp]->packIndex[i];
          double value = ep_work[pp]->packValue[i];
          iwork.push_back(index);
          dwork[index] += value * multiplier;
        }
      }
      dwork[p_row] = 0;  // Force to be 0
    }

    // 6. Update partial BTRAN result by recent U columns
    for (HighsInt pp = 0; pp < cp; pp++) {
      HighsInt kpivot = iRow[pp];
      double value = dwork[kpivot];
      for (HighsInt k = t_start[pp]; k < t_start[pp + 1]; k++)
        value -= dwork[u_index[k]] * u_value[k];
      value /= t_pivot[pp];
      iwork.push_back(kpivot);
      dwork[kpivot] = value;  // Again OK to duplicate
    }

    // 6.x compute current alpha
    double thex = 0;
    for (HighsInt k = u_startX; k < u_countX; k++) {
      HighsInt index = u_index[k];
      double value = u_value[k];
      thex += dwork[index] * value;
    }
    t_pivot[cp] = ppaq + thex * p_value[cp];

    // 7. Store BTRAN result to FT elimination, update logic helper
    dwork[iRow[cp]] = 0;
    double pivot_multiplier = -p_value[cp];
    for (unsigned i = 0; i < iwork.size(); i++) {
      HighsInt index = iwork[i];
      double value = dwork[index];
      dwork[index] = 0;
      if (fabs(value) > kHighsTiny) {
        pf_index.push_back(index);
        pf_value.push_back(value * pivot_multiplier);
      }
    }
    pf_pivot_index.push_back(iRow[cp]);
    u_total_x += pf_index.size() - pf_start.back();
    pf_start.push_back(pf_index.size());

    // 8. Update the sorted ep
    sorted_pp.push_back(make_pair(p_logic[cp], cp));
    pdqsort(sorted_pp.begin(), sorted_pp.end());
  }

  // Now modify the U matrix
  for (HighsInt cp = 0; cp < num_update; cp++) {
    // 1. Delete pivotal row from U
    HighsInt cIndex = iRow[cp];
    HighsInt cLogic = p_logic[cp];
    u_total_x -= ur_lastp[cLogic] - ur_start[cLogic];
    for (HighsInt k = ur_start[cLogic]; k < ur_lastp[cLogic]; k++) {
      // Find the pivotal position
      HighsInt i_logic = u_pivot_lookup[ur_index[k]];
      HighsInt i_find = u_start[i_logic];
      HighsInt i_last = --u_last_p[i_logic];
      for (; i_find <= i_last; i_find++)
        if (u_index[i_find] == cIndex) break;
      // Put last to find, and delete last
      u_index[i_find] = u_index[i_last];
      u_value[i_find] = u_value[i_last];
    }

    // 2. Delete pivotal column from UR
    u_total_x -= u_last_p[cLogic] - u_start[cLogic];
    for (HighsInt k = u_start[cLogic]; k < u_last_p[cLogic]; k++) {
      // Find the pivotal position
      HighsInt i_logic = u_pivot_lookup[u_index[k]];
      HighsInt i_find = ur_start[i_logic];
      HighsInt i_last = --ur_lastp[i_logic];
      for (; i_find <= i_last; i_find++)
        if (ur_index[i_find] == cIndex) break;
      // Put last to find, and delete last
      ur_space[i_logic]++;
      ur_index[i_find] = ur_index[i_last];
      ur_value[i_find] = ur_value[i_last];
    }

    // 3. Insert the (stored) partial FTRAN to the row matrix
    HighsInt u_startX = t_start[cp];
    HighsInt u_endX = t_start[cp + 1];
    u_total_x += u_endX - u_startX;
    // Store column as UR elements
    for (HighsInt k = u_startX; k < u_endX; k++) {
      // Which ETA file
      HighsInt i_logic = u_pivot_lookup[u_index[k]];

      // Move row to the end if necessary
      if (ur_space[i_logic] == 0) {
        // Make pointers
        HighsInt row_start = ur_start[i_logic];
        HighsInt row_count = ur_lastp[i_logic] - row_start;
        HighsInt new_start = ur_index.size();
        HighsInt new_space = row_count * 1.1 + 5;

        // Check matrix UR
        ur_index.resize(new_start + new_space);
        ur_value.resize(new_start + new_space);

        // Move elements
        HighsInt i_from = row_start;
        HighsInt i_end = row_start + row_count;
        HighsInt i_to = new_start;
        copy(&ur_index[i_from], &ur_index[i_end], &ur_index[i_to]);
        copy(&ur_value[i_from], &ur_value[i_end], &ur_value[i_to]);

        // Save new pointers
        ur_start[i_logic] = new_start;
        ur_lastp[i_logic] = new_start + row_count;
        ur_space[i_logic] = new_space - row_count;
      }

      // Put into the next available space
      ur_space[i_logic]--;
      HighsInt i_put = ur_lastp[i_logic]++;
      ur_index[i_put] = cIndex;
      ur_value[i_put] = u_value[k];
    }

    // 4. Save pointers
    u_start.push_back(u_startX);
    u_last_p.push_back(u_endX);

    ur_start.push_back(ur_start[cLogic]);
    ur_lastp.push_back(ur_start[cLogic]);
    ur_space.push_back(ur_space[cLogic] + ur_lastp[cLogic] - ur_start[cLogic]);

    u_pivot_lookup[cIndex] = u_pivot_index.size();
    u_pivot_index[cLogic] = -1;
    u_pivot_index.push_back(cIndex);
    u_pivot_value.push_back(t_pivot[cp]);
  }

  //    // See if we want refactor
  //    if (u_total_x > u_merit_x && pf_pivot_index.size() > 100)
  //        *hint = 1;
  delete[] aq_work;
  delete[] ep_work;
  delete[] p_logic;
  delete[] p_value;
  delete[] p_alpha;
  delete[] t_start;
  delete[] t_pivot;
}

void HFactor::updateFT(HVector* aq, HVector* ep, HighsInt iRow
                       //, HighsInt* hint
) {
  // Store pivot
  HighsInt p_logic = u_pivot_lookup[iRow];
  double pivot = u_pivot_value[p_logic];
  double alpha = aq->array[iRow];
  u_pivot_index[p_logic] = -1;

  // Delete pivotal row from U
  for (HighsInt k = ur_start[p_logic]; k < ur_lastp[p_logic]; k++) {
    // Find the pivotal position
    HighsInt i_logic = u_pivot_lookup[ur_index[k]];
    HighsInt i_find = u_start[i_logic];
    HighsInt i_last = --u_last_p[i_logic];
    for (; i_find <= i_last; i_find++)
      if (u_index[i_find] == iRow) break;
    // Put last to find, and delete last
    u_index[i_find] = u_index[i_last];
    u_value[i_find] = u_value[i_last];
  }

  // Delete pivotal column from UR
  for (HighsInt k = u_start[p_logic]; k < u_last_p[p_logic]; k++) {
    // Find the pivotal position
    HighsInt i_logic = u_pivot_lookup[u_index[k]];
    HighsInt i_find = ur_start[i_logic];
    HighsInt i_last = --ur_lastp[i_logic];
    for (; i_find <= i_last; i_find++)
      if (ur_index[i_find] == iRow) break;
    // Put last to find, and delete last
    ur_space[i_logic]++;
    ur_index[i_find] = ur_index[i_last];
    ur_value[i_find] = ur_value[i_last];
  }

  // Store column to U
  u_start.push_back(u_index.size());
  for (HighsInt i = 0; i < aq->packCount; i++)
    if (aq->packIndex[i] != iRow) {
      u_index.push_back(aq->packIndex[i]);
      u_value.push_back(aq->packValue[i]);
    }
  u_last_p.push_back(u_index.size());
  HighsInt u_startX = u_start.back();
  HighsInt u_endX = u_last_p.back();
  u_total_x += u_endX - u_startX + 1;

  // Store column as UR elements
  for (HighsInt k = u_startX; k < u_endX; k++) {
    // Which ETA file
    HighsInt i_logic = u_pivot_lookup[u_index[k]];

    // Move row to the end if necessary
    if (ur_space[i_logic] == 0) {
      // Make pointers
      HighsInt row_start = ur_start[i_logic];
      HighsInt row_count = ur_lastp[i_logic] - row_start;
      HighsInt new_start = ur_index.size();
      HighsInt new_space = row_count * 1.1 + 5;

      // Check matrix UR
      ur_index.resize(new_start + new_space);
      ur_value.resize(new_start + new_space);

      // Move elements
      HighsInt i_from = row_start;
      HighsInt i_end = row_start + row_count;
      HighsInt i_to = new_start;
      copy(&ur_index[i_from], &ur_index[i_end], &ur_index[i_to]);
      copy(&ur_value[i_from], &ur_value[i_end], &ur_value[i_to]);

      // Save new pointers
      ur_start[i_logic] = new_start;
      ur_lastp[i_logic] = new_start + row_count;
      ur_space[i_logic] = new_space - row_count;
    }

    // Put into the next available space
    ur_space[i_logic]--;
    HighsInt i_put = ur_lastp[i_logic]++;
    ur_index[i_put] = iRow;
    ur_value[i_put] = u_value[k];
  }

  // Store UR pointers
  ur_start.push_back(ur_start[p_logic]);
  ur_lastp.push_back(ur_start[p_logic]);
  ur_space.push_back(ur_space[p_logic] + ur_lastp[p_logic] - ur_start[p_logic]);

  // Update pivot count
  u_pivot_lookup[iRow] = u_pivot_index.size();
  u_pivot_index.push_back(iRow);
  u_pivot_value.push_back(pivot * alpha);

  // Store row_ep as R matrix
  for (HighsInt i = 0; i < ep->packCount; i++) {
    if (ep->packIndex[i] != iRow) {
      pf_index.push_back(ep->packIndex[i]);
      pf_value.push_back(-ep->packValue[i] * pivot);
    }
  }
  u_total_x += pf_index.size() - pf_start.back();

  // Store R matrix pivot
  pf_pivot_index.push_back(iRow);
  pf_start.push_back(pf_index.size());

  // Update total countX
  u_total_x -= u_last_p[p_logic] - u_start[p_logic];
  u_total_x -= ur_lastp[p_logic] - ur_start[p_logic];

  //    // See if we want refactor
  //    if (u_total_x > u_merit_x && pf_pivot_index.size() > 100)
  //        *hint = 1;
}

void HFactor::updatePF(HVector* aq, HighsInt iRow, HighsInt* hint) {
  // Check space
  const HighsInt column_count = aq->packCount;
  const HighsInt* variable_index = &aq->packIndex[0];
  const double* columnArray = &aq->packValue[0];

  // Copy the pivotal column
  for (HighsInt i = 0; i < column_count; i++) {
    HighsInt index = variable_index[i];
    double value = columnArray[i];
    if (index != iRow) {
      pf_index.push_back(index);
      pf_value.push_back(value);
    }
  }

  // Save pivot
  pf_pivot_index.push_back(iRow);
  pf_pivot_value.push_back(aq->array[iRow]);
  pf_start.push_back(pf_index.size());

  // Check refactor
  u_total_x += aq->packCount;
  if (u_total_x > u_merit_x) *hint = 1;
}

void HFactor::updateMPF(HVector* aq, HVector* ep, HighsInt iRow,
                        HighsInt* hint) {
  // Store elements
  for (HighsInt i = 0; i < aq->packCount; i++) {
    pf_index.push_back(aq->packIndex[i]);
    pf_value.push_back(aq->packValue[i]);
  }
  HighsInt p_logic = u_pivot_lookup[iRow];
  HighsInt u_startX = u_start[p_logic];
  HighsInt u_endX = u_start[p_logic + 1];
  for (HighsInt k = u_startX; k < u_endX; k++) {
    pf_index.push_back(u_index[k]);
    pf_value.push_back(-u_value[k]);
  }
  pf_index.push_back(iRow);
  pf_value.push_back(-u_pivot_value[p_logic]);
  pf_start.push_back(pf_index.size());

  for (HighsInt i = 0; i < ep->packCount; i++) {
    pf_index.push_back(ep->packIndex[i]);
    pf_value.push_back(ep->packValue[i]);
  }
  pf_start.push_back(pf_index.size());

  // Store pivot
  pf_pivot_value.push_back(aq->array[iRow]);

  // Refactor or not
  u_total_x += aq->packCount + ep->packCount;
  if (u_total_x > u_merit_x) *hint = 1;
}

void HFactor::updateAPF(HVector* aq, HVector* ep, HighsInt iRow
                        //, HighsInt* hint
) {
  // Store elements
  for (HighsInt i = 0; i < aq->packCount; i++) {
    pf_index.push_back(aq->packIndex[i]);
    pf_value.push_back(aq->packValue[i]);
  }

  HighsInt variable_out = basic_index[iRow];
  if (variable_out >= num_col) {
    pf_index.push_back(variable_out - num_col);
    pf_value.push_back(-1);
  } else {
    for (HighsInt k = a_start[variable_out]; k < a_start[variable_out + 1];
         k++) {
      pf_index.push_back(a_index[k]);
      pf_value.push_back(-a_value[k]);
    }
  }
  pf_start.push_back(pf_index.size());

  for (HighsInt i = 0; i < ep->packCount; i++) {
    pf_index.push_back(ep->packIndex[i]);
    pf_value.push_back(ep->packValue[i]);
  }
  pf_start.push_back(pf_index.size());

  // Store pivot
  pf_pivot_value.push_back(aq->array[iRow]);
}
