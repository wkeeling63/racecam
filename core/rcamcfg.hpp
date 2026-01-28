/*  racecam configuration class header
* rcamcfg.hpp
*/
//#include <iostream>
//#include <fstream>
//#include <filesystem>
//#include <string>
//#include <queue>
//#include <any>
//#include <span>
//#include <condition_variable>
//#include <ctime> 
//#include <iomanip> 
//#include <thread>
//#include <chrono>
//#include <limits>


//#include <libcamera/camera.h>
//#include <libcamera/camera_manager.h>
#include <libcamera/control_ids.h>
#include <libcamera/property_ids.h>
//#include <libcamera/transform.h>
//#include <libcamera/formats.h>
//#include <libcamera/logging.h>
//#include <libcamera/yaml_parser.h>

//#include <sys/mman.h>

#include <boost/json/src.hpp> 
//#include <boost/json.hpp>  //WEK can't figure out how to make meson link to boost_json 1.87 -- try on new build 
#include <yaml-cpp/yaml.h>

//#include "core/dma_heaps.hpp"
#include "core/message_map.hpp"
#include "core/rcamshared.hpp"
//#include "racecamsrc.hpp"
#include "core/youtube.hpp"
//#include "core/logger.hpp"


//extern "C"
//{
//#include "libavcodec/avcodec.h"
//#include "libavcodec/codec_desc.h"
//#include "libavdevice/avdevice.h"
//#include "libavformat/avformat.h"
//#include "libavutil/audio_fifo.h"
//#include "libavutil/hwcontext.h"
//#include "libavutil/hwcontext_drm.h"
//#include "libavutil/imgutils.h"
//#include "libavutil/timestamp.h"
//#include "libavutil/version.h"
//#include "libswresample/swresample.h"
//#include "libavfilter/buffersrc.h"
//#include "libavfilter/buffersink.h"
//#include "libavfilter/avfilter.h"
//}

#pragma once

// #define DEBUG 1
#ifdef DEBUG
    #define DEBUG_PRINT(fmt, ...) \
	fprintf(stderr, "%s:%d:%s" fmt, __FILE__, __LINE__, __PRETTY_FUNCTION__, ##__VA_ARGS__) 
#else
    #define DEBUG_PRINT(fmt, ...)
#endif

// constexpr size_t MAX_SIZE = std::numeric_limits<size_t>::max(); 
#define CLEARSCREEN std::cout << "\033[2J\033[1;1H" << std::flush;
#define CLEARPREVLINE std::cout << "\033[1F\033[2K" << std::flush;
#define CLEARPREVLINES std::cout << "\033[1F\033[2K\033[1F\033[2K" << std::flush;

#define LOC std::string(__FILE__) + ":" + std::string(__PRETTY_FUNCTION__) + ":" + std::to_string(__LINE__)
//#define LOC std::string(__FILE__) + ":" + std::string(__FUNCTION__) + ":" + std::to_string(__LINE__)

using namespace libcamera;
namespace json = boost::json;

typedef struct { 
	libcamera::Size size;
	double fps;
	int bitDepth; 
	std::string crop;
	} modes;

typedef struct {
	std::string location;
	int sl {-1};
	libcamera::Size cSize;
	libcamera::Size slSize;
} CamSL;

typedef struct {
	std::vector<CamSL> camSL;
	unsigned char used;
	unsigned char free;
	const std::string &getloc(int i) {return camSL.at(i).location;}
	const int &getsl(int i) {return camSL.at(i).sl;}
	int &setsl(int i) {return camSL.at(i).sl;}
	const libcamera::Size &getcSize(int i) {return camSL.at(i).cSize;}
	const libcamera::Size &getslSize(int i) {return camSL.at(i).slSize;}
	libcamera::Size &setslSize(int i) {return camSL.at(i).slSize;}
	
} CamsSL;
	
class RCamCfg : public RCamShared
{
public:
	using CameraManager = libcamera::CameraManager;
	using Camera = libcamera::Camera;
	
	RCamCfg(Logger& lptr, std::string const& path, std::string const& cfg = "racecam_config.json");
//	virtual ~RCamCfg(){};  
	virtual ~RCamCfg();  
	
	void	CfgRaceCam(void);
	
	protected:
	std::vector<std::shared_ptr<Camera>> getCameras() {return cm_->cameras();};
//	std::vector<std::shared_ptr<Camera>> getCameras(void);

	std::shared_ptr<Camera> getCamera(const std::string& id) {return cm_->get(id);};
	
	void 	cfgSensor(const std::shared_ptr<Camera>&);
	void 	cfgControl(const std::shared_ptr<Camera>&);
	void	cfgAudio(void);
	void	cfgRaw(void);
	void	cfgRawStreams(void);
	CamsSL  getCamsSL(bool stream=false);
	void	cfgComposite(void); 
	void	cfgCompositeDest(void);
	void	cfgLayer(int, bool stream=false);
	
	// get user input and input helper functions
	msg getMsg(const std::string);
	std::optional<bool> getBool(const std::string, const std::string);
	std::optional<int> getInt(const std::string, const std::string, int, int);
	std::optional<long> getLong(const std::string, const std::string, long, long);
	std::optional<float> getFloat(const std::string, const std::string, float, float);
	std::optional<std::string> getString(const std::string, const std::string);
	std::optional<libcamera::Size> getSize(const std::string, const std::string, const libcamera::Size, const libcamera::Size min = libcamera::Size());
	std::optional<libcamera::Rectangle> getRectangle(const std::string, const std::string, const libcamera::Rectangle, const libcamera::Rectangle min = libcamera::Rectangle());
	json::value getControlValue(const unsigned int id, const std::shared_ptr<Camera> &cam);
	std::string getCntlMsg(std::string cntl);
	json::value toJSON(ControlValue&);
	
	int menuUtil(const std::vector<std::string>&, const bool allownone = true);
	void displayCam(std::shared_ptr<Camera>);
	void displayLayer(json::value);
	void cfgCameras(void);
	void cfgCamera(std::shared_ptr<Camera>);
	
	// config json functions
	bool	dropCfgValue(const std::string&);
	bool	dropCfgValue(const std::string&, json::value& cfg);
	bool	setCfgValue(const std::string& key, const json::value& value, const bool = false);
	bool	setCfgValue(const std::string& key, const json::value& value, json::value& cfg, const bool create = false);
	
private:
	YAML::Node cntlDesc_;

};



