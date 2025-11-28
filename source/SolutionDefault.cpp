/**
 * \file	SolutionDefault.cpp
 * \brief	DeltaMake default solutions
 * \date	27 oct 2024
 * \author	Reklov
 */
#include "SolutionDefault.h"

#include <stddef.h>

#include <new>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <vector>
#include <string>

#include <json/json.h>

#include "deltamake.h"
#include "Terminal.h"
#include "Exception.h"
 
// ******************************************************************************** //

										//										//

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
		terminal->Log(LOG_ERROR, "Can't get version\n");
		return nullptr;
	}

	// TODO: check version
	terminal->Log(LOG_DETAIL, "Solution version: %s\n", version.asCString());

	// Type
	if (root["type"].isString() == true) {
		const Json::Value& type = root["type"];
		terminal->Log(LOG_DETAIL, "Solution type: %s\n", type.asCString());

		auto iterator = config->solutionTypes.find(type.asCString());
		if (iterator == config->solutionTypes.end()) {
			terminal->Log(LOG_ERROR, "Solution type \"%s\" is unknown.\n", type.asCString());
			return nullptr;
		}

		return iterator->second->NewSolution(root, currentPath);
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
inline size_t DeltaMake::CBuild::Build(ITaskList* taskList) {
	bool bReLink = false; // Relink all objects

	// Subs
	for (size_t i = 0; i < m_subs.size(); ++i) {
		if (m_subs[i].build->Build(taskList) != 0)
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
		const std::string stem = std::string(file.path.stem());
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
		
		std::string cmd = cmdBegin + "\"" + file.path.c_str() + "\" -o \"" + outPath.c_str() + "\"";

		terminal->Log(LOG_DETAIL, "\t%s\n", cmd.c_str());
		
		taskList->AddCommand(stem.c_str(), cmd);
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