/**
 * \file	WorkerPool.h
 * \brief	Thread wizardry
 * \date	12 nov 2024
 * \author	Reklov
 */
#ifndef __DELTAMAKE_WORKERPOOL_H__
#define __DELTAMAKE_WORKERPOOL_H__

#include <stddef.h>
#include <time.h>

#include <thread>
#include <vector>
#include <mutex>
#include <queue>
#include <atomic>

#include "deltamake.h"
#include "Exception.h"
#include <condition_variable>
 
// ******************************************************************************** //

										//										//
namespace DeltaMake {
	/**
	 * Command
	 */
	struct SCommand {
		char							title[DELTAMAKE_MAX_WORKER_TITLE]		= { " " /* "Waiting for the command..." */ };
		std::string						command;
	};

	/**
	 * Worker data
	 */
	struct SWorker {
		std::mutex						mutex;
		SCommand						command;
		std::atomic_bool				bRunning								= true;
	};

	/**                                 
	 * Thread pool of workers
	 */
	class CWorkerPool : public IWorkerPool {
		public:
										CWorkerPool();

			virtual bool				AddCommand(const std::string& command, const char title[]) override;

			int							Start(size_t nWorkers);
			void						Stop();

		private:
			void						WorkerLoop(size_t index);


			bool						m_bRunning								= false;

			std::mutex					m_commandsMutex;
			std::condition_variable		m_condition;

			SWorker*					m_workers;
			std::vector<std::thread>	m_threads;

			std::queue<SCommand>		m_commands;
			std::atomic_size_t			m_nExecuted								= 0;
	};
}

// ******************************************************************************** //

/* ****************************************
 * DeltaMake::CWorkerPool::CWorkerPool
 */
inline DeltaMake::CWorkerPool::CWorkerPool() {
	
}

/* ****************************************
 * DeltaMake::CWorkerPool::AddCommand
 */
inline bool DeltaMake::CWorkerPool::AddCommand(const std::string& command, const char title[]) {
	{
		std::lock_guard<std::mutex> lock(m_commandsMutex);
		if (m_bRunning == true)
			return false;
	}

	SCommand c;
	strncpy(c.title, title, DELTAMAKE_MAX_WORKER_TITLE - 1);
	c.command = command;
	m_commands.push(c);

	return true;
}

/* ****************************************
 * DeltaMake::CWorkerPool::Start
 */
inline int DeltaMake::CWorkerPool::Start(size_t nWorkers) {
	terminal->Log(LOG_INFO, "\r\n\n");
	
	const size_t nTotal = m_commands.size();

	// Start
	m_bRunning = true;
	m_workers = new SWorker[nWorkers]();

	for (size_t i = 0; i < nWorkers; ++i)
		m_threads.push_back(std::thread(&CWorkerPool::WorkerLoop, this, i));
	
    m_condition.notify_all();
	
	// Wait
	const size_t messageSize = DELTAMAKE_MAX_WORKER_TITLE + 4; // "[ ] "
	const char spinner[] = { '-', '\\', '|', '/' };
	
	timespec ts;

	size_t offestUp = 0;
	terminal->ShowCursor(false);
	while (true) {
    	timespec_get(&ts, TIME_UTC);
		const size_t spinnerIndex = static_cast<size_t>(ts.tv_nsec >> 26) % (sizeof(spinner) / sizeof(char));

		size_t nCh = messageSize;
		terminal->MoveUp(offestUp);
		terminal->MoveLeft(terminal->GetColumns());
		offestUp = 0;

		// Workers
		size_t nReady = 0;
		for (size_t iWorker = 0; iWorker < nWorkers; ++iWorker) {
			SWorker& worker = m_workers[iWorker];

			if (worker.bRunning == false) {
				terminal->Log(LOG_INFO, "[#] %-*s", DELTAMAKE_MAX_WORKER_TITLE, "");
				++nReady;
			}
			else {
				std::lock_guard<std::mutex> lock(worker.mutex);
				terminal->Log(LOG_INFO, "[%c] %-*s", spinner[spinnerIndex], DELTAMAKE_MAX_WORKER_TITLE, worker.command.title);
			}

			nCh += messageSize;
			if (nCh > terminal->GetColumns()) {
				++offestUp;
				nCh = messageSize;
				terminal->Log(LOG_INFO, "\r\n");
			}
		}

		// Status
		{
			//std::lock_guard<std::mutex> lock(m_commandsMutex);
			const size_t nExecuted = m_nExecuted;
			terminal->Log(LOG_INFO, "\n[%3i/%3i] Compiling...", nExecuted, nTotal);
		}
		++offestUp;

		if (nReady == nWorkers)
			break;
	}
	
	terminal->Log(LOG_INFO, "\n");

	// Stop
	Stop();

	terminal->ShowCursor(true);
	delete[] m_workers;

	return 0;
}

/* ****************************************
 * DeltaMake::CWorkerPool::Stop
 */
inline void DeltaMake::CWorkerPool::Stop() {
    m_condition.notify_all();
	
	{
		std::unique_lock<std::mutex> lock(m_commandsMutex);
		m_bRunning = false;
	}

    m_condition.notify_all();

	for (size_t i = 0; i < m_threads.size(); ++i) {
		m_threads[i].join();
    }
}

/* ****************************************
 * DeltaMake::CWorkerPool::WorkerLoop
 */
inline void DeltaMake::CWorkerPool::WorkerLoop(size_t index) {
	SWorker& data = m_workers[index];
	while (true) {
		{
			std::unique_lock<std::mutex> lock(m_commandsMutex);
			m_condition.wait(lock, [this] {
				return (m_commands.empty() == false) || (m_bRunning == true);
			});

			if ((m_bRunning == false) || (m_commands.empty() == true))
				break;

			//std::lock_guard<std::mutex> lock(data.mutex);
			data.command = m_commands.front();
			m_commands.pop();
		}

		terminal->ExecSystem(data.command.command.c_str());
		++m_nExecuted;
    }
	
	data.bRunning = false;
}

#endif /* !__DELTAMAKE_WORKERPOOL_H__ */