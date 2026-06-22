/* 
 *  rcamshared.cpp
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
 
#include "core/rcamshared.hpp"

RCamShared::RCamShared(Logger& log, std::string const& cfg) 
		: logger_(log)
{	

	bool pi5 {false};
	std::ifstream infile("/proc/cpuinfo"); 
	if (infile.is_open()) {
		std::string line;
		while (std::getline(infile, line)) { 
			std::size_t found = line.find("Model");
			if (found != std::string::npos) {
				found = line.find("Raspberry Pi 5");
				if (found != std::string::npos) pi5 = true;
			}
		}
		infile.close(); 
	} else {
		throw std::runtime_error("Error opening /proc/cpuinfo!");
	}
	if (!pi5) throw std::runtime_error("RaceCam runs only on Pi5!");
	
	size_t pos = cfg.rfind("/");
	if (pos == std::string::npos) {
		cfgfile_ = cfg;
	} else {
		cfgpath_ = cfg.substr(0, ++pos);
		cfgfile_ = cfg.substr(pos);
	}
}

std::string	RCamShared::getLocation(const std::shared_ptr<Camera>& cam)
{
//	fprintf(stderr, "%s:%d:%s\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);
	static const std::map<std::string, std::string> cam_location =
	{
		{ "i2c@88000", "Camera0" },
		{ "i2c@80000", "Camera1" },
		{ "usb@300000-2", "LowerUSB2" },
		{ "usb@200000-2", "UpperUSB2" },
		{ "usb@200000-1", "LowerUSB3" },
		{ "usb@300000-1", "UpperUSB3" },
	}; 

	std::string loc = cam->id();
	size_t spos = -1;
	for (int i = 0; i < 5; ++i) {
        spos = loc.find("/", spos + 1);
        if (spos == std::string::npos) {
            return "5thNotFound"; 
        }
    }
    spos++;
    std::size_t epos = loc.find_first_of("/:",spos);
    if (spos == std::string::npos) {
            return "EndNotFound"; 
    }
	loc = loc.substr (spos, epos - spos);
	const auto &cl = cam_location.find(loc);
	if (cl != cam_location.end()) return cl->second; 	
	return "NotFound";
} 

bool RCamShared::isCSI(const std::shared_ptr<Camera>& cam)
{
//	fprintf(stderr, "%s:%d:%s\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);
	std::string loc = getLocation(cam);
	if (loc == "Camera0" || loc == "Camera1") return true;
	return false;
}

void RCamShared::initCameraManager()
{
//	fprintf(stderr, "%s:%d:%s\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);
	cm_.reset();
	cm_ = std::make_unique<CameraManager>();
	int ret = cm_->start();
	if (ret)
		throw std::runtime_error("camera manager failed to start, code " + std::to_string(-ret));
}

bool RCamShared::fromArray(Rectangle& r,json::value& jv)
{
//	fprintf(stderr, "%s:%d:%s\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);
	if (!jv.is_array()) {
		logger_.Log(LogLevel::ERROR, "toRectangle() is not array!");
		return false;
	}
	auto a = jv.as_array();
	if ( 4 != a.size()) {
		logger_.Log(LogLevel::ERROR, "toRectangle() incorrect array size: " + std::to_string(a.size()));
		return false;
	}
	r.x = json::value_to<int>(a.at(0));
	r.y = json::value_to<int>(a.at(1));
	r.width = json::value_to<unsigned int>(a.at(2)); 
	r.height = json::value_to<unsigned int>(a.at(3));
	return true;
} 

bool RCamShared::fromArray(Point& p, json::value& jv)
{
//	fprintf(stderr, "%s:%d:%s\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);
	if (!jv.is_array()) {
		logger_.Log(LogLevel::ERROR, "toPoint() is not array!");
		return false;
	}
	auto a = jv.as_array();
	if ( 2 != a.size()) {
		logger_.Log(LogLevel::ERROR, "toPoint() incorrect array size: " + std::to_string(a.size()));
		return false;
	}
	p.x = json::value_to<int>(a.at(0));
	p.y = json::value_to<int>(a.at(1));
	return true;
}

bool RCamShared::fromArray(Size& s, json::value& jv)
{
//	fprintf(stderr, "%s:%d:%s\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);
	if (!jv.is_array()) {
		logger_.Log(LogLevel::ERROR, "toSize() is not array!");
		return false;
	}
	auto a = jv.as_array();
	if ( 2 != a.size()) {
		logger_.Log(LogLevel::ERROR, "toSize() incorrect array size: " + std::to_string(a.size()));
		return false;
	}
	s.width = json::value_to<unsigned int>(a.at(0)); 
	s.height = json::value_to<unsigned int>(a.at(1));
	return true;
}

ControlValue RCamShared::toCntlVal(json::value& jv, const ControlId* cip)
{
//	fprintf(stderr, "%s:%d:%s\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);
	ControlValue cv;
	switch (cip->type()) {
		case ControlTypeBool: {
			if (cip->isArray() || (!jv.is_bool())) return cv;
			cv.set(json::value_to<bool>(jv));
			return cv;
		}
		case ControlTypeInteger32: {
			if (cip->isArray() || (!jv.is_int64())) return cv;
			cv.set(json::value_to<int>(jv));
			return cv;
		}
		case ControlTypeInteger64: {	
			if (!cip->isArray()) {
				if (!jv.is_int64()) return cv;
				cv.set(json::value_to<long>(jv));
				return cv;
			} else {
				if (!jv.is_array()) return cv;
				std::vector<long> vl;
				for(json::value av: jv.get_array()) {
					if (!av.is_int64()) return cv;
					vl.insert(vl.end(), json::value_to<long>(av));
				}
				if ((MAX_SIZE == cip->size()) ||
					(MAX_SIZE != cip->size() && vl.size() == cip->size())) {
					cv.set(libcamera::Span(vl.data(), vl.size()));
					return cv;
				} else {
					return cv;
				}
			}	
		}
		case ControlTypeFloat: {
			if (!cip->isArray()) {
				if (!jv.is_double()) return cv;
				cv.set(json::value_to<float>(jv));
				return cv;
			} else {
				if (!jv.is_array()) return cv;
				std::vector<float> vf;
				for(json::value av: jv.get_array()) {
					if (!av.is_int64()) return cv;
					vf.insert(vf.end(), json::value_to<float>(av));
				}
				if ((MAX_SIZE == cip->size()) ||
						(MAX_SIZE != cip->size() && vf.size() == cip->size())) {
					cv.set(libcamera::Span(vf.data(), vf.size()));
					return cv;
				} else {
					return cv;
				}
			}
		}
		case ControlTypeRectangle: {	
			if (!cip->isArray()) {
				Rectangle r = {};
				if (!fromArray(r, jv)) return cv;
				cv.set(r);
				return cv;
			} else {
				if (!jv.is_array()) return cv;
				std::vector<Rectangle> vr;
				for(json::value av: jv.get_array()) {
					Rectangle r = {};
					if (!fromArray(r, av)) return cv;
					vr.insert(vr.end(), r);
				}
				if ((MAX_SIZE == cip->size()) ||
						(MAX_SIZE != cip->size() && vr.size() == cip->size())) {
					cv.set(libcamera::Span(vr.data(), vr.size()));
					return cv;
				} else {
					return cv;
				}
			}
			break;
		}
		case ControlTypeNone:
		case ControlTypePoint:
		case ControlTypeSize:
		case ControlTypeUnsigned16:
		case ControlTypeUnsigned32:	
		case ControlTypeByte:
		case ControlTypeString: {
			logger_.Log(LogLevel::ERROR, "Unhandled control type for: " 
				+ std::to_string(cip->id()) + "-" + cip->name() + " Vendor: " + cip->vendor());
			return cv;
		}
	} 
	return cv;
}

int RCamShared::getBitDepth(const libcamera::PixelFormat& pix)
{
//	fprintf(stderr, "%s:%d:%s\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);
static const std::map<libcamera::PixelFormat, unsigned int> bayer_formats =
{
	{ libcamera::formats::BGR888, 		 8 },
	{ libcamera::formats::RGB888, 		 8 },
	{ libcamera::formats::XBGR8888, 	32 },
	{ libcamera::formats::BGR161616, 	48 },
	{ libcamera::formats::RGB161616, 	48 },
	{ libcamera::formats::YUYV, 		16 },
	{ libcamera::formats::YVYU, 		16 },
	{ libcamera::formats::UYVY, 		16 },
	{ libcamera::formats::VYUY, 		16 },
	{ libcamera::formats::MJPEG, 		0 },  //WEK just a test
	{ libcamera::formats::NV12, 		12 },
	{ libcamera::formats::NV21, 		21 },
	{ libcamera::formats::YUV420, 		12 },
	{ libcamera::formats::YVU420, 		12 },
	{ libcamera::formats::YUV422, 		16 },
	{ libcamera::formats::YVU422, 		16 },
	{ libcamera::formats::YUV444, 		24 },
	{ libcamera::formats::YVU444, 		24 },
	{ libcamera::formats::R8, 			 8 },
	{ libcamera::formats::R16, 			16 },
	{ libcamera::formats::MONO_PISP_COMP1, 8 },
	{ libcamera::formats::SBGGR8, 		 8 },
	{ libcamera::formats::SGBRG8, 		 8 },
	{ libcamera::formats::SGRBG8, 		 8 },
	{ libcamera::formats::SRGGB8, 		 8 },
	{ libcamera::formats::SBGGR10, 		10 },
	{ libcamera::formats::SGBRG10, 		10 },
	{ libcamera::formats::SGRBG10, 		10 },
	{ libcamera::formats::SRGGB10, 		10 },
	{ libcamera::formats::SBGGR12, 		12 },
	{ libcamera::formats::SGBRG12, 		12 },
	{ libcamera::formats::SGRBG12, 		12 },
	{ libcamera::formats::SRGGB12, 		12 },
	{ libcamera::formats::SBGGR14, 		14 },
	{ libcamera::formats::SGBRG14, 		14 },
	{ libcamera::formats::SGRBG14, 		14 },
	{ libcamera::formats::SRGGB14, 		14 },
	{ libcamera::formats::SBGGR16, 		16 },
	{ libcamera::formats::SGBRG16, 		16 },
	{ libcamera::formats::SGRBG16, 		16 },
	{ libcamera::formats::SRGGB16, 		16 },
	{ libcamera::formats::BGGR_PISP_COMP1, 8 },
	{ libcamera::formats::GBRG_PISP_COMP1, 8 },
	{ libcamera::formats::RGGB_PISP_COMP1, 8 },
	{ libcamera::formats::SRGGB10_CSI2P, 10 },
	{ libcamera::formats::SGRBG10_CSI2P, 10 },
	{ libcamera::formats::SBGGR10_CSI2P, 10 },
	{ libcamera::formats::R10_CSI2P,     10 },
	{ libcamera::formats::SGBRG10_CSI2P, 10 },
	{ libcamera::formats::SRGGB12_CSI2P, 12 },
	{ libcamera::formats::SGRBG12_CSI2P, 12 },
	{ libcamera::formats::SBGGR12_CSI2P, 12 },
	{ libcamera::formats::SGBRG12_CSI2P, 12 }
};
	const auto &b = bayer_formats.find(pix);
	if (b == bayer_formats.end()) return 0;
	else return b->second; 
}

json::value RCamShared::getCfgValue(const std::string& key)
{
//	fprintf(stderr, "%s:%d:%s\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);
	return getCfgValue(key, config_);
}
json::value RCamShared::getCfgValue(const std::string& key, const json::value& jv)
{
//	fprintf(stderr, "%s:%d:%s\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);
	try {
		return jv.at_pointer(key);
	}
	catch(...) {
		json::value&& x {};
		return x;
	}
}


