#pragma once

#include "SearchedFiles.hpp"
#include "util.hpp"

struct OutThread {
	SearchedFiles* sf;
	OutThread(SearchedFiles* sf) : sf(sf) {}

	void print() {
		// --- Main Display Logic (Block Update) ---
		size_t idx_startUnprinted = 0; // Tracks the first file that is not yet finalized.
		size_t lastPrintedResultCount = 0; // How many *results* (each 2 lines) were displayed in the *previous* refresh.

		auto allPrinted = [&]() -> bool {
			std::lock_guard<std::mutex> lock(sf->results_mutex);
			for (int i=0;i<sf->results.size();++i) {
				auto r = sf->results[i];
				if (r && !r->getPrinted())
					return false;
			}
			return true;
		};

	#if 1 // old
		std::mutex printMutex;
		bool progress_printed = false;
		int completed_last_iter = 0;
		while (!sf->aborted && (sf->completed_files < sf->total_files || !allPrinted())) {
			// Wait for a short period or until notified that new results are available.
			std::unique_lock<std::mutex> printLock(printMutex);
			sf->queue_cv.wait_for(printLock, std::chrono::milliseconds(200), [&]() {
				//float progress = float(completed_files)*100./total_files;
				//std::cout << std::setw(5)
				//		  << std::fixed << std::setprecision(1)
				//		  << progress << "%\n" << std::flush;
				//delete_last_lines(1);
				return true;
			});

			if (progress_printed)
				terminal::delete_last_lines(1); // print progress
			//assert(lastPrintedResultCount >= completed_last_iter);
			terminal::delete_last_lines((lastPrintedResultCount-completed_last_iter));

			//if (lastPrintedResultCount > 0 && completed_last_iter > 0 && lastPrintedResultCount == completed_last_iter)
			//	std::cout << "break\n";
			//std::cout << "would deleting " << lastPrintedResultCount << " - " << completed_last_iter << " x2 lines\n";
			completed_last_iter = 0;
			size_t printedResultCount = 0; // Reset count for results printed in this cycle

			std::stringstream buf;
			bool startIncompleteSet = false;

			int count = 0;
			{ //  dont lock for whole duration to avoid blocking search threads
				std::lock_guard<std::mutex> lock(sf->results_mutex);
				count = sf->results.size();
			}
			for (int i = idx_startUnprinted; i < count; ++i) {

				std::shared_ptr<SearchResult> res;
				bool completed;
				{
					std::unique_lock<std::mutex> lock(sf->results_mutex);
					res = sf->results[i];
				}
				completed = res->getCompleted();

				auto occurences = res->getOccurences();
				if (completed && occurences.size()==0) {
						res->setPrinted(true);
						continue;
					}
				else if (!completed && occurences.size()==0) {
					continue;
				}

				std::vector<int> display_pages;
				for (const auto& occ : occurences)
					display_pages.push_back(occ.page);
				std::sort(display_pages.begin(), display_pages.end());
				display_pages.erase(std::unique(display_pages.begin(), display_pages.end()), display_pages.end());

				{ // printing
					res->setPrintingHeight(0);
					int consoleWidth = terminal::getConsoleWidth();
					
					// --- Line 1: Filename and optional path ---
					std::string line1_content = res->getPdfPath().filename().string();

					//TODO was arg, reimpl later
					//if (print_path) {
					//	line1_content += "	" + res->getPdfPath().parent_path().string();
					//}
					
					int wrapped_lines_1 = (line1_content.length() + consoleWidth - 1) / consoleWidth;
					
					buf << line1_content << "\n";
					printedResultCount += wrapped_lines_1;
					int resPh = wrapped_lines_1;
					
					// --- Line 2: Tab followed by pages ---
					std::string line2_content = "	"; // Start with the tab
					for (size_t j = 0; j < display_pages.size(); ++j) {
						if (j > 0) line2_content += ", ";
						line2_content += std::to_string(display_pages[j]);
					}
					
					int wrapped_lines_2 = (line2_content.length() + consoleWidth - 1) / consoleWidth;
					
					buf << line2_content << "\n";
					printedResultCount += wrapped_lines_2;
					resPh += wrapped_lines_2;
					res->setPrintingHeight(res->getPrintingHeight()+resPh);
				}

				if (completed && !startIncompleteSet) {
					res->setPrinted(true);
				}
				else if (!startIncompleteSet) {
					startIncompleteSet = true;
				}
			}
			lastPrintedResultCount = printedResultCount;

			{
				std::unique_lock<std::mutex> lock(sf->results_mutex);
				while (idx_startUnprinted < sf->results.size() && sf->results[idx_startUnprinted]->getPrinted()) {
					if (sf->results[idx_startUnprinted]->getOccurences().size() > 0 && idx_startUnprinted)
						completed_last_iter += sf->results[idx_startUnprinted]->getPrintingHeight();
					idx_startUnprinted++;
				}
			}

			float progress = float(sf->completed_files)*100./sf->total_files;
			buf << std::setw(5)
					  << std::fixed << std::setprecision(1)
					  << progress << "%\n" << std::flush;

			std::cout << buf.str();
			progress_printed = true;
		}
	#else
	#endif

	}
};
