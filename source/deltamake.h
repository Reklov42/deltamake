/**
 * \file	deltamake.h
 * \brief	DeltaMake interface
 * \date	22 oct 2024
 * \author	Reklov
 */
#ifndef __DELTAMAKE_H__
#define __DELTAMAKE_H__

#include <stddef.h>
#include <signal.h>

#include <string>
#include <vector>
#include <filesystem>

#include <json/json.h>


#define DELTAMAKE_VERSION_MAJOR			3
#define DELTAMAKE_VERSION_MINOR			5
#define DELTAMAKE_VERSION_PATCH			1

#define DELTAMAKE_CONFIG_FILENAME		"solution.json"
#define DELTAMAKE_DIFF_FILENAME			"deltamake.json"

#define DELTAMAKE_MIN_WORKER_TITLE		32
#define DELTAMAKE_SCHEDULER_DELAY		80 // ms
#define DELTAMAKE_BARRIER_DELAY			10 // ms

#define DELTAMAKE_BARRIER_TITLE			"-= BARRIER =-"

#define DELTAMAKE_POLL_BUFFER_SIZE		512


// ******************************************************************************** //

										//										//
namespace DeltaMake {
	/**
	 * Build basic interface
	 */
	class IBuild {
		public:
			IBuild&						operator=(const IBuild&)				= delete;

			/**
			 * Do some magic of the Begin
			 */
			virtual bool				PreBuild()								= 0;

			/**
			 * Generate command task list
			 * 
			 * \returns Number of commands to execute
			 */
			virtual size_t				Build(class ITaskList* taskList)		= 0;

			/**
			 * Link all fancy stuff and do some magic of the End
			 */
			virtual bool				PostBuild()								= 0;

		protected:
			virtual						~IBuild()								= default;
	};

	/**
	 * Build plugin factory (allocator) basic interface
	 *//* TODO: ?
	class IBuildFactory {
		public:
			IBuildFactory&				operator=(const IBuildFactory&)		= delete;

			virtual IBuildFactory*		NewBuild(...) const = 0;
		protected:
			virtual						~IBuildFactory()						= default;
	};*/


	/**
	 * Solution basic interface
	 */
	class ISolution {
		public:
			ISolution&					operator=(const ISolution&)				= delete;

			/**
			 * Factory with `version` and `type` check
			 */
			static ISolution*			Load(const char path[]);

			/**
			 * Scan `paths.scan` for file types
			 */
			virtual bool				ScanFolders()							= 0;

			/**
			 * Generate `IBuild` or 'nullptr` if build not found
			 */
			virtual IBuild*				GenBuild(const char build[]) 			= 0;

			/**
			 * Load differential file
			 */
			virtual bool				LoadDiff(const char path[])				= 0;

			/**
			 * Save differential file
			 */
			virtual bool				SaveDiff(const char path[])				= 0;

		protected:
			virtual						~ISolution()							= default;
	};

	/**
	 * Plugin type
	 */
	enum class EPluginType {
		SOLUTION,
		BUILD,
	};

	/**
	 * Iternal plugin interface
	 */
	class IPlugin {
		public:
			IPlugin&					operator=(const IPlugin&)				= delete;

			/**
			 * Do some magic of the Begin
			 */
			virtual EPluginType			GetType() const							= 0;

		protected:
			virtual						~IPlugin()								= default;

	};

	/**
	 * Solution plugin factory (allocator) basic interface
	 */
	class ISolutionFactory : public IPlugin {
		public:
			ISolutionFactory&			operator=(const ISolutionFactory&)		= delete;

			virtual EPluginType			GetType() const override final			{ return EPluginType::SOLUTION; }

			/**
			 * \return Solution `type` name
			 */
			virtual const std::string&	GetName() const							= 0;

			/**
			 * \returns newly allocated solution
			 */
			virtual ISolution*			NewSolution(Json::Value& root, std::filesystem::path& currentPath) = 0;
		protected:
			virtual						~ISolutionFactory()						= default;
	};


	/**
	 * Global config
	 */
	struct SConfig {
		ISolution*						root;

		std::vector<const char*>		builds;
		std::map<std::string, ISolutionFactory*> solutionTypes;
		//std::vector<std::string, IBuildFactory*> buildTypes;

		bool							bVerbose								= false;
		bool							bNoBuild								= false;
		bool							bScan									= false;
		bool							bForce									= false;
		bool							bDontSaveDiff							= false;

		size_t							nMaxWorkers								= 0;
		size_t							nCores									= 1;
	};

	extern SConfig* const config;
}

#endif /* !__DELTA_MAKE_H__ */