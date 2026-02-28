#include <string>
#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__) || defined(__APPLE__)
#include <sys/ioctl.h>
#include <unistd.h>
#endif

#include <poppler-document.h>
#include <poppler-page.h>

#include <filesystem>
namespace fs = std::filesystem;

#include <algorithm>
#include <iostream>

// random
#include <random>
#include <cstdio>
#include <cctype>

#include <memory>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <chrono>

#include <assert.h>