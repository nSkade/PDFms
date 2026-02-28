#pragma once

#include <string>
#include <filesystem>
namespace fs = std::filesystem;

#include <vector>
#include <mutex>

struct Occurence {
	int page;
	int line_number;
	std::string line;
};

class SearchResult {
	fs::path pdf_path;
	std::vector<Occurence> occurences;
	bool completed = false;
	bool printed = false; // True when the final 2-line output for this result has been printed and finalized.
	int printingHeight = 0;
	std::mutex mtx;
public:
	const fs::path& getPdfPath() const { return pdf_path; }
	std::vector<Occurence> getOccurences() { std::lock_guard<std::mutex> guard(mtx); return occurences; }
	bool getCompleted() { std::lock_guard<std::mutex> guard(mtx); return completed; }
	bool getPrinted() { std::lock_guard<std::mutex> guard(mtx); return printed; }
	int getPrintingHeight() { std::lock_guard<std::mutex> guard(mtx); return printingHeight; }
	
	void setPdfPath(const fs::path& path) { std::lock_guard<std::mutex> guard(mtx); pdf_path = path; }
	void addOccurrence(const Occurence& occ) { std::lock_guard<std::mutex> guard(mtx); occurences.push_back(occ); }
	void setCompleted(bool status = true) { std::lock_guard<std::mutex> guard(mtx); completed = status; }
	void setPrinted(bool status = true) { std::lock_guard<std::mutex> guard(mtx); printed = status; }
	void setPrintingHeight(int height) { std::lock_guard<std::mutex> guard(mtx); printingHeight = height; }
};