/*
 * racecam_config.cpp - configure racecam parms to json.
 * 
 */

//import std;

//#include "core/rcamshared.hpp"
#include "core/rcamcfg.hpp"
#include "racecamsrc.hpp"

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
//	Logger* g_lptr = nullptr;
//	Logger logger(rcamSrcPath + "/logs/RaceCamCfg.log", g_lptr, LogLevel::WARN);
	Logger logger(rcamSrcPath + "/logs/RaceCamCfg.log", LogLevel::WARN);
	
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

	if (!config_file.empty()) {
		if ((!std::filesystem::exists(rcamSrcPath + "/data/" + config_file)) 
				&& (!std::filesystem::exists(config_file))) {
			std::ofstream cfg_stream(config_file, std::ios::app);
			if (cfg_stream.is_open()) {
				cfg_stream << "{}";
				cfg_stream.close();
			} else {
				logger.Log(LogLevel::ERROR, "Could not open/create file '" + config_file + "'", true);
			}
		} 
	} 

	try 
	{

//WEK add config file parm 
//	RCamCfg app("config.json"); //or take default cfgloc
//		RCamCfg app;
		if (config_file.empty()) {
			RCamCfg app(logger,rcamSrcPath);
			app.CfgRaceCam();
		} else {
			RCamCfg app(logger,rcamSrcPath, config_file);
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







