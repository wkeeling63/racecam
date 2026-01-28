/*  racecam shared class header
* rcamshared.hpp
*/
//#include <iostream>
//#include <fstream>
//#include <filesystem>
//#include <string>
#include <queue>
//#include <any>
//#include <span>
//#include <condition_variable>
//#include <ctime> 
//#include <iomanip>
//#include <thread>
//#include <chrono>
//#include <limits>


#include <libcamera/camera.h>
#include <libcamera/camera_manager.h>
//#include <libcamera/control_ids.h>
//#include <libcamera/property_ids.h>
//#include <libcamera/transform.h>
#include <libcamera/formats.h>
#include <libcamera/logging.h>
//#include <libcamera/yaml_parser.h>

//#include <sys/mman.h>

//#include <boost/json/src.hpp> 
#include <boost/json.hpp>  //WEK can't figure out how to make meson link to boost_json 1.87 -- try on new build 
//#include <yaml-cpp/yaml.h>

//#include "core/dma_heaps.hpp"
//#include "core/message_map.hpp"
//#include "racecamsrc.hpp"
//#include "core/youtube.hpp"
#include "core/logger.hpp"


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

constexpr size_t MAX_SIZE = std::numeric_limits<size_t>::max(); 
#define CLEARSCREEN std::cout << "\033[2J\033[1;1H" << std::flush;
#define CLEARPREVLINE std::cout << "\033[1F\033[2K" << std::flush;
#define CLEARPREVLINES std::cout << "\033[1F\033[2K\033[1F\033[2K" << std::flush;

#define LOC std::string(__FILE__) + ":" + std::string(__PRETTY_FUNCTION__) + ":" + std::to_string(__LINE__)
//#define LOC std::string(__FILE__) + ":" + std::string(__FUNCTION__) + ":" + std::to_string(__LINE__)

using namespace libcamera;
namespace json = boost::json;

//WEKsplit RCamShared (all 3)
struct Sensor {
	libcamera::Size outRes;
	libcamera::Size senRes;
	int senBit {};
	Sensor() : outRes({}),senRes({}), senBit(0) {}
	Sensor(json::value v) {
			DEBUG_PRINT("%s", "\n");
			if (v.is_null()) {
				outRes = senRes = libcamera::Size{};
				senBit = 0;
				return;
			}
			if (!v.is_array()) {
				throw std::runtime_error("Sensor contructor is not array!");
			}
			auto a = v.as_array();
			if ( 2 != a.size() && 5 != a.size()) {
				throw std::runtime_error("Sensor contructor incorrect array size: " + std::to_string(a.size()));
			}
			outRes.width = json::value_to<unsigned int>(a.at(0)); 
			outRes.height = json::value_to<unsigned int>(a.at(1));
			if ( 5 == a.size()) {
				senRes.width = json::value_to<unsigned int>(a.at(2)); 
				senRes.height = json::value_to<unsigned int>(a.at(3));
				senBit = json::value_to<int>(a.at(4));
			}
	}
	Sensor(libcamera::Size o) : outRes(o),senRes({}), senBit(0) {}
	Sensor(libcamera::Size o, libcamera::Size s, int b) : outRes(o),senRes(s), senBit(b) {}
	//WEK for testing if left improve to handle empty and better format
	std::string toString() { return outRes.toString() + " " 
		+ senRes.toString() + " " + std::to_string(senBit); }
	bool hasSensor() { return !senRes.isNull() && senBit; }
	bool isNull() { return outRes.isNull() && senRes.isNull() && !senBit; }
};  

class Sensor2 {
	libcamera::SensorConfiguration sc;
//	Sensor2() : outRes({}),senRes({}), senBit(0) {} empty not needed
	// this overload to go from json to SensorConfiguration
	Sensor2(json::value v) {
		DEBUG_PRINT("%s", "\n");
		if (v.is_null() || !v.is_array()) {
			throw std::runtime_error("JSON Sensor is invalid!");
		}
		auto a = v.as_array();
		if ( 3 != a.size()) {
			throw std::runtime_error("Sensor contructor incorrect array size: " + std::to_string(a.size()));
		}
		sc.outputSize.width = json::value_to<unsigned int>(a.at(0)); 
		sc.outputSize.height = json::value_to<unsigned int>(a.at(1));
		sc.bitDepth = json::value_to<unsigned int>(a.at(2));
	}
	// these 2 overloads for going from data to json 
	Sensor2(libcamera::Size r, unsigned int b) {
		sc.outputSize = r; 
		sc.bitDepth = b;
	}
	Sensor2(int w, int h, int b) {
		sc.outputSize.width = w; 
		sc.outputSize.height = h;
		sc.bitDepth = b;
	}
	// methods get sensor, get json array
	std::string toString() { return sc.outputSize.toString() + " "  + std::to_string(sc.bitDepth); }
	libcamera::SensorConfiguration getSensorCfg() {return sc;}
	json::array getArray() {
		return json::array {sc.outputSize.width, sc.outputSize.height, sc.bitDepth};
	}
};  

struct CameraStream {
	unsigned int stream : 1;
	unsigned int camera : 7;
	CameraStream() : stream(0), camera(0) {}
	CameraStream(int cs) { stream = cs & 1; camera = cs >> 1; }
	CameraStream(unsigned int cs) { stream = cs & 1; camera = cs >> 1; }
	CameraStream(int c, int s) : stream(s), camera(c) {}
	CameraStream(unsigned int c, unsigned int s) : stream(s), camera(c) {}
	std::string toString() { return std::string("Camera: ") + std::to_string(camera) 
		+ " Stream: " + std::to_string(stream); }
	unsigned int getCamera() { return camera; }
	unsigned int getStream() { return stream; }
	unsigned int getCameraStream() { return stream | (camera << 1); }
};  

class Streams {
	std::vector<libcamera::Size> res;
//	Streams() : res({}) {} // should not need or want to create empty Streams 
	Streams(json::value v) {
		DEBUG_PRINT("%s", "\n");
		if (v.is_null() || !v.is_array()) {
			throw std::runtime_error("JSON Stream is invalid!");
		}
		auto a = v.as_array();
		if (a.size() > 2) {
			throw std::runtime_error("JSON Stream is too large!");
		}
		for (size_t i = 0; i < a.size(); i++) {
			if (!a[i].is_array()) throw std::runtime_error("Stream " + std::to_string(i) + " is not array!");
			auto s = a[i].as_array();
			if (!s.size()) throw std::runtime_error("Stream " + std::to_string(i) + " is too large!");			
			res.push_back(libcamera::Size(json::value_to<unsigned int>(s.at(0)), json::value_to<unsigned int>(s.at(1))));
		} 
		if (res.size() == 2) {
			if (res[0] <= res[1]) throw std::runtime_error("Main stream not larger then second stream!");
		}
	}
	Streams(libcamera::Size o) : res({o}) {}
	Streams(libcamera::Size o0, libcamera::Size o1) : res({o0, o1}) {
		if (res[0] <= res[1]) throw std::runtime_error("Main stream not larger then second stream!");
	}
	Streams(int w, int h) : res({libcamera::Size(w, h)}) {}
	Streams(int w0, int h0, int w1, int h1) : res({libcamera::Size(w0, h0), libcamera::Size(w1, h1)}) {
		if (res[0] <= res[1]) throw std::runtime_error("Main stream not larger then second stream!");
	}
	std::vector<libcamera::Size> getStreams() { return res;}
	libcamera::Size getStream(int i) {
		if ((size_t)i >= res.size()) throw std::runtime_error("getStream(" + std::to_string(i) +  ") to out of range!");
		return res[i];
	}
	json::array getArray() {
		json::array a;
		for (size_t i = 0; i < res.size(); i++) {
			a.push_back(json::array{res[i].width, res[i].height});
		} 
		return a;
	}
};  
class RCamShared
{
public:
	using CameraManager = libcamera::CameraManager;
	using Camera = libcamera::Camera;
	
	RCamShared(Logger& log, std::string const& path, std::string const& cfg = "racecam_config.json");
	virtual ~RCamShared(){};  // add write config here or public write???
	

protected:
	void initCameraManager();
	bool 		fromArray(Rectangle& r, json::value& jv);
	bool 		fromArray(Point& r, json::value& jv);
	bool 		fromArray(Size& r, json::value& jv);
	ControlValue toCntlVal(json::value& jv, const ControlId* cip); 
	
	template <typename T> 
	ControlValue toCntlVal(T& v, std::size_t& size)
	{
		DEBUG_PRINT("%s", "\n");
		ControlValue cv;
		if (v.empty()) return cv;
		if (size != MAX_SIZE && v.size() != size) 
			return cv;
		if (v.size() == 1)
			cv.set(v[0]);
		else
			cv.set(libcamera::Span(v.data(), v.size()));
		
		return cv;
	}	

//	json::value toJSON(ControlValue&);
	json::array toArray(Rectangle const r) {return json::array{r.x, r.y, r.width, r.height};};
	json::array toArray(Point const p) {return json::array{p.x, p.y};};
	json::array toArray(Size const s) {return json::array{s.width, s.height};};
	json::array toArray(Sensor& s);
	
	std::string		getLocation(const std::shared_ptr<Camera>& cam);
	bool	isCSI(const std::shared_ptr<Camera>& cam);
	json::value	getCfgValue(const std::string& key);
	json::value	getCfgValue(const std::string& key, const json::value& jv);
	static int 		getBitDepth(const libcamera::PixelFormat& pix);
	
//WEK make this x_ names
	Logger& logger_;
	std::string srcpath {};
	std::string cfgloc {};
	json::value config;
	std::unique_ptr<CameraManager> cm_;
private:
};


