/* youtube.cpp - Helper class for dma-heap allocations.
 */
 
#include "jsonio.hpp"
 
#include "youtube.hpp"

std::string getPath(std::string file)
{
	const char* homedir = getenv("HOME");
    if (homedir == nullptr) {
        struct passwd *pw = getpwuid(getuid());
        if (pw != nullptr) {
            homedir = pw->pw_dir;
        }
    }
    std::string home(homedir);
    std::string fullpath {};
	if (std::filesystem::exists(home + "/racecam/data/" + file)) { 
		fullpath = home + "/racecam/data/" + file;
		return fullpath;
	} else if (std::filesystem::exists("/usr/local/etc/racecam/data/" + file)) {
        fullpath = "/usr/local/etc/racecam/data/" + file;
    } else {
        std::cout << "youtube getpath(" << file << ") not found!" << std::endl;
    }
    return fullpath;
}


std::string tostring(std::time_t p)
{
	std::tm* now_tm = std::localtime(&p);
	char buffer[80]; 
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", now_tm);

    std::string time_str(buffer);
    return time_str;
}

json::value valueat(const std::vector<std::string>& path, const json::object& js)
{
	auto it = js.begin();
	auto obj = js;
	for (std::string s : path) {
		it = obj.find(s);
		if (it == obj.end()) throw std::runtime_error("valueat() " + s + " not found!");
		if (it->value().is_object()) obj = it->value().get_object();
	}
	return it->value();
}
//WEK get client info (id, secret, api_key)
YouTube::YouTube() 
{
//	fprintf(stdout, "%s:%s:%d \n", __FILE__, __PRETTY_FUNCTION__, __LINE__);
	json::value cv = jsonRead("client.json");
	if (cv.is_object()) {
		json::object co = cv.get_object();
		auto it = co.find("client_id");
		if (it == co.end()) throw std::runtime_error("Client ID not found!2");
		id_ = json::value_to<std::string>(it->value());
		it = co.find("client_secret");
		if (it == co.end()) throw std::runtime_error("Client Secret not found!");
		secret_ = json::value_to<std::string>(it->value());
	} else {
		throw std::runtime_error("Client ID and Secret not a object!");
	} 
}
YouTube::~YouTube() {}

size_t YouTube::getRespJson(char* ptr, size_t elem_size, size_t num_elem)
{
	size_t size = elem_size * num_elem;
	return rp_.write(ptr, size);
} 

rsp_type YouTube::postMsg(std::string const& url_str, std::string const& message)
{
	rsp_type rt;
	rp_.reset();
	curlpp::Easy req;
	curlpp::types::WriteFunctionFunctor functor(std::bind(&YouTube::getRespJson, this,
		std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	curlpp::options::WriteFunction *wf = new curlpp::options::WriteFunction(functor);
	req.setOpt(wf);
	const char *url = url_str.c_str();
	const char *msg = message.c_str();
	req.setOpt(new curlpp::options::Url(url));
	req.setOpt(new curlpp::options::PostFields(msg));
	req.setOpt(new curlpp::options::PostFieldSize((long)strlen(msg)));
	req.setOpt(new curlpp::options::Verbose(false));
	req.perform();
	rt.respcode = curlpp::infos::ResponseCode::get(req); 
	rp_.finish();
	json::value jv = rp_.release();
	rt.resp = jv.get_object();
	return rt;
}

rsp_type YouTube::postMsg(std::string const& url_str, std::string const& message, std::list<std::string> const& headers)
{
	rsp_type rt;
	rp_.reset();
	curlpp::Easy req;
	curlpp::types::WriteFunctionFunctor functor(std::bind(&YouTube::getRespJson, this,
		std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	curlpp::options::WriteFunction *wf = new curlpp::options::WriteFunction(functor);
	req.setOpt(wf);
	const char *url = url_str.c_str();
	const char *msg = message.c_str();
	req.setOpt(new curlpp::options::HttpHeader(headers));
	req.setOpt(new curlpp::options::Url(url));
	req.setOpt(new curlpp::options::PostFields(msg));
	req.setOpt(new curlpp::options::PostFieldSize((long)strlen(msg)));
	req.setOpt(new curlpp::options::Verbose(false));
//	req.setOpt(new curlpp::options::Verbose(true));
	req.perform();
	rt.respcode = curlpp::infos::ResponseCode::get(req); 
	rp_.finish();
	json::value jv = rp_.release();
	rt.resp = jv.get_object();
	return rt;
}

json::value YouTube::GetAuth(bool renew)
{
//	fprintf(stdout, "%s:%s:%d \n", __FILE__, __PRETTY_FUNCTION__, __LINE__);
	try {
		json::value av = jsonRead("racecam_auth.json");;
		if (!renew || av.is_null()) {
			//device code
			std::string get_dc_url = "https://oauth2.googleapis.com/device/code";
			std::string get_dc_str  = "client_id=" + id_ + 
					"&scope=https://www.googleapis.com/auth/youtube";
			rsp_type r = postMsg(get_dc_url, get_dc_str);
			auto it = r.resp.find("device_code");
			std::string device_code =  json::value_to<std::string>(it->value());
			it = r.resp.find("user_code");
			std::string uc =  json::value_to<std::string>(it->value());
			it = r.resp.find("expires_in");
			int expires =  json::value_to<int>(it->value());
			it = r.resp.find("interval");
			int intv =  json::value_to<int>(it->value());
			it = r.resp.find("verification_url");
			std::string vurl =  json::value_to<std::string>(it->value());
			std::cout << "At " << vurl  << " Enter verification code: " << uc << std::endl;
		//poll device code	
			std::string dc_str  = "client_id=" + id_ + 
					"&client_secret=" + secret_ + 
					"&device_code=" + device_code + 
					"&grant_type=urn:ietf:params:oauth:grant-type:device_code";
			std::string dc_url = "https://oauth2.googleapis.com/token";

			int tries = expires / intv;
			do {
				std::this_thread::sleep_for(std::chrono::seconds(intv));

				rsp_type dr = postMsg(dc_url, dc_str);
				if (dr.respcode == 428) {
					if (!(tries % 5)) std::cout << "." << std::flush;
					continue;		
				}
				if (dr.respcode == 200) {
					std::cout << " Device authorized." << std::endl;
					av = dr.resp;
					jsonWrite("racecam_auth.json", av);
					return av;
				} else {
					throw std::runtime_error("Auth device responce code: " + std::to_string(dr.respcode));
				}	
				
			} while (tries--);
			throw std::runtime_error("Timed out waiting for device responce!");
		} else {
		//reauth 
			if (!av.is_object()) throw std::runtime_error("racecam_auth.json is not a object!");
			json::object co = av.get_object();
			
			auto it = co.find("expires_in");
			if (it == co.end()) throw std::runtime_error("auth expires_in not found!");
			int expires =  json::value_to<int>(it->value());
							
			struct stat sb;
			if (stat (getPath("racecam_auth.json").c_str(), &sb)) throw std::runtime_error("racecam_auth.json stat failed!");;
			std::time_t expire_time = sb.st_mtime + expires - 300;
			std::time_t current_time = std::time(nullptr);
			
			if (current_time > expire_time) {
				it = co.find("access_token");
				if (it == co.end()) throw std::runtime_error("auth access_token not found!");
				std::string access_token = json::value_to<std::string>(it->value());
				it = co.find("refresh_token");
				if (it == co.end()) throw std::runtime_error("auth refresh_token not found!");
				std::string refresh_token =  json::value_to<std::string>(it->value());
				std::string refresh_str  = "client_id=" + id_ + 
					"&client_secret=" + secret_ + 
					"&refresh_token=" + refresh_token + 
					"&grant_type=refresh_token";
				std::string refresh_url = "https://oauth2.googleapis.com/token";
				rsp_type rr = postMsg(refresh_url, refresh_str);

				if (!(rr.respcode == 200)) throw std::runtime_error("reauth failed!");
	
				auto it = rr.resp.find("access_token");
				access_token = json::value_to<std::string>(it->value());
				co["access_token"] =  access_token;
				it = rr.resp.find("expires_in");
				co["expires_in"] =  json::value_to<int>(it->value());
				av = co;
				jsonWrite("racecam_auth.json",av);
			}
		}
		return av;
	}
	catch ( curlpp::LogicError & e ) {
		std::cout << e.what() << std::endl;
	}
	catch ( curlpp::RuntimeError & e ) {
		std::cout << e.what() << std::endl;
	}
	return nullptr;
}
yt_strm YouTube::StartStrm(std::string const& name, std::string const& publish)
{
	yt_strm stm;
	// call GetAuth
//	std::cout << "Start get OAuth" << std::endl;
	json::value av = GetAuth();
	if (!av.is_object()) throw std::runtime_error("GetAuth did not return a object!");
	json::object ao = av.get_object();
	auto it = ao.find("access_token");
	if (it == ao.end()) throw std::runtime_error("auth access_token not found!");
	std::string access_token = json::value_to<std::string>(it->value());
	//get times display and iso_zulu
//	std::cout << "Start broadcast" << std::endl;
	char c[80];
	const auto now = std::chrono::system_clock::now();
	std::time_t now_tt = std::chrono::system_clock::to_time_t(now);
	std::tm* tm = std::gmtime(&now_tt);
	std::strftime(c, sizeof(c), "%FT%TZ", tm);
	std::string gtd = c;
//	std::cout << gtd << std::endl;

	tm = std::localtime(&now_tt);
	std::strftime(c, sizeof(c), "%B %d, %Y %I:%M:%S %p", tm);
	std::string ltd = c;

	std::string req = 
		"{\"snippet\":{\"title\":\"" + name + " " + ltd + "\"," +
		"\"scheduledStartTime\":\"" + gtd + "\"}," + 
		"\"contentDetails\":{\"enableDvr\":true," +
		"\"recordFromStart\":true," +
		"\"enableAutoStart\":true," +
//		"\"enableAutoStop\":true," +
		"\"startWithSlate\":true}," +
		"\"status\":{\"privacyStatus\":\"" + publish + "\"}}";
//	std::cout << req << std::endl;
//	std::string url = "https://youtube.googleapis.com/youtube/v3/liveBroadcasts?part=snippet%2CcontentDetails%2Cstatus&key=" + apikey;
	std::string url = "https://youtube.googleapis.com/youtube/v3/liveBroadcasts?part=snippet%2CcontentDetails%2Cstatus";
	std::list<std::string> h;
    h.push_back("Content-Type: application/json");
    h.push_back("Accept: application/json");
    h.push_back("Authorization: Bearer " + access_token);
	//post and check for 200
	rsp_type rr = postMsg(url, req, h);
	if (!(rr.respcode == 200)) {
		std::cout << access_token << std::endl;
		std::cout << rr.respcode << std::endl;
		jsonWrite(std::cout, rr.resp);
		throw std::runtime_error("Broadcast insert failed!");
	}
	
	it = rr.resp.find("id");
	if (it == rr.resp.end()) throw std::runtime_error("broadcast id not found!");
	stm.b_id = json::value_to<std::string>(it->value());

//	std::cout << "Start stream" << std::endl;
//WEK get and display monitorstream
/* "contentDetails" : {
        "monitorStream" : {
            "enableMonitorStream" : true,
            "broadcastStreamDelayMs" : 0,
            "embedHtml" : "<iframe width=\"425\" height=\"344\" src=\"https://www.youtube.com/embed/VLNErnCAkx4?autoplay=1&livemonitor=1\" frameborder=\"0\" allow=\"accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture; web-share\" referrerpolicy=\"strict-origin-when-cross-origin\" allowfullscreen></iframe>"
        },
*/
	
	req = "{\"snippet\":{\"title\":\"RaceCamStream " + ltd + "\"}," +
	"\"cdn\":{\"frameRate\":\"variable\"," +
	"\"ingestionType\":\"rtmp\"," +
	"\"resolution\":\"variable\"}," +
	"\"contentDetails\":{\"isReusable\":false}}";
//	url = "https://youtube.googleapis.com/youtube/v3/liveStreams?part=snippet%2Ccdn%2CcontentDetails&key=" + apikey;
	url = "https://youtube.googleapis.com/youtube/v3/liveStreams?part=snippet%2Ccdn%2CcontentDetails";
	rr = postMsg(url, req, h);
	if (!(rr.respcode == 200)) {
		std::cout <<  rr.respcode << std::endl;
		jsonWrite(std::cout, rr.resp);
		throw std::runtime_error("Stream insert failed!");
	}
	
//	jsonWrite(std::cout, rr.resp);
	
	it = rr.resp.find("id");
	if (it == rr.resp.end()) throw std::runtime_error("stream id not found!");
	stm.s_id = json::value_to<std::string>(it->value());
//WEK works as rtmp or rtmps -- make that an option	
	stm.strmurl = json::value_to<std::string>(valueat(
		std::vector<std::string> {"cdn", "ingestionInfo", "ingestionAddress"},
//		std::vector<std::string> {"cdn", "ingestionInfo", "rtmpsIngestionAddress"},
		rr.resp));

/*	size_t pos = stm.strmurl.rfind("/live2");
	if (pos == std::string::npos) {
		throw std::runtime_error("live2 not found!");
	} else {
		stm.strmurl.insert(pos,":443");
	} */
	stm.strmurl += '/';

	stm.strmurl += json::value_to<std::string>(valueat(
		std::vector<std::string> {"cdn", "ingestionInfo", "streamName"},
		rr.resp));
		
//	std::cout << "Start bind" << std::endl;
//WEK get ingestion name and url
/*"cdn" : {
        "ingestionType" : "rtmp",
        "ingestionInfo" : {
            "streamName" : "k58k-um6h-rvq6-xrxf-3q6q",
            "ingestionAddress" : "rtmp://a.rtmp.youtube.com/live2",
            "backupIngestionAddress" : "rtmp://b.rtmp.youtube.com/live2?backup=1",
            "rtmpsIngestionAddress" : "rtmps://a.rtmps.youtube.com/live2",
            "rtmpsBackupIngestionAddress" : "rtmps://b.rtmps.youtube.com/live2?backup=1"
*/
	
	req.clear();
	url = "https://youtube.googleapis.com/youtube/v3/liveBroadcasts/bind?id=" + 
//		stm.b_id + "&part=snippet&streamId=" + stm.s_id +"&key=" + apikey;
		stm.b_id + "&part=snippet&streamId=" + stm.s_id; 
	h.clear();
	h.push_back("Accept: application/json");
    h.push_back("Authorization: Bearer " + access_token);
    
    rr = postMsg(url, req, h);
	if (!(rr.respcode == 200)) {
		std::cout <<  rr.respcode << std::endl;
		jsonWrite(std::cout, rr.resp);
		throw std::runtime_error("bind failed!");
	}

//	std::cout << "Done start" << std::endl;
	return stm;
}

void YouTube::StopStrm(std::string const& id)
{
	//WEK will fail to transition if no stream recived and stat is still ready
	json::value av = GetAuth();
	if (!av.is_object()) throw std::runtime_error("GetAuth did not return a object!");
	json::object ao = av.get_object();
	auto it = ao.find("access_token");
	if (it == ao.end()) throw std::runtime_error("auth access_token not found!");
	std::string access_token = json::value_to<std::string>(it->value());
	std::string url =  "https://youtube.googleapis.com/youtube/v3/liveBroadcasts/transition?broadcastStatus=complete&id=" 
//		+ id + "&part=snippet%2Cstatus&key=" + apikey;
		+ id + "&part=snippet%2Cstatus";
	std::list<std::string> h;
	h.push_back("Accept: application/json");
    h.push_back("Authorization: Bearer " + access_token);
    
    rsp_type rr = postMsg(url, "", h);
	if (!(rr.respcode == 200)) {
		std::cout << url << std::endl;
		std::cout <<  rr.respcode << std::endl;
		jsonWrite(std::cout, rr.resp);
		throw std::runtime_error("transition to complete failed!");
	}
//	jsonWrite(std::cout, rr.resp);
}
