/*
 * racecamd.cpp - racecam daemon driver.
 */

#include <csignal>
#include <linux/input.h>

#include "core/rcam.hpp"

#include <filesystem>
#include <pwd.h>     // Required for getpwuid

#include "core/gpio.hpp"

// #define DEBUG 1
#ifdef DEBUG
    #define DEBUG_PRINT(fmt, ...) \
	fprintf(stderr, "%s:%d:%s" fmt, __FILE__, __LINE__, __PRETTY_FUNCTION__, ##__VA_ARGS__) 
#else
    #define DEBUG_PRINT(fmt, ...)
#endif

//#define HEX( x )   std::setw(2) << std::setfill('0') << std::hex << (int)( x )

GPIO gpio;

Logger* logger_gptr = nullptr;
//Logger logger(rcamSrcPath + "/logs/RaceCamd.log");

std::atomic<bool> RunProgram {true};
std::atomic<bool> Capture {false};
// std::atomic<bool> SwitchTwo {false};
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
void blink()
{
    while (Capture.load()) {
	gpio.toggle(1);
	std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }	
}

inline void wait_read(int fd)
{
    DEBUG_PRINT("%s", "\n");
    struct input_event ev;
//    struct timeval press_time;
    do {
	ssize_t bytes_read = read(fd, &ev, sizeof(ev));
	
	if (!RunProgram) return; // SIGINT and SIGTERM stops read() wait so return

	if (bytes_read == sizeof(ev)) {
            // Process the event
            // ev.type defines the type of event (e.g., EV_KEY for key press/release)
            // ev.code defines which key/axis (e.g., KEY_A, REL_X)
            // ev.value defines the state (e.g., 1 for press, 0 for release, 2 for repeat)
            if (ev.type == EV_KEY) {
                if (ev.value == 1) {
	//	    press_time = ev.time;
		    continue;
                } else if (ev.value == 0) {
	//	    timersub(&press_time, &ev.time, &press_time); 
	//	    auto duration_seconds = std::chrono::seconds{press_time.tv_sec};
	//	    auto duration_microseconds = std::chrono::microseconds{press_time.tv_usec};
	//	    std::chrono::microseconds duration_ms = duration_seconds + duration_microseconds;
		    Capture = !Capture.load();
		    return;
		} else {
    //WEK convert all new messages to logger
		    std::cerr << "wait_read() event type EV_KEY is not press or relase!" << std::endl;
		}		
	    } else if (ev.type == EV_SYN && ev.code == SYN_REPORT) {
		continue;
	    } else {
		std::cerr << "wait_read() event type is not EV_KEY! " << ev.type << std::endl;
	    }
	} else {
	    std::cerr << "wait_read() enent is wrong length! " << 
		bytes_read << " vs " << sizeof(ev) << std::endl;
	}	    
    } while(true);
}

int main(int argc, char *argv[])
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
	logfile = home + "/racecam/logs/" + "RaceCamd.log";
    } else { 
	std::filesystem::create_directories(home + "/racecam/logs");
	if (std::filesystem::exists("/var/log/racecam")) {
	    logfile = std::string("/var/log/racecam/") + "RaceCamd.log";
	} else {
	    throw std::runtime_error("Unable to find path for log file!");
	}
    }
    Logger logger(logfile);
    logger_gptr = &logger;
    logger.SetLevel(LogLevel::INFO);
    
    struct sigaction sa;
    sa.sa_handler = sig_handler; 
    sigemptyset(&sa.sa_mask);      
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) == -1) {
	logger.Log(LogLevel::ERROR, "Error registering SIGINT handler", ToCOUT);
        exit(1);
    }
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
	logger.Log(LogLevel::ERROR, "Error registering SIGTERM handler", ToCOUT);
        exit(1);
    }
 
    std::string config_file {"racecam_config.json"};
    const char* dev = "/dev/input/by-path/platform-button@c-event"; 
    int fd = open(dev, O_RDONLY);
    if (fd == -1) {
        std::perror("Cannot open device");
        std::cerr << "Make sure you have the correct permissions (try 'sudo') "
                  << "and the device name is correct." << std::endl;
        return EXIT_FAILURE;
    }
    
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
		if (!config_file.size()) config_file = "racecam_config.json";
		RCam app(logger, config_file);
		logger.Log(LogLevel::INFO, "Starting capture!", ToCOUT); 
		app.InitCapture();
		std::thread blink_thread = std::thread(&blink);
		wait_read(fd);
		blink_thread.join();
		logger.Log(LogLevel::INFO, "Stopping capture!", ToCOUT);
		app.FreeCapture();
	    } 
	    wait_read(fd);
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
    close(fd);
    exit(0);
}
