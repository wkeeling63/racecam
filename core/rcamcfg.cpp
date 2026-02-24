/* 
*  rcamcfg.cpp
*/
#include "core/jsonio.hpp"
#include "core/rcamcfg.hpp"

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
			} else {
				json::array ja;
				for (const auto& l : cv.get<libcamera::Span<const long>>())
					ja.emplace_back(l);
				jv = ja;
			}	
			return jv;
		}
		case ControlTypeFloat: {
			if (!cv.isArray()) {
				if (cv.numElements() != 1) return jv;
				jv.emplace_double() = cv.get<float>();
			} else {
				json::array ja;
				for (const auto& f : cv.get<libcamera::Span<const float>>())
					ja.emplace_back(f);
				jv = ja;
			}
			return jv;
		}
		case ControlTypeRectangle: {	
			if (!cv.isArray()) {
				if (cv.numElements() != 1) return jv;
				Rectangle r = cv.get<Rectangle>();
				json::array ja = {{r.x, r.y, r.width, r.height}};
				jv = ja;
			} else {
				json::array ja, ja1; 
				for (const auto& r : cv.get<libcamera::Span<const Rectangle>>()) {
					ja1 = {r.x, r.y, r.width, r.height};
					ja.emplace_back(ja1);
				}
				jv = ja;
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

//WEK bug default cfg not working;
RCamCfg::RCamCfg(Logger& lptr, std::string const& cfg) : RCamShared(lptr, cfg)
{
	DEBUG_PRINT("%s", "\n");
	if (cfgpath_.size()) {
		config_ = jsonRead(cfgfile_, &cfgpath_);
	} else {
		config_ = jsonRead(cfgfile_);
	}
    if (config_.is_null()) {
		logger_.Log(LogLevel::INFO, std::string("Configuration " + cfgpath_ + cfgfile_ + " is empty or not found!"));
		config_ = json::object {};
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
	return dropCfgValue(path, config_);
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
	return setCfgValue(path, val, config_, create);
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
		std::cout << ("\tconfiguration file: ") << cfgpath_ << cfgfile_<< "\n\n";
		
		i = menuUtil(menu);
		switch (i) {
			case 1: cfgCameras(); break;
			case 2: cfgAudio(); break;
			case 3: cfgRaw(); break;
			case 4: cfgComposite(); break;
			case 5: {
				jsonWrite(std::cout, config_);
				if (getBool("SaveConfig", "No").value_or(false)) {
					if (cfgpath_.size()) {
						jsonWrite(cfgfile_, config_, &cfgpath_);
					} else {
						jsonWrite(cfgfile_, config_);
					}
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
		std::cout << "\n\tConfigure raw output\n";
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
		std::cout << "\n\tConfigure Raw Streams\n";
		//WEK why extra lines before layer list
		json::value sv = getCfgValue("/Outputs/Raw/Streams");
		if (!sv.is_null()) {
			if (sv.is_array()) {
				for(auto it = sv.get_array().begin();it != sv.get_array().end(); it++) {
					displayLayer(*it);
				}
			}
		} else {
			std::cout << "No Stream configured." << std::endl;
		}

		StrmMap sm(config_);
		std::vector<std::string> menu {};
		for (unsigned int i = 0; i < sm.used(Container::Raw); i++) {
			json::value sv = getCfgValue("/Outputs/Raw/Streams/" + std::to_string(i) + "/Source");
			std::string desc {};
			if (!sv.is_null()) { 
				CamStrm cs(json::value_to<int>(sv));
				desc = sm.getstrmlabel(cs);
			}
			menu.push_back("Update/Delete stream " + std::to_string(i) +
			"(" + desc + ")");
		}
		if (sm.free(Container::Raw)) {
			menu.push_back("Add stream");
		}
		i = menuUtil(menu);
		if (i) {
			cfgLayer(i - 1, Container::Raw);
		}
	} while (i); 
}

void RCamCfg::cfgComposite(void)
{
	DEBUG_PRINT("%s", "\n");
	unsigned int i {0};
	do {
		CLEARSCREEN
		std::cout << "\n\tConfigure composite\n\n";
		StrmMap sm(config_);
		std::vector<std::string> menu;
		json::value dest = getCfgValue("/Outputs/Composite/Destination");
		menu.push_back("Configure composite destination");
		if (!dest.is_null()) {
			for (unsigned int i = 0; i < sm.used(Container::Comp); i++) {
				menu.push_back("Update/Delete " + ( i ? "layer " + std::to_string(i) + "   ": "main layer") + " " + sm.getstrmlabel(i, Container::Comp)); 
			}
			if (sm.free(Container::Comp)) {
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
			cfgLayer(i - 2, Container::Comp);
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
				if (YouTube().GetAuth().is_null()) {
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

void	RCamCfg::cfgLayer(int lnum, Container type)
{
	DEBUG_PRINT("%s", "\n");
	StrmMap sm(config_);
	CamStrm cs {};
	bool Crop {false}, Scale {false}, Overlay {false};  
	std::string key {};
	json::value camv {};
	if (Container::Raw == type) {
		key = "/Outputs/Raw/Streams/" + std::to_string(lnum);
	} else {
		key = "/Outputs/Composite/Layers/" + std::to_string(lnum);
	}
	
	json::value lv = getCfgValue(key);
	if (!lv.is_null()) {
		displayLayer(lv);
		if (Container::Raw == type) {
			if (getBool("DropStream", "N").value()) {
				if (!dropCfgValue(key)) {
					throw std::runtime_error("drop stream failed!");
				} else {
					return;
				}
			}
		} else {
			if (!lnum) { 
				std::cout << "Dropping the main layer requires all layer to be removed!" << std::endl;
				if (getBool("DropLayer", "N").value()) {
					if (!dropCfgValue("/Outputs/Composite/Layers")) {
						throw std::runtime_error("drop all layers failed!");
					} else {
						return;
					}
				}
			} else {
				if (getBool("DropLayer", "N").value()) {
					if (!dropCfgValue(key)) {
						throw std::runtime_error("drop layer failed!");
					} else {
						return;
					}
				}
			}
		}
		
		camv = getCfgValue("/Source", lv);
		if (!camv.is_null()) {
			cs = CamStrm(json::value_to<int>(camv));
		}
		Crop = !getCfgValue("/Crop", lv).is_null();
		Scale = !getCfgValue("/Scale", lv).is_null();
		Overlay = !getCfgValue("/Overlay", lv).is_null();
	}
	
	if (!sm.free(type)) {
		std::cout << "No camera available, remove " << 
			(type ? "Stream" : "Layer") <<
			" to reuse camera.\nPlease press <ENTER> to continue: \n";
		std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
		return;
	}
	bool update = (camv.is_null() ? false : true);
	update = (update ? getBool("UpdateCamera", "N").value() : true);
	if (update) {
		std::vector<std::string> menu;
		std::cout << "Select camera for layer" << std::endl;
		menu = sm.freeStrmLabels(type);
		unsigned int i {0};
		i = menuUtil(menu, false);
		cs = CamStrm(sm.getStream(menu.at( i - 1 )));
		json::value v = cs.getCamStrm();
		if (!setCfgValue(key + "/Source", v)) {
			logger_.Log(LogLevel::WARN, key + "/Source" + std::to_string(cs.getCamStrm()) + " unable to set!");
		}
	}

	update = false;
	if (Crop) {
		update = getBool("UpdateCrop", "N").value();
	} else {
		update = getBool("AddCrop", "N").value();
	}
	if (update) {
		libcamera::Rectangle r {0, 0, sm.camSize(cs)};
		std::optional<Rectangle> o = getRectangle("CropValue", "", r);
		if (o) { 
			if (!setCfgValue(key + "/Crop", json::array {o.value().x, o.value().y, o.value().width, o.value().height} )) {
				logger_.Log(LogLevel::WARN, key + "/Crop unable to set!", true);
			} else {
				sm.loadLayers();
			}
		} else {
			if (Crop) {
				if (!dropCfgValue(key + "/Crop")) {
					throw std::runtime_error("drop crop failed!");
				}
				sm.loadLayers();
			}
		}
	}
	
	update = false;
	if (Scale) {
		update = getBool("UpdateScale", "N").value();
	} else {
		update = getBool("AddScale", "N").value();
	}
		if (update) {
			libcamera::Size s {3840, 2160};
		std::optional<Size> o = getSize("ScaleValue", "", s);
		if (o) {
			if (!setCfgValue(key + "/Scale", json::array {o.value().width, o.value().height} )) {
				logger_.Log(LogLevel::WARN, key + "/Scale unable to set!", true );
			} else {
				sm.loadLayers();
			}
		} else {
			if (Scale) {
				if (!dropCfgValue(key + "/Scale")) {
					throw std::runtime_error("drop scale failed!");
				}
				sm.loadLayers();
			}
		}
	}
	
	if (lnum && Container::Comp == type) {
		CamStrm cs0;
		json::value cv0 = getCfgValue("/Outputs/Composite/Layers/0/Source");
		if (!cv0.is_null()) {
			cs0 = CamStrm(json::value_to<int>(cv0));
		} else {
			throw std::runtime_error("no layer 0!");
		}
		update = true;
		if (Overlay) {
			update = getBool("UpdateOverlay", "N").value();
		} 
		if (update) {
			libcamera::Size zl = sm.outSize(cs0);
			libcamera::Size ol = sm.outSize(cs);
			libcamera::Point min {0 - (int)ol.width, 0 - (int)ol.height};
			libcamera::Point max {(int)zl.width + (int)ol.width, (int)zl.height + (int)ol.height};
			libcamera::Point def {};
			float f = 1.0f/3.0f; //defaut location factor 
			int left = (zl.width-ol.width > 0 ? 0 : zl.width-ol.width);
			int lower = (ol.height <= std::round(f*zl.height) ? zl.height-ol.height : std::round((1.0f-f)*(zl.height-ol.height)));
			int right = (ol.width <= std::round(f*zl.width) ? zl.width-ol.width : std::round((1.0f-f)*(zl.width)));
			int upper = (ol.height <= std::round(f*zl.height) ? 0 : (zl.height-ol.height+(std::round(f*zl.height))));
			if (lnum == 1) {def = {left,lower};
			} else if (lnum == 2) {def = {right,lower};
			} else if (lnum == 3) {def = {right,upper};
			} else {def = {left,upper};
			} 
			std::cout << "def: " << def.toString() << std::endl;
			std::optional<Point> o = getPoint("OverlayValue", def.toString(), max, min);
				if (!setCfgValue(key + "/Overlay", json::array {o.value().x, o.value().y} )) {
					logger_.Log(LogLevel::WARN, key + "/Overlay unable to set!");
				}
		}
	} else {
		if (Overlay) {
			if (!dropCfgValue(key + "/Overlay")) {
				throw std::runtime_error("drop overlay failed!");
			}
		}
	}
	//WEK here add layer parms if !lnum
	if (lnum && Container::Comp == type) return;
	
	std::string comps {"Y"};
	if (Container::Raw == type) comps = "N";
	bool comp = getBool("Compress", comps).value();
	if (comp) {
		comps = "libx264";
	} else {
		comps = "yuv4";
	}
	if (!setCfgValue(key + "/CodecParms/Format", json::string{comps})) {
		throw std::runtime_error("Unable to set " + key + "/CodecParms/Format");
	} 
	
	if (Container::Raw == type) {
		json::value ccrfv = getCfgValue(key + "/CodecParms/CRF");
		if (ccrfv.is_null()) {
			update = getBool("AddCRF", "N").value();
		} else {
			update = getBool("UpdateCRF", "N").value();
			std::cout << "Constant Rate Factor is: " << json::value_to<int>(ccrfv) 
			<< std::endl;
		}
		std::optional<int> ocrf;
		// CRF 0 51
		if (update) {
			std::optional<int> ocrf = getInt("CRF", "", 51, 0);
			if (ocrf) {
				json::value v = ocrf.value();
				if (!setCfgValue(key + "/CodecParms/CRF", v)) {
					throw std::runtime_error("Unable to set " + key + "/CodecParms/CRF");
				} 
			} else {
				if (!dropCfgValue(key + "/CodecParms/CRF")) {
					throw std::runtime_error("Unable to drop " + key + "/CodecParms/CRF");
				}
			}
		}
	} else {
				json::value cbrv = getCfgValue(key + "/CodecParms/BitRate");
		if (cbrv.is_null()) {
			update = getBool("AddBitRate", "N").value();
		} else {
			update = getBool("UpdateBitRate", "N").value();
			std::cout << "Bitrate is: " << json::value_to<int>(cbrv) 
			<< " Kbps" << std::endl;
		}
		std::optional<int> obr;
		//youtube 1500kbps - 6000kbps 
		if (update) {
			std::optional<int> obr = getInt("BiteRate", "", 30000, 1500);
			if (obr) {
				json::value v = obr.value();
				if (!setCfgValue(key + "/CodecParms/BitRate", v)) {
					throw std::runtime_error("Unable to set " + key + "/CodecParms/BitRate");
				} 
			} else {
				if (!dropCfgValue(key + "/CodecParms/BitRate")) {
					throw std::runtime_error("Unable to drop " + key + "/CodecParms/BitRate");
				}
			}
		}
	}
}

void RCamCfg::cfgSensor(const std::shared_ptr<Camera> &cam)
{
	DEBUG_PRINT("%s", "\n");
	std::string loc = getLocation(cam);
	bool csicam = isCSI(cam);
	json::value sensorjv = getCfgValue("/Cameras/" + loc + "/Sensor");
	json::value streamsjv = getCfgValue("/Cameras/" + loc + "/Streams");
	unsigned int idx = 1;
	std::cout << "Modes for: " << cam->id() << std::endl;
		
	std::map<std::string, modes> modeMap;
	cam->acquire();
 
	std::unique_ptr<CameraConfiguration> config;
		//VideoRecording and Raw are needed -- just need for crop to have correct values
	if (csicam) {
		config = cam->generateConfiguration({libcamera::StreamRole::Raw, libcamera::StreamRole::VideoRecording});
	} else {
		config = cam->generateConfiguration({libcamera::StreamRole::Raw});
	}

	if (!config) {
		throw std::runtime_error("failed to generate capture configuration");
	}
		
	const StreamFormats &formats = config->at(0).formats();
	if (!formats.pixelformats().size()) throw std::runtime_error("pixel format is empty");
			
	for (const auto &pix : formats.pixelformats()) {
		if (pix !=libcamera::formats::YUYV && !isCSI(cam)) continue; //if usb and pixel not yuv422 then skip
		int bits = 0;
		if (csicam) bits = getBitDepth(pix);
		for (const auto &sz : formats.sizes(pix)) {
			Sensor s(sz, bits);
			std::string modestr = s.toString();
			config->at(0).size = sz;
			config->at(0).pixelFormat = pix;
			if (bits) {
				config->sensorConfig = s.getSensorCfg();
			}
			auto status = config->validate();
			if (CameraConfiguration::Invalid == status) 
				throw std::runtime_error("invalid sensor configuration");
			cam->configure(config.get());
				
			auto fd_ctrl = cam->controls().find(&controls::FrameDurationLimits);
			double fps = fd_ctrl == cam->controls().end() ? NAN : (1e6 / fd_ctrl->second.min().get<int64_t>());
				
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
		
	for (const auto &m : modeMap) {
		std::string size = m.second.size.toString();
		if (7 == size.size()) size = " " + size;
		size.resize(10, ' ');
		std::string bitstr = std::to_string(m.second.bitDepth);
		if (1 == bitstr.size()) bitstr = " " + bitstr;
		if (std::isnan(m.second.fps)) 
			std::cout << idx++ <<": " << size << std::endl;
		else {
			std::string message = "";
			if (m.second.fps < 30 ) message = " FPS less 30 are not recommended!";
			std::cout << idx++ <<": " << size << bitstr << " bpp " << m.second.crop << "\n       Max FPS: " 
			<< std::fixed << std::setprecision(2) << m.second.fps << message << std::endl;	
		}	
	}

// set defaults 
	std::string defsensorstr = ""; 

	if (!sensorjv.is_null()) {
		Sensor s(sensorjv);
		std::string modestr = s.toString();
		int i=1;
		for (auto it = modeMap.begin(); it != modeMap.end(); ++it, ++i) {
			if (it->first == modestr) defsensorstr = std::to_string(i);
		}
	} 	
	// end display mode 
	std::string defstreamstr[2] {"1920X1080", ""};
	if (!streamsjv.is_null()) {
		Streams strms(streamsjv);
		int i=0;
		for (const libcamera::Size size : strms.getStreams()) {
			defstreamstr[i++] = size.toString();
		}
	}
// get input 
	std::cout << std::endl;
	std::vector<libcamera::Size> out {};
	int mode {};
	if (csicam) {
		out.push_back(getSize("StreamSize", defstreamstr[0], libcamera::Size(3840,2160)).value());
		std::optional<libcamera::Size> tmp;
		if (defstreamstr[1].empty()) {
			tmp = getSize("StreamSize2", defstreamstr[1], out[0]);
		} else {
			bool streamtwo = getBool("SecondStream", "Yes").value();
			if (streamtwo) tmp = getSize("StreamSize2", defstreamstr[1], out[0]);
		}
		if (tmp) out.push_back(tmp.value());
		mode = getInt("ModeSelect", "", --idx, 1).value_or(0);
	} else {
		mode = getInt("OutputSize", defsensorstr, --idx, 1).value_or(1);
		auto it = modeMap.begin();
		std::advance(it, mode-1);
		out.push_back(it->second.size);
		mode = 0;
	}
					
// validate and save config
	config.release();
	if (out.size() == 2) {
		config = cam->generateConfiguration({libcamera::StreamRole::VideoRecording, libcamera::StreamRole::VideoRecording});
		config->at(1).size = out[1];
	} else {
		config = cam->generateConfiguration({libcamera::StreamRole::VideoRecording});
	}
	config->at(0).size = out[0];
	if (mode) {
		auto it = modeMap.begin();
		std::advance(it, mode-1);
		Sensor s (it->second.size,  it->second.bitDepth);
		sensorjv = s.getArray();
	} else {
		sensorjv = nullptr;
	}
	streamsjv = Streams(out).getArray();
	auto status = config->validate();
	if (CameraConfiguration::Invalid == status) throw std::runtime_error("invalid sensor configuration");
	cam->configure(config.get());

	auto fd_ctrl = cam->controls().find(&controls::FrameDurationLimits);
	double fps = fd_ctrl == cam->controls().end() ? NAN : (1e6 / fd_ctrl->second.min().get<int64_t>());
	if (fps < 30 && !std::isnan(fps)) {
		std::cout << "Max frames per second less than 30! \n You should think able reconfiguring \n the sensor! FPS: " << fps << std::endl;
		getBool("Comtinue", ""); 
	}
	if (sensorjv.is_null()) {
		dropCfgValue("/Cameras/" + loc + "/Sensor");
	} else {
		if (!setCfgValue("/Cameras/" + loc + "/Sensor", sensorjv)) {
			throw std::runtime_error("Unable to set /Cameras/" + loc + " Sensor!");
		}
	}
		
	if (!setCfgValue("/Cameras/" + loc + "/Streams", streamsjv)) {
		throw std::runtime_error("Unable to set /Cameras/" + loc + " Streams!");
	}
		
	cam->release();	
}


void RCamCfg::cfgControl(const std::shared_ptr<Camera> &cam)
{
	DEBUG_PRINT("%s", "\n");
	std::string loc = getLocation(cam);
	json::value sensorjv = getCfgValue("/Cameras/" + loc + "/Sensor");
	json::value streamsjv = getCfgValue("/Cameras/" + loc + "/Streams");
	if (streamsjv.is_null()) {
		logger_.Log(LogLevel::WARN, "No streams configured!", true);
		return;
	}
	
	cam->acquire();
	
	std::unique_ptr<CameraConfiguration> config;
	Streams strms(streamsjv);
		std::vector<libcamera::Size> out = strms.getStreams();
	if (out.size() == 2) {
			config = cam->generateConfiguration({libcamera::StreamRole::VideoRecording, libcamera::StreamRole::VideoRecording});
			config->at(1).size = out[1];
		} else {
			config = cam->generateConfiguration({libcamera::StreamRole::VideoRecording});
		}
		config->at(0).size = out[0];

	if (!sensorjv.is_null()) {
		Sensor sens(sensorjv);
		config->sensorConfig = sens.getSensorCfg();
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
	const ControlId *id = nullptr;
	ControlInfo info;
	std::string cntlmsg;
	json::value jv;
	std::vector<std::string> defv;
	
	const libcamera::ControlList& camera_controls = cam->controls();
	const libcamera::ControlInfoMap* info_map = camera_controls.infoMap();
	if (!info_map) {
		return jv;  //WEK continue return nothing or throw??
	}
	auto it = info_map->find(idNum);
	if (it == info_map->end()) {
		return jv;  //WEK continue return nothing or throw??
	}

	id = it->first;
	info = it->second;
	cntlmsg = getCntlMsg(id->name());
		
//	ID 20003 maps to: ScalerCrops
//  ID 27 maps to: ScalerCrop
//fix scalercrops to use crops as defaults and crop as min/max and only take 2 rectangles
	if ("ScalerCrops" == id->name()) {  
		auto it = info_map->find(27); //WEK check by Control::ScalerCrop
		if (it == info_map->end()) {
			return jv;  //WEK continue return nothing or throw??
		}
		defv = { info.min().toString(), info.max().toString() };
		info = it->second;
	}

	CLEARSCREEN	
	std::cout << cntlmsg << std::endl;

	size_t notdone = id->size() ? id->size() : 1;
	size_t size = notdone;
			
	switch (id->type()) 
	{
		case ControlTypeBool: 
		{
			std::optional<bool> o = getBool("GetBool", "");
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
				std::optional<int> o = getInt("GetInt", "", info.max().get<int>(), info.min().get<int>()); 
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
				std::optional<long> o = getLong("GetInt", "", info.max().get<long>(), info.min().get<long>()); 
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
				std::optional<float> o = getFloat("GetFloat","", info.max().get<float>(), info.min().get<float>()); 
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
			if (defv.size()) notdone = defv.size();
			int i {0};
			do
			{
				std::string d = (defv.size() ? defv[i] : ""); 
				std::optional<Rectangle> o = getRectangle("GetRect", d, info.max().get<Rectangle>(), info.min().get<Rectangle>()); 
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
	message.prompt = "Prompt message not found in msgs_map for (" + msg_id + ")!" ;
	message.help = "Help message not found in msgs_map for message ID: " + msg_id;
	auto it_msg = msgs_map.find(msg_id);
	if (it_msg != msgs_map.end())
	{
		message.prompt = it_msg->second.prompt;
		message.help = it_msg->second.help;
	}
	return message;
}

//WEK does () around def make it more readable think about point that has () already
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

std::optional<libcamera::Point> RCamCfg::getPoint(const std::string msg_id, const std::string def, const libcamera::Point max, const libcamera::Point min)
{
	DEBUG_PRINT("%s", "\n");
	msg cur = getMsg(msg_id);

	unsigned int i = 0;
	libcamera::Point point;
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
		if (sscanf(input.c_str(), "(%i,%i) %c", &point.x, &point.y, &junk) != 2) {continue;}
		if (point.x < min.x || point.y < min.y || point.x > max.x || point.y > max.y) {continue;}
		
		return point;
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
		if ((rect.x < min.x || rect.y < min.y || rect.width < min.width || 
				rect.height < min.height) && (rect.x > max.x ||  
				rect.y > max.y ||  rect.width > max.width || 
				rect.height > max.height)) 
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
		json::value snsrv = getCfgValue("/Cameras/" + loc + "/Sensor");
		json::value strmsv = getCfgValue("/Cameras/" + loc + "/Streams");
		if (strmsv.is_null()) {
			std::cout << "\tCamera not configured." << std::endl << std::endl;
			return;
		}
		std::cout << "    Current settings:" << std::endl;
		if (!snsrv.is_null()) std::cout << "      " << Sensor(snsrv).toDisplayString() << std::endl;
		std::vector<libcamera::Size> {Streams(strmsv).getStreams()};
		int i =0;
		for (const libcamera::Size size : Streams(strmsv).getStreams()) {
			std::cout << "          Stream " << i++ << " " << size.toString() << std::endl;
		}
		json::value co = getCfgValue("/Cameras/" + loc + "/Controls");
		if (!co.is_null()) {
			auto const& obj = co.get_object();
			if(!obj.empty()) {
				for(auto it = obj.begin();it != obj.end();it++) {
					std::cout << "             " << it->key() << ":"
						<< json::serialize(it->value()) << std::endl;
				}
			}
		}
	//	}
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

//WEK should camera label be from libcamera or StrmMap
	CamStrm cs(json::value_to<int>(s));
	std::string loc = getLocation(cams.at(cs.getCamera()));
	
	json::value ssv = getCfgValue("/Cameras/" + loc + "/Streams/" + std::to_string(cs.getStream()));
	if (ssv.is_null()) {
		throw std::runtime_error("Display layer camera stream is not found"); 
	}

	Size ss {};
	fromArray(ss, ssv);
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
			"Configure Camera", "Configure Controls"};
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
						logger_.Log(LogLevel::ERROR, "Unable to set /Cameras/" + loc + "/CameraID!", true);
					}
					break;
				}  //WEK add camid
			};
		}
	} while (i);
}
