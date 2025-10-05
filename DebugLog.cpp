// DebugLog.cpp
#include <iostream>
#include <fstream>

void WriteDebugLog(const std::string& message) {
    std::ofstream logFile("destructor_debug.txt", std::ios::app);
    if (logFile.is_open()) {
        logFile << message << std::endl;
        logFile.close();
    }
}
