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

#define DELTAMAKE_VERSION_MAJOR			3
#define DELTAMAKE_VERSION_MINOR			1
#define DELTAMAKE_VERSION_PATCH			2

#define DELTAMAKE_CONFIG_FILENAME		"solution.json"
#define DELTAMAKE_DIFF_FILENAME			"deltamake.json"

#define DELTAMAKE_MAX_WORKER_TITLE		32
 
// ******************************************************************************** //

										//										//
namespace DeltaMake {
	/**
	 * Thread pool interface
	 */
	class IWorkerPool {
		public:
			IWorkerPool&				operator=(const IWorkerPool&)			= delete;

			/**
			 * Add command to queue
			 */
			virtual bool				AddCommand(const std::string& command, const char title[]) = 0;

		protected:
			virtual						~IWorkerPool()							= default;
	};

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
			 * Generate command list for workers
			 * 
			 * \returns number of commands to execute
			 */
			virtual size_t				Build(IWorkerPool* pool)				= 0;

			/**
			 * Link all fancy stuff and do some magic of the End
			 */
			virtual bool				PostBuild()								= 0;

		protected:
			virtual						~IBuild()								= default;
	};

	/**
	 * Solution basic interface
	 */
	class ISolution {
		public:
			ISolution&					operator=(const ISolution&)				= delete;

			/**
			 * Fabric with `version` and `type` check
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
	 * Global config
	 */
	struct SConfig {
		ISolution*						root;

		std::vector<const char*>		builds;

		struct sigaction				oldHandler;

		bool							bVerbose								= false;
		bool							bNoBuild								= false;
		bool							bScan									= false;
		bool							bForce									= false;
		bool							bDontSaveDiff							= false;

		size_t							nMaxWorkers								= 0;
		size_t							nCores									= 1;
	};

	extern SConfig* const config;

	/**
	 * Log level
	 */
	enum ELogLevel {
		LOG_INFO,						/* Default */
		LOG_DETAIL,						/* With `-v` flag */
		LOG_WARNING,					/* Yellow */
		LOG_ERROR,						/* Red */
	};
	
	/**
	 * Terminal wrapper
	 */
	class ITerminal {
		public:
			ITerminal&					operator=(const ITerminal&)				= delete;

			virtual void				MoveUp(size_t offset)					= 0;
			virtual void				MoveDown(size_t offset)					= 0;
			virtual void				MoveRight(size_t offset)				= 0;
			virtual void				MoveLeft(size_t offset)					= 0;

			virtual void				Flush()									= 0;
			virtual void				ShowCursor(bool bShow)					= 0;

			virtual int					Log(ELogLevel level, const char format[], ...) = 0;

			virtual size_t				GetColumns() const						= 0;
			virtual size_t				GetRows() const							= 0;

			virtual int					ExecSystem(const char cmd[])			= 0;
			virtual time_t				GetLastModificationTime(const char path[]) = 0;
		
		protected:
			virtual						~ITerminal()							= default;
	};

	extern ITerminal* const terminal;
}

#endif /* !__DELTA_MAKE_H__ */