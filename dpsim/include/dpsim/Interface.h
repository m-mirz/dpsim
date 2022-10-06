// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <thread>

#include <dpsim-models/Logger.h>
#include <dpsim/Config.h>
#include <dpsim/Definitions.h>
#include <dpsim/Scheduler.h>
#include <dpsim/Interface.h>
#include <dpsim/InterfaceWorker.h>
#include <dpsim-models/Attribute.h>
#include <dpsim-models/Task.h>

#include <readerwriterqueue.h>

namespace DPsim {

	class Interface :
		public SharedFactory<Interface> {

	public:
		typedef std::shared_ptr<Interface> Ptr;

		struct AttributePacket {
			CPS::AttributeBase::Ptr value;
			UInt attributeId; //Used to identify the attribute. Defined by the position in the `mExportAttrsDpsim` and `mImportAttrsDpsim` lists
			UInt sequenceId; //Increasing ID used to discern multiple consecutive updates of a single attribute
			unsigned char flags; //Bit 0 set: Close interface
		} typedef AttributePacket;

		enum AttributePacketFlags {
			PACKET_NO_FLAGS = 0,
			PACKET_CLOSE_INTERFACE = 1,
		};

        Interface(InterfaceWorker::Ptr intf, CPS::Logger::Log log, String name = "", bool syncOnSimulationStart = false, UInt downsampling = 1) : 
			mInterfaceWorker(intf),
			mLog(log),
			mName(name),
			mSyncOnSimulationStart(syncOnSimulationStart),
			mDownsampling(downsampling) {
				mInterfaceWorker->mLog = log;
				mQueueDpsimToInterface = std::make_shared<moodycamel::BlockingReaderWriterQueue<AttributePacket>>();
				mQueueInterfaceToDpsim = std::make_shared<moodycamel::BlockingReaderWriterQueue<AttributePacket>>();
			};

		virtual void open();
		virtual void close();

		//Function used in the interface's simulation task to read all imported attributes from the queue
		//Called once before every simulation timestep
		virtual void pushDpsimAttrsToQueue();
		//Function used in the interface's simulation task to write all exported attributes to the queue
		//Called once after every simulation timestep
		virtual void popDpsimAttrsFromQueue();

		//Function used in the interface thread to read updated attributes from the environment and push them into the queue
		virtual void pushInterfaceAttrsToQueue() {};

		virtual CPS::Task::List getTasks();

		bool shouldSyncOnSimulationStart() const {
			return mSyncOnSimulationStart;
		}

		virtual ~Interface() {
			if (mOpened)
				close();
		}

		// Attributes used in the DPsim simulation. Should only be accessed by the dpsim-thread
		std::vector<std::tuple<CPS::AttributeBase::Ptr, UInt, bool>> mImportAttrsDpsim;
		std::vector<std::tuple<CPS::AttributeBase::Ptr, UInt>> mExportAttrsDpsim;

		virtual void importAttribute(CPS::AttributeBase::Ptr attr, bool blockOnRead = false);
		virtual void exportAttribute(CPS::AttributeBase::Ptr attr);

	protected:
		InterfaceWorker::Ptr mInterfaceWorker;
		CPS::Logger::Log mLog;
		String mName;
		bool mSyncOnSimulationStart;
		UInt mCurrentSequenceDpsimToInterface = 0;
		UInt mCurrentSequenceInterfaceToDpsim = 0;
		UInt mDownsampling;
		std::atomic<bool> mOpened;
		std::thread mInterfaceWriterThread;
		std::thread mInterfaceReaderThread;

		std::shared_ptr<moodycamel::BlockingReaderWriterQueue<AttributePacket>> mQueueDpsimToInterface;
		std::shared_ptr<moodycamel::BlockingReaderWriterQueue<AttributePacket>> mQueueInterfaceToDpsim;

	public:

		class WriterThread {
			private:
				std::shared_ptr<moodycamel::BlockingReaderWriterQueue<AttributePacket>> mQueueDpsimToInterface;
				DPsim::InterfaceWorker::Ptr mInterfaceWorker;

			public:
				WriterThread(
						std::shared_ptr<moodycamel::BlockingReaderWriterQueue<AttributePacket>> queueDpsimToInterface,
				 		DPsim::InterfaceWorker::Ptr intf
					) :
					mQueueDpsimToInterface(queueDpsimToInterface),
					mInterfaceWorker(intf) {};
				void operator() ();
		};

		class ReaderThread {
			private:
				std::shared_ptr<moodycamel::BlockingReaderWriterQueue<AttributePacket>> mQueueInterfaceToDpsim;
				DPsim::InterfaceWorker::Ptr mInterfaceWorker;
				std::atomic<bool>& mOpened;

			public:
				ReaderThread(
						std::shared_ptr<moodycamel::BlockingReaderWriterQueue<AttributePacket>> queueInterfaceToDpsim,
				 		DPsim::InterfaceWorker::Ptr intf,
						std::atomic<bool>& opened
					) :
					mQueueInterfaceToDpsim(queueInterfaceToDpsim),
					mInterfaceWorker(intf),
					mOpened(opened) {};
				void operator() ();
		};

		class PreStep : public CPS::Task {
		public:
			PreStep(Interface& intf) :
				Task(intf.mName + ".Read"), mIntf(intf) {
				for (auto attr : intf.mImportAttrsDpsim) {
					mModifiedAttributes.push_back(std::get<0>(attr));
				}
			}

			void execute(Real time, Int timeStepCount);

		private:
			Interface& mIntf;
		};

		class PostStep : public CPS::Task {
		public:
			PostStep(Interface& intf) :
				Task(intf.mName + ".Write"), mIntf(intf) {
				for (auto attr : intf.mExportAttrsDpsim) {
					mAttributeDependencies.push_back(std::get<0>(attr));
				}
				mModifiedAttributes.push_back(Scheduler::external);
			}

			void execute(Real time, Int timeStepCount);

		private:
			Interface& mIntf;
		};

	};
}

