// need stat corrected everywhere add configure
// and figure out where stopping should change to configed

/* racecam application classes code
*  rcamcfg.cpp
*/
//#include <string>

//#include <boost/json/src.hpp> 
//#include <boost/json.hpp> 
#include "core/jsonio.hpp"

//#include "core/rcamshared.hpp"
#include "core/rcamcfg.hpp"

//#include <linux/dma-buf.h>
//#include <sys/ioctl.h>

//#include <stdio.h>

json::value RCamCfg::toJSON(ControlValue& cv)
{
	DEBUG_PRINT("%s", "\n");
	json::value jv;
	if (cv.isNone()) return jv;
	
	std::string type;
	
	switch (cv.type()) {
		case ControlTypeBool: {
			if (cv.isArray()) return jv;
			jv.emplace_bool() = cv.get<bool>() ? true : false;
			return jv;
		}
		case ControlTypeInteger32: {
			if (cv.isArray()) return jv;
			jv.emplace_int64() = cv.get<int>();
			return jv;
		}
		case ControlTypeInteger64: {	
			if (!cv.isArray()) {
				if (cv.numElements() != 1) return jv;
				jv.emplace_int64() = cv.get<long>();
				return jv;
			} else {
				json::array ja;
				for (const auto& l : cv.get<libcamera::Span<const long>>())
					ja.emplace_back(l);
				jv = ja;
				return jv;
			}	
		}
		case ControlTypeFloat: {
			if (!cv.isArray()) {
				if (cv.numElements() != 1) return jv;
				jv.emplace_double() = cv.get<float>();
				return jv;
			} else {
				json::array ja;
				for (const auto& f : cv.get<libcamera::Span<const float>>())
					ja.emplace_back(f);
				jv = ja;
				return jv;
			}
		}
		case ControlTypeRectangle: {	
			if (!cv.isArray()) {
				if (cv.numElements() != 1) return jv;
				Rectangle r = cv.get<Rectangle>();
				json::array ja = {{r.x, r.y, r.width, r.height}};
				jv = ja;
				return jv;
			} else {
				json::array ja, ja1;
				for (const auto& r : cv.get<libcamera::Span<const Rectangle>>()) {
					ja1 = {{r.x, r.y, r.width, r.height}};
					ja.emplace_back(ja1);
					jv = ja;
					return jv;
				}
				
			}
			return jv;
		}
		case ControlTypeNone: {
			throw std::runtime_error("Unhandled control type for: ControlTypeNone");
		}
		case ControlTypePoint: {
			throw std::runtime_error("Unhandled control type for: ControlTypePoint");
		}
		case ControlTypeSize: {
			throw std::runtime_error("Unhandled control type for: ControlTypeSize");
		}
		case ControlTypeUnsigned16: {
			throw std::runtime_error("Unhandled control type for: ControlTypeUnsigned16");
		}
		case ControlTypeUnsigned32: {
			throw std::runtime_error("Unhandled control type for: ControlTypeUnsigned32");
		}	
		case ControlTypeByte: {
			throw std::runtime_error("Unhandled control type for: ControlTypeByte");
		}
		case ControlTypeString: {
			throw std::runtime_error("Unhandled control type for: ControlTypeString");
		}
	} 
	return jv;
}

RCamCfg::RCamCfg(Logger& lptr, std::string const& path, std::string const& cfg) : RCamShared(lptr, path, cfg)
{
	DEBUG_PRINT("%s", "\n");
    config = jsonRead(cfgloc);
 //   if (config.is_null()) throw std::runtime_error("Configuration " + cfgloc + " Not found!");
    if (config.is_null()) {
		logger_.Log(LogLevel::INFO, std::string("Configuration " + cfgloc + " is empty or not found!"));
		config = json::object {};
	}
    try {
        YAML::Node cntlyaml = YAML::LoadFile("/home/wkeeling/racecam/controls.yaml");
        if (cntlyaml["controls"]) {
			cntlDesc_ = cntlyaml["controls"];
		}
    } 
    catch (const YAML::BadFile& e) {
        logger_.Log(LogLevel::ERROR, std::string("Error loading YAML file: ") + e.what());
		} 
	catch (const YAML::Exception& e) {
        logger_.Log(LogLevel::ERROR, std::string("YAML error: ") + e.what());
		}

    initCameraManager();
}

RCamCfg::~RCamCfg()
{ 
	DEBUG_PRINT("%s", "\n");
//	jsonWrite(std::cout, config);  //TODO remove when dev done
} 

/*std::vector<std::shared_ptr<Camera>> RCamCfg::getCameras(void)
{
	DEBUG_PRINT("%s", "\n");
	std::vector<std::shared_ptr<Camera>> cameras = cm_->cameras();
	auto rem = std::remove_if(cameras.begin(), cameras.end(),
		[](auto &cam) { return cam->id().find("/usb") != std::string::npos; });
	cameras.erase(rem, cameras.end());
	std::sort(cameras.begin(), cameras.end(), [](auto l, auto r) { return l->id() > r->id(); });
	return cameras;
}*/

bool RCamCfg::dropCfgValue(const std::string& path)
{
	DEBUG_PRINT("%s", "\n");
	return dropCfgValue(path, config);
}

bool RCamCfg::dropCfgValue(const std::string& path, json::value& cfg)
{
	DEBUG_PRINT("%s", "\n");
	if (path.empty()) {
		logger_.Log(LogLevel::ERROR, "Key is empty!" + path);
		return false;
	}
	std::vector<std::string> tokens;
	size_t pos = 0;
	size_t last_pos = 1;
	std::string ppath {}; 
	while ((pos = path.find("/", last_pos)) != std::string::npos) {
		std::string t = path.substr(last_pos - 1, pos - last_pos + 1);
		tokens.push_back(t);
		ppath += t;
		last_pos = pos + 1;		
	}
	tokens.push_back(path.substr(last_pos - 1));
	    	
    // set key to last token
	std::string key = tokens.back().substr(1);
	
	if (getCfgValue(path).is_null()) {
		logger_.Log(LogLevel::ERROR, "Key to delete not found: " + path);
		return false;
	}
	json::value parent = cfg;
	if (!ppath.empty()) {
		parent = cfg.at_pointer(ppath);
	} 
	
	if (parent.is_object()) {
		json::object& o = parent.get_object();
		if (!(o.erase(key))) {
			logger_.Log(LogLevel::ERROR, "Object erase failed: " + path);
			return false;
		} else {
			if (parent == cfg) {
				cfg = o;
			} else {
				cfg.at_pointer(ppath) = o;
			}
			return true;
		}
	} else if (parent.is_array()) {
		int num = 0;
		try {
			num = std::stoi(key);
		} catch (...) {
			logger_.Log(LogLevel::ERROR, "Array key not number: " + key);
			return false;
		} 
		json::array& a = parent.get_array();
		int s1 = a.size();
		a.erase(a.begin() + num);
		int s2 = a.size();
		if (s1 == ++s2) {
			if (parent == cfg) {
				cfg = a;
			} else {
				cfg.at_pointer(ppath) = a;
			}
			return true;
		} else {
			logger_.Log(LogLevel::ERROR, "Array erase failed: " + path);
			return false;
			}
	} else {
		logger_.Log(LogLevel::ERROR, "Parent is not object or array: " + ppath);
		return false;	
	}			
}

bool RCamCfg::setCfgValue(const std::string& path, const json::value& val, const bool create)
{
	DEBUG_PRINT("%s", "\n");
	return setCfgValue(path, val, config, create);
}

bool RCamCfg::setCfgValue(const std::string& path, const json::value& val, json::value& cfg, const bool create)
{
	DEBUG_PRINT("%s", "\n");
	if (val.is_null()) {
		logger_.Log(LogLevel::ERROR, "Value is null in setCfgValue()!" + path);
		return false;
	}
	//parse tokens from key
	std::vector<std::string> tokens;
	size_t pos = 0;
	size_t last_pos = 1;
	std::string ppath {}; 
	while ((pos = path.find("/", last_pos)) != std::string::npos) {
		std::string t = path.substr(last_pos - 1, pos - last_pos + 1);
		tokens.push_back(t);
		ppath += t;
		last_pos = pos + 1;		
	}
	tokens.push_back(path.substr(last_pos - 1));
	// walk tokens and check exist in json in correct node
	json::value fval = cfg;
	std::string fpath {}, fkey {}, nfkey {};
	bool root = true;
	std::vector<std::string>::iterator it;
	for (it = tokens.begin(); it != tokens.end(); ++it) {
		try {
			fval = fval.at_pointer(*it);
			fpath += *it;
			fkey = it->substr(1);	
			root = false;
		} catch(...) {
			nfkey = it->substr(1);
			break;
		}
    }
    std::string key = nfkey.empty() ? fkey : nfkey;
    
    // update leaf values 
    if (!(fval.is_object() || fval.is_array())) {
		cfg.at_pointer(path) = val;
		return true;
	}

	//build new value from unused path 
	json::value nv = val;
	std::string nkey;
	if (!nfkey.empty()|| root) {
		nkey = it++->substr(1);
		bool first = true;
		std::string npath, ckey; 
		int ix = -1;
		json::value lv = nullptr;
		for ( ; it != tokens.end(); ++it) {
			ckey = it->substr(1);
			try {
				ix = std::stoi(ckey);
			} catch (...) {
				ix = -1;
			} 
			if (it == --tokens.end()) {
				lv = val;
			} 
			if (first) {
				first = false;
				if (ix < 0) {
					json::object o;
					o.insert(std::make_pair(ckey, lv));
					nv = o;
				} else {
					json::array a;
					a.push_back(lv);
					nv = a;
				} 
			} else {
				if (ix > 0) {
					logger_.Log(LogLevel::ERROR, "Array index not 0 for new item: " + ix );
					return false;
				}
				if (ix < 0) {
					json::object o;
					o.insert(std::make_pair(ckey, lv));
					nv.at_pointer(npath) = o;
				} else {
					json::array a;
					a.push_back(lv);
					nv.at_pointer(npath) = a;
				} 
			}
			npath += *it;
		}
	} // build new value done

	int ix = -1;
	try {
		ix = std::stoi(key);
	} catch (...) {
		ix = -1;
	} 

	if (ix == -1) {
		if (root) {
			cfg.as_object().insert_or_assign(key, nv);
			return true;
		}
		if (nfkey.empty()) {
			fpath = ppath;
			cfg.at_pointer(fpath).as_object().insert_or_assign(key, nv);
			return true;
		} else if (fval.is_object()) {
			cfg.at_pointer(fpath).as_object().insert_or_assign(nkey, nv);
			return true;
		} else {
			logger_.Log(LogLevel::WARN, "Key is object but value found is not -- overwritting! " + fpath + " " + path);
			cfg.at_pointer(fpath) = nv;
			return true;
		}
	}

	if (ix >= 0) {
		if (ix == (int)fval.as_array().size()) {
			if (root) {
				cfg.as_array().push_back(nv);
			} else {
				cfg.at_pointer(fpath).as_array().push_back(nv);
			}
			return true;
		} else if (ix < (int)fval.as_array().size()) {
			if (root) {
				cfg.as_array().at(ix) = nv;
			} else { 
				cfg.at_pointer(fpath).as_array().at(ix) = nv;
			}
			return true;
		} else {
			logger_.Log(LogLevel::ERROR, "Array index out of range!");
			return false;
		}	
	}

	logger_.Log(LogLevel::ERROR, "setCfgValue failed! should never hit this error");
    return false;
} 

void RCamCfg::cfgAudio(void)
{
	DEBUG_PRINT("%s", "\n");
	unsigned int i {0};
	do {
		CLEARSCREEN
		std::cout << "\n\tConfigure audio\n\n";
		std::vector<std::string> menu = {"Disable Audio",
			"Configure Audio"};
		json::value av = getCfgValue("/Audio/Device");
		std::string as = "plughw:RacecamMic";
		if (!av.is_null()) {
			if (av.is_string()) {
				as = json::value_to<std::string>(av);
				std::cout << "\n\tCurrent Audio device: " << as << "\n\n";
			} else {
				throw std::runtime_error("/Audio/Device is not a string!");
			}
		}
		if (av.is_null()) {
			menu.clear();
			menu.push_back("Enable Audio");
			std::cout << "\n\tAudio device is not set.\n\n" ;
		}
		
		i = menuUtil(menu);
		if (menu.size() == 1) {
			switch (i) {
				case 1: {
					auto os = getString("AudioString", as);
					json::string js {os.value()};
					if (!setCfgValue("/Audio/Device", js))
						std::cout << "Unable to set /Audio/Device!" << std::endl;
					}
					break;
				};
		} else {
			switch (i) {
				case 1: {
					if (!dropCfgValue("/Audio")) {
						logger_.Log(LogLevel::ERROR, "Drop failed for /Audio", true);
					}
					break;   
				}
				case 2: {
					auto os = getString("AudioString", as);
					json::string js {os.value()};
					if (!setCfgValue("/Audio/Device", js))
						logger_.Log(LogLevel::ERROR, "Unable to set /Audio/Device!", true);
					break;
				}
			};
		}
	} while (i);
}

void RCamCfg::CfgRaceCam(void)
{
	DEBUG_PRINT("%s", "\n");
	std::vector<std::string> menu = {"Configure Cameras",
		"Configure Audio Source", "Configure Raw Output", 
		"Configure Composite Output", "Save Configuration"};
	unsigned int i;
	do {
		CLEARSCREEN
		std::cout << "\n\t\tRaceCam Setup\n\n";
		std::cout << ("\tconfiguration file: ") << cfgloc << "\n\n";
		
		i = menuUtil(menu);
		switch (i) {
			case 1: cfgCameras(); break;
			case 2: cfgAudio(); break;
			case 3: cfgRaw(); break;
			case 4: cfgComposite(); break;
			case 5: {
				jsonWrite(std::cout, config);
				if (getBool("SaveConfig", "No").value_or(false)) {
					jsonWrite(cfgloc, config);
				}
				break;
			};	
		};
	} while (i);
}

void RCamCfg::cfgRaw(void)
{
	DEBUG_PRINT("%s", "\n");
	unsigned int i {0};
	do {
		CLEARSCREEN
		std::cout << "\n\tConfigure raw output\n\n";
		std::vector<std::string> menu = {"Disable Raw Output",
			"Configure Raw Output",
			"Configure Streams for Raw Output"};
		json::value rv = getCfgValue("/Outputs/Raw/Destination");
		std::string rs {};
		// WEK default to srcpath + /data/raw if no value 
		if (!rv.is_null()) {
			if (rv.is_string()) {
				rs = json::value_to<std::string>(rv);
				std::cout << "\n\tCurrent Raw Destination: " << rs << "\n\n";
			} else {
				throw std::runtime_error("/Outputs/Raw/Destination is not a string!");
			}
		}
		if (rv.is_null()) {
			menu.clear();
			menu.push_back("Enable Raw Stream");
			std::cout << "\n\tAudio device is not set.\n\n" ;
		}
		
		i = menuUtil(menu);
		if (menu.size() == 1) {
			switch (i) {
				case 1: {
					auto os = getString("RawString", rs);
					json::string js {os.value()};
					if (!setCfgValue("/Outputs/Raw/Destination", js))
						logger_.Log(LogLevel::ERROR, "Unable to set /Outputs/Raw/Destination!", true);
					}
					break;
				};
		} else {
			switch (i) {
				case 1: {
					if (!dropCfgValue("/Outputs/Raw")) {
						logger_.Log(LogLevel::ERROR, "Drop failed for /Outputs/Raw", true);

					}
					break;   
				}
				case 2: {
					auto os = getString("RawString", rs);
					json::string js {os.value()};
					if (!setCfgValue("/Outputs/Raw/Destination", js))
						logger_.Log(LogLevel::ERROR, "Unable to set /Outputs/Raw/Destination!", true);
					break;
				}
				case 3: {
					cfgRawStreams();
					break;
				}
			};
		}
	} while (i);
}
void RCamCfg::cfgRawStreams(void)
{
	DEBUG_PRINT("%s", "\n");
	unsigned int i {0};
	do {
		CLEARSCREEN
		std::cout << "\n\tConfigure Raw Streams\n\n";
		json::value sv = getCfgValue("/Outputs/Raw/Streams");
		if (sv.is_array()) {
			for(auto it = sv.get_array().begin();it != sv.get_array().end(); it++) {
				displayLayer(*it);
            }
		}

		//WEK show all streams
		CamsSL vcl = getCamsSL(true);
		std::vector<std::string> menu {};
		for (int i = 0; i < vcl.used; i++) {
			menu.push_back("Update/Delete stream " + std::to_string(i));

		}
		if (vcl.free) {
			menu.push_back("Add stream");

		}
		i = menuUtil(menu);
		if (i) {
			cfgLayer(i - 1, true);
		}
	} while (i); 
}

CamsSL  RCamCfg::getCamsSL(bool stream)
{
	DEBUG_PRINT("%s", "\n");
	std::vector<std::shared_ptr<Camera>> cams = getCameras();
	CamsSL ccl;
	for (auto const &cam : cams) { 
		json::value s = getCfgValue("/Cameras/" + getLocation(cam) + "/Sensor");
		if (!s.is_null()) {
			Size ss {};
			if (!fromArray(ss, s)) {
				throw std::runtime_error("fromArray(Size) failed!"); 
			}	
		ccl.camSL.push_back(CamSL {getLocation(cam), -1, ss, ss});	
		}
	}
	ccl.free = ccl.camSL.size();
	ccl.used = 0;
	json::value lsv {};
	if (stream) {
		lsv = getCfgValue("/Outputs/Raw/Streams");
	} else {
		lsv = getCfgValue("/Outputs/Composite/Layers");
	}
	if (lsv.is_null()) {
		return ccl;
	}
	int i {0};
	for (auto& l : lsv.as_array()) {
		Size ls {};
		json::value sv = getCfgValue("/Source", l);
		if (!sv.is_null()) {
			ccl.free--;
			ccl.used++;
			ccl.setsl(i++) = json::value_to<int>(sv);
			json::value c = getCfgValue("/Crop", l);
			if (!c.is_null()) {
				Rectangle r {};
				if (fromArray(r, c)) {
					ls.width = r.width;
					ls.height = r.height;
					ccl.setslSize(json::value_to<int>(sv)) = ls;
				}
			}
			json::value s = getCfgValue("/Scale", l);
			if (!s.is_null()) {
				Size ss {};
				if (fromArray(ss, s)) {
					ccl.setslSize(json::value_to<int>(sv)) = ss;
				}	
			}
		}
	}
	return ccl;
}

void RCamCfg::cfgComposite(void)
{
	DEBUG_PRINT("%s", "\n");
	unsigned int i {0};
	do {
		CLEARSCREEN
		std::cout << "\n\tConfigure composite\n\n";
		CamsSL vcl = getCamsSL();
		//WEK if new layer the need to update layer in vcl with cam#
		std::vector<std::string> menu;
		json::value dest = getCfgValue("/Outputs/Composite/Destination");
		menu.push_back("Configure composite destination");
		if (!dest.is_null()) {
			for (int io = 0; io < (int)vcl.camSL.size(); ++io) {
				for (size_t ii = 0; ii < vcl.camSL.size(); ++ii) {
					if (vcl.getsl(ii) == io ) {
						menu.push_back("Update/Delete " + ( io ? "layer " + std::to_string(io) : "main layer") ); 
					}
				}
			}
			if (vcl.free) {
				menu.push_back("Add Layer");
			}
		}
		i = menuUtil(menu);
		if ( i == 0 ) {
			continue;
		}
		if ( i == 1 ) {
			cfgCompositeDest();
		} else {
			cfgLayer(i - 2);
		}
	} while (i);	
	return;

	//unsigned int intra; from opts	 (less than 2secs and not more than 4secs per intra 
	// 2secs * fps = recommended GOPSize or less
//	codec->gop_size = options->intra ? options->intra : (int)(options->framerate_a[cam].value_or(DEF_FRAMERATE));
}

void	RCamCfg::cfgCompositeDest(void)
{
	DEBUG_PRINT("%s", "\n");
	unsigned int i {0};
	do {
		CLEARSCREEN
		std::cout << "\n\tConfigure composite destination\n\n";
		std::vector<std::string> menu = {"Configure destination as file",
			"Configure YouTube destination"};
		json::value dest = getCfgValue("/Outputs/Composite/Destination");
		std::string s, t, p {};
		bool file {false};
		if (!dest.is_null()) {
			menu.push_back("Drop all composite configuration (destination and layers)");
			if (dest.is_string()) {
				file = true;
				std::cout << dest.as_string().c_str() << std::endl;
				s = json::value_to<std::string>(dest);
			} else {
				jsonWrite(std::cout, dest);
				t = json::value_to<std::string>(getCfgValue("/Outputs/Composite/Destination/YouTubeTitle"));
				p = json::value_to<std::string>(getCfgValue("/Outputs/Composite/Destination/YouTubePrivacy"));
			}
		} else {
			std::cout << "No composite destination configured." << std::endl;
		}
		i = menuUtil(menu);
		switch (i) {
			case 1: {
				auto os = getString("CompositeString", s);
				json::string js {os.value()};
				// WEK default to srcpath + /data/composite if no value 
				if (!setCfgValue("/Outputs/Composite/Destination", js))
					logger_.Log(LogLevel::WARN, "Unable to set /Outputs/Composite/Destination!", true);
				}
				break;
			case 2: {
				if (file) {
					if (!dropCfgValue("/Outputs/Composite/Destination")) {
						logger_.Log(LogLevel::WARN, "Drop failed for /Outputs/Composite/Destination", true);
					}
				}
				auto os = getString("YouTubeTitle", t);
				json::string js {os.value()};
				if (!setCfgValue("/Outputs/Composite/Destination/YouTubeTitle", js))
					logger_.Log(LogLevel::WARN, "Unable to set /Outputs/Composite/Destination/YouTubeTitle!", true );		
				unsigned int i;
				std::cout << "\nSelect video privacy level\n\n";
				std::vector<std::string> menu = {"Private", "Unlisted", "Public"};
				i = menuUtil(menu, false);
				std::string ps;
				switch (i) {
					case 1: js = "PRIVATE"; break;
					case 2: js = "UNLISTED"; break;
					case 3: js = "PUBLIC"; break;
				};
				if (!setCfgValue("/Outputs/Composite/Destination/YouTubePrivacy", js))
					logger_.Log(LogLevel::WARN, "Unable to set /Outputs/Composite/Destination/YouTubePrivacy!", true);
				}
				if (YouTube(srcpath).GetAuth().is_null()) {
					throw std::runtime_error("YouTube GetAuth() failed!");
				} 
				break;
			case 3: {
				if (!dropCfgValue("/Outputs/Composite")) {
						std::cout << "Drop failed for /Outputs/Composite"  << std::endl;
						logger_.Log(LogLevel::WARN, "Drop failed for /Outputs/Composite", true);
					}
					break; 
				}
		};
	} while (i);
}

void	RCamCfg::cfgLayer(int l, bool stream)
{
	DEBUG_PRINT("%s", "\n");
	CamsSL vcl = getCamsSL(stream);
	int cam {0};

	bool noCrop {true}, noScale {true}, noOverlay {true};  
	std::string key {};
	json::value cv0 {};
	if (stream) {
		key = "/Outputs/Raw/Streams/" + std::to_string(l);
		l = 0;
	} else {
		key = "/Outputs/Composite/Layers/" + std::to_string(l);
		cv0 = getCfgValue("/Outputs/Composite/Layers/0/Source");
	}
	
	json::value lv = getCfgValue(key);
	if (!lv.is_null()) {
		displayLayer(lv);
		if (stream) {
			if (getBool("DropStream", "N").value()) {
				if (!dropCfgValue(key)) {
					throw std::runtime_error("drop stream failed!");
				}
			}
		} else {
			if (getBool("DropLayer", "N").value()) {
				if (!dropCfgValue(key)) {
					throw std::runtime_error("drop layer failed!");
				}
			}
		}
		//WEK prompt to drop stream or layer
		json::value camv = getCfgValue("/Source", lv);
		if (!camv.is_null()) {
			cam = json::value_to<int>(camv);
		}
		noCrop = getCfgValue("/Crop", lv).is_null();
		noScale = getCfgValue("/Scale", lv).is_null();
		noOverlay = getCfgValue("/Overlay", lv).is_null();
	}
	
	if (vcl.free) {
		std::vector<std::string> menu;
		std::cout << "Select camera for layer" << std::endl;
		for (int it = 0; it < (int)vcl.camSL.size(); it++) {
			if (vcl.getsl(it) < 0) {
				menu.push_back(vcl.getloc(it));
			}
		}
		unsigned int i {0};
		i = menuUtil(menu, false);
		for (int it = 0; it < (int)vcl.camSL.size(); it++) {
			if (menu.at( i - 1 ) == vcl.getloc(it))  {
				json::value v = it;	
				cam = it;
				if (!setCfgValue(key + "/Source", v)) {
					logger_.Log(LogLevel::WARN, key + "/Source" + std::to_string(it) + " unable to set!");
				}
			}
		} 
	}

	bool update = false;
	if (!noCrop) {
		update = getBool("UpdateCrop", "N").value();
	} else {
		update = getBool("AddCrop", "N").value();
	}
	if (update) {
		libcamera::Rectangle r {(int)vcl.getcSize(cam).width, (int)vcl.getcSize(cam).height, vcl.getcSize(cam)};
		std::optional<Rectangle> o = getRectangle("CropValue", "", r);
		if (o) { 
			if (!setCfgValue(key + "/Crop", json::array {o.value().x, o.value().y, o.value().width, o.value().height} )) {
				logger_.Log(LogLevel::WARN, key + "/Crop unable to set!", true);
			} else {
				vcl.setslSize(cam).width = o.value().width;
				vcl.setslSize(cam).height = o.value().height;			
			}
		} else {
			if (!noCrop) {
				if (!dropCfgValue(key + "/Crop")) {
					throw std::runtime_error("drop crop failed!");
				}
				vcl.setslSize(cam).width = vcl.getcSize(cam).width;
				vcl.setslSize(cam).height = vcl.getcSize(cam).height;
			}
		}
	}
	
	update = false;
	if (!noScale) {
		update = getBool("UpdateScale", "N").value();
	} else {
		update = getBool("AddScale", "N").value();
	}
		if (update) {
			libcamera::Size s {std::numeric_limits<unsigned int>::max(), std::numeric_limits<unsigned int>::max()};
		std::optional<Size> o = getSize("ScaleValue", "", s);
		if (o) {
			if (!setCfgValue(key + "/Scale", json::array {o.value().width, o.value().height} )) {
				std::cout << key << "/Scale" <<" unable to set!" << std::endl;
				logger_.Log(LogLevel::WARN, key + "/Scale unable to set!", true );
			} else {
				vcl.setslSize(cam).width = o.value().width;
				vcl.setslSize(cam).height = o.value().height;
			}
		} else {
			if (!noScale) {
				if (!dropCfgValue(key + "/Scale")) {
					throw std::runtime_error("drop scale failed!");
				}
				if (noCrop) { 
					vcl.setslSize(cam).width = vcl.getcSize(cam).width;
					vcl.setslSize(cam).height = vcl.getcSize(cam).height;
				} else {
					json::value cv = getCfgValue(key + "/Crop", lv);
					if (!fromArray(vcl.setslSize(cam), cv)) {
						throw std::runtime_error("bad json crop array to size");
					}
					
				}
			}
		}
	}
	
	if (l) {
		int cam0;
		if (!cv0.is_null()) {
			cam0 = json::value_to<int>(cv0);
		} else {
			throw std::runtime_error("no layer 0!");
		}
		update = true;
		if (!noOverlay) {
			update = getBool("UpdateOverlay", "N").value();
		} 
		if (update) {
			libcamera::Rectangle r {(int)vcl.getslSize(cam0).width - (int)vcl.getslSize(cam).width,
				(int)vcl.getslSize(cam0).height - (int)vcl.getslSize(cam).height,
				vcl.getslSize(cam)};
			// WEK should have some def so that overlay is always create if (l) default to above would put in lower right
			std::optional<Rectangle> o = getRectangle("OverlayValue", r.toString(), r);
			// WEK don't need if as it has def and can't be optional
				if (!setCfgValue(key + "/Overlay", json::array {o.value().x, o.value().y, o.value().width, o.value().height} )) {
					logger_.Log(LogLevel::WARN, key + "/Overlay unable to set!");
				}
		}
	} else {
		if (!noOverlay) {
			if (!dropCfgValue(key + "/Overlay")) {
				throw std::runtime_error("drop overlay failed!");
			}
		}
	}
	 
	jsonWrite(std::cout, config);
}

void RCamCfg::cfgSensor(const std::shared_ptr<Camera> &cam)
{
	DEBUG_PRINT("%s", "\n");
	bool accepted {false};
	do {
		unsigned int idx = 1;
		std::cout << "Modes for: " << cam->id() << std::endl;
		
		std::map<std::string, modes> modeMap;
		cam->acquire();

//how to list all properties some not valid unitl after 		
/*		for (const auto &c :  cam->properties()) 
		{
			auto ctrlid = properties::properties.at(c.first);
			std::cout << ctrlid->name() << " " << c.second.toString() << std::endl;
		}  */
		
		std::string loc = getLocation(cam);
		// needs non Raw to report crop info 
		std::unique_ptr<CameraConfiguration> config;
		//WEK is VideoRecording needed here -- just need crop anbd FPS not a real useable config
		if (isCSI(cam)) {
			config = cam->generateConfiguration({libcamera::StreamRole::Raw, libcamera::StreamRole::VideoRecording});
		} else {
			config = cam->generateConfiguration({libcamera::StreamRole::Raw});
		}

		if (!config) {
			throw std::runtime_error("failed to generate capture configuration");
		}
		
		const StreamFormats &formats = config->at(0).formats();
//		std::cout << "1" << std::endl;
		if (!formats.pixelformats().size())
			throw std::runtime_error("pixel format is empty");
			
		for (const auto &pix : formats.pixelformats()) {
			if (pix !=libcamera::formats::YUYV && !isCSI(cam)) continue;
			int bits = getBitDepth(pix);
//			std::cout << "bits: " << bits <<std::endl;	
			for (const auto &sz : formats.sizes(pix)) {
//				std::cout << "2" << std::endl;
				std::string size =  sz.toString() ;
				if (7 == size.size()) size = " " + size;
				std::string bitstr = std::to_string(bits);
				if (1 == bitstr.size()) bitstr = " " + bitstr;
				std::string modestr = bitstr + size;
				
				config->at(0).size = sz;
				config->at(0).pixelFormat = pix;
				if (bits) {
					config->sensorConfig = libcamera::SensorConfiguration();
					config->sensorConfig->outputSize = sz;
					config->sensorConfig->bitDepth = bits;
				}
				auto status = config->validate();
				if (CameraConfiguration::Invalid == status) 
					throw std::runtime_error("invalid sensor configuration");
				cam->configure(config.get());
				
				auto fd_ctrl = cam->controls().find(&controls::FrameDurationLimits);
				double fps = fd_ctrl == cam->controls().end() ? NAN : (1e6 / fd_ctrl->second.min().get<int64_t>());
				
	//			if (std::isnan(fps)) std::cout << "not a number" << std::endl;
	//			else std::cout << fps << std::endl;
				
				std::string cropstr;
				if (cam->controls().count(&controls::ScalerCrop))
					cropstr = cam->controls().at(&controls::ScalerCrop).max().get<Rectangle>().toString();
				if (cropstr.length() < 22) 
					cropstr.append(22 - cropstr.length(), ' ');
				
				if (24 < fps || std::isnan(fps)) {
					modeMap.try_emplace(modestr , modes{sz, fps, bits, cropstr});
				}
			}
		}
		
//		std::cout << "3" << std::endl;

		for (const auto &m : modeMap)
		{
			std::string size = m.second.size.toString();
			if (7 == size.size()) size = " " + size;
			size.resize(10, ' ');
			std::string bitstr = std::to_string(m.second.bitDepth);
			if (1 == bitstr.size()) bitstr = " " + bitstr;
			if (std::isnan(m.second.fps)) 
	//			std::cout << idx++ <<": " << size << bitstr << " bpp " << m.second.crop << std::endl;
				std::cout << idx++ <<": " << size << std::endl;
			else
			{
				std::string message = "";
				if (m.second.fps < 30 ) message = " FPS less 30 are not recommended!";
				std::cout << idx++ <<": " << size << bitstr << " bpp " << m.second.crop << "\n       Max FPS: " 
				<< std::fixed << std::setprecision(2) << m.second.fps << message << std::endl;	
			}	
		}

		json::value defval = getCfgValue("/Cameras/" + loc + "/Sensor");
		std::string defstr = ""; 
		Sensor s(defval);
		if (s.hasSensor()) {
			std::string sizestr =  s.senRes.toString();
			if (7 == sizestr.size()) sizestr = " " + sizestr;
			std::string bitstr = std::to_string(s.senBit);
			if (1 == bitstr.size()) bitstr = " " + bitstr;
			std::string modestr = bitstr + sizestr;
			int i=1;
			for (auto it = modeMap.begin(); it != modeMap.end(); ++it, ++i) {
	//			std::cout << it->first << std::endl;
				if (it->first == sizestr) defstr = std::to_string(i);
			}
		} else {
			int i=1;
			for (auto it = modeMap.begin(); it != modeMap.end(); ++it, ++i) {
	//			std::cout << it->first << std::endl;
				if (it->second.size == s.outRes) defstr = std::to_string(i);
			}
		} 			
	// end display mode 
//		std::cout << defstr << std::endl;
		std::string outdefstr {"1280X720"};
		if (!s.outRes.isNull()) {
			outdefstr = s.outRes.toString();
		}

		std::cout << std::endl;
//		std::cout << s.tostring() << std::endl;
		libcamera::Size out {};
		int mode {};
		if (isCSI(cam)) {
			out = getSize("OutputSize", outdefstr, libcamera::Size(3840,2160)).value();
			mode = getInt("ModeSelect", defstr, --idx, 1).value_or(0);
		} else {
			mode = getInt("ModeSelect", defstr, --idx, 1).value_or(1);
			auto it = modeMap.begin();
			std::advance(it, mode-1);
			out = it->second.size;
			mode = 0;
		}
				
		Sensor ss(out);
		if (mode) {
			auto it = modeMap.begin();
			std::advance(it, mode-1);
			ss.senRes = it->second.size;
			ss.senBit = it->second.bitDepth;
		}  

		json::value jv = toArray(ss);
		if (!setCfgValue("/Cameras/" + loc + "/Sensor", jv))
		//WEK throw
			std::cout << "Unable to set /Cameras/" + loc << " Sensor!" << std::endl;

		config->at(0).size = ss.outRes; 
		if (ss.hasSensor()) {
			config->sensorConfig = libcamera::SensorConfiguration();
			config->sensorConfig->outputSize = ss.senRes;
			config->sensorConfig->bitDepth = ss.senBit;
		}
		
		auto status = config->validate();
		if (CameraConfiguration::Invalid == status) throw std::runtime_error("invalid sensor configuration");
		cam->configure(config.get());
		
		if (ss.hasSensor()) {
		auto fd_ctrl = cam->controls().find(&controls::FrameDurationLimits);
		double fps = fd_ctrl == cam->controls().end() ? NAN : (1e6 / fd_ctrl->second.min().get<int64_t>());
		if (fps < 30){
			std::cout << "Max frames per second less than 30! \n You should think able reconfiguring \n the sensor! FPS: " << fps << std::endl;
			accepted = getBool("AcceptFPS", "No").value_or(false);
		} else {
			accepted = true;
		}
		} else {
			accepted = true;
		}

		cam->release();	
	} while (!accepted);
}


void RCamCfg::cfgControl(const std::shared_ptr<Camera> &cam)
{
	DEBUG_PRINT("%s", "\n");
	std::string loc = getLocation(cam);
	json::value sval = getCfgValue("/Cameras/" + loc + "/Sensor");
	if (sval.is_null()) {
		logger_.Log(LogLevel::WARN, "Sensor not configured!", true);
		return;
	}
	
/*	Size size {};
	if (!fromArray(size, sval)) {
		throw std::runtime_error("Size from array failed!");
	}	*/
	
	cam->acquire();
	
	std::unique_ptr<CameraConfiguration> config = cam->generateConfiguration({libcamera::StreamRole::Viewfinder});
	
//	config->at(0).size = size;
	Sensor sens(sval);
	config->at(0).size = sens.outRes;
	if (sens.hasSensor()) {
		config->sensorConfig = libcamera::SensorConfiguration();
		config->sensorConfig->outputSize = sens.senRes;
		config->sensorConfig->bitDepth = sens.senBit;
	}
		
	auto status = config->validate();
	if (CameraConfiguration::Invalid == status) throw std::runtime_error("invalid sensor configuration");
	cam->configure(config.get());
		
	const ControlInfoMap cm = cam->controls();
	std::map<unsigned int, std::string>  Cntl;
	for (auto const &[id2, info2] : cm) 
		Cntl.emplace(id2->id(), id2->name()); 

	std::optional<int> sel;
	do 
	{
		int i = 0;
		CLEARSCREEN
		std::cout << "\n\tConfigure camera controls\n\n";
		for (auto const &it : Cntl)
		{	
			json::value val = getCfgValue("/Cameras/" + loc + "/Controls/" + it.second);
			std::string vs;
			if (!val.is_null()) 
				vs = " is --> " + serialize(val);
			else
			{
				auto cmit = cm.find(it.first);
				if (!cmit->second.def().isNone())
					vs = " defaulting to -> " + cmit->second.def().toString();
			}
			std::cout << ++i << " " << it.second << vs << std::endl;
		}
		sel = getInt("ControlSelect", "", i, 1);
		if (sel.has_value()) 
		{
			auto it = Cntl.begin();
			std::advance(it, sel.value()-1);
			json::value jv = getControlValue(it->first, cam);

			if (!setCfgValue("/Cameras/" + loc + "/Controls/" + it->second, jv))
				logger_.Log(LogLevel::ERROR, "Unable to set /Cameras/" + loc + "/Controls/" + it->second);
		} 
	} while (sel.has_value());
	cam->release();	
	return;
} 

json::value RCamCfg::getControlValue(const unsigned int idNum, const std::shared_ptr<Camera> &cam)
{
	DEBUG_PRINT("%s", "\n");
	const ControlInfoMap cm = cam->controls();
	std::string loc = getLocation(cam);
	const ControlId *id = nullptr;
	ControlInfo info;
	std::string cntl;
	std::string cntlmsg;
	
	for (auto const &cim : cm)
	{
		if (cim.first->id() == idNum)
		{
			std::cout << cim.first->id() << std::endl;
			cntl = cim.first->name();
			cntlmsg = getCntlMsg(cntl);
			id = cim.first;
			info = cim.second;
		}
	}
	
	CLEARSCREEN	
	std::cout << cntlmsg << std::endl;

	json::value jv;
	size_t notdone = id->size() ? id->size() : 1;
	size_t size = notdone;
			
	switch (id->type()) 
	{
		case ControlTypeBool: 
		{
			std::optional<bool> o = getBool(cntl, "");
			if (!o.has_value()) return jv;
			ControlValue cv {o.value()};
			if (!info.def().isNone() && cv == info.def())
				return jv;
			return toJSON(cv);
		}
		case ControlTypeByte:
		case ControlTypeInteger32: 
		{
			std::vector<int> v;
			do
			{
				std::optional<int> o = getInt(cntl, "", info.max().get<int>(), info.min().get<int>()); 
				if (o.has_value()) 
					v.push_back(o.value());
				else
					notdone = 1;
			} while (--notdone);
			ControlValue cv = toCntlVal(v, size);
			if (cv.isNone()) return jv;
			if (!info.def().isNone() && cv == info.def())
				return jv;
			return toJSON(cv);
		}
		case ControlTypeInteger64: 
		{
			std::vector<long> v;
			do
			{
				std::optional<long> o = getLong(cntl, "", info.max().get<long>(), info.min().get<long>()); 
				if (o.has_value()) 
					v.push_back(o.value());
				else
					notdone = 1;
			} while (--notdone);
			ControlValue cv = toCntlVal(v, size);
			if (cv.isNone()) return jv;
			if (!info.def().isNone() && cv == info.def())
				return jv;
			return toJSON(cv);
		}
		case ControlTypeFloat: 
		{
			std::vector<float> v;
			do
			{
				std::optional<float> o = getFloat(cntl,"", info.max().get<float>(), info.min().get<float>()); 
				if (o.has_value()) 
					v.push_back(o.value());
				else
					notdone = 1;
			} while (--notdone);
			ControlValue cv = toCntlVal(v, size);
			if (cv.isNone()) return jv;
			if (!info.def().isNone() && cv == info.def())
				return jv;
			return toJSON(cv);
		}
		case ControlTypeRectangle: 
		{
			std::vector<Rectangle> v;
			do
			{
				std::optional<Rectangle> o = getRectangle(cntl, "", info.max().get<Rectangle>(), info.min().get<Rectangle>()); 
				if (o.has_value()) 
					v.push_back(o.value());
				else
					notdone = 1;
			} while (--notdone);
			ControlValue cv = toCntlVal(v, size);
			if (cv.isNone()) return jv;
			if (!info.def().isNone() && cv == info.def())
				return jv;
			return toJSON(cv);
		}
		case ControlTypeNone: 
			throw std::runtime_error("Type none controlvalue type should be unused");
		case ControlTypePoint:
			throw std::runtime_error("Point controlvalue type should be unused");
		case ControlTypeUnsigned16: 
			throw std::runtime_error("Unsigned 16 controlvalue type should be unused");
		case ControlTypeUnsigned32: 
			throw std::runtime_error("Unsigned 32 controlvalue type should be unused");
		case ControlTypeString: 
			throw std::runtime_error("String controlvalue type should be unused");
		case ControlTypeSize: 
			throw std::runtime_error("Size controlvalue type should be unused");
	}
	return jv;
}


msg RCamCfg::getMsg(const std::string msg_id)
{
	DEBUG_PRINT("%s", "\n");
	msg message;
	message.prompt = "Prompt message not found in msgs_map!";
	message.help = "Help message not found in msgs_map for message ID: " + msg_id;
	auto it_msg = msgs_map.find(msg_id);
	if (it_msg != msgs_map.end())
	{
		message.prompt = it_msg->second.prompt;
		message.help = it_msg->second.help;
	}
	return message;
}

std::optional<bool> RCamCfg::getBool(const std::string msg_id, const std::string def)
{
	DEBUG_PRINT("%s", "\n");
	msg cur = getMsg(msg_id);

	std::string input, save_input;
	std::map<std::string, bool>::iterator it;
	std::map<std::string, bool> valids = 
	{
		{"?", false}, {"HELP", false},
		{"YES", true}, {"Y", true}, {"TRUE", true}, {"T", true},
		{"NO", false}, {"N", false}, {"FALSE", false}, {"F", false}
	};

	bool rc = false;
	bool found = false;
	unsigned int i = 0;
	do { 
		do { 
			if (i++)
			{
				std::cin.ignore();
				if (input == "?" || input == "HELP")
					std::cout << cur.help << std::endl;
				else
					std::cout << "Invalid entry: " << save_input << std::endl;
			}
			
			std::cout << cur.prompt <<  "Y/N" << " " << (def.size() ? "(" + def + ")" : "") 
				<< ": " << std::flush;
			if ('\n' == std::cin.peek()) 
			{	
				if (0 == def.size())
				{
					std::cin.ignore();
					return std::nullopt;
				}
				else
					input = def;
			}
			else std::cin>>input;
			
			save_input = input;
			
			for (auto & c: input) c = (char)toupper(c); 
			it = valids.find(input);

			if (it != valids.end()) 
			{
				rc = it->second;
				found=true;
			} 
			} while (!(found));	
		} while (it->first == "?" || it->first == "HELP");
	std::cin.ignore();
	return rc;
}

std::optional<int> RCamCfg::getInt(const std::string msg_id, const std::string def, int max, int min = 0)
{
	DEBUG_PRINT("%s", "\n");
	msg cur = getMsg(msg_id);

	unsigned int i = 0;
	int num = 0;
	std::string input, save_input;
	std::cout << std::endl;
	do
	{
		size_t sz = 0;
		if (i++)
		{
			std::cin.ignore();
			if (input == "?" || input == "HELP")
				std::cout << cur.help << std::endl;
			else 
			{
				CLEARPREVLINES
				std::cout << "Invalid entry: " << save_input << " ";		
			}	
		}
		
		std::cout << cur.prompt << " [" << min << ".." << max << "] " 
			<< (def.size() ? "(" + def + ")" : "") << ": " << std::flush;
		if ('\n' == std::cin.peek()) 
		{	
			if (0 == def.size())
			{
				std::cin.ignore();
				return std::nullopt;
			}
			else
				input = def;
		}
		else std::cin>>input;
		
		save_input = input;

		for (auto & c: input) c = (char)toupper(c); 

		try {num = std::stoi(input, &sz);}
		catch (const std::invalid_argument&) {continue;}
		catch(const std::out_of_range&) {continue;}
		if (sz != input.length()) continue;
		if (num < min || num > max)	{continue;}
		std::cin.ignore();
		return num;
	} while (1);
}

std::optional<long> RCamCfg::getLong(const std::string msg_id, const std::string def, long max, long min = 0)
{
	DEBUG_PRINT("%s", "\n");
	msg cur = getMsg(msg_id);

	unsigned int i = 0;
	long num = 0;
	std::string input, save_input;
	do {
		size_t sz = 0;
		if (i++) {
			std::cin.ignore();
			if (input == "?" || input == "HELP")
				std::cout << cur.help << std::endl;
			else 
				std::cout << "Invalid entry: " << save_input << std::endl;			
		}
		
		std::cout << cur.prompt << " [" << min << ".." << max << "] " 
			<< (def.size() ? "(" + def + ")" : "") << ": " << std::flush;
		if ('\n' == std::cin.peek()) {	
			if (0 == def.size()) {
				std::cin.ignore();
				return std::nullopt;
			}
			else
				input = def;
		}
		else std::cin>>input;
		
		save_input = input;

		for (auto & c: input) c = (char)toupper(c); 

		try {num = std::stol(input, &sz);}
		catch (const std::invalid_argument&) {continue;}
		catch(const std::out_of_range&) {continue;}
		if (sz != input.length()) continue;
		if (num < min || num > max)	{continue;}
		std::cin.ignore();
		return num;
	} while (1);
}

std::optional<float> RCamCfg::getFloat(const std::string msg_id, const std::string def, float max, float min = 0)
{
	DEBUG_PRINT("%s", "\n");
	msg cur = getMsg(msg_id);

	unsigned int i = 0;
	float num = 0;
	std::string input, save_input;
	do {
		size_t sz = 0;
		if (i++) {
			std::cin.ignore();
			if (input == "?" || input == "HELP")
				std::cout << cur.help << std::endl;
			else 
				std::cout << "Invalid entry: " << save_input << std::endl;			
		}
		
		std::cout << cur.prompt << " [" << min << ".." << max << "] " 
			<< (def.size() ? "(" + def + ")" : "") << ": " << std::flush;
		if ('\n' == std::cin.peek()) {	
			if (0 == def.size()) {
				std::cin.ignore();
				return std::nullopt;
			}
			else
				input = def;
		}
		else std::cin>>input;
		
		save_input = input;

		for (auto & c: input) c = (char)toupper(c); 

		try {num = std::stof(input, &sz);}
		catch (const std::invalid_argument&) {continue;}
		catch(const std::out_of_range&) {continue;}
		if (sz != input.length()) continue;
		if (num < min || num > max)	{continue;}
		std::cin.ignore();
		return num;
	} while (1);
}

std::optional<std::string> RCamCfg::getString(const std::string msg_id, const std::string def)
{
	DEBUG_PRINT("%s", "\n");
	msg cur = getMsg(msg_id);

	unsigned int i = 0;
	std::string str = "";
	std::string input, save_input;
	do {
		if (i++) {
			std::cin.ignore();
			if (input == "?" || input == "HELP")
				std::cout << cur.help << std::endl;
			else 
				std::cout << "Invalid entry: " << save_input << std::endl;			
		}
		
		std::cout << cur.prompt <<  " " 
			<< (def.size() ? "(" + def + ")" : "") << ": " << std::flush;
		if ('\n' == std::cin.peek()) {	
			if (0 == def.size()) {
				std::cin.ignore();
				return std::nullopt;
			}
			else
				input = def;
		}
		else std::cin>>input;
		
		save_input = input;

		if (input == "?" || input == "HELP") {continue;}
		std::cin.ignore();
		return input;
	} while (1);
}

std::optional<libcamera::Size> RCamCfg::getSize(const std::string msg_id, const std::string def, const libcamera::Size max, const libcamera::Size min)
{
	DEBUG_PRINT("%s", "\n");
	msg cur = getMsg(msg_id);

	unsigned int i = 0;
	libcamera::Size size;
	char junk;
	std::string input, save_input;
	do {
		if (i++) {
			if (input == "?" || input == "HELP")
				std::cout << cur.help << std::endl;
			else 
				std::cout << "Invalid entry: " << save_input << std::endl;			
		}

		std::cout << cur.prompt << " [" << min.toString() << ".." << max.toString() << "] " 
			<< (def.size() ? "(" + def + ")" : "") << ": " << std::flush;
		if ('\n' == std::cin.peek()) {	
			std::cin.ignore();
			if (0 == def.size()) {
				return std::nullopt;
			}
			else
				input = def;
		}
		else getline(std::cin, input);
		
		save_input = input;

		for (auto & c: input) c = (char)toupper(c); 
		if (sscanf(input.c_str(), "%uX%u %c", &size.width, &size.height, &junk) != 2) {continue;}
		if (size.width < min.width || size.width > max.width || size.height < min.height || size.height > max.height) {continue;}
		
		return size;
	} while (1);
}

std::optional<libcamera::Rectangle> RCamCfg::getRectangle(const std::string msg_id, const std::string def, const libcamera::Rectangle max, const libcamera::Rectangle min)
{
	DEBUG_PRINT("%s", "\n");
	msg cur = getMsg(msg_id);

	unsigned int i = 0;
	libcamera::Rectangle rect;
	char junk;
	std::string input, save_input;
	do {
		if (i++) {
			if (input == "?" || input == "HELP")
				std::cout << cur.help << std::endl;
			else 
				std::cout << "Invalid entry: " << save_input << std::endl;			
		}
		std::cout << cur.prompt << " [" << min.toString() << ".." << max.toString() << "] " 
			<< (def.size() ? "(" + def + ")" : "") << ": " << std::flush;
		if ('\n' == std::cin.peek()) {	
			std::cin.ignore();
			if (0 == def.size()) {
				return std::nullopt;
			}
			else
				input = def;
		}
		else getline(std::cin, input);
		
		save_input = input;

		for (auto & c: input) c = (char)toupper(c); 
		if (sscanf(input.c_str(), "(%i,%i)/%uX%u %c", 
				&rect.x, &rect.y,&rect.width, &rect.height, &junk) != 4) 
			{continue;}
		if (rect.x < min.x || rect.x > max.x || rect.y < min.y || 
				rect.y > max.y || rect.width < min.width || 
				rect.width > max.width || rect.height < min.height || 
				rect.height > max.height) 
			{continue;}
		
		return rect;
	} while (1);
}
std::string RCamCfg::getCntlMsg(std::string cntl)
{
	DEBUG_PRINT("%s", "\n");
	std::string message {"Description of " + cntl + " not found!"};
	try {
        for (const auto& cntls : cntlDesc_) {
			for (const auto& key_value : cntls) {
				if (key_value.first.as<std::string>() == cntl) {
					message = key_value.first.as<std::string>() + "\n\n" +
					key_value.second["description"].as<std::string>() + "\n";
					if (key_value.second["enum"]) {
						for (const auto& enums : key_value.second["enum"]) {
							std::string opts;
							for (const auto& key_value : enums) {
								if (key_value.first.as<std::string>() != "name") 
									opts = opts + key_value.second.as<std::string>() + " ";
							}
							message = message + opts + "\n";
						}
					}
					return message;
				}
			}
		}
    } catch (const YAML::Exception& e) {
        logger_.Log(LogLevel::ERROR, std::string("YAML error: ") + e.what(), true);
		}
	return message;
} 

int RCamCfg::menuUtil(const std::vector<std::string>& m, const bool allownone)
{
	DEBUG_PRINT("%s", "\n");
	if (!m.size()) {
		throw std::runtime_error("Menu vector is empty!");
	}
	unsigned int i = 0;
	for (auto s:m) {
		std::cout << ++i << " " << s << std::endl;
	}
	
	i = 0;
	unsigned int num = 0;
	std::string input, save_input;
	std::cout << std::endl;
	do {
		size_t sz = 0;
		if (i++) {
			std::cin.ignore();
			CLEARPREVLINES
				std::cout << "Invalid entry: " << save_input <<  std:: endl;			
		}
		
		if (allownone) {
			std::cout << "Selection (or Return for no selection): " << std::flush;
		} else {
			std::cout << "Selection: " << std::flush;
		}
		
		if ('\n' == std::cin.peek()) {	
			if (allownone) {
				std::cin.ignore();
				return 0;
			} else {
				input = "No selection";
			}
		}
		else std::cin>>input;
		
		save_input = input;

		try {num = std::stoi(input, &sz);}
		catch (const std::invalid_argument&) {continue;}
		catch(const std::out_of_range&) {continue;}
		if (sz != input.length()) continue;
		if (num < 1 || num > (unsigned int)m.size())	{continue;}
		std::cin.ignore();
		return num;
	} while (1);
}

void RCamCfg::displayCam(std::shared_ptr<Camera> cam)
{
	DEBUG_PRINT("%s", "\n");
	std::string loc = getLocation(cam);
//	std::string model = *cam->properties().get(properties::Model);
	std::string model(*cam->properties().get(properties::Model));
	std::stringstream sensor_props;
	sensor_props << "  " << loc << " Model: " << model << " [";
	auto area = cam->properties().get(properties::PixelArrayActiveAreas);
	if (area) sensor_props << (*area)[0].size().toString() << " ";
	sensor_props.seekp(-1, sensor_props.cur);
	sensor_props << "]";
	std::cout << sensor_props.str() << std::endl;

	json::value id = getCfgValue("/Cameras/" + loc + "/CameraID");
	
	if (id.is_null()) {
		std::cout << "\tCamera not configured." << std::endl << std::endl;
		return;
	}
		
				
	if (cam->id() == json::value_to<std::string>(id)) {
		
		json::value sv = getCfgValue("/Cameras/" + loc + "/Sensor");
		if (sv.is_null()) {
			std::cout << "\tCamera not configured." << std::endl << std::endl;
			return;
		}
		std::cout << "    Current settings:" << std::endl;
		Size size {};
		if (fromArray(size, sv)) {
			std::cout << "      Sensor: " << size.toString() << std::endl;
			json::value co = getCfgValue("/Cameras/" + loc + "/Controls");
			if (!co.is_null()) {
				auto const& obj = co.get_object();
				if(!obj.empty()) {
					for(auto it = obj.begin();it != obj.end();it++) {
						std::cout << "        " << it->key() << ":"
							<< json::serialize(it->value()) << std::endl;
					}
				}
			}
		}
	} else {
		std::cout << "\tCamera not configured." << std::endl;
	}
		std::cout << std::endl;
}

//WEK create displayLayers to iterate thru all layers
void RCamCfg::displayLayer(json::value l)
{
	DEBUG_PRINT("%s", "\n");
	if (!l.is_object()) {
		throw std::runtime_error("Display layer is not a object"); 
	}
	std::vector<std::shared_ptr<Camera>> cams = getCameras();
	json::value s = getCfgValue("/Source", l);
	if (s.is_null()) {
		throw std::runtime_error("Display layer source is not found"); 
	}
	std::string loc = getLocation(cams.at(json::value_to<int>(s)));
	json::value sensor = getCfgValue("/Cameras/" + loc + "/Sensor");
	if (sensor.is_null()) {
		throw std::runtime_error("Display layer sensor is not found"); 
	}

	Size cs {};
	fromArray(cs, sensor);

	std::cout << loc << " " << cs.toString();
	json::value crop = getCfgValue("/Crop", l);
	if (!crop.is_null()) {
		Rectangle cr {};
		if (fromArray(cr, crop)) {
			std::cout <<  " Crop to " << cr.toString();
		}
	}
	json::value scale = getCfgValue("/Scale", l);
	if (!scale.is_null()) {
		Size ss {};
		if (fromArray(ss, scale)) {
			std::cout <<  " Scale to " << ss.toString();
		}
	}
	json::value overlay = getCfgValue("/Overlay", l);
	if (!overlay.is_null()) {
		Size os {};
		if (fromArray(os, overlay)) {
			std::cout <<  " Overlay at " << os.toString();
		}
	}
	std::cout << "\n";
}

void RCamCfg::cfgCameras(void)
{
	DEBUG_PRINT("%s", "\n");
	unsigned int i {0};
	do {
		std::vector<std::shared_ptr<Camera>> cams = getCameras();
		std::vector<std::string> camsMenu {};
		if (cams.size() != 0) {
			CLEARSCREEN
			std::cout << "\n\tConfigure cameras\n\n";
			std::cout << std::endl << "Available cameras:" << std::endl;
			for (auto const &cam : cams) { 
				displayCam(cam);
				camsMenu.push_back("Select/Configure " + getLocation(cam));
			}
		} else {
			throw std::runtime_error("No cameras available!");
		}
		i = menuUtil(camsMenu);
		if (i) cfgCamera(cams.at(i-1));
	} while (i);
}

void RCamCfg::cfgCamera(std::shared_ptr<Camera> cam)
{
	DEBUG_PRINT("%s", "\n");
	std::string loc = getLocation(cam);

	unsigned int i {0};
	do {
		std::vector<std::string> menu = {"Disable Camera",
			"Configure Sensor", "Configure Controls"};
		json::value camv = getCfgValue("/Cameras/" + loc);
		if (camv.is_null()) {
			menu.clear();
			menu.push_back("Enable Camera");
		}
		CLEARSCREEN
		std::cout << "\n\tConfigure camera\n\n";
		displayCam(cam);
		i = menuUtil(menu);
		if (menu.size() != 1) {
			switch (i) {
				case 1: {
					if (!dropCfgValue("/Cameras/" + loc)) {
						logger_.Log(LogLevel::ERROR, "Drop failed for /Cameras/" + loc + "/CameraID!", true);
					}
					break;   //WEK delete
				}
				case 2: cfgSensor(cam); break;
				case 3: cfgControl(cam); break;
			};
		} else {
			switch (i) {
				case 1: {
					json::value jv;
					jv.emplace_string() = cam->id();
					if (!setCfgValue("/Cameras/" + loc + "/CameraID", jv)) {
	//					std::cout << "Unable to set /Cameras/" + loc << "/CameraID!" << std::endl;
						logger_.Log(LogLevel::ERROR, "Unable to set /Cameras/" + loc + "/CameraID!", true);
					}
					break;
				}  //WEK add camid
			};
		}
	} while (i);
}
