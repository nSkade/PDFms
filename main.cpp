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

namespace fs = std::filesystem;

// Result structure
struct SearchResult	{
	fs::path pdf_path;
	int	page;
	int	line_number;
	std::string	line;
};

// Get all PDF files in	directory (optionally shuffled)
std::vector<fs::path> get_pdf_files(const fs::path&	directory, bool	shuffle) {
	std::vector<fs::path> pdf_files;
	for	(const auto& entry : fs::recursive_directory_iterator(directory)) {
		if (entry.is_regular_file()	&& entry.path().extension()	== ".pdf") {
			pdf_files.push_back(entry.path());
		}
	}
	if (shuffle) {
		std::random_device rd;
		std::mt19937 g(rd());
		std::shuffle(pdf_files.begin(),	pdf_files.end(), g);
	}
	return pdf_files;
}

void suppress_poppler_stderr() {
#ifdef _WIN32
	freopen("NUL", "w", stderr);
#else
	freopen("/dev/null", "w", stderr); // On Windows: "NUL"
#endif
}

#ifdef _WIN32
#include <windows.h>
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


int	main(int argc, char* argv[]) {
	suppress_poppler_stderr();
	enable_ansi_escape_codes();
	// --- Settings	---
	bool shuffle = false;
	bool sort_result = false;
	bool print_line = false;
	bool print_path = false;
	std::string	directory;
	std::string	target;

	// --- Parse command line ---
	for	(int i = 1;	i <	argc; ++i) {
		std::string	arg(argv[i]);
		if (arg	== "--shuffle")	shuffle	= true;
		else if	(arg ==	"--sort") sort_result =	true;
		else if	(arg ==	"--printline") print_line=	true;
		else if	(arg ==	"--printpath") print_path=	true;
		else if	(directory.empty())	directory =	arg;
		else if	(target.empty()) target	= arg;
	}

	if (target.empty())	{
		if (!directory.empty())	{
			std::swap(directory, target);
		} else {
			std::cerr << "Usage: " << argv[0] << " [<directory>] <search-string> [--shuffle] [--sort] [--printline] [--printpath]\n";
			return 1;
		}
	}

	fs::path dir = directory.empty() ? fs::current_path() :	fs::path(directory);

	// Prepare file	list
	std::vector<fs::path> pdfs = get_pdf_files(dir,	shuffle);
	std::atomic<size_t>	file_index{0};
	std::atomic<bool> aborted{false};
	size_t total_files = pdfs.size();

	// Thread-safe result queue
	std::queue<SearchResult> result_queue;
	std::mutex queue_mutex;
	std::condition_variable	queue_cv;

	std::atomic<size_t>	completed_files{0};
	std::atomic<bool> all_done{false};

	// --- Abort input thread ---
	std::thread	abort_thread([&aborted]() {
		std::string	line;
		while (std::getline(std::cin, line)) {
			if (line ==	"q"	|| line	== "Q")	{
				aborted	= true;
				break;
			}
		}
	});

	// --- Worker thread function ---
	auto worker	= [&]()	{
		while (!aborted) {
			size_t idx = file_index.fetch_add(1);
			if (idx	>= total_files)	break;

			const auto&	pdf_path = pdfs[idx];
			auto doc = poppler::document::load_from_file(pdf_path.string());
			if (!doc) {
				std::cerr << "Failed to	open: "	<< pdf_path	<< "\n";
				completed_files++;
				continue;
			}

			int	pages =	doc->pages();
			for	(int i = 0;	i <	pages && !aborted; ++i)	{
				std::unique_ptr<poppler::page> page(doc->create_page(i));
				if (!page)
					continue;

				auto utf8 =	page->text().to_utf8();
				std::string	page_text(utf8.data(), utf8.size());

				std::istringstream iss(page_text);
				std::string	line;
				int	line_number	= 0;

				while (std::getline(iss, line))	{
					++line_number;
					if (line.find(target) != std::string::npos)	{
						SearchResult result{ pdf_path, i + 1, line_number, line	};
						{
							std::lock_guard<std::mutex>	lock(queue_mutex);
							result_queue.push(result);
						}
						queue_cv.notify_one();
					}
				}
			}
			completed_files++;
			queue_cv.notify_one(); // in case main thread is waiting
		}
	};

	// --- Launch worker threads ---
	size_t num_threads = std::max<size_t>(1, std::thread::hardware_concurrency() - 2);
	std::vector<std::thread> pool;
	for	(size_t	i =	0; i < num_threads;	++i) {
		pool.emplace_back(worker);
	}

	// --- Main	results	printing + progress	loop ---
	std::vector<SearchResult> all_results;

	while (!aborted	&& (completed_files	< total_files || !result_queue.empty())) {
		std::unique_lock<std::mutex> lock(queue_mutex);
		queue_cv.wait_for(lock,	std::chrono::milliseconds(200),	[&]() {
			size_t done = completed_files.load();
			float progress = static_cast<float>(done) / total_files * 100.0f;
			static int last_percent = -1;
			int current_percent = static_cast<int>(progress);
			if (current_percent != last_percent) {
				last_percent = current_percent;
				std::cout << std::setw(5)
						  << std::fixed << std::setprecision(1)
						  << progress << "%" << std::flush;
				std::cout << std::flush << "\033[6D";
			}
			return !result_queue.empty() ||	aborted;
		});

		bool was_results_queue_empty = result_queue.empty();
		while (!result_queue.empty()) {
			// remove percentage
			SearchResult res = result_queue.front();
			result_queue.pop();
			all_results.push_back(res);

			static fs::path previousFile;

			if (!sort_result) {
				static int prev_page= -1;
				if (previousFile != res.pdf_path) {
					// Progress	display
					std::cout << "\n" << res.pdf_path.filename();
					if (print_path)
						std::cout << " " << res.pdf_path.parent_path();
					prev_page = -1;
					std::cout << ": Page ";
				}

				if (res.page != prev_page) {
					std::cout << res.page << ", ";
				
					//TODO(skade) make option to print line number
					//		  << ",	Line " << res.line_number;
					//if (print_line) //TODO(skade) reimplement
					//	std::cout << ":	" << res.line << "\n";
					//else
					//	std::cout << "\n";
				}
				prev_page = res.page;
			}
			previousFile = res.pdf_path;
		}
	}

	// --- Cleanup ---
	aborted	= true;
	for	(auto& t : pool) t.join();
	if (abort_thread.joinable()) abort_thread.detach();

	// --- Print final sorted results if enabled ---
	if (sort_result) {
		std::sort(all_results.begin(), all_results.end(), [](const SearchResult& a,	const SearchResult&	b) {
			if (a.pdf_path != b.pdf_path) return a.pdf_path	< b.pdf_path;
			if (a.page != b.page) return a.page	< b.page;
			return a.line_number < b.line_number;
		});

		if (all_results.empty()) {
			std::cout << "No PDF files containing \"" << target	<< "\" found in	" << dir <<	"\n";
		} else {
			std::cout << "\nFinal matching results:\n";
			for	(const auto& res : all_results)	{
				std::cout << res.pdf_path << ":	Page " << res.page
						  << ",	Line " << res.line_number
						  << ":	" << res.line << "\n";
			}
		}
	}

	//TODO(skade) aborted is used for logic it means something different here
	//std::cout << (aborted ?	"**	Search aborted by user **\n" : "** Search complete **\n");
	return 0;
}
