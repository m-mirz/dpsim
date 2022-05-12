/* Copyright 2017-2020 Institute for Automation of Complex Power Systems,
 *                     EONERC, RWTH Aachen University
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *********************************************************************************/

#include <cps/EMT/EMT_Ph3_CurrentSource.h>


using namespace CPS;

EMT::Ph3::CurrentSource::CurrentSource(String uid, String name, Logger::Level logLevel)
	: SimPowerComp<Real>(uid, name, logLevel) {
	mPhaseType = PhaseType::ABC;
	setVirtualNodeNumber(0);
	setTerminalNumber(2);
	mIntfVoltage = Matrix::Zero(3, 1);
	mIntfCurrent = Matrix::Zero(3, 1);

	addAttribute<MatrixComp>("I_ref", Flags::read | Flags::write);  // rms-value
	addAttribute<Real>("f_src", Flags::read | Flags::write);
	addAttribute<Complex>("sigOut", Flags::read | Flags::write);
}


void EMT::Ph3::CurrentSource::initializeFromNodesAndTerminals(Real frequency) {
	mSLog->info("\n--- Initialization from node voltages and terminal ---");	
	if (!mParametersSet) {
		auto srcSigSine = Signal::SineWaveGenerator::make(mName + "_sw", Logger::Level::off);
		// Complex(1,0) is used as initialPhasor for signal generator as only phase is used
		srcSigSine->setParameters(Complex(1,0), frequency);
		mSrcSig = srcSigSine; 

		Complex v_ref = initialSingleVoltage(1) - initialSingleVoltage(0);
		Complex s_ref = terminal(1)->singlePower() - terminal(0)->singlePower();

		// Current flowing from T1 to T0 (rms value)
		Complex i_ref = std::conj(s_ref/v_ref/sqrt(3.));

		attribute<MatrixComp>("I_ref")->set(CPS::Math::singlePhaseVariableToThreePhase(i_ref));
		setAttributeRef("f_src", mSrcSig->attribute<Real>("freq"));

		mSLog->info("\nReference current: {:s}"
					"\nReference voltage: {:s}"
					"\nReference power: {:s}"
					"\nTerminal 0 voltage: {:s}"
					"\nTerminal 1 voltage: {:s}"
					"\nTerminal 0 power: {:s}"
					"\nTerminal 1 power: {:s}",
					Logger::phasorToString(i_ref),
					Logger::phasorToString(v_ref),
					Logger::complexToString(s_ref),
					Logger::phasorToString(initialSingleVoltage(0)),
					Logger::phasorToString(initialSingleVoltage(1)),
					Logger::complexToString(terminal(0)->singlePower()),
					Logger::complexToString(terminal(1)->singlePower()));
	} else {
		mSLog->info("\nInitialization from node voltages and terminal omitted (parameter already set)."
					"\nReference voltage: {:s}",
					Logger::matrixCompToString(attribute<MatrixComp>("I_ref")->get()));
	}
	mSLog->info("\n--- Initialization from node voltages and terminal ---");
	mSLog->flush();
}

SimPowerComp<Real>::Ptr EMT::Ph3::CurrentSource::clone(String name) {
	auto copy = CurrentSource::make(name, mLogLevel);
	// TODO: implement setParameters
	// copy->setParameters(attribute<MatrixComp>("I_ref")->get(), attribute<Real>("f_src")->get());
	return copy;
}


void EMT::Ph3::CurrentSource::mnaInitialize(Real omega, Real timeStep, Attribute<Matrix>::Ptr leftVector) {
	MNAInterface::mnaInitialize(omega, timeStep);

	updateMatrixNodeIndices();

	mMnaTasks.push_back(std::make_shared<MnaPreStep>(*this));
	mMnaTasks.push_back(std::make_shared<MnaPostStep>(*this, leftVector));

	mRightVector = Matrix::Zero(leftVector->get().rows(), 1);

}

void EMT::Ph3::CurrentSource::mnaApplyRightSideVectorStamp(Matrix& rightVector) {
	if (terminalNotGrounded(1)) {
		Math::setVectorElement(rightVector, matrixNodeIndex(1, 0), - mIntfCurrent(0, 0));
		Math::setVectorElement(rightVector, matrixNodeIndex(1, 1), - mIntfCurrent(1, 0));
		Math::setVectorElement(rightVector, matrixNodeIndex(1, 2), - mIntfCurrent(2, 0));
	}
	if (terminalNotGrounded(0)) {
		Math::setVectorElement(rightVector, matrixNodeIndex(0, 0), mIntfCurrent(0, 0));
		Math::setVectorElement(rightVector, matrixNodeIndex(0, 1), mIntfCurrent(1, 0));
		Math::setVectorElement(rightVector, matrixNodeIndex(0, 2), mIntfCurrent(2, 0));
	}
}

void EMT::Ph3::CurrentSource::updateCurrent(Real time) {
	if(mSrcSig != nullptr) {
		mSrcSig->step(time);
		for(int i = 0; i < 3; i++) {
			mIntfCurrent(i, 0) = RMS_TO_PEAK * Math::abs(attribute<MatrixComp>("I_ref")->get()(i, 0)) 
				* cos(Math::phase(mSrcSig->getSignal()) + Math::phase(attribute<MatrixComp>("I_ref")->get()(i, 0)));
		}
	} else {
		mIntfCurrent = RMS_TO_PEAK * attribute<MatrixComp>("I_ref")->get().real();
	}
	mSLog->debug(
		"\nUpdate current: {:s}",
		Logger::matrixToString(mIntfCurrent)
	);
}

void EMT::Ph3::CurrentSource::mnaAddPreStepDependencies(AttributeBase::List &prevStepDependencies, AttributeBase::List &attributeDependencies, AttributeBase::List &modifiedAttributes) {
	attributeDependencies.push_back(attribute("I_ref"));
	modifiedAttributes.push_back(attribute("right_vector"));
	modifiedAttributes.push_back(attribute("v_intf"));
}

void EMT::Ph3::CurrentSource::mnaPreStep(Real time, Int timeStepCount) {
	updateCurrent(time);
	mnaApplyRightSideVectorStamp(mRightVector);
}

void EMT::Ph3::CurrentSource::mnaAddPostStepDependencies(AttributeBase::List &prevStepDependencies, AttributeBase::List &attributeDependencies, AttributeBase::List &modifiedAttributes, Attribute<Matrix>::Ptr &leftVector) {
	attributeDependencies.push_back(leftVector);
	modifiedAttributes.push_back(attribute("v_intf"));
};

void EMT::Ph3::CurrentSource::mnaPostStep(Real time, Int timeStepCount, Attribute<Matrix>::Ptr &leftVector) {
	mnaUpdateVoltage(*leftVector);
}

void EMT::Ph3::CurrentSource::mnaUpdateVoltage(const Matrix& leftVector) {
	// v1 - v0
	mIntfVoltage = Matrix::Zero(3,1);
	if (terminalNotGrounded(1)) {
		mIntfVoltage(0, 0) = Math::realFromVectorElement(leftVector, matrixNodeIndex(1, 0));
		mIntfVoltage(1, 0) = Math::realFromVectorElement(leftVector, matrixNodeIndex(1, 1));
		mIntfVoltage(2, 0) = Math::realFromVectorElement(leftVector, matrixNodeIndex(1, 2));
	}
	if (terminalNotGrounded(0)) {
		mIntfVoltage(0, 0) = mIntfVoltage(0, 0) - Math::realFromVectorElement(leftVector, matrixNodeIndex(0, 0));
		mIntfVoltage(1, 0) = mIntfVoltage(1, 0) - Math::realFromVectorElement(leftVector, matrixNodeIndex(0, 1));
		mIntfVoltage(2, 0) = mIntfVoltage(2, 0) - Math::realFromVectorElement(leftVector, matrixNodeIndex(0, 2));
	}
}