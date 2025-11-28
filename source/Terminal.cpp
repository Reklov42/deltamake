/**
 * \file	Terminal.cpp
 * \brief	Termial wrapper
 * \date	1 jul 2025
 * \author	Reklov
 */
#include "Terminal.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "deltamake.h"

using namespace DeltaMake;

// ******************************************************************************** //
										//										//

/**
 * CTerminalLocal
 */
class CTerminalLocal final : public ITerminal {
	public:
		virtual							~CTerminalLocal() override				= default;

		virtual void					Init() override;
	
		virtual void					UpdateSize() override;

		virtual void					MoveUp(size_t offset) override;
		virtual void					MoveDown(size_t offset) override;
		virtual void					MoveRight(size_t offset) override;
		virtual void					MoveLeft(size_t offset) override;

		virtual void					Flush() override;
		virtual bool					SetBuffering(ELogBuffering mode) override;

		virtual void					ShowCursor(bool bShow) override;
		virtual void					GetCursorPosition(int& x, int& y) override;

		virtual void					ClearDown() override;
		virtual void					ClearLeft() override;

		virtual int						Log(ELogLevel level, const char format[], ...) override;
		virtual int						Write(const char msg[]) override;

		virtual size_t					GetColumns() const override;
		virtual size_t					GetRows() const override;

		virtual int						ExecSystem(const char cmd[]) override;
		virtual time_t					GetLastModificationTime(const char path[]) override;

	private:

		size_t							m_nColumns;
		size_t							m_nRows;
};

CTerminalLocal g_terminalLocal;
extern DeltaMake::ITerminal* const DeltaMake::terminal = &g_terminalLocal;

// ******************************************************************************** //

#if defined(__linux__)

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <termios.h>

/* ****************************************
 * CTerminalLocal::Init
 */
void CTerminalLocal::Init() {
	UpdateSize();
}

/* ****************************************
 * CTerminalLocal::MoveUp
 */
void CTerminalLocal::MoveUp(size_t offset) {
	fprintf(stdout, "\033[%zuA", offset);
}

/* ****************************************
 * CTerminalLocal::MoveDown
 */
void CTerminalLocal::MoveDown(size_t offset) {
	fprintf(stdout, "\033[%zuB", offset);
}

/* ****************************************
 * CTerminalLocal::MoveRight
 */
void CTerminalLocal::MoveRight(size_t offset) {
	fprintf(stdout, "\033[%zuC", offset);
}

/* ****************************************
 * CTerminalLocal::MoveLeft
 */
void CTerminalLocal::MoveLeft(size_t offset) {
	fprintf(stdout, "\033[%zuD", offset);
}

/* ****************************************
 * CTerminalLocal::Flush
 */
void CTerminalLocal::Flush() {
	fflush(stdout);
}

/* ****************************************
 * CTerminalLocal::SetBuffering
 */
bool CTerminalLocal::SetBuffering(ELogBuffering mode) {
	int bufMode;

	switch (mode) {
		case ELogBuffering::FULL: bufMode = _IOFBF; break;
		case ELogBuffering::LINE: bufMode = _IOLBF; break;
		case ELogBuffering::NONE: bufMode = _IONBF; break;
		
		default:
			return false;
	}

	return setvbuf(stdout, NULL, bufMode, 0) == 0;
}

/* ****************************************
 * CTerminalLocal::ShowCursor
 */
void CTerminalLocal::ShowCursor(bool bShow) {
	fprintf(stdout, (bShow == true) ? "\033[?25h" : "\033[?25l");
}

/* ****************************************
 * CTerminalLocal::GetCursorPosition
 */
void CTerminalLocal::GetCursorPosition(int& x, int& y) {
	x = 0;
	y = 0;

	termios lastState;
	termios newState;

	tcgetattr(STDIN_FILENO, &lastState);
	
	newState = lastState;
	newState.c_lflag &= ~(ICANON | ECHO); // Canonical | Echo
	tcsetattr(STDIN_FILENO, TCSANOW, &newState);

	write(STDIN_FILENO, "\033[6n", 4);

	char buff[32];
	size_t i;
	for (i = 0; i < sizeof(buff); ++i) { // "\033[row;col"
		if (read(STDIN_FILENO, &buff[i], 1) != 1)
			break;

		if (buff[i] == 'R')
			break;
	}

	buff[i] = '\0'; // F for `R`

	tcsetattr(STDIN_FILENO, TCSANOW, &lastState);

	const char* row = strrchr(buff, '[');
	if (row == NULL)
		return;

	++row;

	const char* col = strrchr(row, ';');
	if (row == NULL)
		return;

	++col;

	y = atoi(row);
	x = atoi(col);
}

/* ****************************************
 * CTerminalLocal::ClearDown
 */
void CTerminalLocal::ClearDown() {
	fputs("\033[0J", stdout);
}

/* ****************************************
 * CTerminalLocal::ClearLeft
 */
void CTerminalLocal::ClearLeft() {
	fputs("\033[0K", stdout);
}

/* ****************************************
 * CTerminalLocal::Log
 */
int CTerminalLocal::Log(ELogLevel level, const char format[], ...) {
	if ((level == ELogLevel::LOG_DETAIL) && (config->bVerbose == false))
		return 0;

	FILE* const out = (level == ELogLevel::LOG_ERROR) ? stderr : stdout;

	switch (level) {
		case ELogLevel::LOG_ERROR:		fputs("\033[0;31m", out); break; // Red
		case ELogLevel::LOG_WARNING:	fputs("\033[0;33m", out); break; // Yellow
		case ELogLevel::LOG_DETAIL:		fputs("\033[0;36m", out); break; // Cyan
	
		default:						fputs("\033[0m", out); break; // Reset
	}

	va_list argptr;
	va_start(argptr, format);
	int size = vfprintf(out, format, argptr);
	va_end(argptr);

	fputs("\033[0m", out); // Reset color

	return size;
}

/* ****************************************
 * CTerminalLocal::Write
 */
int CTerminalLocal::Write(const char msg[]) {
	return fputs(msg, stdout);
}

/* ****************************************
 * CTerminalLocal::ExecSystem
 */
int CTerminalLocal::ExecSystem(const char cmd[]) {
	int status = system(cmd);
    status = WIFEXITED(status) ? WEXITSTATUS(status) : 0;
	
	// TODO: Save exit
	if (status != 0)
		exit(EXIT_FAILURE);

	return status;
}

/* ****************************************
 * CTerminalLocal::GetLastModificationTime
 */
time_t CTerminalLocal::GetLastModificationTime(const char path[]) {
	struct stat stats;
	stat(path, &stats);
	
	return stats.st_mtime;
}

/* ****************************************
 * CTerminalLocal::UpdateSize
 */
void CTerminalLocal::UpdateSize() {
	struct winsize ws;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);

	m_nColumns = static_cast<size_t>(ws.ws_col);
	m_nRows = static_cast<size_t>(ws.ws_row);
}

#endif

/* ****************************************
 * CTerminalLocal::GetColumns
 */
size_t CTerminalLocal::GetColumns() const {
	return m_nColumns;
}

/* ****************************************
 * CTerminalLocal::GetRows
 */
size_t CTerminalLocal::GetRows() const {
	return m_nRows;
}