/**
 * \file	Solution.cpp
 * \brief	DeltaMake C/CPP solution
 * \date	27 oct 2024
 * \author	Reklov
 */
#include "SolutionCPP.h"

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

 
using namespace DeltaMake;

// ******************************************************************************** //

										//										//

/**
 * Local factory for C/CPP solution
 */
class CSolutionFactoryCPP final : public ISolutionFactory {
	public:
		virtual							~CSolutionFactoryCPP() override			= default;
		
		virtual const std::string&		GetName() const override;
		virtual ISolution*				NewSolution(Json::Value& root, std::filesystem::path& currentPath) override;
};

// ******************************************************************************** //

/* ****************************************
 * DeltaMake::CSolutionCPP::CSoluCSolutiontionCPP
 */
DeltaMake::CSolutionCPP::CSolutionCPP(Json::Value& root, std::filesystem::path& currentPath) : CSolutionDefault(root, currentPath) {
	Json::Value& ccpp = root["c/cpp"];
	if (ccpp.isObject() == false) {
		terminal->Log(LOG_DETAIL, "Config object \"c/cpp\" does not exists. Creating...\n");
		ccpp = Json::Value(Json::ValueType::objectValue);
		ccpp["headers"] = Json::Value(Json::ValueType::arrayValue); // TODO:

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
 * DeltaMake::CSolutionCPP::GetInstance
 */
DeltaMake::IPlugin* DeltaMake::CSolutionCPP::GetInstance() {
	static CSolutionFactoryCPP factory;

	return &factory;
}

// ******************************************************************************** //

/* ****************************************
 * DeltaMake::CSolutionFactoryCPP::GetName
 */
const std::string& CSolutionFactoryCPP::GetName() const {
	static const std::string name = SOLUTION_CPP_TYPE_NAME;

	return name;
}

/* ****************************************
 * DeltaMake::CSolutionFactoryCPP::NewSolution
 */
ISolution* CSolutionFactoryCPP::NewSolution(Json::Value& root, std::filesystem::path& currentPath) {
	return new(std::nothrow) CSolutionCPP(root, currentPath);
}
