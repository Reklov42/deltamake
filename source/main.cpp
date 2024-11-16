/**
 * \file	main.cpp
 * \brief	DeltaMake entry point
 * \date	22 oct 2024
 * \author	Reklov
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include <vector>
#include <map>
#include <thread>

#include "deltamake.h"
#include "Exception.h"
#include "Solutions.h"
#include "WorkerPool.h"

using namespace DeltaMake;

// ******************************************************************************** //

DeltaMake::SConfig g_config;
extern DeltaMake::SConfig* const DeltaMake::config = &g_config;

CWorkerPool g_pool;

// ******************************************************************************** //
										//										//

/**
 * Input argument stream-like wrapper
 */
class CArgStream {
	public:
										CArgStream(int argc, char* argv[]);

	const char*							GetCurret() const;
	const char*							GetNext();
	bool								IsLast() const;

	private:
		const int						m_argc;
		const char* const * const		m_argv;

		int								m_index									= 0;
		const char*						m_arg;
};

/**
 * 
 */
class CTerminalLocal final : public DeltaMake::ITerminal {
	public:
		virtual							~CTerminalLocal() override				= default;

		void							Init();

		virtual void					MoveUp(size_t offset) override;
		virtual void					MoveDown(size_t offset) override;
		virtual void					MoveRight(size_t offset) override;
		virtual void					MoveLeft(size_t offset) override;

		virtual void					Flush() override;
		virtual void					ShowCursor(bool bShow) override;

		virtual int						Log(ELogLevel level, const char format[], ...) override;

		virtual size_t					GetColumns() const override;
		virtual size_t					GetRows() const override;

		virtual int						ExecSystem(const char cmd[]) override;
		virtual time_t					GetLastModificationTime(const char path[]) override;

	private:
		size_t							m_nColumns;
		size_t							m_nRows;
};

CTerminalLocal g_terminalLocal;
extern DeltaMake::ITerminal* const DeltaMake::terminal = &g_terminalLocal;

// ******************************************************************************** //

/**
 * Parse input arguments
 */
void ParseArgs(CArgStream&& stream);

/**
 * If `arg[0] == '-'` then `-name[0]`
 * if `arg[0:1] == '--'` then `--name`
 */
bool CheckArg(const char arg[], const char name[]);

/**
 * Show help info
 */
void PrintHelp();

/**
 * Initialize system stuff
 */
void Init();

/**
 * SIGINT Handler
 */
void IntHandler(int signal);


#include "AutoGen.h"

/**
 * Entry point
 */
int main(int argc, char* argv[]) {
	g_terminalLocal.Init();

	terminal->Log(
		LOG_INFO,
		"DeltaMake v%i.%i.%i [" DELTAMAKE_COMMIT_HASH " on " DELTAMAKE_COMMIT_DATE "]\n",
		DELTAMAKE_VERSION_MAJOR,
		DELTAMAKE_VERSION_MINOR,
		DELTAMAKE_VERSION_PATCH
	);
	ParseArgs(CArgStream(argc, argv));

	//
	Init();

	// Loading
	try {
		g_config.root = ISolution::Load(DELTAMAKE_CONFIG_FILENAME);
		if (g_config.root == nullptr)
			return EXIT_FAILURE;
	}
	catch (const DeltaMake::CConfigValueNotSet& error) {
		terminal->Log(LOG_ERROR, "Value not set: %s\n", error.GetMessage());
		return EXIT_FAILURE;
	}
	catch (const DeltaMake::CFileNotExists& error) {
		terminal->Log(LOG_ERROR, "Can't open file: %s\n", error.GetMessage());
		return EXIT_FAILURE;
	}
	catch (...) {
		terminal->Log(LOG_ERROR, "Unknown exeption!\n");
		return EXIT_FAILURE;
	}

	if (g_config.bScan == true) {
		if (g_config.root->ScanFolders() == false)
			return EXIT_FAILURE;
	}


	// Building
	if (g_config.bNoBuild == true)
		return EXIT_SUCCESS;

	if (g_config.bForce == false)
		g_config.root->LoadDiff(DELTAMAKE_DIFF_FILENAME);

	if (g_config.builds.size() == 0) {
		terminal->Log(LOG_DETAIL, "No builds setted. Default value is used\n");
		g_config.builds.push_back("default");
	}

	terminal->Log(LOG_DETAIL, "Selected builds:\n");
	std::vector<DeltaMake::IBuild*> builders(g_config.builds.size());
	for (size_t i = 0; i < g_config.builds.size(); ++i) {
		builders[i] = g_config.root->GenBuild(g_config.builds[i]);
		if (builders[i] == nullptr) {
			terminal->Log(LOG_ERROR, "Build not found: \"%s\"\n", g_config.builds[i]);
			return EXIT_FAILURE;
		}

		terminal->Log(LOG_DETAIL, "\t\"%s\"\n", g_config.builds[i]);
	}

	for (size_t i = 0; i < g_config.builds.size(); ++i) {
		builders[i]->PreBuild();
		builders[i]->Build(&g_pool);
	}

	if (g_pool.Size() == 0) {
		terminal->Log(LOG_INFO, "Nothing to do.\n");
		return EXIT_SUCCESS;
	}

	// DANGER: THREADS!
	{
		struct sigaction intHandler;

		intHandler.sa_handler = IntHandler;
		sigemptyset(&intHandler.sa_mask);
		intHandler.sa_flags = 0;

		sigaction(SIGINT, &intHandler, &g_config.oldHandler);

		g_pool.Start(g_config.nMaxWorkers);
	}

	for (size_t i = 0; i < g_config.builds.size(); ++i)
		builders[i]->PostBuild();

	if (g_config.bDontSaveDiff == false)
		g_config.root->SaveDiff(DELTAMAKE_DIFF_FILENAME);

	terminal->Log(LOG_INFO, "Done.\n");

	return EXIT_SUCCESS;
}

// ******************************************************************************** //

/* ****************************************
 * CheckArg
 */
bool CheckArg(const char arg[], const char name[]) {
	if ((arg[1] != '-') && strlen(arg) == 2)
		return arg[1] == name[0];
	
	return strcmp(arg + 2, name) == 0;
}

/* ****************************************
 * ParseArgs
 */
void ParseArgs(CArgStream&& stream) {
	if (stream.IsLast() == true) {
		// TODO: ?
	}

	while (stream.GetNext() != nullptr) {
		const char* arg = stream.GetCurret();
		if (arg[0] == '-') {
			if (CheckArg(arg, "verbose"))
				g_config.bVerbose = true;
			else if (CheckArg(arg, "no-build"))
				g_config.bNoBuild = true;
			else if (CheckArg(arg, "force"))
				g_config.bForce = true;
			else if (CheckArg(arg, "dont-save-diff"))
				g_config.bDontSaveDiff = true;
			else if (CheckArg(arg, "workers")) {
				if (stream.GetNext() == nullptr) {
					PrintHelp();
					exit(EXIT_SUCCESS);
				}

				g_config.nMaxWorkers = static_cast<size_t>(atoll(stream.GetCurret()));
				g_config.nMaxWorkers = (g_config.nMaxWorkers == 0) ? 1 : g_config.nMaxWorkers;
			}
			else if (CheckArg(arg, "help"))
				PrintHelp();
			else {
				PrintHelp();
				exit(EXIT_SUCCESS);
			}
		}
		else {
			g_config.builds.push_back(arg);
		}
	}
}

/* ****************************************
 * PrintHelp
 */
void PrintHelp() {
	const char help[] = \
		"Usage:\n" \
		"    deltamake [flags] [build1, build2, ...]\n" \
		"Note:\n" \
		"    If build names are not specified, the \"default\" build name will be used.\n" \
		"flags:\n" \
		"    -d --dont-save-diff\n" \
		"        Don't save differential file\n" \
		"    -f --force\n" \
		"        Force rebuild all solutions (ignore all pre-builds)\n" \
		"    -h --help\n" \
		"        Show this help text\n" \
		"    -n --no-build\n" \
		"        Don't build anything (useful with scan flag)\n" \
		"    -v --verbose\n" \
		"        Enable verbose logging\n" \
		"    -w <count> --workers <count>\n" \
		"        Max number of workers\n";

	terminal->Log(LOG_INFO, "%s\n", help);
}

// ******************************************************************************** //

/* ****************************************
 * CArgStream::CArgStream
 */
CArgStream::CArgStream(int argc, char* argv[]) : m_argc(argc), m_argv(argv) {
	m_arg = m_argv[m_index];
}

/* ****************************************
 * CArgStream::GetCurret
 */
const char* CArgStream::GetCurret() const {
	return m_arg;
}

/* ****************************************
 * CArgStream::GetNext
 */
const char* CArgStream::GetNext() {
	++m_index;
	if (m_index > m_argc) {
		terminal->Log(LOG_ERROR, "Unexpected end of argument stream. Abort\n");
		exit(EXIT_FAILURE);
	}

	m_arg = (m_argc == m_index) ? nullptr : m_argv[m_index];

	return m_arg;
}

/* ****************************************
 * CArgStream::IsLast
 */
bool CArgStream::IsLast() const {
	return m_argc == (m_index + 1);
}

// ******************************************************************************** //

/* ****************************************
 * Init
 */
void Init() {
	terminal->Log(LOG_DETAIL, "Terminal: %zux%zu\n", terminal->GetColumns(), terminal->GetRows());

	//
	g_config.nCores = static_cast<size_t>(std::thread::hardware_concurrency());
	g_config.nCores = (g_config.nCores == 0) ? 1 : g_config.nCores;
	terminal->Log(LOG_DETAIL, "CPU Cores:   %zu\n", g_config.nCores);

	if (g_config.nMaxWorkers == 0)
		g_config.nMaxWorkers = g_config.nCores;
	
	terminal->Log(LOG_DETAIL, "CPU Workers: %zu\n", g_config.nCores);
}

/* ****************************************
 * Init
 */
void IntHandler(int signal) {
	sigaction(SIGINT, &g_config.oldHandler, NULL);

	terminal->ShowCursor(true);
	terminal->Log(LOG_ERROR, "\n\n\n\n\nCaught signal: %i\nStopping workers... ", signal);

	g_pool.Stop();
	terminal->Log(LOG_ERROR, "\nDone. Exit\n");

	exit(EXIT_FAILURE);
}

// ******************************************************************************** //

#if defined(__linux__)
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/stat.h>

/* ****************************************
 * CTerminalLocal::Init
 */
void CTerminalLocal::Init() {
	struct winsize ws;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);

	m_nColumns = static_cast<size_t>(ws.ws_col);
	m_nRows = static_cast<size_t>(ws.ws_row);
}

/* ****************************************
 * CTerminalLocal::MoveUp
 */
void CTerminalLocal::MoveUp(size_t offset) {
	printf("\033[%zuA", offset);
}

/* ****************************************
 * CTerminalLocal::MoveDown
 */
void CTerminalLocal::MoveDown(size_t offset) {
	printf("\033[%zuB", offset);
}

/* ****************************************
 * CTerminalLocal::MoveRight
 */
void CTerminalLocal::MoveRight(size_t offset) {
	printf("\033[%zuC", offset);
}

/* ****************************************
 * CTerminalLocal::MoveLeft
 */
void CTerminalLocal::MoveLeft(size_t offset) {
	printf("\033[%zuD", offset);
}

/* ****************************************
 * CTerminalLocal::Flush
 */
void CTerminalLocal::Flush() {
	fflush(stdout);
}

/* ****************************************
 * CTerminalLocal::ShowCursor
 */
void CTerminalLocal::ShowCursor(bool bShow) {
	printf((bShow == true) ? "\033[?25h" : "\033[?25l");
}

/* ****************************************
 * CTerminalLocal::Log
 */
int CTerminalLocal::Log(ELogLevel level, const char format[], ...) {
	if ((level == ELogLevel::LOG_DETAIL) && (g_config.bVerbose == false))
		return 0;

	FILE* const out = (level == ELogLevel::LOG_ERROR) ? stderr : stdout;

	switch (level) {
		case ELogLevel::LOG_ERROR:		fputs("\033[0;31m", out); break; // Red
		case ELogLevel::LOG_WARNING:	fputs("\033[0;31m", out); break; // Yellow
		case ELogLevel::LOG_DETAIL:		fputs("\033[0;36m", out); break; // Cyan
	
		default:						fputs("\033[0m", out); break; // Reset
	}

	va_list argptr;
	va_start(argptr, format);
	int size = vfprintf(out, format, argptr);
	va_end(argptr);

	fputs("\033[0m", out); // Reset color

	return size;
}


/* ****************************************
 * CTerminalLocal::ExecSystem
 */
int CTerminalLocal::ExecSystem(const char cmd[]) {
	int status = system(cmd);
    status = WIFEXITED(status) ? WEXITSTATUS(status) : 0;
	
	// TODO: Save exit
	if (status != 0)
		exit(EXIT_FAILURE);

	return status;
}

/* ****************************************
 * CTerminalLocal::GetLastModificationTime
 */
time_t CTerminalLocal::GetLastModificationTime(const char path[]) {
	struct stat stats;
	stat(path, &stats);
	
	return stats.st_mtime;
}

#endif

/* ****************************************
 * CTerminalLocal::GetColumns
 */
size_t CTerminalLocal::GetColumns() const {
	return m_nColumns;
}

/* ****************************************
 * CTerminalLocal::GetRows
 */
size_t CTerminalLocal::GetRows() const {
	return m_nRows;
}