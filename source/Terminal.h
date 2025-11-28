/**
 * \file	Terminal.h
 * \brief	Termial wrapper
 * \date	1 jul 2025
 * \author	Reklov
 */
#ifndef __DELTAMAKE_TERMINAL_H__
#define __DELTAMAKE_TERMINAL_H__

#include <stddef.h>
#include <stdlib.h>

										//										//
namespace DeltaMake {
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
	 * Terminal output buffering
	 */
	enum class ELogBuffering {
		FULL, /* Show only after `Flush()` */
		LINE, /* Show after `'\n'` */
		NONE  /* Show immediately */
	};
	
	/**
	 * Terminal wrapper
	 */
	class ITerminal {
		public:
			ITerminal&					operator=(const ITerminal&)				= delete;

			virtual void				Init()									= 0;

			virtual void				UpdateSize()							= 0;

			virtual void				MoveUp(size_t offset)					= 0;
			virtual void				MoveDown(size_t offset)					= 0;
			virtual void				MoveRight(size_t offset)				= 0;
			virtual void				MoveLeft(size_t offset)					= 0;

			virtual void				Flush()									= 0;
			virtual bool				SetBuffering(ELogBuffering mode)		= 0;

			virtual void				ShowCursor(bool bShow)					= 0;
			virtual void				GetCursorPosition(int& x, int& y)		= 0;

			/**
			 * Clear to the left and below of the cursor
			 */
			virtual void				ClearDown()								= 0;

			/**
			 * Clear to the left of the cursor
			 */
			virtual void				ClearLeft()								= 0;

			virtual int					Log(ELogLevel level, const char format[], ...) = 0;
			virtual int					Write(const char msg[])					= 0;

			virtual size_t				GetColumns() const						= 0;
			virtual size_t				GetRows() const							= 0;

			virtual int					ExecSystem(const char cmd[])			= 0;
			virtual time_t				GetLastModificationTime(const char path[]) = 0;
		
		protected:
			virtual						~ITerminal()							= default;
	};

	extern ITerminal* const terminal;
}

#endif /* !__DELTAMAKE_TERMINAL_H__ */