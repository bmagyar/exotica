/*
 *  Created on: 22 Jun 2015
 *      Author: Yiming Yang
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

#include <ompl_solver/OMPLSE3RNCompoundStateSpace.h>
namespace exotica
{

  OMPLSE3RNCompoundStateSpace::OMPLSE3RNCompoundStateSpace(unsigned int dim,
      const Server_ptr &server, OMPLProblem_ptr &prob)
      : ob::CompoundStateSpace(), realvectordim_(dim), server_(server), prob_(
          prob), SO3Bounds_(3), useGoal_(false)
  {
    setName("OMPLSE3RNCompoundStateSpace");
    addSubspace(ob::StateSpacePtr(new ob::SE3StateSpace()), 10.0);
    addSubspace(ob::StateSpacePtr(new ob::RealVectorStateSpace(dim)), 1.0);
    weights_.reset(new Vector());
    weights_->data.resize(dim + 6);
    for (int i = 0; i < dim + 6; i++)
      weights_->data[i] = 1;
    if (server_->hasParam(server_->getName() + "/SE3RNSpaceWeights"))
    {
      EParam<exotica::Vector> tmp;
      if (ok(server_->getParam(server_->getName() + "/SE3RNSpaceWeights", tmp)))
      {
        if (tmp->data.size() == dim + 6)
        {
          weights_ = tmp;
        }
      }
    }
    if (server_->hasParam(server_->getName() + "/SE3RNSpaceRNBias")
        && ok(
            server_->getParam(server_->getName() + "/SE3RNSpaceRNBias",
                rn_bias_percentage_)))
    {
      useGoal_ = true;
    }

    lock();
  }

  OMPLSE3RNCompoundStateSpace::~OMPLSE3RNCompoundStateSpace()
  {

  }

  ob::StateSamplerPtr OMPLSE3RNCompoundStateSpace::allocDefaultStateSampler()
  {
    OMPLSE3RNCompoundStateSampler *ss = new OMPLSE3RNCompoundStateSampler(this);
    return ob::StateSamplerPtr(ss);
  }

  boost::shared_ptr<OMPLSE3RNCompoundStateSpace> OMPLSE3RNCompoundStateSpace::FromProblem(
      OMPLProblem_ptr prob, const Server_ptr &server)
  {
    unsigned int n = prob->getSpaceDim();
    boost::shared_ptr<OMPLSE3RNCompoundStateSpace> ret;
    BASE_TYPE base_type = prob->getScenes().begin()->second->getBaseType();

    int rn = n - 6;
    if (rn <= 0)
    {
      ERROR("State space size error!");
      return ret;
    }

    ret.reset(new OMPLSE3RNCompoundStateSpace(rn, server, prob));
    if (prob->getBounds().size() == 2 * n)
    {
      ompl::base::RealVectorBounds RNbounds(rn);
      ompl::base::RealVectorBounds SE3bounds(3);

      for (int i = 0; i < 3; i++)
      {
        SE3bounds.setHigh(i, prob->getBounds()[i + n]);
        SE3bounds.setLow(i, prob->getBounds()[i]);
      }
      ret->SO3Bounds_.resize(3);
      for (int i = 3; i < 6; i++)
        ret->SO3Bounds_.low[i - 3] = prob->getBounds()[i];
      for (int i = 3; i < 6; i++)
        ret->SO3Bounds_.high[i - 3] = prob->getBounds()[i + n];

      ret->setSE3StateSpaceBounds(SE3bounds);
      for (int i = 6; i < n; i++)
      {
        RNbounds.setHigh(i - 6, prob->getBounds()[i + n]);
        RNbounds.setLow(i - 6, prob->getBounds()[i]);
      }
      ret->setRealVectorStateSpaceBounds(RNbounds);

    }
    else
    {
      WARNING(
          "State space bounds were not specified!\n"<< prob->getBounds().size() << " " << n);
    }

    return ret;
  }
  unsigned int OMPLSE3RNCompoundStateSpace::getDimension() const
  {
    return realvectordim_ + 6;
  }
  void OMPLSE3RNCompoundStateSpace::setRealVectorStateSpaceBounds(
      const ob::RealVectorBounds &bounds)
  {
    bounds.check();
    getSubspace(1)->as<ob::RealVectorStateSpace>()->setBounds(bounds);
  }

  const ob::RealVectorBounds & OMPLSE3RNCompoundStateSpace::getRealVectorStateSpaceBounds() const
  {
    return getSubspace(1)->as<ob::RealVectorStateSpace>()->getBounds();
  }

  void OMPLSE3RNCompoundStateSpace::setSE3StateSpaceBounds(
      const ob::RealVectorBounds &xyz, const double dist)
  {
    xyz.check();
    getSubspace(0)->as<ob::SE3StateSpace>()->setBounds(xyz);
  }

  const ob::RealVectorBounds & OMPLSE3RNCompoundStateSpace::getSE3StateSpaceBounds() const
  {
    return getSubspace(0)->as<ob::SE3StateSpace>()->getBounds();
  }

  void OMPLSE3RNCompoundStateSpace::setStart(const Eigen::VectorXd &start)
  {
    start_ = start;
    base_dist_ = (goal_.segment(0, 2) - start_.segment(0, 2)).norm();
  }

  void OMPLSE3RNCompoundStateSpace::setGoal(const Eigen::VectorXd &goal)
  {
    if (goal.rows() == realvectordim_ + 6)
    {
      goal_ = goal;
    }
  }
  void OMPLSE3RNCompoundStateSampler::sampleUniform(ob::State *state)
  {
    OMPLSE3RNCompoundStateSpace::StateType *rstate =
        static_cast<OMPLSE3RNCompoundStateSpace::StateType*>(state);

    //	Sample for the SE3 space
    const OMPLSE3RNCompoundStateSpace* space =
        static_cast<const OMPLSE3RNCompoundStateSpace*>(space_);
    const ob::RealVectorBounds &se3_bounds =
        static_cast<const OMPLSE3RNCompoundStateSpace*>(space_)->getSE3StateSpaceBounds();
    ob::RealVectorBounds so3_bounds =
        static_cast<const OMPLSE3RNCompoundStateSpace*>(space_)->SO3Bounds_;
    rstate->SE3StateSpace().setXYZ(
        rng_.uniformReal(se3_bounds.low[0], se3_bounds.high[0]),
        rng_.uniformReal(se3_bounds.low[1], se3_bounds.high[1]),
        rng_.uniformReal(se3_bounds.low[2], se3_bounds.high[2]));
    //	Only rotates along Z, for base movement
    rstate->SE3StateSpace().rotation().setAxisAngle(0, 0, 1,
        rng_.uniformReal(so3_bounds.low[0], so3_bounds.high[0]));
    rstate->SE3StateSpace().rotation().setAxisAngle(0, 1, 0,
        rng_.uniformReal(so3_bounds.low[1], so3_bounds.high[1]));
    rstate->SE3StateSpace().rotation().setAxisAngle(1, 0, 0,
        rng_.uniformReal(so3_bounds.low[2], so3_bounds.high[2]));
    //	Sample for the RN space
    const ob::RealVectorBounds &realvector_bounds =
        static_cast<const OMPLSE3RNCompoundStateSpace*>(space_)->getRealVectorStateSpaceBounds();
    const unsigned int dim = realvector_bounds.high.size();
    if (space->useGoal_)
    {
      Eigen::VectorXd tmp(2);
      tmp << rstate->SE3StateSpace().getX(), rstate->SE3StateSpace().getY();
      double percentage = (space->goal_.segment(0, 2) - tmp).norm()
          / space->base_dist_;
      if (percentage < space->rn_bias_percentage_->data)
      {
        for (unsigned int i = 0; i < dim; ++i)
          rstate->RealVectorStateSpace().values[i] = rng_.uniformReal(
              realvector_bounds.low[i], realvector_bounds.high[i]);
      }
      else
        for (unsigned int i = 0; i < dim; ++i)
          rstate->RealVectorStateSpace().values[i] = space->start_(i + 6);
    }
    else
      for (unsigned int i = 0; i < dim; ++i)
        rstate->RealVectorStateSpace().values[i] = rng_.uniformReal(
            realvector_bounds.low[i], realvector_bounds.high[i]);
  }

  void OMPLSE3RNCompoundStateSampler::sampleUniformNear(ob::State *state,
      const ob::State *near, const double distance)
  {
    //	First sample for the upper body
    OMPLSE3RNCompoundStateSpace::StateType *rstate =
        static_cast<OMPLSE3RNCompoundStateSpace::StateType*>(state);
    const OMPLSE3RNCompoundStateSpace::StateType *nstate =
        static_cast<const OMPLSE3RNCompoundStateSpace::StateType*>(near);
    const ob::RealVectorBounds &realvector_bounds =
        static_cast<const OMPLSE3RNCompoundStateSpace*>(space_)->getRealVectorStateSpaceBounds();
    const unsigned int dim = realvector_bounds.high.size();
    for (unsigned int i = 0; i < dim; ++i)
      rstate->RealVectorStateSpace().values[i] = rng_.uniformReal(
          std::max(realvector_bounds.low[i],
              nstate->RealVectorStateSpace().values[i]
                  - distance * weightImportance_[i + 6]),
          std::min(realvector_bounds.high[i],
              nstate->RealVectorStateSpace().values[i]
                  + distance * weightImportance_[i + 6]));
    //	Now sample for the SE3 space
    const ob::RealVectorBounds &se3_bounds =
        static_cast<const OMPLSE3RNCompoundStateSpace*>(space_)->getSE3StateSpaceBounds();
    rstate->SE3StateSpace().setX(
        rng_.uniformReal(
            std::max(se3_bounds.low[0],
                nstate->SE3StateSpace().getX()
                    - distance * weightImportance_[0]),
            std::min(se3_bounds.high[0],
                nstate->SE3StateSpace().getX()
                    + distance * weightImportance_[0])));
    rstate->SE3StateSpace().setY(
        rng_.uniformReal(
            std::max(se3_bounds.low[1],
                nstate->SE3StateSpace().getY()
                    - distance * weightImportance_[1]),
            std::min(se3_bounds.high[1],
                nstate->SE3StateSpace().getY()
                    + distance * weightImportance_[1])));
    rstate->SE3StateSpace().setZ(
        rng_.uniformReal(
            std::max(se3_bounds.low[2],
                nstate->SE3StateSpace().getZ()
                    - distance * weightImportance_[2]),
            std::min(se3_bounds.high[2],
                nstate->SE3StateSpace().getZ()
                    + distance * weightImportance_[2])));
    rstate->SE3StateSpace().rotation().setAxisAngle(0, 0, 1,
        rng_.uniformReal(-1.57, 1.57));
  }

  void OMPLSE3RNCompoundStateSampler::sampleGaussian(ob::State *state,
      const ob::State * mean, const double stdDev)
  {
    WARNING_NAMED("OMPLFullBodyStateSampler", "sampleGaussian not implemented");
  }

  EReturn OMPLSE3RNCompoundStateSpace::OMPLStateToEigen(const ob::State *ompl,
      Eigen::VectorXd &eigen)
  {
    eigen.setZero(getDimension());
    const OMPLSE3RNCompoundStateSpace::StateType *statetype =
        static_cast<const OMPLSE3RNCompoundStateSpace::StateType*>(ompl);
    memcpy(eigen.segment(6, eigen.rows() - 6).data(),
        statetype->RealVectorStateSpace().values,
        sizeof(double) * (eigen.rows() - 6));
    eigen(0) = statetype->SE3StateSpace().getX();
    eigen(1) = statetype->SE3StateSpace().getY();
    eigen(2) = statetype->SE3StateSpace().getZ();

    KDL::Rotation tmp = KDL::Rotation::Quaternion(
        statetype->SE3StateSpace().rotation().x,
        statetype->SE3StateSpace().rotation().y,
        statetype->SE3StateSpace().rotation().z,
        statetype->SE3StateSpace().rotation().w);
    tmp.GetEulerZYX(eigen(3), eigen(4), eigen(5));
    return SUCCESS;
  }

  EReturn OMPLSE3RNCompoundStateSpace::EigenToOMPLState(
      const Eigen::VectorXd &eigen, ob::State *ompl)
  {
    OMPLSE3RNCompoundStateSpace::StateType *statetype =
        static_cast<OMPLSE3RNCompoundStateSpace::StateType*>(ompl);
    statetype->SE3StateSpace().setXYZ(eigen(0), eigen(1), eigen(2));
    KDL::Rotation tmp = KDL::Rotation::EulerZYX(eigen(3), eigen(4), eigen(5));
    tmp.GetQuaternion(statetype->SE3StateSpace().rotation().x,
        statetype->SE3StateSpace().rotation().y,
        statetype->SE3StateSpace().rotation().z,
        statetype->SE3StateSpace().rotation().w);

    memcpy(statetype->RealVectorStateSpace().values,
        eigen.segment(6, eigen.rows() - 6).data(),
        sizeof(double) * (eigen.rows() - 6));

    return SUCCESS;
  }
}

