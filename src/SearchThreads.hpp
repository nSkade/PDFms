#pragma once

#include "SearchedFiles.hpp"
#include "util.hpp"

struct SearchThreads {
	std::vector<std::thread> pool;
	SearchedFiles* sf;
	std::string targetLower;
	SearchThreads(SearchedFiles* sf) : sf(sf) {
		
	}

	void search() {
		targetLower = pdf::tolower(sf->searchWord);

		auto worker_func = [&]() {
			while (!sf->aborted) {
				size_t idx = sf->file_index.fetch_add(1);
				if (idx >= sf->total_files) {
					//std::cout << "No more files to process\n";
					break; // No more files to process
				}

				const auto& pdf_path = sf->pdfFileNames[idx];
				std::string pdf_path_str;
				try {
					pdf_path_str = pdf_path.string();
				} catch (...) {
					sf->completed_files++;
					sf->erroredPaths.push_back(pdf_path.u8string());
					continue;
				}

				poppler::document* doc = nullptr;
				try {
					doc = poppler::document::load_from_file(pdf_path_str);
				} catch (...) {
					doc = nullptr;
				}

				if (!doc) {
					sf->completed_files++;
					continue;
				}
				std::shared_ptr<SearchResult> current_res = std::make_shared<SearchResult>();
				//current_res->pdf_path = pdf_path;
				current_res->setPdfPath(pdf_path);
				{
					std::lock_guard<std::mutex> lock(sf->results_mutex);
					sf->results.push_back(current_res);
				}

				int pages = doc->pages();
				for (int i = 0; i < pages && !sf->aborted; ++i) {
					auto page = std::unique_ptr<poppler::page>(doc->create_page(i));
					if (!page) continue;
					auto utf8 = page->text().to_utf8();
					std::string page_text(utf8.data(), utf8.size());
					std::istringstream iss(page_text);
					std::string line;
					int line_number = 0;
					while (std::getline(iss, line)) {
						++line_number;
						if (pdf::tolower(line).find(targetLower) != std::string::npos) {
							Occurence occurrence{ i + 1, line_number, line };
							// Add occurrence directly to shared SearchResult
							current_res->addOccurrence(occurrence);
							//current_res->occurences.push_back(occurrence); //TODO(remove)
							// Notify the main thread that there's an update.
							// This notification is what allows the main thread to pick up incremental page findings.
							sf->queue_cv.notify_one();
						}
					}
				}

				// After processing all pages for this PDF
				{
					//current_res->completed = true; //TODO(remove)
					current_res->setCompleted(true);
					sf->completed_files++;
					sf->queue_cv.notify_one(); // Notify main thread that this file is fully completed
				}

				delete doc; // Clean up Poppler document
			}
		};

		// --- Launch worker threads ---
		size_t num_threads = std::max<size_t>(1, std::thread::hardware_concurrency() - 1);
		for (size_t i = 0; i < num_threads; ++i) {
			pool.emplace_back(worker_func);
		}
	}
};
