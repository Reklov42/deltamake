/**
 * \file	Exception.h
 * \brief	DeltaMake exceptions
 * \date	27 oct 2024
 * \author	Reklov
 */
#ifndef __DELTAMAKE_EXCEPTION_H__
#define __DELTAMAKE_EXCEPTION_H__

#include <stddef.h>

#include <string>

// ******************************************************************************** //

										//										//
namespace DeltaMake {
	/**
	 * Default exception
	 */
	class CException {
		public:
			const char*					GetMessage() const { return "Bruh"; }
	};

	/**
	 * Can't find value
	 */
	class CConfigValueNotSet final : public CException {
		public:
										CConfigValueNotSet(std::string name) : m_name(name) { }

			const char*					GetMessage() const { return m_name.c_str(); }

		private:
			std::string					m_name;
	};

	/**
	 * Can't open file
	 */
	class CFileNotExists final : public CException {
		public:
										CFileNotExists(std::string name) : m_name(name) { }

			const char*					GetMessage() const { return m_name.c_str(); }

		private:
			std::string					m_name;
	};
}

#endif /* !__DELTAMAKE_EXCEPTION_H__ */