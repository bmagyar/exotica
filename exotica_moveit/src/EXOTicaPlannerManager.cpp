/*
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

#include "exotica_moveit/EXOTicaPlannerManager.h"

namespace exotica
{

  EXOTicaPlannerManager::EXOTicaPlannerManager()
      : planning_interface::PlannerManager(), nh_("~")
  {
  }

  EXOTicaPlannerManager::~EXOTicaPlannerManager()
  {
    // TODO Auto-generated destructor stub
  }

  bool EXOTicaPlannerManager::initialize(
      const robot_model::RobotModelConstPtr& model, const std::string &ns)
  {
    Initialiser ini;

    std::string filename;
    nh_.getParam("/EXOTica/exotica_config", filename);
    ROS_ERROR_STREAM("Exotica config file: "<<filename);
    if (filename.length() == 0)
    {
      ROS_ERROR_STREAM("Can't load exotica config file!!");
      return false;
    }

    ini.listSolversAndProblems(filename, problems_, solvers_);
    return true;
  }

  bool EXOTicaPlannerManager::canServiceRequest(
      const planning_interface::MotionPlanRequest& req) const
  {
    // TODO: this is a dummy implementation
    //      capabilities.dummy = false;
    return true;
  }

  planning_interface::PlanningContextPtr EXOTicaPlannerManager::getPlanningContext(
      const planning_scene::PlanningSceneConstPtr& planning_scene,
      const planning_interface::MotionPlanRequest &req,
      moveit_msgs::MoveItErrorCodes &error_code) const
  {
    planning_interface::PlanningContextPtr context_;
    if (req.group_name.empty())
    {
      logError("No group specified to plan for");
      error_code.val = moveit_msgs::MoveItErrorCodes::INVALID_GROUP_NAME;
      context_.reset();
      return context_;
    }

    error_code.val = moveit_msgs::MoveItErrorCodes::FAILURE;

    if (!planning_scene)
    {
      logError("No planning scene supplied as input");
      context_.reset();
      return context_;
    }

    {
      std::string problem_name;
      std::string solver_name;

      bool found = false;
      for (std::string s : solvers_)
      {
        for (std::string p : problems_)
        {
          if (req.planner_id.compare(s + " - " + p) == 0)
          {
            found = true;
            problem_name = p;
            solver_name = s;
            break;
          }
        }
      }
      if (!found)
      {
        logError("Problem or solver not found!");
        context_.reset();
        return context_;
      }

      const robot_model::JointModelGroup* model_group =
          planning_scene->getRobotModel()->getJointModelGroup(req.group_name);
      logDebug("Creating new planning context");
      context_.reset(
          new EXOTicaPlanningContext("EXOTICA", model_group->getName(),
              planning_scene->getRobotModel(), problem_name, solver_name));
    }
    if (context_)
    {
      context_->clear();
      robot_state::RobotStatePtr start_state =
          planning_scene->getCurrentStateUpdated(req.start_state);
      // Setup the context
      context_->setPlanningScene(planning_scene);
      context_->setMotionPlanRequest(req);
      boost::static_pointer_cast<EXOTicaPlanningContext>(context_)->setCompleteInitialState(
          *start_state);

      if (boost::static_pointer_cast<EXOTicaPlanningContext>(context_)->configure(
          planning_scene))
      {
        logDebug("%s: New planning context is set.",
            context_->getName().c_str());
        error_code.val = moveit_msgs::MoveItErrorCodes::SUCCESS;
      }
      else
      {
        logError("EXOTica encountered an error!");
        context_.reset();
      }
    }

    return context_;
  }

  void EXOTicaPlanningContext::setCompleteInitialState(
      const robot_state::RobotState &complete_initial_robot_state)
  {
    start_state_ = complete_initial_robot_state;
  }

  std::string EXOTicaPlannerManager::getDescription() const
  {
    return "EXOTica";
  }

  void EXOTicaPlannerManager::getPlanningAlgorithms(
      std::vector<std::string> &algs) const
  {
    algs.clear();
    for (std::string s : solvers_)
    {
      for (std::string p : problems_)
      {
        algs.push_back(s + " - " + p);
      }
    }
  }

  EXOTicaPlanningContext::EXOTicaPlanningContext(const std::string &name,
      const std::string &group, const robot_model::RobotModelConstPtr& model,
      const std::string &problem_name, const std::string &solver_name)
      : planning_interface::PlanningContext(name, group), start_state_(model), goal_state_(
          model), tau_(0.0), nh_("~"), problem_name_(problem_name), solver_name_(
          solver_name), client_("/ExoticaPlanning", true)
  {
  }

  bool EXOTicaPlanningContext::configure(
      const planning_scene::PlanningSceneConstPtr & scene)
  {
    Initialiser ini;

    std::string filename;
    nh_.getParam("/EXOTica/exotica_config", filename);
    if (filename.length() == 0)
    {
      ROS_ERROR_STREAM("Can't load exotica config file!");
      return false;
    }
    config_file_ = filename;
    if (client_.waitForServer(ros::Duration(5)))
    {
      return true;
    }
    else
    {
      ROS_ERROR("Can not connect to EXOTica Planning Action server");
      return false;
    }
  }

  /** \brief Solve the motion planning problem and store the result in \e res. This function should not clear data structures before computing. The constructor and clear() do that. */
  bool EXOTicaPlanningContext::solve(
      planning_interface::MotionPlanResponse &res)
  {
    ros::WallTime start_time = ros::WallTime::now();
    bool fullbody = false;
    nh_.getParam("/EXOTica/fullbody", fullbody);
    const moveit::core::JointModelGroup* model_group =
        planning_scene_->getRobotModel()->getJointModelGroup(
            request_.group_name);
    ROS_ERROR_STREAM("Move group: '"<< model_group->getName() <<"'");
    std::vector<std::string> names = model_group->getVariableNames();

    for (int i = 0; i < names.size(); i++)
      goal_state_.setVariablePosition(
          getMotionPlanRequest().goal_constraints[0].joint_constraints[i].joint_name,
          getMotionPlanRequest().goal_constraints[0].joint_constraints[i].position);
    goal_state_.update(true);
    used_names_ = names;
    Eigen::VectorXd q0, qT;
    std::string tmp_name = "";
    if (fullbody)
    {
      int size = names.size() - 1;
      q0.setZero(size);
      qT.setZero(size);
      for (int i = 0; i < 3; i++)
      {
        q0(i) = start_state_.getVariablePosition(names[i]);
        qT(i) = goal_state_.getVariablePosition(names[i]);
      }
      KDL::Rotation rot_s = KDL::Rotation::Quaternion(
          start_state_.getVariablePosition(names[3]),
          start_state_.getVariablePosition(names[4]),
          start_state_.getVariablePosition(names[5]),
          start_state_.getVariablePosition(names[6]));
      KDL::Rotation rot_g = KDL::Rotation::Quaternion(
          goal_state_.getVariablePosition(names[3]),
          goal_state_.getVariablePosition(names[4]),
          goal_state_.getVariablePosition(names[5]),
          goal_state_.getVariablePosition(names[6]));
      rot_s.GetEulerZYX(q0(3), q0(4), q0(5));
      rot_g.GetEulerZYX(qT(3), qT(4), qT(5));
      for (int i = 6; i < size; i++)
      {
        q0(i) = start_state_.getVariablePosition(names[i + 1]);
        tmp_name = tmp_name + " " + names[i];
        qT(i) = goal_state_.getVariablePosition(names[i + 1]);
      }
    }
    else
    {
      int size = names.size();
      q0.setZero(size);
      qT.setZero(size);
      for (int i = 0; i < size; i++)
      {
        q0(i) = start_state_.getVariablePosition(names[i]);
        tmp_name = tmp_name + " " + names[i];
        qT(i) = goal_state_.getVariablePosition(names[i]);
      }
    }

    ROS_ERROR_STREAM("Joints="<<tmp_name);
    ROS_ERROR_STREAM("q0="<<q0.transpose());
    ROS_ERROR_STREAM("qT="<<qT.transpose());

    Eigen::MatrixXd solution;
    bool found_solution = false;

    exotica_moveit::ExoticaPlanningGoal goal;
    vectorEigenToExotica(q0, goal.q0);
    vectorEigenToExotica(qT, goal.qT);
    goal.xml_file_ = config_file_;
    moveit_msgs::PlanningScene tmp;
    planning_scene_->getPlanningSceneMsg(tmp);
    goal.scene_ = tmp;
    goal.group_name_ = request_.group_name;
    goal.max_time_ = getMotionPlanRequest().allowed_planning_time;
    goal.problem_ = problem_name_;
    goal.solver_ = solver_name_;
    client_.sendGoal(goal);
    bool finished_before_timeout = client_.waitForResult(
        ros::Duration(goal.max_time_));
    if (finished_before_timeout)
    {
      if (client_.getResult()->succeeded_)
      {
        if (ok(matrixExoticaToEigen(client_.getResult()->solution_, solution)))
        {
          res.trajectory_.reset(
              new robot_trajectory::RobotTrajectory(
                  planning_scene_->getRobotModel(), model_group->getName()));
          res.planning_time_ = client_.getResult()->planning_time_;
          copySolution(solution, res.trajectory_.get());
          solution_ = solution;
          return true;
        }
        else
        {
          INDICATE_FAILURE
          return false;
        }
      }
    }
    else
    {
      std::cout << "Result " << client_.getResult()->planning_time_
          << std::endl;
      ROS_ERROR("Calling Exotica Planning service failed");
    }
    res.error_code_.val = moveit_msgs::MoveItErrorCodes::PLANNING_FAILED;
    return false;
  }

  /** \brief Solve the motion planning problem and store the detailed result in \e res. This function should not clear data structures before computing. The constructor and clear() do that. */
  bool EXOTicaPlanningContext::solve(
      planning_interface::MotionPlanDetailedResponse &res)
  {
    planning_interface::MotionPlanResponse res2;
    const moveit::core::JointModelGroup* model_group =
        planning_scene_->getRobotModel()->getJointModelGroup(
            request_.group_name);
    if (solve(res2))
    {
      res.trajectory_.reserve(1);
      res.trajectory_.resize(res.trajectory_.size() + 1);
      res.trajectory_.back().reset(
          new robot_trajectory::RobotTrajectory(
              planning_scene_->getRobotModel(), model_group->getName()));
      copySolution(solution_, res.trajectory_.back().get());
      res.description_.push_back("plan");
      res.processing_time_.push_back(res2.planning_time_);
      res.error_code_ = res2.error_code_;
      return true;
    }
    else
    {
      return false;
    }
  }

  void EXOTicaPlanningContext::copySolution(
      const Eigen::Ref<const Eigen::MatrixXd> & solution,
      robot_trajectory::RobotTrajectory* traj)
  {
    const moveit::core::JointModelGroup* model_group =
        planning_scene_->getRobotModel()->getJointModelGroup(
            request_.group_name);
    traj->clear();
    moveit::core::RobotState state = start_state_;
    bool fullbody = false;
    nh_.getParam("/EXOTica/fullbody", fullbody);
    for (int t = 0; t < solution.rows(); t++)
    {
      if (fullbody)
      {
        for (int i = 0; i < 3; i++)
          state.setVariablePosition(used_names_[i], solution(t, i));
        KDL::Rotation rot = KDL::Rotation::EulerZYX(solution(t, 3),
            solution(t, 4), solution(t, 5));
        Eigen::VectorXd quat(4);
        rot.GetQuaternion(quat(0), quat(1), quat(2), quat(3));
        state.setVariablePosition(used_names_[3], quat(0));
        state.setVariablePosition(used_names_[4], quat(1));
        state.setVariablePosition(used_names_[5], quat(2));
        state.setVariablePosition(used_names_[6], quat(3));
        for (int i = 7; i < used_names_.size(); i++)
          state.setVariablePosition(used_names_[i], solution(t, i - 1));
      }
      else
      {
        for (int i = 0; i < used_names_.size(); i++)
          state.setVariablePosition(used_names_[i], solution(t, i));
      }
      state.update();
      traj->addSuffixWayPoint(state, tau_);
    }
  }

  /** \brief If solve() is running, terminate the computation. Return false if termination not possible. No-op if solve() is not running (returns true).*/
  bool EXOTicaPlanningContext::terminate()
  {
    //TODO - make interruptible
    ROS_WARN_STREAM("Attempting to terminate");
    return true;
  }

  /** \brief Clear the data structures used by the planner */
  void EXOTicaPlanningContext::clear()
  {
    sol.reset();
  }

} /* namespace exotica */

CLASS_LOADER_REGISTER_CLASS(exotica::EXOTicaPlannerManager,
    planning_interface::PlannerManager);
