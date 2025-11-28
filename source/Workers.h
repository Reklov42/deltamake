/**
 * \file	Workers.h
 * \brief	Thread wizardry
 * \date	12 nov 2024
 * \author	Reklov
 */
#ifndef __DELTAMAKE_WORKERS_H__
#define __DELTAMAKE_WORKERS_H__

#include <string>

#include "deltamake.h"
 
// ******************************************************************************** //

										//										//
namespace DeltaMake {
	/**
	 * Scheduler list of tasks
	 */
	class ITaskList {
		public:
			ITaskList&					operator=(const ITaskList&)				= delete;

			/**
			 * \param title Title of task
			 * \param command Full system command string
			 * \param bFailIfNonZero Treat a non-zero return of command as an error and stop worker
			 */
			virtual void				AddCommand(const char title[], const std::string& command, bool bFailIfNonZero = true) = 0;

			virtual void				AddBarrier()							= 0;

			virtual size_t				GetTaskCount() const					= 0;

		protected:
			virtual						~ITaskList()							= default;
	};

	/**
	 * Task scheduler
	 */
	class IScheduler {
		public:
			IScheduler&					operator=(const IScheduler&)			= delete;

			virtual void				Init(size_t nWorkers)					= 0;

			virtual ITaskList*			GetList()								= 0;

			virtual void				Start()									= 0;

			/**
			 * Stop task queue and wait for current tasks to end
			 */
			virtual void				Stop()									= 0;

			/**
			 * Stop task queue and kill all current tasks
			 */
			virtual void				Kill()									= 0;
		
		protected:
			virtual						~IScheduler()							= default;
	};

	extern IScheduler* const scheduler;
}

#endif /* !__DELTAMAKE_WORKERS_H__ */