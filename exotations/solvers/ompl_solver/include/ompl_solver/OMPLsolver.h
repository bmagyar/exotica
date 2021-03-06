/*
 *  Created on: 19 Jun 2014
 *      Author: Vladimir Ivan
 * 
 * Copyright (c) 2016, University Of Edinburgh 
 * All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met: 
 * 
 *  * Redistributions of source code must retain the above copyright notice, 
 *    this list of conditions and the following disclaimer. 
 *  * Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 *  * Neither the name of  nor the names of its contributors may be used to 
 *    endorse or promote products derived from this software without specific 
 *    prior written permission. 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 * POSSIBILITY OF SUCH DAMAGE. 
 *
 */

#ifndef OMPLSOLVER_H_
#define OMPLSOLVER_H_

#include <exotica/MotionSolver.h>
#include "ompl_solver/OMPLProblem.h"
#include "ompl_solver/OMPLStateSpace.h"
#include "ompl_solver/OMPLSE3RNCompoundStateSpace.h"
#include "ompl_solver/common.h"
#include "ompl_solver/OMPLStateValidityChecker.h"
#include "ompl_solver/OMPLGoalState.h"
#include "projections/OMPLProjection.h"
#include "projections/DMeshProjection.h"
#include <ompl/geometric/PathSimplifier.h>
#include <ompl/base/DiscreteMotionValidator.h>
#include <boost/thread/mutex.hpp>
#include <boost/shared_ptr.hpp>
#include <ros/package.h>
#include <fstream>
namespace exotica
{

  class OMPLsolver: public MotionSolver, public boost::enable_shared_from_this<
      OMPLsolver>
  {
    public:
      OMPLsolver();
      virtual
      ~OMPLsolver();

      /**
       * \brief Binds the solver to a specific problem which must be pre-initalised
       * @param pointer Shared pointer to the motion planning problem
       * @return        Successful if the problem is a valid AICOProblem
       */
      virtual EReturn specifyProblem(PlanningProblem_ptr pointer);
      virtual EReturn specifyProblem(PlanningProblem_ptr goals,
          PlanningProblem_ptr costs, PlanningProblem_ptr goalBias,
          PlanningProblem_ptr samplingBias);

      /*
       * \brief	Check if a problem is solvable by this solver (Pure Virtual)
       * @param	prob		Planning problem
       * @return	True if solvable, false otherwise
       */
      virtual bool isSolvable(const PlanningProblem_ptr & prob);

      std::vector<std::string> getPlannerNames();

      /**
       * \brief Solves the problem
       * @param q0 Start state.
       * @param solution This will be filled with the solution in joint space.
       * @return SUCESS if solution has been found, corresponding error code if not.
       */
      EReturn Solve(Eigen::VectorXdRefConst q0, Eigen::MatrixXd & solution);

      /**
       * \brief Terminates planning
       */
      bool terminate();

      const boost::shared_ptr<ob::StateSpace> getOMPLStateSpace() const
      {
        return state_space_;
      }

      const boost::shared_ptr<og::SimpleSetup> getOMPLSimpleSetup() const
      {
        return ompl_simple_setup_;
      }

      const std::string getAlgorithm()
      {
        return selected_planner_;
      }

      const OMPLProblem_ptr getProblem() const
      {
        return prob_;
      }

      bool getFinishedSolving();

      int getGoalMaxAttempts();

      void setMaxPlanningTime(double t);

      EReturn resetIfNeeded();
      ros::Duration planning_time_;

      virtual std::string print(std::string prepend);

      void getOriginalSolution(Eigen::MatrixXd & orig);

      EReturn setGoalState(const Eigen::VectorXd & qT, const double eps =
          std::numeric_limits<double>::epsilon());
    protected:
      /**
       * \brief Registers default planners
       */
      void registerDefaultPlanners();

    private:
      /**
       * \brief Derived-elements initialiser: Pure Virtual
       * @param handle XMLHandle to the Solver element
       * @return       Should indicate success or otherwise
       */
      virtual EReturn initDerived(tinyxml2::XMLHandle & handle);

      /**
       * \biref Registers a planner allocator
       * @param planner_id Planner name
       * @param pa Planner allocation function
       */
      void registerPlannerAllocator(const std::string &planner_id,
          const ConfiguredPlannerAllocator &pa)
      {
        known_planners_[planner_id] = pa;
      }

      /**
       * \biref Converts OMPL trajectory into Eigen Matrix
       * @param pg OMPL trajectory
       * @param traj Eigen Matrix trajectory
       * @return Indicates success
       */
      EReturn convertPath(const ompl::geometric::PathGeometric &pg,
          Eigen::MatrixXd & traj);

      EReturn getSimplifiedPath(ompl::geometric::PathGeometric &pg,
          Eigen::MatrixXd & traj, ob::PlannerTerminationCondition &ptc);
      /**
       * \brief Registers trajectory termination condition
       * @param ptc Termination criteria
       */
      void registerTerminationCondition(
          const ob::PlannerTerminationCondition &ptc);

      /**
       * \brief Unregisters trajectory termination condition
       */
      void unregisterTerminationCondition();

      /**
       * \brief Starts goal sampling
       */
      void startSampling();
      /**
       * \brief Stops goal sampling
       */
      void stopSampling();
      /**
       * \brief Executes pre solve steps
       */
      void preSolve();
      /**
       * \brief Executes post solve steps
       */
      void postSolve();

      /**
       * Constructs goal representation
       */
      ompl::base::GoalPtr constructGoal();

      /**
       * \brief	Record data for analysis
       */
      void recordData();

      bool isFlexiblePlanner();

      OMPLProblem_ptr prob_; //!< Shared pointer to the planning problem.
      OMPLProblem_ptr costs_; //!< Shared pointer to the planning problem.
      OMPLProblem_ptr goalBias_; //!< Shared pointer to the planning problem.
      OMPLProblem_ptr samplingBias_; //!< Shared pointer to the planning problem.

      /// \brief List of all known OMPL planners
      std::map<std::string, ConfiguredPlannerAllocator> known_planners_;

      /// The OMPL planning context; this contains the problem definition and the planner used
      boost::shared_ptr<og::SimpleSetup> ompl_simple_setup_;

      /// \brief OMPL state space specification
      boost::shared_ptr<ob::StateSpace> state_space_;
      bool compound_;

      /// \brief Planner name
      std::string selected_planner_;

      /// \brief Maximum allowed time for planning
      double timeout_;

      /// \brief Planner termination criteria
      const ob::PlannerTerminationCondition *ptc_;
      /// \brief Planner termination criteria mutex
      boost::mutex ptc_lock_;

      /// \bierf True when the solver finished its work and background threads need to finish.
      bool finishedSolving_;

      // \brief Max number of attempts at sampling the goal region
      int goal_ampling_max_attempts_;

      /// \brief	Indicate if trajectory smoother is required
      EParam<std_msgs::Bool> smooth_;

      ///	\brief	Maximum step
      EParam<std_msgs::String> range_;

      ///	\brief	View original solution before trajectory smoothness
      Eigen::MatrixXd original_solution_;

      EParam<std_msgs::Bool> saveResults_;

      ///	\brief	Porjection type
      std::string projector_;
      ///	\brief	Projection components
      std::vector<std::string> projection_components_;

      ///	\brief	Threads locker
      boost::mutex goal_lock_;

      std::ofstream result_file_;

      boost::shared_ptr<OMPLGoalState> goal_state_;
      int succ_cnt_;
  };

  typedef boost::shared_ptr<exotica::OMPLsolver> OMPLsolver_ptr;

} /* namespace exotica */

#endif /* OMPLSOLVER_H_ */
