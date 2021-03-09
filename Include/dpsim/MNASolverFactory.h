/* Copyright 2017-2020 Institute for Automation of Complex Power Systems,
*                     EONERC, RWTH Aachen University
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at https://mozilla.org/MPL/2.0/.
*********************************************************************************/

#pragma once

#include <dpsim/MNASolver.h>
#include <dpsim/MNASolverEigenDense.h>
#ifdef WITH_SPARSE
#include <dpsim/MNASolverEigenSparse.h>
#endif
#ifdef WITH_CUDA
	#include <dpsim/MNASolverGpuDense.h>
#ifdef WITH_SPARSE
	#include <dpsim/MNASolverGpuSparse.h>
#endif
#endif

namespace DPsim {

class MnaSolverFactory {
	public:
	/// \brief The implementations of the MNA solvers MnaSolver can support.
	///
	enum MnaSolverImpl {
		EigenDense,
		EigenSparse,
		CUDADense,
		CUDASparse,
	};

	/// MNA implementations supported by this compilation
	static const std::vector<MnaSolverImpl> mSupportedSolverImpls(void) {
		static std::vector<MnaSolverImpl> ret = {
			EigenDense,
#ifdef WITH_SPARSE
			EigenSparse,
#endif //WITH_SPARSE
#ifdef WITH_CUDA
			CUDADense,
#ifdef WITH_SPARSE
			CUDASparse,
#endif //WITH_SPARSE
#endif //WITH_CUDA
		};
		return ret;
	}

	/// sovlerImpl: choose the most advanced solver implementation available by default
	template <typename VarType>
	static std::shared_ptr<MnaSolver<VarType>> factory(String name,
		CPS::Domain domain = CPS::Domain::DP,
		CPS::Logger::Level logLevel = CPS::Logger::Level::info,
		MnaSolverImpl implementation = *mSupportedSolverImpls().end())
	{
		switch(implementation) {
		case MnaSolverImpl::EigenDense:
			return std::make_shared<MnaSolverEigenDense<VarType>>(name, domain, logLevel);
#ifdef WITH_SPARSE
		case MnaSolverImpl::EigenSparse:
			return std::make_shared<MnaSolverEigenSparse<VarType>>(name, domain, logLevel);
#endif
#ifdef WITH_CUDA
		case MnaSolverImpl::CUDADense:
			return std::make_shared<MnaSolverGpuDense<VarType>>(name, domain, logLevel);
#ifdef WITH_SPARSE
		case MnaSolverImpl::CUDASparse:
			return std::make_shared<MnaSolverGpuSparse<VarType>>(name, domain, logLevel);
#endif
#endif
		default:
			throw CPS::SystemError("unsupported MNA implementation.");

		}
	}
};
}