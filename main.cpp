#include <filesystem>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <algorithm>
#include <random>
#include <chrono>
#include <poppler-document.h>
#include <poppler-page.h>
#include <cstdio>
#include <cctype>

#include <assert.h>

// ftxui might be useful in the future but for now I have all printing features i want
// addback "ftxui" in vcpkg.json
//#include <ftxui/dom/elements.hpp>
//#include <ftxui/screen/screen.hpp>
//#include <ftxui/component/component.hpp>
//#include <ftxui/component/screen_interactive.hpp>

namespace fs = std::filesystem;

#ifdef _WIN32
#include <windows.h>
#endif

struct Occurence {
	int page;
	int line_number;
	std::string line;
};

struct SearchResult {
	fs::path pdf_path;
	std::vector<Occurence> occurences;
	bool completed = false;
	bool printed = false; // True when the final 2-line output for this result has been printed and finalized.
	int printingHeight = 0;
};

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

// User's provided delete_last_lines function - DO NOT CHANGE
void delete_last_lines(int count) {
	for (int i = 0; i < count; ++i) // Loop count-1 times to move up and clear
		std::cout << "\033[1A" // Move cursor up
#if 0 // clear line
				  << "\033[2K\r";  // Clear entire line and return cursor to beginning
#else // overwrite line, no flicker
				  << "\r";  // Clear entire line and return cursor to beginning
#endif
	std::cout.flush();
}

std::string tolower(const std::string& s) {
	std::string ret = s;
	std::transform(ret.begin(),ret.end(),ret.begin(),[](unsigned char c) {return std::tolower(c);});
	return ret;
}

#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__) || defined(__APPLE__)
#include <sys/ioctl.h>
#include <unistd.h>
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

int main(int argc, char* argv[]) {
	suppress_poppler_stderr();
	enable_ansi_escape_codes();

	//auto screen = ftxui::ScreenInteractive::TerminalOutput();
	//auto renderer = ftxui::Renderer([&] {
	//	return ftxui::vbox({
	//		ftxui::text("Hello, FTXUI!"),
	//		ftxui::separator(),
	//		ftxui::text("Welcome to the terminal UI world."),
	//	});
	//});
	//screen.Loop(renderer);
	//return 0;

	// --- Settings ---
	bool shuffle = false;
	bool sort_result = false;
	bool print_line = false;
	bool print_path = false;
	std::string directory;
	std::string target;

	// --- Parse command line ---
	for (int i = 1; i < argc; ++i) {
		std::string arg(argv[i]);
		if (arg == "--shuffle") shuffle = true;
		else if (arg == "--sort") sort_result = true;
		else if (arg == "--printline") print_line = true;
		else if (arg == "--printpath") print_path = true;
		else if (directory.empty()) directory = arg;
		else if (target.empty()) target = arg;
	}

	if (target.empty()) {
		if (!directory.empty()) {
			std::swap(directory, target);
		} else {
			std::cout << "Usage: " << argv[0] << " [<directory>] <search-string> [--shuffle] [--sort] [--printline] [--printpath]\n";
			return 1;
		}
	}

	fs::path dir = directory.empty() ? fs::current_path() : fs::path(directory);
	std::vector<fs::path> pdfs = get_pdf_files(dir, shuffle);
	size_t total_files = pdfs.size();

	std::vector<std::shared_ptr<SearchResult>> results;
	std::mutex results_mutex; // Mutex to protect 'results' vector modifications

	std::condition_variable queue_cv; // Condition variable to signal updates to main thread

	std::atomic<size_t> file_index{ 0 }; // Atomic counter for files to be processed by workers
	std::atomic<size_t> completed_files{ 0 }; // Atomic counter for completed files
	std::atomic<bool> aborted{ false }; // Flag to signal threads to stop

	// --- Abort input thread ---
	std::thread abort_thread([&aborted]() {
		std::string line;
		std::getline(std::cin, line);
		aborted = true;
	});

	std::string targetLower = tolower(target);

	auto worker_func = [&]() {
		while (!aborted) {
			size_t idx = file_index.fetch_add(1);
			if (idx >= total_files) {
				//std::cout << "No more files to process\n";
				break; // No more files to process
			}

			const auto& pdf_path = pdfs[idx];
			std::string pdf_path_str = pdf_path.string();

			poppler::document* doc = nullptr;
			try {
				doc = poppler::document::load_from_file(pdf_path_str);
			} catch (...) {
				doc = nullptr;
			}

			if (!doc) {
				completed_files++;
				continue;
			}
			std::shared_ptr<SearchResult> current_res = std::make_shared<SearchResult>();
			current_res->pdf_path = pdf_path;
			{
				std::unique_lock<std::mutex> lock(results_mutex);
				results.push_back(current_res);
			}

			int pages = doc->pages();
			for (int i = 0; i < pages && !aborted; ++i) {
				auto page = std::unique_ptr<poppler::page>(doc->create_page(i));
				if (!page) continue;
				auto utf8 = page->text().to_utf8();
				std::string page_text(utf8.data(), utf8.size());
				std::istringstream iss(page_text);
				std::string line;
				int line_number = 0;
				while (std::getline(iss, line)) {
					++line_number;
					if (tolower(line).find(targetLower) != std::string::npos) {
						Occurence occurrence{ i + 1, line_number, line };
						// Add occurrence directly to shared SearchResult
						current_res->occurences.push_back(occurrence);
						// Notify the main thread that there's an update.
						// This notification is what allows the main thread to pick up incremental page findings.
						queue_cv.notify_one();
					}
				}
			}

			// After processing all pages for this PDF
			{
				current_res->completed = true;
				completed_files++;
				queue_cv.notify_one(); // Notify main thread that this file is fully completed
			}

			delete doc; // Clean up Poppler document
		}
	};

	// --- Launch worker threads ---
	size_t num_threads = std::max<size_t>(1, std::thread::hardware_concurrency() - 1);
	std::vector<std::thread> pool;
	for (size_t i = 0; i < num_threads; ++i) {
		pool.emplace_back(worker_func);
	}

	// --- Main Display Logic (Block Update) ---
	size_t idx_startUnprinted = 0; // Tracks the first file that is not yet finalized.
	size_t lastPrintedResultCount = 0; // How many *results* (each 2 lines) were displayed in the *previous* refresh.

	auto allPrinted = [&]() -> bool {
		for (int i=0;i<results.size();++i) {
			auto r = results[i].get();
			if (r && !r->printed)
				return false;
		}
		return true;
	};

	std::mutex printMutex;
	bool progress_printed = false;
	int completed_last_iter = 0;
	while (!aborted && (completed_files < total_files || !allPrinted())) {
		// Wait for a short period or until notified that new results are available.
		std::unique_lock<std::mutex> printLock(printMutex);
		queue_cv.wait_for(printLock, std::chrono::milliseconds(200), [&]() {
			//float progress = float(completed_files)*100./total_files;
			//std::cout << std::setw(5)
			//		  << std::fixed << std::setprecision(1)
			//		  << progress << "%\n" << std::flush;
			//delete_last_lines(1);
			return true;
		});

		if (progress_printed)
			delete_last_lines(1); // print progress
		//assert(lastPrintedResultCount >= completed_last_iter);
		delete_last_lines((lastPrintedResultCount-completed_last_iter));

		//if (lastPrintedResultCount > 0 && completed_last_iter > 0 && lastPrintedResultCount == completed_last_iter)
		//	std::cout << "break\n";
		//std::cout << "would deleting " << lastPrintedResultCount << " - " << completed_last_iter << " x2 lines\n";
		completed_last_iter = 0;
		size_t printedResultCount = 0; // Reset count for results printed in this cycle

		std::stringstream buf;
		bool startIncompleteSet = false;
		for (size_t i = idx_startUnprinted; i < results.size(); ++i) {

			std::shared_ptr<SearchResult> res;
			bool completed;
			{
				std::unique_lock<std::mutex> lock(results_mutex);
				res = results[i];
				completed = res->completed;
			}

			if (completed && res->occurences.size()==0) {
					res->printed = true;
					continue;
				}
			else if (!completed && res->occurences.size()==0) {
				continue;
			}

			std::vector<int> display_pages;
			for (const auto& occ : res->occurences)
				display_pages.push_back(occ.page);
			std::sort(display_pages.begin(), display_pages.end());
			display_pages.erase(std::unique(display_pages.begin(), display_pages.end()), display_pages.end());

			{ // printing
				res->printingHeight = 0;
				int consoleWidth = getConsoleWidth();
				
				// --- Line 1: Filename and optional path ---
				std::string line1_content = res->pdf_path.filename().string();
				if (print_path) {
					line1_content += "    " + res->pdf_path.parent_path().string();
				}
				
				int wrapped_lines_1 = (line1_content.length() + consoleWidth - 1) / consoleWidth;
				
				buf << line1_content << "\n";
				printedResultCount += wrapped_lines_1;
				res->printingHeight += wrapped_lines_1;
				
				// --- Line 2: Tab followed by pages ---
				std::string line2_content = "    "; // Start with the tab
				for (size_t j = 0; j < display_pages.size(); ++j) {
					if (j > 0) line2_content += ", ";
					line2_content += std::to_string(display_pages[j]);
				}
				
				int wrapped_lines_2 = (line2_content.length() + consoleWidth - 1) / consoleWidth;
				
				buf << line2_content << "\n";
				printedResultCount += wrapped_lines_2;
				res->printingHeight += wrapped_lines_2;
			}

			if (completed && !startIncompleteSet) {
				res->printed = true;
			}
			else if (!startIncompleteSet) {
				startIncompleteSet = true;
			}
		}
		lastPrintedResultCount = printedResultCount;

		while (idx_startUnprinted < results.size() && results[idx_startUnprinted]->printed) {
			if (results[idx_startUnprinted]->occurences.size() > 0 && idx_startUnprinted)
				completed_last_iter += results[idx_startUnprinted]->printingHeight;
			idx_startUnprinted++;
		}

		float progress = float(completed_files)*100./total_files;
		buf << std::setw(5)
				  << std::fixed << std::setprecision(1)
				  << progress << "%\n" << std::flush;

		std::cout << buf.str();
		progress_printed = true;
	}

	//// --- Cleanup ---
	aborted = true;
	queue_cv.notify_all(); // Wake up all workers to see the aborted flag
	for (auto& t : pool) t.join(); // Wait for all worker threads to finish
	if (abort_thread.joinable()) abort_thread.detach(); // Detach the input thread

	const bool debug_check = true;
	if (sort_result) {

		//delete_last_lines(results.size() * 2); // Clear the remaining active lines
		// Create a copy to sort, so original `results` order (by processing start time) is preserved if needed.
		std::vector<std::shared_ptr<SearchResult>> resultsSorted = results;
		//std::sort(
		//	resultsSorted.begin(), 
		//	resultsSorted.end(), 
		//	[](const std::shared_ptr<SearchResult>& a, const std::shared_ptr<SearchResult>& b) {
		//		return a->pdf_path < b->pdf_path;
		//	}
		//);

		if (resultsSorted.empty()) {
			std::cout << "No PDF files containing \"" << target << "\" found in " << dir << "\n";
		} else {
			std::cout << "\nFinal matching results (sorted):\n";
			for (auto res : resultsSorted) {
				// Only print if there were any occurrences found
				if (!res->occurences.empty()) {
					// Line 1: Filename and optional path
					std::cout << res->pdf_path.filename().string();
					if (print_path)
						std::cout << "    " << res->pdf_path.parent_path();
					std::cout << "\n";

					// Line 2: Tab followed by pages (ensuring sorted and unique)
					std::cout << "    ";
					std::vector<int> final_pages_for_output;
					for(const auto& occ : res->occurences) {
						final_pages_for_output.push_back(occ.page);
					}
					std::sort(final_pages_for_output.begin(), final_pages_for_output.end());
					final_pages_for_output.erase(std::unique(final_pages_for_output.begin(), final_pages_for_output.end()), final_pages_for_output.end());

					for (size_t i = 0; i < final_pages_for_output.size(); ++i) {
						if (i > 0) std::cout << ", ";
						std::cout << final_pages_for_output[i];
					}
					std::cout << "\n";

					// If print_line is requested, show full line details (these are separate, not part of the 2-line result)
					if (print_line) {
						for (const auto& occ : res->occurences) {
							std::cout << "        Page " << occ.page << ", Line " << occ.line_number << ": " << occ.line << "\n";
						}
					}
				}
			}
		}
	}

	return 0;
}