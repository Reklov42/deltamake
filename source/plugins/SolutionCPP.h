/**
 * \file	Solution.h
 * \brief	DeltaMake C/CPP solution
 * \date	27 oct 2024
 * \author	Reklov
 */
#ifndef __DELTAMAKE_SOLUTION_CPP_H__
#define __DELTAMAKE_SOLUTION_CPP_H__

#include <stddef.h>

#include <vector>
#include <map>
#include <string>

#include <json/json.h>

#include "deltamake.h"
#include "SolutionDefault.h"
#include "Exception.h"


#define SOLUTION_CPP_TYPE_NAME			"c/cpp"

 
// ******************************************************************************** //

										//										//
namespace DeltaMake {
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
										CSolutionCPP(Json::Value& root, std::filesystem::path& currentPath);
			virtual						~CSolutionCPP() override				= default;

			virtual bool				ScanFolders() override;
			virtual bool				ScanHeaders();

			static IPlugin*				GetInstance();

		protected:

			std::map<Json::String, SHeaderFile> m_headers;

	};
}

#endif /* !__DELTAMAKE_SOLUTION_CPP_H__ */