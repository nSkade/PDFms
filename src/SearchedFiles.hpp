#pragma once

#include "SearchResult.hpp"

struct SearchedFiles {
	std::string searchWord;
	std::vector<fs::path> pdfFileNames;
	
	std::vector<std::string> erroredPaths; //TODO make SearchResult with error instead

	size_t total_files = 0;

	std::vector<std::shared_ptr<SearchResult>> results;
	std::mutex results_mutex; // Mutex to protect 'results' vector modifications

	std::condition_variable queue_cv; // Condition variable to signal updates to main thread

	std::atomic<size_t> file_index{ 0 }; // Atomic counter for files to be processed by workers
	std::atomic<size_t> completed_files{ 0 }; // Atomic counter for completed files
	std::atomic<bool> aborted{ false }; // Flag to signal threads to stop
};