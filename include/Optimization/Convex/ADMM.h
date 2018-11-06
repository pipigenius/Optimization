/** This header file provides several lightweight alias templates and template
 * classes implementing the Alternating Direction Method of Multipliers (ADMM)
 * algorithm for solving convex minimization problems of the form:
 *
 * min f(x) + g(y)
 *
 * s.t. Ax + By = c
 *
 * via operator splitting.  This implementation is based upon the one described
 * in Section 3.1 of "Distributed Optimization and Statistical Learning via the
 * Alternating Direction Method of Multipliers", by S. Boyd, N. Parikh, E. Chu,
 * B. Peleato, and J. Eckstein.
 *
 * Copyright (C) 2018 by David M. Rosen (drosen2000@gmail.com)
 */

#pragma once

#include "Optimization/Convex/Concepts.h"
#include "Optimization/Util/Stopwatch.h" // Useful timing functions

#include <algorithm>
#include <cmath>
#include <iostream>

namespace Optimization {
namespace Convex {

/** Alias templates for functions that return the minimizers of the augmented
 * Lagrangian:
 *
 * L_rho(x, y, lambda) := f(x) + g(y) + lambda' * (Ax + By - c)
 *                          + (rho / 2) * |Ax + By - c |_2^2
 *
 * considered as a function of its first and second arguments, respectively.
 */

template <typename VariableX, typename VariableY, typename VariableR,
          typename... Args>
using AugLagMinX =
    std::function<VariableX(const VariableX &x, const VariableY &y,
                            const VariableR &lambda, double rho,
                            Args &... args)>;

template <typename VariableX, typename VariableY, typename VariableR,
          typename... Args>
using AugLagMinY =
    std::function<VariableY(const VariableX &x, const VariableY &y,
                            const VariableR &lambda, double rho,
                            Args &... args)>;

/** A simple enumeration type describing the strategy used to adapt the penalty
 * parameter rho in the augmented Lagrangian */

enum class ADMMPenaltyAdaptation {
  /** Vanilla ADMM: No parameter adaptation */
  None,

  /** Use the primal- and dual-residual balancing approach described in the
     paper "Alternating Direction Method with Self-Adaptive Penalty Parameters",
     by B. He, H. Yang, and S. Wang */
  Residual_Balance,

  /** Use the spectral penalty (Barzilai-Borwein-based) selection method
     described in the paper "Adaptive ADMM with Spectral Penalty Parameter
     Selection", by Z. Xu, M.A.T. Figueiredo, and T. Goldstein.*/
  Spectral
};

struct ADMMParams : public OptimizerParams {

  /// Penalty parameter settings

  /** (Initial) value of penalty parameter rho */
  double rho = 1.0;

  /** Adaptation strategy for penalty parameter */
  ADMMPenaltyAdaptation penalty_adaptation_mode = ADMMPenaltyAdaptation::None;

  /** If penalty_adaptation_mode != None, this parameter controls how frequently
   * (in terms of number of iterations) the penalty parameter is updated */
  unsigned int penalty_adaptation_period = 2;

  /** This value sets an upper limit (in terms of number of iterations) on the
   * window within which the augmented Lagrangian penalty parameter will be
   * adjusted -- this is to ensure that the penalty parameter will eventually be
   * constant, so that the ADMM algorithm is guaranteed to converge */
  unsigned int penalty_adaptation_window = 1000;

  /** If the 'Residual_Balance' adaptation strategy is used, this value sets the
   * threshold for the maximum admissible ratio between the primal and dual
   * residuals before increasing or decreasing the penalty parameter (cf.
   * equation (3.13) of "Distributed Optimization and Statistical Learning via
   * the Alternating Direction Method of Multipliers).  This value should be
   * positive, and greater than 1. */
  double residual_balance_mu = 10;

  /** If the 'Residual_Balance' adaptation strategy is used, this is the
   * multiplicative factor used to increase or decrease the penalty parameter
   * when cf. equation (3.13) of "Distributed Optimization and Statistical
   * Learning via the Alternating Direction Method of Multipliers).  This value
   * should be positive, and greater than 1. */
  double residual_balance_tau = 2;

  /** If the 'Spectral' parameter adaptation strategy is used, this value
   * controls the minimum acceptable quality of the quasi-Newton Hessian
   * approximation (used to compute the spectral stepsize estimate) that must be
   * obtained before the estimate is accepted (cf. eq. (29) in the paper
   * "Adaptive ADMM with Spectral Penalty Parameter Selection", by Z. Xu, M.A.T.
   * Figueiredo, and T. Goldstein).  Valid values of this parameter are in the
   * range (0,1) (with 1 being more stringent).
   */
  double spectral_penalty_minimum_correlation = .2;

  /** Termination criteria:
   *
   * We employ a termination criterion based upon the primal and dual residuals:
   *
   * r_k := Ax + By - c
   * s_k := rho * A^t * B * (y_k - y_{k-1})
   *
   * as suggested in Section 3.3.1 of "Distributed Optimization and Statistical
   * Learning via the Alternating Direction Method of Multipliers"; namely, we
   * define combined (absolute + relative) primal and dual residual stopping
   * tolerances eps_pri_k and eps_dual_k at iteration k according to:
   *
   * eps_pri_k := eps_abs_pri + eps_rel * max { |Ax_k|_2, |By_k|_2, |c|_2 }
   *
   * eps_dual_k := eps_abs_dual+ eps_rel * |A^t lambda_k|_2
   *
   * where eps_abs_pri, eps_abs_dual, and eps_rel are user-supplied tolerances,
   * and terminate if:
   *
   * |r_k|_2 <= eps_pri_k AND |s_k|_2 <= eps_dual_k
   */

  /** Absolute primal stopping tolerance */
  double eps_abs_pri = 1e-2;

  /** Absolute dual stopping tolerance */
  double eps_abs_dual = 1e-2;

  /** Relative stopping tolerance */
  double eps_rel = 1e-3;
};

/** Enum type that describes the termination status of the algorithm */
enum class ADMMStatus {
  /** ADMM algorithm terminated because the residual stopping criteria were
     satisfied */
  RESIDUAL_TOLERANCE,

  /** ADMM algorithm terminated because it exhausted the alloted number of
     iterations before achieving the desired residual tolerances */
  ITERATION_LIMIT,

  /** ADMM algorithm terminated because it exceeded the alloted computation time
     before achieving the desired residual tolerances */
  ELAPSED_TIME
};

/** A useful struct used to hold the output of the ADMM algorithm */
template <typename VariableX, typename VariableY>
struct ADMMResult : OptimizerResult<std::pair<VariableX, VariableY>> {

  /** The stopping condition that triggered algorithm termination */
  ADMMStatus status;

  /** Primal residual at the end of each iteration */
  std::vector<double> primal_residuals;

  /** Dual residual at the end of each iteration */
  std::vector<double> dual_residuals;

  /** The sequence of augmented Lagrangian penalty parameters employed by the
   * algorithm at each iteration.*/
  std::vector<double> penalty_parameters;
};

/** Helper function:  This function implements the residual-balancing updating
 * strategy for the augmented Lagrangian penalty parameter (cf. equation (3.13)
 * of "Distributed Optimization and Statistical Learning via the Alternating
 * Direction Method of Multipliers) */
double residual_balance_penalty_parameter_update(double primal_residual,
                                                 double dual_residual,
                                                 double mu, double tau,
                                                 double rho) {
  if (primal_residual > mu * dual_residual)
    return tau * rho;
  else if (dual_residual > mu * primal_residual)
    return rho / tau;
  else
    return rho;
}

/** Helper function:  This function implements the augmented Lagrangian penalty
 * parameter update rule described in the paper "Adaptive ADMM with Spectral
 * Penalty Parameter Selection", by Z. Xu, M.A.T. Figueiredo, and T. Goldstein
 */

template <typename VariableR, typename... Args>
double spectral_penalty_parameter_update(
    const VariableR &delta_lambda_hat, const VariableR &delta_lambda,
    const VariableR &delta_H_hat, const VariableR &delta_G_hat,
    const InnerProduct<VariableR, Args...> &inner_product, double eps_cor,
    double rho, Args... args) {

  /// First, compute steepest descent and minimum-gradient stepsizes

  // Compute and cache a bunch of pair-wise inner products for computing alphas
  double delta_lambda_hat_delta_lambda_hat =
      inner_product(delta_lambda_hat, delta_lambda_hat, args...);

  double delta_H_hat_delta_lambda_hat =
      inner_product(delta_H_hat, delta_lambda_hat, args...);

  double delta_H_hat_delta_H_hat =
      inner_product(delta_H_hat, delta_H_hat, args...);

  // Compute and cache a bunch of pair-wise inner products for computing betas
  double delta_lambda_delta_lambda =
      inner_product(delta_lambda, delta_lambda, args...);

  double delta_G_hat_delta_lambda =
      inner_product(delta_G_hat, delta_lambda, args...);

  double delta_G_hat_delta_G_hat =
      inner_product(delta_G_hat, delta_G_hat, args...);

  // Compute stepsizes using equation (26) -- (28) from the paper

  /// alphas

  // alpha_SD = <delta_lambda_hat, delta_lambda_hat> /
  //            <delta_H_hat, delta_lambda_hat>
  double alpha_SD =
      delta_lambda_hat_delta_lambda_hat / delta_H_hat_delta_lambda_hat;

  // alpha_MG = <delta_H_hat, delta_lambda_hat> / <delta_H_hat, delta_H_hat>
  double alpha_MG = delta_H_hat_delta_lambda_hat / delta_H_hat_delta_H_hat;

  /// betas

  // beta_SD = <delta_lambda, delta_lambda> / <delta_G_hat, delta_lambda>
  double beta_SD = delta_lambda_delta_lambda / delta_G_hat_delta_lambda;

  // beta_MG = <delta_G_hat, delta_lambda> / <delta_G_hat, delta_G_hat>
  double beta_MG = delta_G_hat_delta_lambda / delta_G_hat_delta_G_hat;

  /// Compute hybrid stepsizes following the recommendation of the paper
  /// "Gradient Methods with Adaptive Step-Sizes", by B. Zhou, L. Gao, and Y.-H.
  /// Dai

  // Equation (27) from "Adaptive ADMM ..."

  double alpha =
      ((2 * alpha_MG) > alpha_SD ? alpha_MG : alpha_SD - (alpha_MG / 2));

  double beta = ((2 * beta_MG) > beta_SD ? beta_MG : beta_SD - (beta_MG / 2));

  // Compute "correlations" (eq. 29)

  double delta_lambda_hat_norm = sqrt(delta_lambda_hat_delta_lambda_hat);

  double alpha_cor = delta_H_hat_delta_lambda_hat /
                     (sqrt(delta_H_hat_delta_H_hat) * delta_lambda_hat_norm);

  double delta_lambda_norm = sqrt(delta_lambda_delta_lambda);

  double beta_cor = delta_G_hat_delta_lambda /
                    (sqrt(delta_G_hat_delta_G_hat) * delta_lambda_norm);

  // Implement safeguarding strategy (eq. (30)) and return

  if ((alpha_cor > eps_cor) && (beta_cor > eps_cor))
    return sqrt(alpha * beta);
  else if ((alpha_cor > eps_cor) && (beta_cor <= eps_cor))
    return alpha;
  else if ((alpha_cor <= eps_cor) && (beta_cor > eps_cor))
    return beta;
  else
    return rho;
}

template <typename VariableX, typename VariableY, typename VariableR,
          typename... Args>
ADMMResult<VariableX, VariableY>
ADMM(const AugLagMinX<VariableX, VariableY, VariableR, Args...> &minLx,
     const AugLagMinY<VariableX, VariableY, VariableR, Args...> &minLy,
     const LinearOperator<VariableX, VariableR, Args...> &A,
     const LinearOperator<VariableY, VariableR, Args...> &B,
     const LinearOperator<VariableR, VariableX, Args...> &At,
     const InnerProduct<VariableX, Args...> inner_product_x,
     const InnerProduct<VariableR, Args...> inner_product_r, const VariableR &c,
     const VariableX &x0, const VariableY &y0, Args... args,
     const ADMMParams &params = ADMMParams()) {

  /// Declare some useful variables

  /// Iterates required by main ADMM loop

  // Current iterates
  VariableX x;

  VariableY y;

  VariableR lambda;

  // Previous iterate of y (needed for the dual residual computation)

  VariableX y_prev;

  // Current value of augmented Lagrangian penalty parameter
  double rho;

  // Primal residual vector
  VariableR r;

  // Dual residual vector
  VariableX s;

  // Primal and dual residuals
  double primal_residual, dual_residual;

  // Cache variables for intermediate products Ax, By
  VariableR Ax, By;

  double c_norm = sqrt(inner_product_r(c, c, args...));

  /// Extra variables required for the spectral penalty parameter estimation
  /// procedure

  VariableR lambda_hat;

  VariableX x_k0;
  VariableY y_k0;
  VariableR lambda_k0, lambda_hat_k0;

  /// Output struct
  ADMMResult<VariableX, VariableY> result;
  result.status = ADMMStatus::ITERATION_LIMIT;

  /// INITIALIZATION

  x = x0;
  y = y0;
  y_prev = y0;
  rho = params.rho;
  // Compute initial value of dual variable lambda
  lambda = rho * (A(x) + B(y) - c);

  if (params.penalty_adaptation_mode == ADMMPenaltyAdaptation::Spectral) {
    // Additional initializations for spectral penalty adaptation procedure
    x_k0 = x;
    y_k0 = y;
    lambda_k0 = lambda;
    lambda_hat_k0 = lambda;
  }

  if (params.verbose) {
    std::cout << std::scientific;
    std::cout.precision(params.precision);
  }
  // Field width for displaying outer iterations
  unsigned int iter_field_width = floor(log10(params.max_iterations)) + 1;

  if (params.verbose)
    std::cout << "ADMM optimization: " << std::endl << std::endl;

  /// ITERATE!
  auto start_time = Stopwatch::tick();
  for (unsigned int i = 0; i < params.max_iterations; ++i) {

    // Record the elapsed time at the START of this iteration
    double elapsed_time = Stopwatch::tock(start_time);

    // Test elapsed-time-based stopping criterion
    if (elapsed_time > params.max_computation_time) {
      result.status = ADMMStatus::ELAPSED_TIME;
      break;
    }

    /// ADMM ITERATION
    // Update x by minimizing augmented Lagrangian with respect to x
    x = minLx(x, y, lambda, rho, args...);

    // Update y by minimizing augmented Lagrangian with respect to y
    y = minLy(x, y, lambda, rho, args...);

    // Compute primal residual vector
    Ax = A(x);
    By = B(y);
    r = Ax + By - c;

    if ((params.penalty_adaptation_mode == ADMMPenaltyAdaptation::Spectral) &&
        ((i % params.penalty_adaptation_period) == 0) &&
        (i < params.penalty_adaptation_window)) {
      // Compute lambda_hat *before* updating lambda
      lambda_hat = lambda + rho * (Ax + B(y_prev) - c);
    }

    // Update dual variable lambda
    lambda = lambda + rho * r;

    // Compute dual residual vector
    s = rho * At(B(y - y_prev, args...), args...);

    // Compute primal and dual residual norms
    primal_residual = sqrt(inner_product_r(r, r, args...));
    dual_residual = sqrt(inner_product_x(s, s, args...));

    /// Display output for this iteration

    // Display information about this iteration, if requested
    if (params.verbose) {
      std::cout << "Iter: ";
      std::cout.width(iter_field_width);
      std::cout << i << ", time: " << elapsed_time << ", primal residual: ";
      std::cout.width(params.precision + 7);
      std::cout << primal_residual << ", dual residual:";
      std::cout.width(params.precision + 7);
      std::cout << dual_residual << ", penalty:";
      std::cout.width(params.precision + 7);
      std::cout << rho << std::endl;
    }

    /// Record output
    result.time.push_back(elapsed_time);
    result.primal_residuals.push_back(primal_residual);
    result.dual_residuals.push_back(dual_residual);
    result.penalty_parameters.push_back(rho);

    if (params.log_iterates)
      result.iterates.emplace_back(x, y);

    /// TEST STOPPING CRITERIA

    /// Compute primal and dual stopping tolerances
    // Primal stopping tolerance
    double Ax_norm = sqrt(inner_product_r(Ax, Ax, args...));
    double By_norm = sqrt(inner_product_r(By, By, args...));
    double eps_primal =
        params.eps_abs_pri +
        params.eps_rel * std::max<double>({Ax_norm, By_norm, c_norm});

    // Dual stopping tolerance
    const VariableX At_lambda = At(lambda, args...);
    double At_lambda_norm =
        sqrt(inner_product_x(At_lambda, At_lambda, args...));
    double eps_dual = params.eps_abs_dual + params.eps_rel * At_lambda_norm;

    // Test residual-based stopping criterion
    if ((primal_residual < eps_primal) && (dual_residual < eps_dual)) {
      result.status = ADMMStatus::RESIDUAL_TOLERANCE;
      break;
    }

    /// PENALTY PARAMETER UPDATE

    if ((params.penalty_adaptation_mode != ADMMPenaltyAdaptation::None) &&
        ((i % params.penalty_adaptation_period) == 0) &&
        (i < params.penalty_adaptation_window)) {
      // Update penalty parameter

      /// Residual balancing
      if (params.penalty_adaptation_mode ==
          ADMMPenaltyAdaptation::Residual_Balance) {
        rho = residual_balance_penalty_parameter_update(
            primal_residual, dual_residual, params.residual_balance_mu,
            params.residual_balance_tau, rho);
      }

      /// Spectral parameter update
      if (params.penalty_adaptation_mode == ADMMPenaltyAdaptation::Spectral) {

        // delta lambda
        VariableR delta_lambda = lambda - lambda_k0;
        VariableR delta_lambda_hat = lambda_hat - lambda_hat_k0;

        // NB:  The augmented Lagrangian used in "Adaptive ADMM with Spectral
        // Penalty Parameter Selection" negates the signs of the residuals
        // used in their version of the augmented Lagrangian relative to ours;
        // therefore, we must use a negative sign in front of our linear
        // operators
        VariableR delta_H = -A(x - x_k0, args...);
        VariableR delta_G = -B(y - y_k0, args...);

        // Update rho
        rho = spectral_penalty_parameter_update<VariableR, Args...>(
            delta_lambda_hat, delta_lambda, delta_H, delta_G, inner_product_r,
            params.spectral_penalty_minimum_correlation, rho, args...);

        /// Cache these values
        x_k0 = x;
        y_k0 = y;
        lambda_k0 = lambda;
        lambda_hat_k0 = lambda_hat;
      } // spectral penalty parameter update
    }   // penalty parameter update

    /// CACHE PARAMETERS AND PREPARE FOR NEXT ITERATION

    y_prev = y;

  } // ADMM ITERATIONS

  /// RECORD FINAL OUTPUT
  result.x = {x, y};
  result.elapsed_time = Stopwatch::tock(start_time);

  /// Print final output, if requested
  if (params.verbose) {
    std::cout << std::endl << "Optimization finished!" << std::endl;

    // Print the reason for termination
    switch (result.status) {
    case ADMMStatus::RESIDUAL_TOLERANCE:
      std::cout << "Found minimizer!" << std::endl;
      break;
    case ADMMStatus::ITERATION_LIMIT:
      std::cout << "Algorithm exceeded maximum number of outer iterations"
                << std::endl;
      break;
    case ADMMStatus::ELAPSED_TIME:
      std::cout << "Algorithm exceeded maximum allowed computation time: "
                << result.elapsed_time << " > " << params.max_computation_time
                << std::endl;
      break;
    }

    std::cout << std::endl
              << "Final primal residual: " << primal_residual
              << ", final dual residual: " << dual_residual
              << ", total elapsed computation time: " << result.elapsed_time
              << " seconds" << std::endl;
  } // if(verbose)

  return result;
}

/** This next function provides a convenient specialization of the ADMM
 * interface for the (common) use case in which a single data type is used to
 * represent each variable */

template <typename Variable, typename... Args>
ADMMResult<Variable, Variable>
ADMM(const AugLagMinX<Variable, Variable, Variable, Args...> &minLx,
     const AugLagMinY<Variable, Variable, Variable, Args...> &minLy,
     const LinearOperator<Variable, Variable, Args...> &A,
     const LinearOperator<Variable, Variable, Args...> &B,
     const LinearOperator<Variable, Variable, Args...> &At,
     const InnerProduct<Variable, Args...> inner_product, const Variable &c,
     const Variable &x0, const Variable &y0, Args... args,
     const ADMMParams &params = ADMMParams()) {
  return ADMM<Variable, Variable, Variable, Args...>(
      minLx, minLy, A, B, At, inner_product, inner_product, c, x0, y0, args...,
      params);
}

} // namespace Convex
} // namespace Optimization