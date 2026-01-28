/*
 * racecamd.cpp - racecam daemon driver.
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
// #define DEBUG 1
#ifdef DEBUG
    #define DEBUG_PRINT(fmt, ...) \
	fprintf(stderr, "%s:%d:%s" fmt, __FILE__, __LINE__, __PRETTY_FUNCTION__, ##__VA_ARGS__) 
#else
    #define DEBUG_PRINT(fmt, ...)
#endif

//#define HEX( x )   std::setw(2) << std::setfill('0') << std::hex << (int)( x )

GPIO gpio;

// Logger* g_lptr = nullptr;
// Logger logger(rcamSrcPath + "/logs/RaceCamd.log", g_lptr);
Logger logger(rcamSrcPath + "/logs/RaceCamd.log");

std::atomic<bool> RunProgram {true};
std::atomic<bool> Capture {false};
std::atomic<bool> SwitchTwo {false};
bool ToCOUT = {false};

void printUsage(char * arg)
{
    DEBUG_PRINT("%s", "\n");
    std::cout << " usage: " << arg << " [configfile] [option] ..." << std::endl;
    std::cout << "  -h, --help          display command line options" << std::endl;
    std::cout << "  -c, --forground     display messages for forground use" << std::endl;
    exit(1);
}

void sig_handler(int signum)
{
    DEBUG_PRINT("%s", "\n");
    
    if (signum == SIGTERM || signum == SIGINT) 
    {
	RunProgram = false;
	Capture = false;
    }
}

inline void wait()
{
    DEBUG_PRINT("%s", "\n");
    bool save_state = Capture.load();
    do { 
	if (!RunProgram.load()) return;
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	if (Capture) gpio.toggle(1);
	if (gpio.get() == 1) Capture = !Capture.load();
	if (gpio.get() == 2) SwitchTwo = !SwitchTwo.load();
    } while (save_state == Capture.load());
}

int main(int argc, char *argv[])
{
    DEBUG_PRINT("%s", "\n");
    logger.SetLevel(LogLevel::INFO);
    
    struct sigaction sa;
    sa.sa_handler = sig_handler; 
    sigemptyset(&sa.sa_mask);      
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) == -1) {
	logger.Log(LogLevel::ERROR, "Error registering SIGINT handler", ToCOUT);
  //      std::cerr << "Error registering SIGINT handler" << std::endl;
        exit(1);
    }
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
	logger.Log(LogLevel::ERROR, "Error registering SIGTERM handler", ToCOUT);
  //      std::cerr << "Error registering SIGTERM handler" << std::endl;
        exit(1);
    }
 
    std::string config_file {"racecam_config.json"};
    
    for (int i = 1; i < argc; ++i) 
    {
	std::string arg = argv[i];
	if (arg == "-h" || arg == "--help") 
	{
	    printUsage(argv[0]);
	    exit(0);
	} else if (arg == "-f" || arg == "--forground") 
	{
	    ToCOUT = true;
	} else {
	    config_file = argv[i];
	}
    }
    
    do {
	try {
	    gpio.set(1, true);

	    if (Capture) {
		RCam app(logger, rcamSrcPath, config_file);
		logger.Log(LogLevel::INFO, "Starting capture!", ToCOUT); 
		app.InitCapture();
		wait();
		logger.Log(LogLevel::INFO, "Stopping capture!", ToCOUT);
		app.FreeCapture();
	    } 
	    wait();
	    gpio.set(1, false);
	}
	catch(const std::runtime_error& error) {
	    std::string temp{"Runtime exception: "};
	    temp.append(error.what());
	    logger.Log(LogLevel::ERROR, temp, ToCOUT);
	}
	catch(...) {
	    logger.Log(LogLevel::ERROR, "Found unhandled exception", ToCOUT);
	} 
    } while (RunProgram.load());
    exit(0);
}
