/* Copyright 2017-2021 Institute for Automation of Complex Power Systems,
 *                     EONERC, RWTH Aachen University
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *********************************************************************************/

#include <DPsim.h>
#include "../Examples.h"

using namespace DPsim;
using namespace CPS;

int main(int argc, char* argv[]) {

	// CIM::Examples::Grids::GridForming::ScenarioConfig1 scenario;
	CIM::Examples::Grids::GridForming::Yazdani scenario;

	Real finalTime = 2;
	Real timeStep = 0.0001;
	String simName = "SP_Slack_PiLine_VSI_VoltageControlled_SteadyState_with_PF_Init";
	Bool pvWithControl = true;


	// ----- POWERFLOW FOR INITIALIZATION -----
	Real timeStepPF = finalTime;
	Real finalTimePF = finalTime+timeStepPF;
	String simNamePF = simName + "_PF";
	Logger::setLogDir("logs/" + simNamePF);
	
	// Components Powerflow Init
	auto n1PF = SimNode<Complex>::make("n1", PhaseType::Single);
	auto n2PF = SimNode<Complex>::make("n2", PhaseType::Single);

	auto extnetPF = SP::Ph1::NetworkInjection::make("Slack", Logger::Level::debug);
	extnetPF->setParameters(scenario.systemNominalVoltage);
	extnetPF->setBaseVoltage(scenario.systemNominalVoltage);
	extnetPF->modifyPowerFlowBusType(PowerflowBusType::VD);
	
	auto linePF = SP::Ph1::PiLine::make("PiLine", Logger::Level::debug);
	linePF->setParameters(scenario.lineResistance, 0, 0, 0);
	linePF->setBaseVoltage(scenario.systemNominalVoltage);

	Complex load1_s=3*std::pow(scenario.systemNominalVoltage, 2)/(Complex(scenario.loadRes1, scenario.loadInd1*scenario.systemNominalOmega));
	Real loadActivePower=load1_s.real();
	Real loadReactivePower=load1_s.imag();

	auto loadPF = SP::Ph1::Load::make("Load", Logger::Level::debug);
	loadPF->setParameters(loadActivePower, loadReactivePower, scenario.systemNominalVoltage);
	loadPF->modifyPowerFlowBusType(PowerflowBusType::PQ);

	// auto loadPF = SP::Ph1::Load::make("Load", Logger::Level::debug);
	// loadPF->setParameters(scenario.loadActivePower, scenario.loadReactivePower, scenario.systemNominalVoltage);
	// loadPF->modifyPowerFlowBusType(PowerflowBusType::PQ);

	// Topology
	extnetPF->connect({ n1PF });
	linePF->connect({ n1PF, n2PF });
	loadPF->connect({ n2PF });
	auto systemPF = SystemTopology(scenario.systemNominalFreq,
			SystemNodeList{n1PF, n2PF},
			SystemComponentList{linePF, extnetPF, loadPF});
	
	// Logging
	auto loggerPF = DataLogger::make(simNamePF);
	loggerPF->logAttribute("v1", n1PF->attribute("v"));
	loggerPF->logAttribute("v2", n2PF->attribute("v"));

	// Simulation
	Simulation simPF(simNamePF, Logger::Level::debug);
	simPF.setSystem(systemPF);
	simPF.setTimeStep(timeStepPF);
	simPF.setFinalTime(finalTimePF);
	simPF.setDomain(Domain::SP);
	simPF.setSolverType(Solver::Type::NRP);
	simPF.setSolverAndComponentBehaviour(Solver::Behaviour::Initialization);
	simPF.doInitFromNodesAndTerminals(false);
	simPF.addLogger(loggerPF);
	simPF.run();



	// ----- SP SIMULATION -----
	Real timeStepSP = timeStep;
	Real finalTimeSP = finalTime+timeStepSP;
	String simNameSP = simName+"_SP";
	Logger::setLogDir("logs/" + simNameSP);

	// Components
	auto n1SP = SimNode<Complex>::make("n1", PhaseType::Single);
	auto n2SP = SimNode<Complex>::make("n2", PhaseType::Single);
	
	auto loadSP = SP::Ph1::Load::make("Load", Logger::Level::debug);
	loadSP->setParameters(loadActivePower, loadReactivePower, scenario.systemNominalVoltage);

	auto pv = SP::Ph1::VSIVoltageControlDQ::make("pv", "pv", Logger::Level::debug, false);
	pv->setParameters(scenario.systemNominalOmega, scenario.Vdref, scenario.Vqref);
	pv->setControllerParameters(scenario.KpVoltageCtrl, scenario.KiVoltageCtrl, scenario.KpCurrCtrl, scenario.KiCurrCtrl, scenario.systemNominalOmega);
	pv->setFilterParameters(scenario.Lf, scenario.Cf, scenario.Rf, scenario.Rc);
	pv->setInitialStateValues(scenario.phi_dInit, scenario.phi_qInit, scenario.gamma_dInit, scenario.gamma_qInit);
	pv->withControl(pvWithControl);

	// auto lineSP = SP::Ph1::PiLine::make("PiLine", Logger::Level::debug);
	// lineSP->setParameters(scenario.lineResistance, scenario.lineInductance, 0, 0);

	auto lineSP = SP::Ph1::Resistor::make("Line", Logger::Level::debug);
	lineSP->setParameters(scenario.lineResistance);

	// Topology
	pv->connect({ n1SP });
	lineSP->connect({n1SP, n2SP});
	loadSP->connect({n2SP});

	auto systemSP = SystemTopology(scenario.systemNominalFreq,
			SystemNodeList{n1SP, n2SP},
			SystemComponentList{loadSP, lineSP, pv});

	// Initialization of dynamic topology
	systemSP.initWithPowerflow(systemPF);
	Complex initial1PhPowerVSI= Complex(linePF->attributeTyped<Real>("p_inj")->get(), linePF->attributeTyped<Real>("q_inj")->get());

	pv->terminal(0)->setPower(initial1PhPowerVSI);

	// Logging
	auto loggerSP = DataLogger::make(simNameSP);
    loggerSP->logAttribute("Controlled_source_PV", pv->attribute("Vs"));
	loggerSP->logAttribute("Voltage_terminal_PV", n1SP->attribute("v"));
	loggerSP->logAttribute("Voltage_PCC", n2SP->attribute("v"));
	loggerSP->logAttribute("Strom_RLC", pv->attribute("i_intf"));
	loggerSP->logAttribute("VCO_output", pv->attribute("vco_output"));
	loggerSP->logAttribute("P_elec", pv->attribute("P_elec"));
	loggerSP->logAttribute("Q_elec", pv->attribute("Q_elec"));
	
	// Simulation
	Simulation sim(simNameSP, Logger::Level::debug);
	sim.setSystem(systemSP);
	sim.setTimeStep(timeStepSP);
	sim.setFinalTime(finalTimeSP);
	sim.setDomain(Domain::SP);
	sim.addLogger(loggerSP);
	sim.run();
}