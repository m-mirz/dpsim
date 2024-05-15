/* Copyright 2017-2021 Institute for Automation of Complex Power Systems,
 *                     EONERC, RWTH Aachen University
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *********************************************************************************/
#pragma once

#include <dpsim-models/CompositePowerComp.h>
#include <dpsim-models/Solver/MNAVariableCompInterface.h>
#include <dpsim-models/Base/Base_VSIVoltageSourceInverterDQ.h>
#include <dpsim-models/DP/DP_Ph1_Capacitor.h>

namespace CPS
{
	namespace DP
	{
		namespace Ph1
		{
			class VSIVoltageControlVBR : public CompositePowerComp<Complex>,
										 public Base::VSIVoltageSourceInverterDQ<Complex>,
										 public MNAVariableCompInterface,
										 public SharedFactory<VSIVoltageControlVBR>
			{
			public:
				/// Defines name amd logging level
				VSIVoltageControlVBR(String name, Logger::Level logLevel = Logger::Level::off)
					: VSIVoltageControlVBR(name, name, logLevel) {}
				/// Defines UID, name, logging level and connection trafo existence
				VSIVoltageControlVBR(String uid, String name,
									 Logger::Level logLevel = Logger::Level::off,
									 Bool modelAsCurrentSource = false);

				// #### General ####
				/// Initializes component from power flow data
				void initializeFromNodesAndTerminals(Real frequency) final;
				///
				void initVars(Real timeStep);

				// #### MNA section ####
				/// Initializes internal variables of the component
				void mnaParentInitialize(Real omega, Real timeStep,
										 Attribute<Matrix>::Ptr leftVector) final;
				/// Stamps system matrix
				void mnaParentApplySystemMatrixStamp(SparseMatrixRow &systemMatrix) final;
				/// Add MNA pre step dependencies
				void mnaParentAddPreStepDependencies(
					AttributeBase::List &prevStepDependencies,
					AttributeBase::List &attributeDependencies,
					AttributeBase::List &modifiedAttributes) final;
				/// MNA pre step operations
				void mnaParentPreStep(Real time, Int timeStepCount) final;
				/// Add MNA post step dependencies
				void
				mnaParentAddPostStepDependencies(AttributeBase::List &prevStepDependencies,
												 AttributeBase::List &attributeDependencies,
												 AttributeBase::List &modifiedAttributes,
												 Attribute<Matrix>::Ptr &leftVector) final;
				/// Updates current through the component
				void mnaCompUpdateCurrent(const Matrix &leftVector) final;
				/// Updates voltage across component
				void mnaCompUpdateVoltage(const Matrix &leftVector) final;
				/// MNA post step operations
				void mnaParentPostStep(Real time, Int timeStepCount,
									   Attribute<Matrix>::Ptr &leftVector) final;
				///
				void mnaParentApplyRightSideVectorStamp(Matrix &rightVector) final;

				/// Mark that parameter changes so that system matrix is updated
				Bool hasParameterChanged() final { return true; };

			protected:
				// ### Electrical Subcomponents ###
				/// Capacitor Cf as part of LC filter
				std::shared_ptr<DP::Ph1::Capacitor> mSubCapacitorF;

			private:
				///
				void createSubComponents() final;
				///
				void updatePower();
				///
				void calculateResistanceMatrix();
				///
				void update_DqToComplexATransformMatrix();

			private:
				// ### Model variables used for the discretization of the inductor equations
				/// DC equivalent current source for harmonics [A]
				MatrixComp mEquivCurrent;
				/// Equivalent conductance for harmonics [S]
				MatrixComp mEquivCond;
				/// Coefficient in front of previous current value for harmonics
				MatrixComp mPrevCurrFac;

				// ### VBR Model specific variables
				/// History voltage in dq domain (output of VSI_Controller)
				Complex mVhist;
				/// Current flowing throw the filter inductor in dp domain
				MatrixComp mFilterCurrent;

				/// Auxiliar Matrix
				Matrix mA;
				Matrix mE;
				Matrix mZ;
				Matrix mAMatrix;
				Matrix mEMatrix;

				/// Park Transformation
				///
				Matrix mDqToComplexA;
				///
				Matrix mComplexAToDq;
			};
		} // namespace Ph1
	}	  // namespace DP
} // namespace CPS