/*
 * racecam.cpp - racecam main driver.
 * 
 */

//import std;

//#include <iostream>
//#include <limits>
#include <csignal>
//#include <chrono>
//#include <atomic>

//#include "core/rcam_app.hpp"
#include "core/rcam.hpp"
//#include "core/logger.hpp"
#include "racecamsrc.hpp"

#include "core/gpio.hpp"

//#include <stdio.h>
//#define DEBUG 1
#ifdef DEBUG
    #define DEBUG_PRINT(fmt, ...) \
	fprintf(stderr, "%s:%d:%s" fmt, __FILE__, __LINE__, __PRETTY_FUNCTION__, ##__VA_ARGS__) 
#else
    #define DEBUG_PRINT(fmt, ...)
#endif

//#define HEX( x )   std::setw(2) << std::setfill('0') << std::hex << (int)( x )

GPIO gpio;
//Logger* g_lptr = nullptr;
// Logger logger(rcamSrcPath + "/logs/RaceCam.log", g_lptr);
Logger logger(rcamSrcPath + "/logs/RaceCam.log");
//Logger logger(rcamSrcPath + "/logs/RaceCam.log", LogLevel::INFO);

std::atomic<bool> RunProgram {true};
int Duration {0};

void printUsage(char * arg)
{
    DEBUG_PRINT("%s", "\n");
    std::cout << " usage: " << arg << " [option] ..." << std::endl;
    std::cout << "  -h, --help          display command line options" << std::endl;
    std::cout << "  -c, --config_file   configuration file (full or relative path)" << std::endl;
    std::cout << "  -d, --duration      number of seconds to capture" << std::endl;
}

void sig_handler(int signum)
{
    DEBUG_PRINT("%s", "\n");
    if (signum == SIGTERM || signum == SIGINT) {
	RunProgram = false;
    }
}

inline void wait()
{
    DEBUG_PRINT("%s", "\n");
    int dur = Duration;
    while (RunProgram.load()) {
	std::this_thread::sleep_for(std::chrono::milliseconds(500));
	gpio.toggle(1);
	std::this_thread::sleep_for(std::chrono::milliseconds(500));
	gpio.toggle(1);
	if (!--dur) {
	    RunProgram = false;
	}
    }
 }

int main(int argc, char *argv[])
{
    DEBUG_PRINT("%s", "\n");
    struct sigaction sa;
    sa.sa_handler = sig_handler; 
    sigemptyset(&sa.sa_mask);      
    sa.sa_flags = 0;
    // Register signal handler for SIGINT and SIGTERM
    if (sigaction(SIGINT, &sa, NULL) == -1) {
	logger.Log(LogLevel::ERROR, "Error registering SIGINT handler", true);
        exit(1);
    }
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
	logger.Log(LogLevel::ERROR, "Error registering SIGTERM handler", true);
        exit(1);
    }
     // parms
    std::string config_file {"racecam_config.json"};
    for (int i = 1; i < argc; ++i) {
	std::string arg = argv[i];
	if (arg == "-h" || arg == "--help") {
	    printUsage(argv[0]);
	    exit(0);
	} else if (arg == "-c" || arg == "--config_file") {
	    if (i + 1 < argc) { 
		config_file = argv[i+1];
 		logger.Log(LogLevel::INFO, std::string("configuration file: ") + argv[i+1], true);
                i++; 
	    } else {
		logger.Log(LogLevel::ERROR, "Error: --config_file requires a file.", true);
	    }
	} else if (arg == "-d" || arg == "--duration") {
	    if (i + 1 < argc) { 
		Duration = std::stol(argv[i+1]);
		logger.Log(LogLevel::INFO, std::string("duration: ") + argv[i+1], true);
		i++; 
	    } else {
		logger.Log(LogLevel::ERROR, "Error: --duration requires number of seconds.", true);
            }
	} else {
	    logger.Log(LogLevel::ERROR, "Invalid option found!!", true);
	    printUsage(argv[0]);
	    exit(1);
	}
    }

// main logic
    if (!Duration) Duration = 15;
    try {
	gpio.set(1, true);
	RCam app(logger, rcamSrcPath, config_file);
	logger.Log(LogLevel::INFO, "Starting capture!", true);
	app.InitCapture();
	wait();
	logger.Log(LogLevel::INFO, "Stopping capture!", true);
	app.FreeCapture();
	gpio.set(1, false);
	logger.Log(LogLevel::INFO, "exiting all good!", true);
	exit(0);
    }
    catch(const std::runtime_error& error) {
	logger.Log(LogLevel::ERROR, std::string("Runtime exception: ") + error.what(), true);
	exit(1);
    }
    catch(...) {
	logger.Log(LogLevel::ERROR, "exiting with unhandled exception", true);
	exit(1);
    } 
}
