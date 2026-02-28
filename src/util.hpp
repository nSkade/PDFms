#pragma once

namespace pdf {
	// Get all PDF files in directory (optionally shuffled)
	std::vector<fs::path> get_pdf_files(const fs::path& directory, bool shuffle) {
		std::vector<fs::path> pdf_files;
		for (const auto& entry : fs::recursive_directory_iterator(directory)) {
			if (entry.is_regular_file() && entry.path().extension() == ".pdf") {
				pdf_files.push_back(entry.path());
			}
		}
		if (shuffle) {
			std::random_device rd;
			std::mt19937 g(rd());
			std::shuffle(pdf_files.begin(), pdf_files.end(), g);
		}
		return pdf_files;
	}

	void suppress_poppler_stderr() {
	#ifdef _WIN32
		freopen("NUL", "w", stderr);
	#else
		freopen("/dev/null", "w", stderr);
	#endif
	}
	
	std::string tolower(const std::string& s) {
		std::string ret = s;
		std::transform(ret.begin(),ret.end(),ret.begin(),[](unsigned char c) {return std::tolower(c);});
		return ret;
	}
};

namespace terminal {
	#ifdef _WIN32
	void enable_ansi_escape_codes() {
		HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
		if (hOut == INVALID_HANDLE_VALUE) return;
		DWORD dwMode = 0;
		if (!GetConsoleMode(hOut, &dwMode)) return;
		dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
		SetConsoleMode(hOut, dwMode);
	}
	#else
	void enable_ansi_escape_codes() {}
	#endif

	int getConsoleWidth() {
		int columns = 80; // Default or fallback value

	#ifdef _WIN32
		CONSOLE_SCREEN_BUFFER_INFO csbi;
		if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
			columns = csbi.srWindow.Right - csbi.srWindow.Left + 1;
		}
	#elif defined(__linux__) || defined(__APPLE__)
		struct winsize w;
		if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
			columns = w.ws_col;
		}
	#endif
		return columns;
	}
	
	void delete_last_lines(int count) {
		for (int i = 0; i < count; ++i) // Loop count-1 times to move up and clear
			std::cout << "\033[1A" // Move cursor up
	#if 0 // clear line
					  << "\033[2K\r";  // Clear entire line and return cursor to beginning
	#else // overwrite line, no flicker
					  << "\r";	// Clear entire line and return cursor to beginning
	#endif
		std::cout.flush();
	}

	// Helper to move cursor up and clear everything below it
	void reset_cursor(int lineCount) {
		if (lineCount > 0) {
			// \033[F moves cursor to the beginning of the previous line, N times
			std::cout << "\033[" << lineCount << "F";
			// \033[J clears from cursor to end of screen
			//std::cout << "\033[J";
		}
	}
};
