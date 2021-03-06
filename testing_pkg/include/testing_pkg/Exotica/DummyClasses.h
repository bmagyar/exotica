/*
 *      Author: Michael Camilleri
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

#ifndef TESTING_DUMMY_CLASSES_H
#define TESTING_DUMMY_CLASSES_H

#include <exotica/EXOTica.hpp>

/** Type definitions first **/
namespace testing
{
  class DTaskMap: public exotica::TaskMap //!< A concrete implementation of the TaskMap class
  {
    public:
      DTaskMap()
          : scene_ptr_(scene_)  //!< Default constructor
      {
        derived_called = false;
        update_called = false;
      }
      ;

      void clearFlags()
      {
        derived_called = update_called = false;
      }

      virtual exotica::EReturn update(Eigen::VectorXdRefConst x)
      {
        LOCK(scene_lock_);

        update_called = true; //!< Indicate entry into function
        if (x.size() != 3)
        {
          return exotica::PAR_ERR;
        }
        Eigen::VectorXd phi(1);
        phi(0) = std::pow(x[0], 3) + 2 * std::pow(x[1], 2) - std::pow(x[2], 4);
        exotica::EReturn temp_return = setPhi(phi);
        if (temp_return)
        {
          return temp_return;
        }
        Eigen::MatrixXd jacobian(1, 3);
        jacobian(0, 0) = 3 * std::pow(x[0], 2);
        jacobian(0, 1) = 4 * x[1];
        jacobian(0, 2) = -4 * std::pow(x[2], 3);
        temp_return = setJacobian(jacobian);
        return temp_return;
      }

      bool derived_called;
      bool update_called;

      virtual exotica::EReturn taskSpaceDim(int & task_dim)
      {
        task_dim = -1;
        return derived_called ? exotica::SUCCESS : exotica::MMB_NIN;
      }

      exotica::KinematicScene_ptr & scene_ptr_;
    protected:
      virtual exotica::EReturn initDerived(tinyxml2::XMLHandle & handle)
      {
        derived_called = true;
        return exotica::SUCCESS;
      }
  };

  class DTaskDefinition: public exotica::TaskDefinition
  {
    public:
      DTaskDefinition()
      {
        derived_called = false;
      }

      bool derived_called;

      void clearFlags()
      {
        derived_called = false;
      }
    protected:
      exotica::EReturn initDerived(tinyxml2::XMLHandle & handle)
      {
        derived_called = true;
        return exotica::SUCCESS;
      }
  };

  class DPlanningProblem: public exotica::PlanningProblem
  {
    public:
      DPlanningProblem()
          : scenes_ref_(k_scenes_), maps_ref_(task_maps_), defs_ref_(task_defs_)
      {
        derived_called = false;
      }

      bool derived_called;

      exotica::KinematicScene_map & scenes_ref_; //!< Kinematic scene(s) indexed by name
      exotica::TaskMap_map & maps_ref_; //!< The set of taskmaps we will be using, which will be shared between task-definitions
      exotica::TaskDefinition_map & defs_ref_; //!< The set of task definition objects

      void refresh()
      {
        k_scenes_.clear();
        task_maps_.clear();
        task_defs_.clear();
        derived_called = false;
      }

      /** Returns true if the entire class is invalidated and false otherwise **/

      bool checkInvalid()
      {
        if (k_scenes_.size()) return false;
        if (task_maps_.size()) return false;
        if (task_defs_.size()) return false;
        if (derived_called) return false;
        return true;  //!< If passed all tests...
      }

    protected:
      exotica::EReturn initDerived(tinyxml2::XMLHandle & handle)
      {
        derived_called = true;
        return exotica::SUCCESS;
      }

  };

  class DMotionSolver: public exotica::MotionSolver
  {
    public:

      DMotionSolver()
      {
        refresh();
      }

      bool derived_called;

      void refresh()
      {
        derived_called = false;
      }

    protected:
      exotica::EReturn initDerived(tinyxml2::XMLHandle & handle)
      {
        derived_called = true;
        return exotica::SUCCESS;
      }

  };

}

REGISTER_PROBLEM_TYPE("DPlanningProblem", testing::DPlanningProblem)
REGISTER_TASKDEFINITION_TYPE("DTaskDefinition", testing::DTaskDefinition)
REGISTER_TASKMAP_TYPE("DTaskMap", testing::DTaskMap); //!< Register the class

REGISTER_FOR_XML_TEST("DTaskMap", "DTaskMap.xml");    //!< Register for testing

#endif
