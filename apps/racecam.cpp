/*
 * racecam.cpp - racecam main driver.
 * 
 * RaceCam Is an app for multiple camera video capture both locally and streaming.
 * Copyright (C) <2026> <William Keeling>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <csignal>
#include <fcntl.h> 

#include "core/rcam.hpp"

#include <filesystem>
#include <pwd.h>     // Required for getpwuid

#include "core/led.hpp"

//#define DEBUG 1
#ifdef DEBUG
    #define DEBUG_PRINT(fmt, ...) \
	fprintf(stderr, "%s:%d:%s" fmt, __FILE__, __LINE__, __PRETTY_FUNCTION__, ##__VA_ARGS__) 
#else
    #define DEBUG_PRINT(fmt, ...)
#endif

//#define HEX( x )   std::setw(2) << std::setfill('0') << std::hex << (int)( x )

LED led;

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

//WEK if not runing then just exit here?
void sig_handler(int signum)
{
    DEBUG_PRINT("%s", "\n");
    if (signum == SIGTERM || signum == SIGINT) {
	RunProgram.store(false);
    }
}

inline void wait()
{
    DEBUG_PRINT("%s", "\n");
    int dur = Duration;
    while (RunProgram.load()) {
//	fprintf(stderr, "%s:%d:%s\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);
	std::this_thread::sleep_for(std::chrono::milliseconds(500));
	led.toggle(1);
	std::this_thread::sleep_for(std::chrono::milliseconds(500));
	led.toggle(1);
	if (!--dur) {
	    RunProgram.store(false);
	}
    }
 }

int main(int argc, char *argv[])
{
//    logger.SetLevel(LogLevel::INFO);
    DEBUG_PRINT("%s", "\n");
    const char* homedir = getenv("HOME");
    if (homedir == nullptr) {
        struct passwd *pw = getpwuid(getuid());
        if (pw != nullptr) {
            homedir = pw->pw_dir;
        }
    }
    std::string home(homedir);
    std::string logfile;
    if (std::filesystem::exists(home + "/racecam/logs")) { 
	logfile = home + "/racecam/logs/" + "RaceCam.log";
    } else {
	std::filesystem::create_directories(home + "/racecam/logs");
	if (std::filesystem::exists("/var/log/racecam")) {
	    logfile = std::string("/var/log/racecam/") + "RaceCam.log";
	} else {
	    throw std::runtime_error("Unable to find path for log file!");
	}
    }
    Logger logger(logfile);
    
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
    std::string config_file {};
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
 //   fprintf(stderr, "%s:%d:%s\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);
    if (!Duration) Duration = 15; 
    try {
//	led.set(1, true);
	std::optional<bool> led_state = led.get(1);
	if (!config_file.size()) config_file = "racecam_config.json";
	RCam app(logger, config_file);
	logger.Log(LogLevel::ALWAYS, "Starting capture!", true);
	app.InitCapture();
	wait();
	logger.Log(LogLevel::ALWAYS, "Stopping capture!", true);
	app.FreeCapture();
//	led.set(1, false);
	if (led_state.has_value()) led.set(1, led_state.value());
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
