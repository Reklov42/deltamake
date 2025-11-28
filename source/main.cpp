/**
 * \file	main.cpp
 * \brief	DeltaMake entry point
 * \date	22 oct 2024
 * \author	Reklov
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <vector>
#include <map>
#include <thread>

#include "deltamake.h"
#include "Terminal.h"

#include "Exception.h"
#include "SolutionDefault.h"
#include "Workers.h"

using namespace DeltaMake;

// ******************************************************************************** //

DeltaMake::SConfig g_config;
extern DeltaMake::SConfig* const DeltaMake::config = &g_config;

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

// ******************************************************************************** //

/**
 * Parse input arguments
 */
void ParseArgs(CArgStream&& stream);

/**
 * If `arg[0] == '-'` then `'-'name[0]`
 * if `arg[0:1] == '--'` then `'--'name`
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
 * Register plugin factory
 */
void RegisterPlugin(IPlugin* plugin);


#include "AutoGen.h"

/**
 * Entry point
 */
int main(int argc, char* argv[]) {
	terminal->Init();

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
	LoadPlugins();



	DeltaMake::scheduler->Init(g_config.nMaxWorkers);
	//DeltaMake::scheduler->Start();

	//return 0;

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

	ITaskList* taskList = scheduler->GetList();

	for (size_t i = 0; i < g_config.builds.size(); ++i) {
		builders[i]->PreBuild();
		builders[i]->Build(taskList);
	}

	if (taskList->GetTaskCount() == 0) {
		terminal->Log(LOG_INFO, "Nothing to do.\n");
		return EXIT_SUCCESS;
	}

	// DANGER: THREADS!
	DeltaMake::scheduler->Start();

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
void RegisterPlugin(IPlugin* plugin) {
	const EPluginType type = plugin->GetType();

	switch (type) {
		case EPluginType::SOLUTION: {
				ISolutionFactory* factory = static_cast<ISolutionFactory*>(plugin);
				g_config.solutionTypes[factory->GetName()] = factory;
				terminal->Log(LOG_DETAIL, "Solution plugin loaded: %s\n", factory->GetName().c_str());
			}
			break;
		
		default:
			terminal->Log(LOG_WARNING, "Unknown plugin type (%i). Ignoring...\n", static_cast<int>(type));
			break;
	}
}
