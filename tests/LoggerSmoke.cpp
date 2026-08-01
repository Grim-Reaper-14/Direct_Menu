#include "logging/Logger.hpp"

#include <iostream>
#include <sstream>
#include <string>

int main() {
    std::ostringstream consoleCapture;
    std::streambuf* previous = std::clog.rdbuf(consoleCapture.rdbuf());

    smf::logging::LoggerApi logger;
    logger.SetConsoleOutputEnabled(true);
    logger.Info("console mirror smoke test");
    logger.SetConsoleOutputEnabled(false);

    std::clog.rdbuf(previous);
    if (consoleCapture.str().find("[INFO]") == std::string::npos ||
        consoleCapture.str().find("console mirror smoke test") == std::string::npos) {
        std::cerr << "Logger did not mirror the formatted entry to the console.\n";
        return 1;
    }

    return 0;
}
