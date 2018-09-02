/** Example of shared memory interface
 *
 * @author Markus Mirz <mmirz@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *
 * DPsim
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *********************************************************************************/

#include <DPsim.h>

using namespace DPsim;
using namespace CPS::DP::Ph1;

int main(int argc, char* argv[])
{
	// Nodes
	auto n1 = Node::make("n1");
	auto n2 = Node::make("n2");
	auto n3 = Node::make("n3");

	// Components
	auto vs = VoltageSourceNorton::make("v_s", Complex(10000, 0), 1);
	auto l1 = Inductor::make("l_1", 0.1);
	auto r1 = Resistor::make("r_1", 1);
	auto r2A = Resistor::make("r_2", 10);
	auto r2B = Resistor::make("r_2", 8);

	vs->connect({GND, n1});
	l1->connect({n1, n2});
	r1->connect({n2, n3});
	r2A->connect({n3, GND});
	r2B->connect({n3, GND});

	auto nodes = SystemNodeList{GND, n1, n2, n3};

	auto sys1 = SystemTopology(50, nodes, SystemComponentList{vs, l1, r1, r2A});
	auto sys2 = SystemTopology(50, nodes, SystemComponentList{vs, l1, r1, r2B});

	auto sim = Simulation("ShmemDistributedRef", sys1, 0.001, 20);
	sim.addSystemTopology(sys2);
	sim.setSwitchTime(10, 1);
	sim.run();

	return 0;
}
