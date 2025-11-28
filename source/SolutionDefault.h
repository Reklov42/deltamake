/**
 * \file	SolutionDefault.h
 * \brief	DeltaMake default solutions
 * \date	27 oct 2024
 * \author	Reklov
 */
#ifndef __DELTAMAKE_SOLUTION_DEFAULT_H__
#define __DELTAMAKE_SOLUTION_DEFAULT_H__

#include <stddef.h>

#include <filesystem>
#include <vector>
#include <map>
#include <string>

#include <json/json.h>

#include "deltamake.h"
#include "Exception.h"
#include "Workers.h"
 
// ******************************************************************************** //

										//										//
namespace DeltaMake {
	/**
	 * Source code file
	 */
	struct SSourceFile {
		std::filesystem::path			path;
		time_t							mtime; /* Last modification time in diff file */
		bool							bToCompile;
	};

	/**
	 * Default solution
	 */
	class CSolutionDefault : public ISolution {
		public:
										CSolutionDefault(Json::Value root, std::filesystem::path currentPath);
			virtual						~CSolutionDefault() override			= default;

			virtual bool				ScanFolders() override;

			virtual IBuild*				GenBuild(const char build[]) override;

			virtual bool				LoadDiff(const char path[]) override;
			virtual bool				SaveDiff(const char path[]) override;
		protected:
		
			friend class CBuild;

			const std::filesystem::path m_currentPath;

			Json::Value					m_diffFile								= Json::Value(Json::nullValue);

			std::vector<std::filesystem::path> m_sourcePaths;
			std::filesystem::path		m_buildPath;
			std::filesystem::path		m_tmpPath;

			std::map<Json::String, SSourceFile>	m_sources;
			std::map<Json::String, Json::String> m_subSolutions;
			std::map<Json::String, Json::Value> m_builds;
	};

	/**
	 * Sub Solution
	 */
	struct SSubSolution {
		CSolutionDefault*				solution;
		IBuild*							build;
		std::filesystem::path			path;
	};

	/**
	 * Default build implementation
	 */
	class CBuild : public IBuild {
		public:
										CBuild(CSolutionDefault* solution, const Json::Value& build, std::string name);
			virtual						~CBuild()								= default;

			virtual bool				PreBuild() override;
			virtual size_t				Build(ITaskList* taskList) override;
			virtual bool				PostBuild() override;

		protected:
			bool						m_bLink									= false;

			const std::string			m_name;
			Json::Value					m_build;
			CSolutionDefault*			m_solution;

			std::vector<SSubSolution>	m_subs;

			std::vector<std::filesystem::path> m_objects;
	};
}

#endif /* !__DELTAMAKE_SOLUTION_DEFAULT_H__ */