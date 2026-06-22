/* 
 * rcamcfg.hpp
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
#include <libcamera/control_ids.h>
#include <libcamera/property_ids.h>

#include <boost/json.hpp>  

#include "core/message_map.hpp"
#include "core/rcamshared.hpp"
#include "core/youtube.hpp"

#pragma once

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

enum Container { Comp, Raw };  

class StrmMap {
	
	struct camData {
		int stream {-1};
		int layer {-1};
		libcamera::Size insize;
		libcamera::Size outsize;
		std::string camstr;
		std::string strmstr;
	};
	std::map<unsigned int, camData> camsData;
	int streams;
	int layers;
	json::value cfg;
	bool rawvalid {true};
	bool compvalid {true};
	
	json::value getCfgValue(const std::string& key,  const json::value& v) {
		try {
			return v.at_pointer(key);
		}
		catch(...) {
			json::value&& x {};
			return x;
		}
	}
	
	libcamera::Size getSize(const json::value& v) {
		if (!v.is_array()) throw std::runtime_error("Value is not array!");
		auto a = v.as_array();
		if ( 2 != a.size() && 4 != a.size()) throw std::runtime_error("Array is wrong size!");
		libcamera::Size s;
		if ( 2 == a.size()) { 
			s.width = json::value_to<unsigned int>(a.at(0)); 
			s.height = json::value_to<unsigned int>(a.at(1));
		} else {
			s.width = json::value_to<unsigned int>(a.at(2)); 
			s.height = json::value_to<unsigned int>(a.at(3));
		}
		return s;
	}
	
	void initAll() {
		json::value cv = getCfgValue("/Cameras", cfg);
		if (!cv.is_object()) throw std::runtime_error("Cameras is not an object!");
		int camnum = 0;
		for (const auto& pair : cv.as_object()) {
			camData cd;
			json::value strms = getCfgValue("/Streams", pair.value());
			if (strms.is_array()) {
				int strmnum = 0;
				for (auto& strm : strms.as_array()) {
					cd.camstr = pair.key();
					cd.strmstr = cd.camstr + ":" + std::to_string(strmnum);
					cd.insize = cd.outsize = getSize(strm);
					camsData[CamStrm(camnum, strmnum).getCamStrm()] = cd;
					strmnum++;
				}
			}	
			camnum++;
		}	
		streams = layers = 0; 
		calcData(streams, Container::Raw);
		calcData(layers, Container::Comp);
	}
		
	void calcData(int& n, Container t) {
//		dumpAll();
		for (auto& pair : camsData) {
			if (Container::Raw == t) {
					pair.second.stream = -1;
				} else {
					pair.second.layer = -1;
				}
		}
		json::value v;
		if (Container::Raw == t) {
			v = getCfgValue("/Outputs/Raw/Streams", cfg);
		} else {
			v = getCfgValue("/Outputs/Composite/Layers", cfg);
		}
		if (v.is_null()) {
			return;
		}
		if (!v.is_array()) throw std::runtime_error("Streams/Layers is not an array!");
		auto it = camsData.begin();
		int i = 0;
		for (auto& l : v.as_array()) {
			json::value sourcev = getCfgValue("/Source", l);
			if (!sourcev.is_null()) {
				n++;
				it = camsData.find(json::value_to<int>(sourcev));
				if (it != camsData.end()) {
					if (Container::Raw == t) {
						it->second.stream = i++;
					} else {
						it->second.layer = i++;
					}
				} else {
					if (Container::Raw == t) {
						rawvalid = false;
					} else {
						compvalid = false;
					}
					
				}
			} else { 
				throw std::runtime_error("Streams/Layers has no source!");
			}
			json::value cropv = getCfgValue("/Crop", l);
			if (!cropv.is_null()) {
				it->second.outsize = getSize(cropv);
			}
			json::value scalev = getCfgValue("/Scale", l);
			if (!scalev.is_null()) {
				it->second.outsize = getSize(scalev);
			}
		}
	}
	//WEK remove just a debug tool??
/*	void dumpAll() {
		for (auto& pair : camsData) {
			std::cout << "ID: " << pair.first <<
			"Layer: " << pair.second.layer <<
			"Stream: " << pair.second.stream <<
			"InSize: " << pair.second.insize.toString() <<
			"OutSize: " << pair.second.outsize.toString() <<
			" " << pair.second.camstr << " " << pair.second.strmstr <<
			std::endl;
		}
		std::cout << "Streams: " << streams <<
		"Layers: " << layers <<
		std::boolalpha << "RawSatus " << rawvalid << 
		"CompSatus " << compvalid << std::noboolalpha <<
		std::endl;
		std::cout << json::serialize(cfg) << std::endl;
	} */
	
public:
	StrmMap(const json::value& config) : cfg(config) {
		initAll();
//		dumpAll();
		}
	//WEK need loadStreams()?? or just load(Container t)
 	void loadLayers() {
		layers = 0; 
		calcData(streams, Container::Comp);
//		dumpAll();
	}
	
	libcamera::Size camSize(CamStrm c) {
		auto it = camsData.find(c.getCamStrm());
		if (it == camsData.end()) throw std::runtime_error("CameraStream in size not found!");
		return it->second.insize;
		}
		
	libcamera::Size outSize(CamStrm c) {
		auto it = camsData.find(c.getCamStrm());
		if (it == camsData.end()) throw std::runtime_error("CameraStream out size not found!");
		return it->second.outsize;
		}
		
	std::vector<std::string> freeStrmLabels(Container t) {
		std::vector<std::string> fsl;
		for (const auto& c : camsData) {
			if (Container::Raw == t) {
				if (c.second.stream == -1) fsl.push_back(c.second.strmstr + "\t" + c.second.outsize.toString());
			} else {
				if (c.second.layer == -1) fsl.push_back(c.second.strmstr + "\t" + c.second.outsize.toString());
			}
		}
		return fsl;
	}
	
	unsigned int getStream(std::string& l) {
		size_t pos = l.find("\t");
		std::string s {l};
		if (pos != std::string::npos) {
			s = l.substr(0, pos);
		} 
		for (const auto& c : camsData) {
			if (c.second.strmstr == s) return c.first;
		}
		throw std::runtime_error("CameraStream label not found!"); //WEK do I need a better way to handle not found (use std::optional?/make signed?)
	}
	
	unsigned int cams() {return camsData.size();}

	unsigned int free(Container t) {
		if (Container::Raw == t) {
			return camsData.size() - streams;
		} else {
			return camsData.size() - layers;
		}
	}

	unsigned int used(Container t) {
		if (Container::Raw == t) {
			return streams;
		} else {
			return layers;
		}
	}

	bool valid(Container t) {
		if (Container::Raw == t) {
			return rawvalid;
		} else {
			return compvalid;
		}
	}

	const std::string getstrmlabel(CamStrm i) {
		auto it = camsData.find(i.getCamStrm());
		if (it == camsData.end()) return "NotFound";
		return it->second.strmstr;
	}
	
	const std::string getstrmlabel(int i, Container t) {
		for (const auto& c : camsData) {
			if (Container::Raw == t) {
				if (c.second.stream == i) return c.second.strmstr + "\t" + c.second.outsize.toString();
			} else {
				if (c.second.layer == i) return c.second.strmstr + "\t" + c.second.outsize.toString();
			}
		
		}
		return "NotFound";
	}
};
	
class RCamCfg : public RCamShared
{
public:
	using CameraManager = libcamera::CameraManager;
	using Camera = libcamera::Camera;
	
//	wEK bug default cfg not working;
	RCamCfg(Logger& lptr, std::string const& cfg = "racecam_config.json");
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
	void	cfgComposite(void); 
	void	cfgCompositeDest(void);
	void	cfgLayer(int, Container);
	
	// get user input and input helper functions
	msg getMsg(const std::string);
	std::optional<bool> getBool(const std::string, const std::string);
	std::optional<int> getInt(const std::string, const std::string, int, int);
	std::optional<long> getLong(const std::string, const std::string, long, long);
	std::optional<float> getFloat(const std::string, const std::string, float, float);
	std::optional<std::string> getString(const std::string, const std::string);
	std::optional<libcamera::Size> getSize(const std::string, const std::string, const libcamera::Size, const libcamera::Size min = libcamera::Size());
	std::optional<libcamera::Point> getPoint(const std::string, const std::string, const libcamera::Point, const libcamera::Point min = libcamera::Point());
	std::optional<libcamera::Rectangle> getRectangle(const std::string, const std::string, const libcamera::Rectangle, const libcamera::Rectangle min = libcamera::Rectangle());
	json::value getControlValue(const unsigned int id, const std::shared_ptr<Camera> &cam);
	void getCntlMsg(std::string &);
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
};



