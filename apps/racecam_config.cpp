/*
 * racecam_config.cpp - configure racecam parms to json.
 * 
 */

#include "core/rcamcfg.hpp"

#include <filesystem>
#include <pwd.h>     // Required for getpwuid

//#define DEBUG 1
#ifdef DEBUG
    #define DEBUG_PRINT(fmt, ...) \
	fprintf(stderr, "%s:%d:%s" fmt, __FILE__, __LINE__, __PRETTY_FUNCTION__, ##__VA_ARGS__) 
#else
    #define DEBUG_PRINT(fmt, ...)
#endif

// #define HEX( x )   std::setw(2) << std::setfill('0') << std::hex << (int)( x )

void printUsage(char * arg)
{
    DEBUG_PRINT("%s", "\n");
    std::cout << " usage: " << arg << " [option] ..." << std::endl;
    std::cout << "  -h, --help          display command line options" << std::endl;
    std::cout << "  -c, --config_file   configuration file (full or relative path)" << std::endl;
}

int main(int argc, char *argv[])
//int main()
{
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
		logfile = home + "/racecam/logs/" + "RaceCamCfg.log";
	} else {
		std::filesystem::create_directories(home + "/racecam/logs");
		if (std::filesystem::exists("/var/log/racecam")) {
			logfile = std::string("/var/log/racecam/") + "RaceCamCfg.log";
		} else {
			throw std::runtime_error("Unable to find path for log file!");
		}
	}
	
	Logger logger(logfile, LogLevel::WARN);
	
	std::string config_file {};
	for (int i = 1; i < argc; ++i) {
		std::string arg = argv[i];
		if (arg == "-h" || arg == "--help") {
			printUsage(argv[0]);
			exit(0);
		} else if (arg == "-c" || arg == "--config_file") {
			if (i + 1 < argc) { 
				config_file = argv[i+1];
				i++; 
			} else {
				logger.Log(LogLevel::ERROR, "Error: --config_file requires a file.", true);
				printUsage(argv[0]);
				exit(1);
			}
		} else {
			logger.Log(LogLevel::ERROR, "Invalid option found!!", true);
			printUsage(argv[0]);
			exit(1);
		}
	}

	try 
	{

		if (config_file.empty()) {
			RCamCfg app(logger);
			app.CfgRaceCam();
		} else {
			RCamCfg app(logger, config_file);
			app.CfgRaceCam();
		}
	}
	catch(const std::runtime_error& error) 
	{
		logger.Log(LogLevel::ERROR, std::string("Runtime exception: ") + error.what(), true);
        exit(1);
    }
	catch(...)
	{
		logger.Log(LogLevel::ERROR, "exiting with exception", true);
		exit(1);
	}
	std::cout << std::endl;
	exit(0);
}







