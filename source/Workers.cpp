/**
 * \file	Workers.cpp
 * \brief	Thread wizardry
 * \date	1 jul 2025
 * \author	Reklov
 */
#include "Workers.h"

#include <stdio.h>

#include <poll.h> // TODO: Win impl
#include <paths.h>
#include <wait.h>

#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>

#include "Terminal.h"

using namespace DeltaMake;

// ******************************************************************************** //

#if defined(__linux__) // TODO: Win impl

#define PROCESS_POLL_OUT				0
#define PROCESS_POLL_ERR				1

#define PROCESS_CLOSE_PIPE(pipe) \
	{ close(pipe); pipe = 0; }

#define PROCESS_REDIRECT_OUTPUT_PIPE(pipe, std) { \
		PROCESS_CLOSE_PIPE(pipe[0]); \
		if (pipe[1] != std) { \
			dup2(pipe[1], std); \
			PROCESS_CLOSE_PIPE(pipe[1]); \
		} \
	}


extern char** environ; // For POSIX.1

/**
 * Wrapper for system process and output streams
 */
class CProcess final {
	public:
										CProcess();
										~CProcess();

		bool							Exec(const char command[], int& rReturnStatus);

		bool							Kill();

		const std::string&				GetOutBuffer() const;
		const std::string&				GetErrBuffer() const;

	private:

		bool							Clear();
		bool							CheckPollFD(size_t index);

		std::string						m_outBuffer;
		std::string						m_errBuffer;

		int								m_outPipe[2]							= { 0 };
		int								m_errPipe[2]							= { 0 };

		pid_t							m_pid									= -1;
		pollfd							m_pfd[2]								= { 0 }; // { `stdout`, `stderr` }
		int								m_status								= -1;
};

/**
 * SIGINT catcher
 */
namespace SignalInterruptCatcher {
	void								Init();

	/**
	 * Gentle SIGINT catcher
	 */
	static void							FirstHandler(int signal);

	/**
	 * Brutal SIGINT catcher
	 */
	static void							SecondHandler(int signal);


	static struct sigaction					oldHandler;
}

#endif

// ******************************************************************************** //
										//										//

/**
 * Task type
 */
enum ETaskType {
	COMMAND,
	BARRIER,
	END,
};

/**
 * Scheduler task
 */
class ITask {
	public:
		virtual							~ITask()								= default;
		
		virtual const char*				GetTitle() const						= 0;
		virtual ETaskType				GetType() const							= 0;
		virtual bool					Execute()								= 0;
};

/**
 * Execution queue barrier
 */
class CBarrierTask final : public ITask {
	public:
										CBarrierTask(int value);
		virtual							~CBarrierTask()	override				= default;
		
		virtual const char*				GetTitle() const override;
		virtual ETaskType				GetType() const override;

		/**
		 * Wait untill `m_counter >= m_value`
		 */
		virtual bool					Execute() override;

		/**
		 * Sets `m_counter` to `m_value`
		 */
		void							Skip();

		bool							IsDone() const;

	protected:
		const char						m_title[14]								= DELTAMAKE_BARRIER_TITLE;

		std::atomic_int					m_counter								= 0;
		const int						m_value;
};

/**
 * System command
 */
class CCommandTask final : public ITask {
	public:
										CCommandTask(const char title[], const std::string& command, bool bFailIfNonZero);
		virtual							~CCommandTask()	override				= default;
		
		virtual const char*				GetTitle() const override;
		virtual ETaskType				GetType() const override;
		virtual bool					Execute() override;

		/**
		 * \warning Return is not valid until `Execute()` is called
		 * \returns Exit status (`return` or `exit()`) of command
		 */
		int								GetReturnValue() const;


		const CProcess&					GetProcess() const;
		void							KillProcess();

	private:
		const std::string				m_title;
		const std::string				m_command;
		const bool						m_bFailIfNonZero;
		
		int								m_returnValue;

		CProcess						m_process;
};

/**
 * Worker Status
 */
enum class EWorkerStatus : int {
	WAIT_TASK,
	WORKING,
	FAIL,
	STOPPED
};

/**
 * Worker Data
 */
struct SWorker {
	/* Critical section */

	std::mutex							mutex;
	ITask*								task									= nullptr;

	/* Not so critical section */

	std::atomic<EWorkerStatus>			status									= EWorkerStatus::WAIT_TASK;

	std::thread							thread;
};

/**
 * Worker thread entry point
 */
void WorkerRoutine(SWorker* worker);

/**
 * Worker Status
 */
enum class ESchedulerStatus : int {
	IDLE,
	RUNNING,
	STOPPING,
	KILLING,
};

/**
 * CSchedulerLocal
 */
class CSchedulerLocal final : public IScheduler, public ITaskList {
	public:
		virtual							~CSchedulerLocal() override;

		virtual void					Init(size_t nWorkers) override;

		virtual ITaskList*				GetList() override;

		virtual void					Start() override;

		virtual void					Stop() override;
		virtual void					Kill() override;


		virtual void					AddCommand(const char title[], const std::string& command, bool bFailIfNonZero = true) override;
		virtual void					AddBarrier() override;
		virtual size_t					GetTaskCount() const override;

	private:
		void							KillWorkerTask(SWorker* worker);
		void							GiveWorkerTask(SWorker* worker);

		void							ShowCommandStatus(SWorker* worker);
		void							UpdateStatus();
		bool							CheckRunning() const;
		char							GetSpinner(const SWorker* worker) const;

		std::vector<ITask*>				m_tasks;
		size_t							m_nextTask								= 0;

		std::vector<SWorker*>			m_workers;

		ESchedulerStatus				m_status								= ESchedulerStatus::IDLE;

		size_t							m_spinnerIndex							= 0;
		size_t							m_topOffset								= 0;
};

CSchedulerLocal g_schedulerLocal;
extern DeltaMake::IScheduler* const DeltaMake::scheduler = &g_schedulerLocal;

// ******************************************************************************** //

/* ****************************************
 * CSchedulerLocal::~CSchedulerLocal
 */
CSchedulerLocal::~CSchedulerLocal() {
	for (size_t i = 0; i < m_workers.size(); ++i)
		delete m_workers[i];
}

/* ****************************************
 * CSchedulerLocal::Init
 */
void CSchedulerLocal::Init(size_t nWorkers) {
	m_workers.resize(nWorkers);
	for (size_t i = 0; i < m_workers.size(); ++i)
		m_workers[i] = new SWorker();
}

/* ****************************************
 * CSchedulerLocal::GetList
 */
ITaskList* CSchedulerLocal::GetList() {
	return this;
}

/* ****************************************
 * CSchedulerLocal::Start
 */
void CSchedulerLocal::Start() {
	/*
	for (size_t i = 0; i < 10; ++i) {
		char buff[128];
		sprintf(buff, "Task A: %i", static_cast<int>(i));

		AddCommand(buff, "echo \"hello A\nX\nV\"; sleep 1");
	}

	AddBarrier();

	for (size_t i = 0; i < 10; ++i) {
		char buff[128];
		sprintf(buff, "Task B: %i", static_cast<int>(i));

		if (i % 8 == 0)
			AddBarrier();

		AddCommand(buff, "echo \"hello B\"; gcc");
	}*/

	if (m_tasks.size() == 0) {
		terminal->Log(LOG_WARNING, "Scheduler task list is empty! Abort start.\n");
		return;
	}

	SignalInterruptCatcher::Init();

	// Start the worker threads
	for (size_t i = 0; i < m_workers.size(); ++i) {
		m_workers[i]->thread = std::thread(WorkerRoutine, m_workers[i]);
	}

	terminal->ShowCursor(false);
	m_status = ESchedulerStatus::RUNNING;
	while (true) {
		std::this_thread::sleep_for(std::chrono::milliseconds(DELTAMAKE_SCHEDULER_DELAY)); // Let's not bother the CPU too much

		size_t nStopped = 0;
		for (size_t i = 0; i < m_workers.size(); ++i) {
			SWorker* worker = m_workers[i];

			const EWorkerStatus status = worker->status;
			switch (status) {
				case EWorkerStatus::WORKING:
					if (m_status != ESchedulerStatus::RUNNING) {
						ITask* task = worker->task;
						if (task->GetType() == ETaskType::BARRIER)
							static_cast<CBarrierTask*>(task)->Skip();
					}

					if (m_status == ESchedulerStatus::KILLING)
						KillWorkerTask(worker);

					break;
				
				case EWorkerStatus::WAIT_TASK:
					GiveWorkerTask(worker);
					break;

				case EWorkerStatus::FAIL:
					if (m_status != ESchedulerStatus::STOPPING)
						Stop();

					++nStopped;
					break;

				case EWorkerStatus::STOPPED:
					++nStopped;
					break;

				default:
					break;
			}
		}

		if (nStopped == m_workers.size())
			break;

		UpdateStatus();
	}

	// Let's show failed workers' log
	for (size_t i = 0; i < m_workers.size(); ++i) { 
		SWorker* worker = m_workers[i];
		if (worker->status == EWorkerStatus::FAIL) {
			if (worker->task != nullptr)
				ShowCommandStatus(worker); // If it is a command task
		}
		else
			worker->status = EWorkerStatus::STOPPED;
	}

	UpdateStatus();

	// Wait for everyone to end tasks
	for (size_t i = 0; i < m_workers.size(); ++i) {
		if (m_workers[i]->thread.joinable() == true)
			m_workers[i]->thread.join();
	}

	m_status = ESchedulerStatus::IDLE;
	UpdateStatus();

	// Clearing
	for (size_t i = 0; i < m_workers.size(); ++i)
		delete m_workers[i];

	for (size_t i = 0; i < m_tasks.size(); ++i)
		delete m_tasks[i];

	m_workers.clear();
	m_tasks.clear();
	m_nextTask = 0;

	// Restoring
	terminal->ShowCursor(true);
}

/* ****************************************
 * CSchedulerLocal::Stop
 */
void CSchedulerLocal::Stop() {
	m_status = ESchedulerStatus::STOPPING;
	m_nextTask = m_tasks.size(); // Yeap, like this
}

/* ****************************************
 * CSchedulerLocal::Kill
 */
void CSchedulerLocal::Kill() {
	Stop();

	m_status = ESchedulerStatus::KILLING;
}

/* ****************************************
 * CSchedulerLocal::ShowCommandStatus
 */
void CSchedulerLocal::ShowCommandStatus(SWorker* worker) { // TODO: Enhance cleaning to reduce flickering
	if (worker->task == nullptr)
		return;

	if (worker->task->GetType() != ETaskType::COMMAND)
		return;

	const CCommandTask* command = static_cast<CCommandTask*>(worker->task);
	const CProcess& process = command->GetProcess();
	const std::string& rOut = process.GetOutBuffer();
	const std::string& rErr = process.GetErrBuffer();

	if ((rOut.length() == 0) && (rErr.length() == 0))
		return;
	
	terminal->MoveUp(m_topOffset);
	terminal->MoveLeft(terminal->GetColumns());
	terminal->ClearDown();
	terminal->Flush();

	int oldX;
	int oldY;
	terminal->GetCursorPosition(oldX, oldY);

	if (rOut.length() != 0) {
		terminal->Log(LOG_INFO, "%s | %s", command->GetTitle(), rOut.c_str());
		if (rOut[rOut.size() - 1] != '\n')
			terminal->Write("\n");
	}

	if (rErr.length() != 0) {
		terminal->Log(LOG_ERROR, "%s | %s", command->GetTitle(), rErr.c_str());
		if (rErr[rErr.size() - 1] != '\n')
			terminal->Write("\n");
	}

	int newX;
	int newY;
	terminal->Flush();
	terminal->GetCursorPosition(newX, newY);
	if (oldY == newY) // Same line so not full line
		++newY;

	const size_t offset = static_cast<size_t>(newY - oldY);

	if (offset >= m_topOffset) // TODO: correct?
		m_topOffset = 0;
	else
		m_topOffset -= offset;
	
	terminal->MoveDown(m_topOffset);
	
	UpdateStatus(); // To reduce flickering
}

/* ****************************************
 * CSchedulerLocal::AddCommand
 */
void CSchedulerLocal::AddCommand(const char title[], const std::string& command, bool bFailIfNonZero) {
	if (CheckRunning() == true)
		return;

	ITask* task = new CCommandTask(title, command, bFailIfNonZero);
	m_tasks.push_back(task);

	terminal->Log(LOG_DETAIL, "%s:\n\t%s\n", title, command.c_str());
}

/* ****************************************
 * CSchedulerLocal::GetTaskCount
 */
size_t CSchedulerLocal::GetTaskCount() const {
	return m_tasks.size();
}

/* ****************************************
 * CSchedulerLocal::AddBarrier
 */
void CSchedulerLocal::AddBarrier() {
	if (CheckRunning() == true)
		return;

	ITask* task = new CBarrierTask(m_workers.size());
	m_tasks.push_back(task);

	terminal->Log(LOG_DETAIL, DELTAMAKE_BARRIER_TITLE "\n");
}

/* ****************************************
 * CSchedulerLocal::KillWorkerTask
 */
inline void CSchedulerLocal::KillWorkerTask(SWorker* worker) {
	std::lock_guard<std::mutex> workerLock(worker->mutex);

	ITask* task = worker->task;
	if (task->GetType() == ETaskType::COMMAND)
		static_cast<CCommandTask*>(task)->KillProcess();

	worker->status = EWorkerStatus::FAIL;
}

/* ****************************************
 * CSchedulerLocal::GiveWorkerTask
 */
void CSchedulerLocal::GiveWorkerTask(SWorker* worker) {
	if (m_nextTask == m_tasks.size()) { // No task left
		worker->task = nullptr;
	}
	else {
		ITask* current = m_tasks[m_nextTask];

		ShowCommandStatus(worker); // Let's show log of previous command task
		
		// Let's work the work
		{
			std::lock_guard<std::mutex> workerLock(worker->mutex);
			worker->task = ((m_nextTask != m_tasks.size()) ? m_tasks[m_nextTask] : nullptr);
		}

		if (current->GetType() == ETaskType::BARRIER) {
			CBarrierTask* barrier = static_cast<CBarrierTask*>(current);

			if (barrier->IsDone() == true) // Next task only after all workers are waited
				++m_nextTask;
		}
		else {
			++m_nextTask; // Or just because task is not a barrier
		}
	}

	worker->status = EWorkerStatus::WORKING;
}

/* ****************************************
 * CSchedulerLocal::UpdateStatus
 */
inline void CSchedulerLocal::UpdateStatus() {
	++m_spinnerIndex; // Get rotated

	terminal->UpdateSize();

	const size_t nWorkers			= m_workers.size();
	const size_t columns			= terminal->GetColumns();
	const size_t minWorkerSize		= 4 + DELTAMAKE_MIN_WORKER_TITLE; // `[X] ` + title
	const size_t maxWorkersInLine	= columns / minWorkerSize;
	const size_t nWorkerLines		= nWorkers / maxWorkersInLine + ((nWorkers % maxWorkersInLine != 0) ? 1 : 0) + 1; // Workers + status
	const size_t maxTitleSize		= DELTAMAKE_MIN_WORKER_TITLE + (columns - maxWorkersInLine * minWorkerSize) / maxWorkersInLine;

	if (nWorkerLines > m_topOffset) { // Add lines to fit
		const size_t n = nWorkerLines - m_topOffset;
		for (size_t i = 0; i < n; ++i)
			terminal->Log(LOG_INFO, "\n");

		m_topOffset = nWorkerLines;
	}

	terminal->MoveUp(m_topOffset);
	terminal->MoveLeft(columns);
	
	size_t nInLine = 0;
	for (size_t i = 0; i < m_workers.size(); ++i) {
		std::lock_guard<std::mutex> workerLock(m_workers[i]->mutex);

		ITask* task = m_workers[i]->task;

		const char* title = "";
		if (task != nullptr)
			title = task->GetTitle();

		terminal->Log(
			LOG_INFO, "[%c] %-*s",
			GetSpinner(m_workers[i]),
			maxTitleSize,
			title
		);

		++nInLine;
		if (nInLine == maxWorkersInLine) {
			nInLine = 0;
			terminal->Log(LOG_INFO, "\n\r");
		}	
	}

	if (nInLine != 0)
		terminal->Log(LOG_INFO, "\n\r");

	
	switch (m_status) {
		case ESchedulerStatus::IDLE:
			terminal->ClearDown();
			terminal->Log(LOG_INFO, "Ready.\n\r");
			break;
		case ESchedulerStatus::RUNNING:
			terminal->Log(LOG_INFO, "[%3zu/%-3zu]\n\r", m_nextTask, m_tasks.size());
			break;
		case ESchedulerStatus::STOPPING:
			terminal->Log(LOG_INFO, "Stopping workers...\n\r");
			break;
		case ESchedulerStatus::KILLING:
			terminal->Log(LOG_INFO, "Zat vas doctor-assisted homicide!\n\r");
			break;
	
		default:
			terminal->Log(LOG_INFO, "bruh?\n\r");
	}

	terminal->Flush();
}

/* ****************************************
 * CSchedulerLocal::CheckRunning
 */
inline bool CSchedulerLocal::CheckRunning() const {
	const bool bRunning = (m_status == ESchedulerStatus::RUNNING);

	if (bRunning)
		terminal->Log(LOG_WARNING, "Scheduler is running!\n");

	return bRunning;
}

/* ****************************************
 * CSchedulerLocal::GetSpinner
 */
inline char CSchedulerLocal::GetSpinner(const SWorker* worker) const {
	const EWorkerStatus status = worker->status;

	switch (status) {
		case EWorkerStatus::WAIT_TASK:	return '*';
		case EWorkerStatus::WORKING:	return "-\\|/"[m_spinnerIndex % 4];
		case EWorkerStatus::FAIL:		return 'X';
		case EWorkerStatus::STOPPED:	return '=';
		
		default:
			return '?';
	}
}

// ******************************************************************************** //

/* ****************************************
 * CBarrierTask::CBarrierTask
 */
CBarrierTask::CBarrierTask(int value) : m_value(value) {
}

/* ****************************************
 * CBarrierTask::GetTitle
 */
const char* CBarrierTask::GetTitle() const {
	return m_title;
}

/* ****************************************
 * CBarrierTask::GetType
 */
ETaskType CBarrierTask::GetType() const {
	return ETaskType::BARRIER;
}

/* ****************************************
 * CBarrierTask::Execute
 */
bool CBarrierTask::Execute() {
	++m_counter; // Another one in the loop

	// The scheduler will set `m_value` to the number of workers so wait until all workers are waiting
	while (m_counter < m_value) {
		std::this_thread::sleep_for(std::chrono::milliseconds(DELTAMAKE_BARRIER_DELAY));
		// Let's do nothing... or some sort of nothing
		// After all, synchronization is the scheduler's problem
	}

	return true; // But the nothing executed good
}

/* ****************************************
 * CBarrierTask::Skip
 */
void CBarrierTask::Skip() {
	m_counter = m_value;
}

/* ****************************************
 * CBarrierTask::IsDone
 */
bool CBarrierTask::IsDone() const {
	return m_counter == m_value;
}

// ******************************************************************************** //

/* ****************************************
 * CCommandTask::CCommandTask
 */
CCommandTask::CCommandTask(const char title[], const std::string& command, bool bFailIfNonZero) :
	m_title(title), m_command(command), m_bFailIfNonZero(bFailIfNonZero) { }

/* ****************************************
 * CCommandTask::GetTitle
 */
const char* CCommandTask::GetTitle() const {
	return m_title.c_str();
}

/* ****************************************
 * CCommandTask::GetType
 */
ETaskType CCommandTask::GetType() const {
	return ETaskType::COMMAND;
}

/* ****************************************
 * CCommandTask::Execute
 */
bool CCommandTask::Execute() {
	// Once upon a time, an implementation with `popen()` was used
	// But it was not perfect enough for me

	if (m_process.Exec(m_command.c_str(), m_returnValue) == false)
		return false;

	if (m_bFailIfNonZero == true)
		return m_returnValue == 0;

	return true;
}

/* ****************************************
 * CCommandTask::GetReturnValue
 */
int CCommandTask::GetReturnValue() const {
	return m_returnValue;
}

/* ****************************************
 * CCommandTask::KillProcess
 */
void CCommandTask::KillProcess() {
	m_process.Kill();
}

/* ****************************************
 * CCommandTask::GetProcess
 */
const CProcess& CCommandTask::GetProcess() const {
	return m_process;
}

// ******************************************************************************** //

/* ****************************************
 * WorkerRoutine
 */
void WorkerRoutine(SWorker* worker) {
	while (true) {
		worker->status = EWorkerStatus::WAIT_TASK;

		while (worker->status == EWorkerStatus::WAIT_TASK); // Wait for a task
		if (worker->task == nullptr)
			break;

		worker->status = EWorkerStatus::WORKING;
		const bool bStatus = worker->task->Execute();
		if (bStatus == false) {
			worker->status = EWorkerStatus::FAIL;
			return;
		}
	}

	worker->status = EWorkerStatus::STOPPED;
}

// ******************************************************************************** //

#if defined(__linux__) // TODO: Win impl

/* ****************************************
 * CProcess::CProcess
 */
CProcess::CProcess() {
}

/* ****************************************
 * CProcess::~CProcess
 */
CProcess::~CProcess() {
}

/* ****************************************
 * CProcess::Clear
 */
bool CProcess::Clear() {
	pid_t pid = 0;

	if (m_pid > 0) {
		do {
			pid = waitpid(m_pid, &m_status, 0);
		} while (pid == -1 && errno == EINTR);
		
		m_pid = 0;
	}

	for (size_t i = 0; i < 2; ++i) {
		if (m_outPipe[i] != 0)
			close(m_outPipe[i]);

		m_outPipe[i] = 0;
	}
	

	for (size_t i = 0; i < 2; ++i) {
		if (m_errPipe[i] != 0)
			close(m_errPipe[i]);

		m_errPipe[i] = 0;
	}

	return pid != -1;
}

/* ****************************************
 * CProcess::CheckPollFD
 */
bool CProcess::CheckPollFD(size_t index) {
	if (m_pfd[index].revents != 0) {
		if (m_pfd[index].revents & POLLIN) {
			char buffer[DELTAMAKE_POLL_BUFFER_SIZE];
			const ssize_t nCh = read(m_pfd[index].fd, buffer, sizeof(buffer));
			if (nCh == -1) {
				m_errBuffer = "read() failed";

				return false;
			}

			if (nCh != 0) {
				// Yes, I can turn it into hackery
				// `((index == 0) ? &m_rOutBuffer : &m_rErrBuffer)->append(buffer, nCh);`
				// But it's to hard to read later

				if (index == 0) // Process' `stdout`
					m_outBuffer.append(buffer, nCh);
				else // Process' `stderr`
					m_errBuffer.append(buffer, nCh);
			}
		}
		else // POLLERR | POLLHUP
			return false;
	}

	return true;
}

/* ****************************************
 * CProcess::Exec
 */
bool CProcess::Exec(const char command[], int& rReturnStatus) {
	if (pipe(m_outPipe) < 0){
		m_errBuffer = "pipe(m_outPipe) failed";

		return false;
	}
	
	if (pipe(m_errPipe) < 0) {
		Clear();
		m_errBuffer = "pipe(m_errPipe) failed";

		return false;
	}

	// Fork go brrr
	m_pid = fork();
	if (m_pid == -1) { // Bruh
		Clear();
		m_errBuffer = "fork() failed";

		return false;
	}
	else if (m_pid == 0) { // Child
		// Let's ignore SIGINT!
		struct sigaction sa;
		sa.sa_handler = SIG_IGN;
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = 0;

		if (sigaction(SIGINT, &sa, NULL) == -1)
			_exit(127);

		// Let's redirect some pipes
		PROCESS_REDIRECT_OUTPUT_PIPE(m_outPipe, STDOUT_FILENO); // Redirect child's `stdout` to `m_outPipe`
		PROCESS_REDIRECT_OUTPUT_PIPE(m_errPipe, STDERR_FILENO); // Redirect child's `stderr` to `m_errPipe`

		// Let's replace some process
		const char* args[] = { "sh", "-c", command, NULL };
		execve(_PATH_BSHELL, const_cast<char**>(args), environ);
		_exit(127); // As in implementation of `popen()`
	}

	// Parent
	PROCESS_CLOSE_PIPE(m_outPipe[1]);
	PROCESS_CLOSE_PIPE(m_errPipe[1]);
	
	// Events for `poll()`
	m_pfd[0].fd = m_outPipe[0]; // Process' `stdout`
	m_pfd[0].events = POLLIN; // Read condition

	m_pfd[1].fd = m_errPipe[0]; // Process' `stderr`
	m_pfd[1].events = POLLIN; // Read condition

	while (true) {
		const int ready = poll(m_pfd, 2, -1); // Waiting for event with infinite timeout
		if (ready < 0) { // Smh bad happened
			m_errBuffer = "poll() failed";
			Clear();

			return false;
		}

		if (CheckPollFD(PROCESS_POLL_OUT) == false) // Why?
			break;

		CheckPollFD(PROCESS_POLL_ERR); // Don't care
	}

	Clear();

	if (WIFEXITED(m_status) == 0) { // And here's why
		m_errBuffer = "WIFEXITED() is zero";
		
		return false;
	}

	rReturnStatus = WEXITSTATUS(m_status);

	return true;
}

/* ****************************************
 * CProcess::Kill
 */
bool CProcess::Kill() {
	return kill(m_pid, SIGKILL) != -1;
}

/* ****************************************
 * CProcess::GetOutBuffer
 */
const std::string& CProcess::GetOutBuffer() const {
	return m_outBuffer;
}

/* ****************************************
 * CProcess::GetErrBuffer
 */
const std::string& CProcess::GetErrBuffer() const {
	return m_errBuffer;
}

// ******************************************************************************** //

/* ****************************************
 * SignalInterruptCatcher::Init
 */
void SignalInterruptCatcher::Init() {
	struct sigaction sa;

	sa.sa_handler = SignalInterruptCatcher::FirstHandler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	sigaction(SIGINT, &sa, &oldHandler);
}

/* ****************************************
 * SignalInterruptCatcher::FirstHandler
 */
void SignalInterruptCatcher::FirstHandler(int signal) {
	struct sigaction sa;

	sa.sa_handler = SignalInterruptCatcher::SecondHandler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	sigaction(SIGINT, &sa, NULL);

	g_schedulerLocal.Stop();
}

/* ****************************************
 * SignalInterruptCatcher::SecondHandler
 */
void SignalInterruptCatcher::SecondHandler(int signal) {
	sigaction(SIGINT, &oldHandler, NULL);

	g_schedulerLocal.Kill();
}

#endif
