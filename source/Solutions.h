/**
 * \file	Solutions.h
 * \brief	DeltaMake default solutions
 * \date	27 oct 2024
 * \author	Reklov
 */
#ifndef __DELTAMAKE_SOLUTIONS_H__
#define __DELTAMAKE_SOLUTIONS_H__

#include <stddef.h>

#include <new>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <vector>
#include <string>

#include <json/json.h>

#include "deltamake.h"
#include "Exception.h"
 
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
	 * C/C++ header file state
	 */
	struct SHeaderFile {
		std::vector<SSourceFile*>		files;
		time_t							mtime;
	};

	/**
	 * Solution for C/C++ projects
	 */
	class CSolutionCPP : public CSolutionDefault {
		public:
										CSolutionCPP(Json::Value root, std::filesystem::path currentPath);
			virtual						~CSolutionCPP() override				= default;

			virtual bool				ScanFolders() override;
			virtual bool				ScanHeaders();

		protected:

			std::map<Json::String, SHeaderFile> m_headers;

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
			virtual size_t				Build(IWorkerPool* pool) override;
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

// ******************************************************************************** //

/* ****************************************
 * DeltaMake::ISolution::Load
 */
inline DeltaMake::ISolution* DeltaMake::ISolution::Load(const char path[]) {
	terminal->Log(LOG_DETAIL, "Loading solution \"%s\"...\n", path);
	
	std::filesystem::path currentPath = std::filesystem::absolute(path).parent_path();
	terminal->Log(LOG_DETAIL, "Absolute path: \"%s\"\n", currentPath.c_str());

	// I hate C++ I/O but JSONCPP uses it...
	std::ifstream solutionFile(path);
	if (solutionFile.good() == false) {
		terminal->Log(LOG_ERROR, "Can't open \"%s\"!\n", path);
		return nullptr;
	}

	Json::Value root;
	solutionFile >> root;

	// Version
	const Json::Value& version = root["version"];
	if (version.isString() == false) {
		terminal->Log(LOG_ERROR, "Can't get version\n", path);
		return nullptr;
	}

	// TODO: check version
	terminal->Log(LOG_DETAIL, "Solution version: %s\n", version.asCString());

	// Type
	if (root["type"].isString() == true) {
		const Json::Value& type = root["type"];
		terminal->Log(LOG_DETAIL, "Solution type: %s\n", type.asCString());

		if (type == "c/cpp")
			return new(std::nothrow) CSolutionCPP(root, currentPath);
	}

	terminal->Log(LOG_DETAIL, "Solution type is not set. Default value is used.\n");
	return new(std::nothrow) CSolutionDefault(root, currentPath);
}

// ******************************************************************************** //

/* ****************************************
 * DeltaMake::CSolutionDefault::CSolutionDefault
 */
DeltaMake::CSolutionDefault::CSolutionDefault(Json::Value root, std::filesystem::path currentPath) : m_currentPath(currentPath) {
	const Json::Value& paths = root["paths"];
	if (paths.isObject() == false) {
		terminal->Log(LOG_ERROR, "Solution paths not set!\n");
		throw CConfigValueNotSet("paths");
	}

	// paths.scan
	const Json::Value& scan = paths["scan"];
	if (scan.isArray() == true) {
		terminal->Log(LOG_DETAIL, "Multiple scan paths:\n");
		for (Json::ArrayIndex i = 0; i < scan.size(); ++i) {
			terminal->Log(LOG_DETAIL, "\t\"%s\"\n", scan[i].asCString());
			m_sourcePaths.push_back(m_currentPath / scan[i].asString());
		}
	}
	else if (scan.isString() == true) {
		terminal->Log(LOG_DETAIL, "Single scan path: \"%s\"\n", scan.asCString());
		m_sourcePaths.push_back(m_currentPath / scan.asString());
	}
	else
		throw CConfigValueNotSet("paths.scan");
	
	// paths.build
	const Json::Value& build = paths["build"];
	if (build.isString() == false)
		throw CConfigValueNotSet("paths.build");
		
	terminal->Log(LOG_DETAIL, "Build path: \"%s\"\n", build.asCString());
	m_buildPath = m_currentPath / build.asString();

	// paths.tmp
	const Json::Value& tmp = paths["tmp"];
	if (tmp.isString() == false)
		throw CConfigValueNotSet("paths.tmp");
	
	terminal->Log(LOG_DETAIL, "Temporary path: \"%s\"\n", tmp.asCString());
	m_tmpPath = m_currentPath / tmp.asString();
	
	// solutions
	const Json::Value& solutions = root["solutions"];
	if (solutions.isObject() == false)
		terminal->Log(LOG_DETAIL, "No sub solutions setted. Ignoring...\n");
	else {
		terminal->Log(LOG_DETAIL, "Sub solutions:\n");
		for(Json::Value::const_iterator iterator = solutions.begin(); iterator != solutions.end(); ++iterator) {
			const Json::String name  = iterator.key().asString();
			const Json::String value = (*iterator).asString();
			
			terminal->Log(LOG_DETAIL, "\t\"%s\" -> \"%s\"\n", name.c_str(), value.c_str());
			m_subSolutions[name] = value;
		}
	}


	const Json::Value& files = root["files"];
	if (files.isArray() == false) // TODO: make as only sub build?
		throw CConfigValueNotSet("files");
	else {
		terminal->Log(LOG_DETAIL, "Files:\n");
		for (Json::ArrayIndex i = 0; i < files.size(); ++i) {
			const Json::String relativePath = files[i].asString();
			terminal->Log(LOG_DETAIL, "\t\"%s\"\n", relativePath.c_str());
			SSourceFile file;
			file.path = m_currentPath / relativePath.c_str();

			if (std::filesystem::exists(file.path) == false) {
				terminal->Log(LOG_WARNING, "File \"%s\" does not exists!..\n", file.path.c_str());
				//throw CFileNotExists(file.path.c_str());
				continue; // TODO: rewrite solution
			}

			file.mtime = terminal->GetLastModificationTime(file.path.c_str());

			m_sources[relativePath] = file;
		}
	}

	// Builds
	const Json::Value& builds = root["builds"];
	if (builds.isObject() == false)
		throw CConfigValueNotSet("builds");
	else {
		terminal->Log(LOG_DETAIL, "Builds:\n");
		for(Json::Value::const_iterator iterator = builds.begin(); iterator != builds.end(); ++iterator) {
			const Json::String name  = iterator.key().asString();
			
			terminal->Log(LOG_DETAIL, "\t\"%s\"\n", name.c_str());
			m_builds[name] = *iterator;
		}
	}
}

/* ****************************************
 * DeltaMake::CSolutionDefault::ScanFolders
 */
inline bool DeltaMake::CSolutionDefault::ScanFolders() {
	terminal->Log(LOG_ERROR, "Default solution type does not have scan mode!\n");
	return false;
}

/* ****************************************
 * DeltaMake::CSolutionDefault::GenBuild
 */
inline DeltaMake::IBuild* DeltaMake::CSolutionDefault::GenBuild(const char build[]) {
	if (m_diffFile.isNull() == true) {
		m_diffFile = Json::Value(Json::objectValue);
		char buffer[32];
		snprintf(buffer, 32, "%i.%i.%i", DELTAMAKE_VERSION_MAJOR, DELTAMAKE_VERSION_MINOR, DELTAMAKE_VERSION_PATCH);
		m_diffFile["version"] = buffer;
	}

	auto iterator = m_builds.find(build);
	if (iterator == m_builds.end())
		return nullptr;

	return new(std::nothrow) CBuild(this, iterator->second, build);
}

/* ****************************************
 * DeltaMake::CSolutionDefault::LoadDiff
 */
inline bool DeltaMake::CSolutionDefault::LoadDiff(const char path[]) {
	terminal->Log(LOG_DETAIL, "Loading diff \"%s\"...\n", path);
	
	std::ifstream solutionFile(path);
	if (solutionFile.good() == false) {
		terminal->Log(LOG_DETAIL, "Can't open \"%s\". Ignoring..\n", path);
		return false;
	}
	
	Json::Value diff;
	solutionFile >> diff;

	// Version
	const Json::Value& version = diff["version"];
	if (version.isString() == false) {
		terminal->Log(LOG_ERROR, "Can't get version\n", path);
		return false;
	}

	// TODO: check version
	terminal->Log(LOG_DETAIL, "Diff version: %s\n", version.asCString());

	m_diffFile = diff;

	return true;
}

/* ****************************************
 * DeltaMake::CSolutionDefault::SaveDiff
 */
inline bool DeltaMake::CSolutionDefault::SaveDiff(const char path[]) {
	terminal->Log(LOG_DETAIL, "Saving diff \"%s\"...\n", path);

	std::ofstream file;
	file.open(path);

	Json::StyledWriter writer;
	file << writer.write(m_diffFile);

	file.close();
	return true;
}

// ******************************************************************************** //

/* ****************************************
 * DeltaMake::CSolutionCPP::CSoluCSolutiontionCPP
 */
DeltaMake::CSolutionCPP::CSolutionCPP(Json::Value root, std::filesystem::path currentPath) : CSolutionDefault(root, currentPath) {
	Json::Value& ccpp = root["c/cpp"];
	if (ccpp.isObject() == false) {
		terminal->Log(LOG_DETAIL, "Config object \"c/cpp\" does not exists. Creating...\n");
		ccpp = Json::Value(Json::ValueType::objectValue);
		ccpp["headers"] = Json::Value(Json::ValueType::arrayValue);

		root["c/cpp"] = ccpp;
		ccpp = root["c/cpp"]; // TODO: Remove?
	}
}

/* ****************************************
 * DeltaMake::CSolutionCPP::ScanFolders
 */
inline bool DeltaMake::CSolutionCPP::ScanFolders() { // TODO:
	return false;
}

/* ****************************************
 * DeltaMake::CSolutionCPP::ScanHeaders
 */
inline bool DeltaMake::CSolutionCPP::ScanHeaders() { // TODO:
	return false;
}

// ******************************************************************************** //

/* ****************************************
 * DeltaMake::CBuild::CBuild
 */
DeltaMake::CBuild::CBuild(CSolutionDefault* solution, const Json::Value& build, std::string name) : m_solution(solution), m_build(build), m_name(name) {
	const Json::Value subs = m_build["solutions"];
	if (subs.isObject() == false)
		terminal->Log(LOG_DETAIL, "No sub solutions setted. Ignoring...\n");
	else {
		for(auto iterator = subs.begin(); iterator != subs.end(); ++iterator) {
			const Json::String code = iterator.key().asString();
			auto iterName = solution->m_subSolutions.find(code);
			if (iterName == solution->m_subSolutions.end()) {
				terminal->Log(LOG_ERROR, "Codename not found: \"%s\"\n", code.c_str());
				return; // TODO: exception
			}

			SSubSolution sub;
			sub.path = m_solution->m_currentPath / iterName->second;
			sub.solution = dynamic_cast<CSolutionDefault*>(ISolution::Load((sub.path / std::string(DELTAMAKE_CONFIG_FILENAME)).c_str()));
			if (sub.solution == nullptr) {
				terminal->Log(LOG_ERROR, "Can't load solution: \"%s\"\n", iterName->second.c_str());
				return; // TODO: exception
			}

			sub.solution->m_buildPath = m_solution->m_buildPath;
			sub.solution->m_tmpPath   = m_solution->m_tmpPath;

			const Json::Value& params = (*iterator);
			const Json::Value& buildName = params["build"];
			std::string name = "default";
			if (buildName.isString() == false)
				terminal->Log(LOG_DETAIL, "build is not set. Default value is used.\n");
			else
				name = buildName.asString();

			sub.build = sub.solution->GenBuild(name.c_str());
			if (sub.build == nullptr) {
				terminal->Log(LOG_ERROR, "Build not found: \"%s\"\n", name.c_str());
				return; // TODO: exception
			}

			if (config->bForce == false)
				sub.solution->LoadDiff((sub.path / std::string(DELTAMAKE_DIFF_FILENAME)).c_str());

			m_subs.push_back(sub);
		}
	}
}

/* ****************************************
 * DeltaMake::CBuild::PreBuild
 */
inline bool DeltaMake::CBuild::PreBuild() {
	// Check paths
	if (std::filesystem::exists(m_solution->m_buildPath) == false) {
		terminal->Log(LOG_DETAIL, "Build directory does not exists. Creating...\n");
		std::filesystem::create_directory(m_solution->m_buildPath);
	}

	if (std::filesystem::exists(m_solution->m_tmpPath) == false) {
		terminal->Log(LOG_DETAIL, "Temporary directory does not exists. Creating...\n");
		std::filesystem::create_directory(m_solution->m_tmpPath);
	}

	// Subs
	for (size_t i = 0; i < m_subs.size(); ++i)
		m_subs[i].build->PreBuild();

	// Pre
	const Json::Value pre = m_build["pre"];
	if (pre.isString() == true) {
		const char* cmd = pre.asCString();
		terminal->Log(LOG_DETAIL, "Pre build command: \"%s\"\n", cmd);
		return terminal->ExecSystem(cmd);
	}

	return true;
}

/* ****************************************
 * DeltaMake::CBuild::PostBuild
 */
inline size_t DeltaMake::CBuild::Build(IWorkerPool* pool) {
	bool bReLink = false; // Relink all objects

	// Subs
	for (size_t i = 0; i < m_subs.size(); ++i) {
		if (m_subs[i].build->Build(pool) != 0)
			bReLink = true;
	}

	// Build
	std::string cmdBegin = "";
	const Json::Value& compiler = m_build["compiler"];
	if (compiler.isString() == false) {
		terminal->Log(LOG_DETAIL, "Compiler is not set. Default value is used.\n");
		cmdBegin += "g++ ";
	}
	else
		cmdBegin += compiler.asString() + " ";


	const Json::Value& compilerFlags = m_build["compilerFlags"];
	if (compilerFlags.isString() == false)
		terminal->Log(LOG_DETAIL, "No compiler flags setted. Ignoring...\n");
	else
		cmdBegin += compilerFlags.asString() + " ";

	
	const Json::Value& paths = m_build["paths"];
	if (paths.isObject() == false)
		terminal->Log(LOG_DETAIL, "No paths setted. Ignoring...\n");
	else {
		const Json::Value& include = paths["include"];
		if (include.isArray() == false)
			terminal->Log(LOG_DETAIL, "No paths.include setted. Ignoring...\n");
		else {
			for (Json::ArrayIndex i = 0; i < include.size(); ++i)
				cmdBegin += "-I\"" + include[i].asString() + "\" ";
		}

		const Json::Value& lib = paths["lib"];
		if (lib.isArray() == false)
			terminal->Log(LOG_DETAIL, "No paths.lib setted. Ignoring...\n");
		else {
			for (Json::ArrayIndex i = 0; i < lib.size(); ++i)
				cmdBegin += "-L\"" + lib[i].asString() + "\" ";
		}
	}


	const Json::Value& defines = m_build["defines"];
	if (defines.isArray() == false)
		terminal->Log(LOG_DETAIL, "No defines setted. Ignoring...\n");
	else {
		for (Json::ArrayIndex i = 0; i < defines.size(); ++i)
			cmdBegin += "-D\"" + defines[i].asString() + "\" ";
	}

	cmdBegin += "-c ";

	Json::Value& diff = m_solution->m_diffFile["diff"];
	if (diff.isObject() == false) {
		terminal->Log(LOG_DETAIL, "No diff data. Ignoring...\n");
		diff = Json::Value(Json::objectValue);
	}
	
	Json::Value& buildDiff = diff[m_name];
	if (buildDiff.isObject() == false) {
		terminal->Log(LOG_DETAIL, "No build diff data. Ignoring...\n");
		buildDiff = Json::Value(Json::objectValue);
	}

	size_t nToExecute = 0;
	terminal->Log(LOG_DETAIL, "Commands:\n");
	for(auto iterator = m_solution->m_sources.begin(); iterator != m_solution->m_sources.end(); ++iterator) {
		const SSourceFile& file = iterator->second;
		const char* stem = file.path.stem().c_str();
		const std::filesystem::path outPath = m_solution->m_tmpPath / (m_name + "_" + stem);
		m_objects.push_back(outPath);

		const Json::Value& fileTime = buildDiff[iterator->first];
		if (fileTime.isNumeric() == true) {
			time_t lastTime = static_cast<time_t>(buildDiff[iterator->first].asLargestInt());
			if (lastTime >= file.mtime) {
				continue;
			}
		}

		m_bLink = true;
		
		++nToExecute;
		buildDiff[iterator->first] = file.mtime;

		size_t size = strlen(stem);
		size = (size > DELTAMAKE_MAX_WORKER_TITLE - 1) ? DELTAMAKE_MAX_WORKER_TITLE - 1 : size;
		char title[DELTAMAKE_MAX_WORKER_TITLE] = { 0 };
		strncpy(title, stem, size);
		
		std::string cmd = cmdBegin + "\"" + file.path.c_str() + "\" -o \"" + outPath.c_str() + "\"";

		terminal->Log(LOG_DETAIL, "\t%s\n", cmd.c_str());
		
		pool->AddCommand(cmd, title);
	}

	return nToExecute;
}

/* ****************************************
 * DeltaMake::CBuild::PostBuild
 */
inline bool DeltaMake::CBuild::PostBuild() {
	// Subs
	for (size_t i = 0; i < m_subs.size(); ++i) {
		m_subs[i].build->PostBuild();

		if (config->bForce == false)
			m_subs[i].solution->SaveDiff((m_subs[i].path / std::string(DELTAMAKE_DIFF_FILENAME)).c_str());
	}

	if (m_bLink == false) {
		terminal->Log(LOG_DETAIL, "Nothing to link.\n");
		return true;
	}

	Json::Value type = m_build["type"];
	if (type.isString() == false) {
		terminal->Log(LOG_DETAIL, "Compiler is not set. Default value is used.\n");
		type = "exec";
	}

	Json::Value out = m_build["outname"];
	if (out.isString() == false) {
		terminal->Log(LOG_DETAIL, "outname is not set. Default value is used.\n");
		out = "out";
	}

	const char* workingPathName = m_solution->m_currentPath.filename().c_str();
	std::string cmdBegin = "";
	if (type == "exec") {
		terminal->Log(LOG_INFO, "Linking \"%s\"...\n", workingPathName);

		const Json::Value& linker = m_build["linker"];
		if (linker.isString() == false) {
			terminal->Log(LOG_DETAIL, "linker is not set. Default value is used.\n");
			cmdBegin += "g++ ";
		}
		else
			cmdBegin += linker.asString() + " ";

		const Json::Value& linkerFlags = m_build["linkerFlags"];
		if (linkerFlags.isString() == false) {
			terminal->Log(LOG_DETAIL, "No linkerFlags setted. Ignoring...\n");
		}
		else
			cmdBegin += linkerFlags.asString() + " ";

		for (size_t i = 0; i < m_objects.size(); ++i)
			cmdBegin += std::string("\"") + m_objects[i].c_str() + "\" ";

		const Json::Value& staticLibs = m_build["staticLibs"];
		if (staticLibs.isArray() == false)
			terminal->Log(LOG_DETAIL, "No staticLibs setted. Ignoring...\n");
		else {
			for (Json::ArrayIndex i = 0; i < staticLibs.size(); ++i)
				cmdBegin += "\"" + staticLibs[i].asString() + "\" ";
		}

		cmdBegin += std::string("-o \"") + (m_solution->m_buildPath / out.asCString()).c_str() + "\"";
	}
	else if (type == "lib") {
		terminal->Log(LOG_INFO, "Archiving \"%s\"...\n", workingPathName);

		const Json::Value& archiver = m_build["archiver"];
		if (archiver.isString() == false) {
			terminal->Log(LOG_DETAIL, "archiver is not set. Default value is used.\n");
			cmdBegin += "ar ";
		}
		else
			cmdBegin += archiver.asString() + " rcs \"" + (m_solution->m_buildPath / out.asCString()).c_str() + "\" ";

		for (size_t i = 0; i < m_objects.size(); ++i)
			cmdBegin += std::string("\"") + m_objects[i].c_str() + "\" ";
	}

	terminal->Log(LOG_DETAIL, "Command:\n\t%s\n", cmdBegin.c_str());
	terminal->ExecSystem(cmdBegin.c_str());

	const Json::Value post = m_build["post"];
	if (post.isString() == true) {
		const char* cmd = post.asCString();
		terminal->Log(LOG_DETAIL, "Post build command: \"%s\"\n", cmd);
		return terminal->ExecSystem(cmd);
	}

	return true;
}

#endif /* !__DELTAMAKE_SOLUTIONS_H__ */