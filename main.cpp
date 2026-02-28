#include "src/util.hpp"
#include "src/SearchThreads.hpp"
#include "src/OutThread.hpp"

int main(int argc, char* argv[]) {
	pdf::suppress_poppler_stderr();
	terminal::enable_ansi_escape_codes();

	// --- Settings ---
	bool shuffle = false;
	bool sort_result = false;
	bool print_line = false;
	bool print_path = false;
	std::string directory;
	SearchedFiles sf;

	// --- Parse command line ---
	for (int i = 1; i < argc; ++i) {
		std::string arg(argv[i]);
		if (arg == "--shuffle") shuffle = true;
		else if (arg == "--sort") sort_result = true;
		else if (arg == "--printline") print_line = true;
		else if (arg == "--printpath") print_path = true;
		else if (directory.empty()) directory = arg;
		else if (sf.searchWord.empty()) sf.searchWord = arg;
	}

	if (sf.searchWord.empty()) {
		if (!directory.empty()) {
			std::swap(directory, sf.searchWord);
		} else {
			std::cout << "Usage: " << argv[0] << " [<directory>] <search-string> [--shuffle] [--sort] [--printline] [--printpath]\n";
			return 1;
		}
	}

	fs::path dir = directory.empty() ? fs::current_path() : fs::path(directory);
	sf.pdfFileNames = pdf::get_pdf_files(dir, shuffle);

	sf.total_files = sf.pdfFileNames.size();

	SearchThreads st(&sf);
	OutThread ot(&sf);
	
	st.search();
	ot.print();
	
		//// --- Cleanup ---
		sf.aborted = true;
		sf.queue_cv.notify_all(); // Wake up all workers to see the aborted flag
		for (auto& t : st.pool) t.join(); // Wait for all worker threads to finish
		
		// --- Abort input thread ---
		std::thread abort_thread([&]() {
			std::string line;
			std::getline(std::cin, line);
			sf.aborted = true;
		});
		if (abort_thread.joinable()) abort_thread.detach(); // Detach the input thread

		//const bool debug_check = true;

		std::cout << "completed!\n";
		if (sf.erroredPaths.size())
			std::cout << "\nerroredPaths:\n";
		for (auto& s : sf.erroredPaths)
			std::cout << s << std::endl;

		//TODO fix for new mutex
	#if 0
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
							std::cout << "	" << res->pdf_path.parent_path();
						std::cout << "\n";

						// Line 2: Tab followed by pages (ensuring sorted and unique)
						std::cout << "	";
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
								std::cout << "		Page " << occ.page << ", Line " << occ.line_number << ": " << occ.line << "\n";
							}
						}
					}
				}
			}
		}
	#endif
	return 0;
}