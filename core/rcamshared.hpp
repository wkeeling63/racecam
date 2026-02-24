/* 
* rcamshared.hpp
*/
#include <queue>

#include <libcamera/camera.h>
#include <libcamera/camera_manager.h>
#include <libcamera/formats.h>
#include <libcamera/logging.h>

#include <boost/json.hpp>  
#include "core/logger.hpp"

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

class Sensor {
	libcamera::SensorConfiguration sc;
public:
	Sensor(json::value v) {
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
	Sensor(libcamera::Size r, unsigned int b) {
		sc.outputSize = r; 
		sc.bitDepth = b;
	}
	Sensor(int w, int h, int b) {
		sc.outputSize.width = w; 
		sc.outputSize.height = h;
		sc.bitDepth = b;
	}
	std::string toString() {
		std::string sizestr =  sc.outputSize.toString();
		if (7 == sizestr.size()) sizestr = " " + sizestr;
		std::string bitstr = std::to_string(sc.bitDepth);
		if (1 == bitstr.size()) bitstr = " " + bitstr;
		return bitstr + sizestr;
	}
	std::string toDisplayString() {
		return std::string("Sensor size: ") + sc.outputSize.toString() + 
		" Bit Depth: " + std::to_string(sc.bitDepth);
	}
	libcamera::SensorConfiguration getSensorCfg() {return sc;}
	json::array getArray() {
		return json::array {sc.outputSize.width, sc.outputSize.height, sc.bitDepth};
	}
};  

class CamStrm {
	int camera;
	int stream;
public:
	CamStrm() : camera(-1), stream(-1) {}  
	CamStrm(int cs) { stream = cs & 1; camera = cs >> 1; }
	CamStrm(unsigned int cs) { stream = cs & 1; camera = cs >> 1;} 
	CamStrm(int c, int s) : camera(c), stream(s) {}
	CamStrm(unsigned int c, unsigned int s) : camera(c), stream(s) {}
	std::string toString() { return std::string("Camera: ") + std::to_string(camera) 
		+ " Stream: " + std::to_string(stream); }
	unsigned int getCamera() { return camera; }
	unsigned int getStream() { return stream; }
	unsigned int getCamStrm() { return stream | (camera << 1); }
	bool operator==(const CamStrm& rhs) const {
        return (stream == rhs.stream && camera == rhs.camera);
    //WEK do I need copy or move operator
    }
};  

class Streams {
	std::vector<libcamera::Size> res;
public:
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
			if (res[0] < res[1]) throw std::runtime_error("Main stream not larger then second stream!");
		}
	}
	Streams(libcamera::Size o) : res({o}) {}
	Streams(libcamera::Size o0, libcamera::Size o1) : res({o0, o1}) {
		if (res[0] < res[1]) throw std::runtime_error("Main stream not larger then second stream!");
	}
	Streams(int w, int h) : res({libcamera::Size(w, h)}) {}
	Streams(int w0, int h0, int w1, int h1) : res({libcamera::Size(w0, h0), libcamera::Size(w1, h1)}) {
		if (res[0] < res[1]) throw std::runtime_error("Main stream not larger then second stream!");
	}
	Streams(std::vector<libcamera::Size> ss) : res(ss) {
		if (res.size() < 1 || res.size() > 2) throw std::runtime_error("Invalid number of streams! (" + std::to_string(res.size()) + ")" );
		if (res.size() == 2 && res[0] < res[1]) throw std::runtime_error("Main stream not larger then second stream!");
	}
	std::vector<libcamera::Size> getStreams() { return res;}
	libcamera::Size getStreamSize(int i) {
		if ((size_t)i >= res.size()) throw std::runtime_error("getStreamSize(" + std::to_string(i) +  ") to out of range!");
		return res[i];
	}
	libcamera::Size setStreamSize(int i, libcamera::Size s) {
		if ((size_t)i >= res.size()) throw std::runtime_error("setStreamSize(" + std::to_string(i) +  ") to out of range!");
		res[i] = s;
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
	
//	RCamShared(Logger& log, std::string const& path, std::string const& cfg = "racecam_config.json");
	RCamShared(Logger& log, std::string const& cfg = "racecam_config.json");
	virtual ~RCamShared(){};  
	
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

	json::array toArray(Rectangle const r) {return json::array{r.x, r.y, r.width, r.height};};
	json::array toArray(Point const p) {return json::array{p.x, p.y};};
	json::array toArray(Size const s) {return json::array{s.width, s.height};};
	json::array toArray(Sensor& s);
	
	std::string		getLocation(const std::shared_ptr<Camera>& cam);
	bool	isCSI(const std::shared_ptr<Camera>& cam);
	json::value	getCfgValue(const std::string& key);
	json::value	getCfgValue(const std::string& key, const json::value& jv);
	static int 		getBitDepth(const libcamera::PixelFormat& pix);

	Logger& logger_;
//	std::string srcpath_ {};
//	std::string cfgloc_ {};
	std::string cfgpath_ {};
	std::string cfgfile_ {};
	json::value config_;
	std::unique_ptr<CameraManager> cm_;
private:
};


