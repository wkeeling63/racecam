// need stat corrected everywhere add configure
// and figure out where stopping should change to configed

/* racecam capture class code
*  rcam.cpp
*/
//#include <string>

//#include <boost/json/src.hpp> 
//#include <boost/json.hpp>  
#include "core/jsonio.hpp"

//#include "core/rcamshared.hpp"
#include "core/rcam.hpp"

#include <linux/dma-buf.h>
#include <sys/ioctl.h>

//#include <stdio.h>

RCam::RCam(Logger& lptr, std::string const& path, std::string const& cfg) : RCamShared(lptr, path, cfg)
{
	DEBUG_PRINT("%s", "\n");

// libcamera::LoggingTargetNone, libcamera::LoggingTargetSyslog, libcamera::LoggingTargetFile, libcamera::LoggingTargetStream 
//	logSetTarget(LoggingTargetSyslog);   //libcamera logging
//	logSetTarget(LoggingTargetNone);   //libcamera logging
	// DEBUG, INFO, WARN, ERROR and FATAL
	// WEK FYI you can't call logSetTarget twice -- so it can't be here or we need some sort of 
//	logSetLevel("*", "INFO"); //("Camera", "INFO");
//	logSetLevel("*", "ERROR");
/*	AV_LOG_QUIET   -8 	Print no output.
   	AV_LOG_PANIC   0  	Something went really wrong and we will crash now.
   	AV_LOG_FATAL   8  	Something went wrong and recovery is not possible.
   	AV_LOG_ERROR   16  	Something went wrong and cannot losslessly be recovered.
   	AV_LOG_WARNING   24 Something somehow does not look correct.
   	AV_LOG_INFO   32  	Standard information.
   	AV_LOG_VERBOSE   40 Detailed information.
    AV_LOG_DEBUG   48	Stuff which is only useful for libav* developers. */
}

unsigned int RCam::getCntlID(const std::string cntlname)
{
	DEBUG_PRINT("%s", "\n");
	for (auto const &cm : controls::controls) {
		if (cntlname == cm.second->name()) return cm.first;
	}
	return false;
}

void RCam::InitCapture()
{
	DEBUG_PRINT("%s", "\n");
	config = jsonRead(cfgloc); 
	if (config.is_null()) throw std::runtime_error("Configuration " + cfgloc + " not found!");
	initCameraManager(); 
	
	json::object cams = getCfgValue("/Cameras").get_object();
	initCams(cams);

	initOutputs();

	startCams(cams);
}

void RCam::FreeCapture()
{
	DEBUG_PRINT("%s", "\n");
//WEK why not drive off the camera status struct
	json::object cams = getCfgValue("/Cameras").get_object();

	stopCams(cams);
	stop_threads_ = true;
	
	freeOutputs();

	freeCams(cams);
	
	cm_->stop();
	cm_.reset();
	for (auto it = rcams_.begin(); it != rcams_.end(); it++)
		it->stat = RCunknown;
}

void RCam::initCams(const json::object& cams)
{
	DEBUG_PRINT("%s", "\n");
	unsigned int i = 0;
	// iterates "/Cameras" json and builds rcam_[cam#]	
	for(auto it = cams.begin(); it != cams.end(); ++it) {
		DEBUG_PRINT(" camera id %d%s", i, "\n");	
		// get/save camid as .cam
		std::string camid = json::value_to<std::string>(getCfgValue("/CameraID", it->value()));
		rcams_[i].cam = cm_->get(camid);
		if (!rcams_[i].cam)
			throw std::runtime_error("failed to find camera " + camid);
		// get/save location as .loc	
		rcams_[i].loc = getLocation(rcams_[i].cam);
		// acquire cam and set .stat and cam_cnt_
		if (rcams_[i].cam->acquire())
			throw std::runtime_error("failed to acquire camera " + camid);
		rcams_[i].stat = RCaquired;
		cam_cnt_ = i + 1;  
	
				//	rcams_[i].cfg = rcams_[i].cam->generateConfiguration({StreamRole::VideoRecording, StreamRole::VideoRecording}); 
				// fix  "concurrent streams need matching numbers of buffers"
				// done't get error with 2 raw or raw and video
		//
		// gen and save  std::unique_ptr<CameraConfiguration> as .cfg
		 
		//WEK dual stream 
		// if cam dual stream set roles, update stream sizes and set scalecrops
	
		rcams_[i].cfg = rcams_[i].cam->generateConfiguration({StreamRole::VideoRecording}); 
		
		if (!rcams_[i].cfg)
			throw std::runtime_error("failed to generate video configuration");

		// Now we override any of the default settings from config file to the StreamConfiguration of the CameraConfiguration
		StreamConfiguration &cfg = rcams_[i].cfg->at(0);
		
//		std::cout << "pixel b4: " << cfg.pixelFormat.toString() << std::endl;

		if (isCSI(rcams_[i].cam)) {
			cfg.pixelFormat = libcamera::formats::YUV420;
		} else {
			cfg.pixelFormat = libcamera::formats::YUYV;
		}
		
//		std::cout << "pixel after: " << cfg.pixelFormat.toString() << std::endl;
		// add buffer parm if needed here
		cfg.bufferCount = 6; 

		auto jv = getCfgValue("/Sensor", it->value());

/*		Size sens {}; // WEK set defaults here for fail to be values from config
		if (fromArray(sens, jv))
		{
			cfg.size.width = sens.width;
			cfg.size.height = sens.height;
	//					// configure sensor as it is CSI-2
	// 		rcams_[i].cfg->sensorConfig = libcamera::SensorConfiguration();
	//		rcams_[i].cfg->sensorConfig->outputSize.width = 1640;
	//		rcams_[i].cfg->sensorConfig->outputSize.height = 1232;
	//		rcams_[i].cfg->sensorConfig->bitDepth = 10;  
		} else
		{
			logger_.Log(LogLevel::ERROR, "Unable to set sensor for " + camid);
		} */

		Sensor sens(jv);
		cfg.size = sens.outRes;
		if (sens.hasSensor()) {
			rcams_[i].cfg->sensorConfig = libcamera::SensorConfiguration();
			rcams_[i].cfg->sensorConfig->outputSize = sens.senRes;
			rcams_[i].cfg->sensorConfig->bitDepth = sens.senBit;
		}
	
		// why not use color space of config gen??
		if (cfg.size.width >= 1280 || cfg.size.height >= 720)
			cfg.colorSpace = libcamera::ColorSpace::Rec709;   //HDTV colorspace
		else
			cfg.colorSpace = libcamera::ColorSpace::Smpte170m; // NTSC/PAL/SDTV colorspace	
//		DEBUG_PRINT("%s", "\n");
//	add parm for rotate
//		rcams_[i].cfg->orientation = libcamera::Orientation::Rotate0 * (libcamera::Transform) options_->orientation_v[cam];
/* enum  	Orientation {
  Orientation::Rotate0 = 1, Orientation::Rotate0Mirror, Orientation::Rotate180, Orientation::Rotate180Mirror,
  Orientation::Rotate90Mirror, Orientation::Rotate270, Orientation::Rotate270Mirror, Orientation::Rotate90
} */
		jv = getCfgValue("/Orientation", it->value());
		if (!jv.is_null())
			rcams_[i].cfg->orientation = (libcamera::Orientation) json::value_to<int>(jv);

		// set stride 0 before validation and validation will set it as need for format
		for (auto &config : *rcams_[i].cfg) config.stride = 0; 
		CameraConfiguration::Status validation = rcams_[i].cfg->validate();
		if (validation == CameraConfiguration::Invalid)
			throw std::runtime_error("failed to validate stream configurations");
		else if (validation == CameraConfiguration::Adjusted)
			logger_.Log(LogLevel::INFO, "Stream configuration adjusted for " 
			+ json::serialize(it->key()));
			
//		std::cout << "pixel validate: " << cfg.pixelFormat.toString() << std::endl;

	//	if (!isCSI(rcams_[i].cam)) {
	//		rcams_[i].cntls.set(libcamera::controls::FrameRate, 30.0);
	//		int64_t frame_time_us = 1000000 / 30; 
	//		rcams_[i].cntls.set(libcamera::controls::FrameDurationLimits, 
     //         libcamera::Span<const int64_t, 2>({frame_time_us, frame_time_us}));
	//	}

		if (rcams_[i].cam->configure(rcams_[i].cfg.get()) < 0)
			throw std::runtime_error("failed to configure streams");
			
		rcams_[i].stat = RCconfigured;
		
		//the display of camera configuration
		logger_.Log(LogLevel::INFO, "Cfg toString: " + cfg.toString() + "-" +
		libcamera::ColorSpace::toString(cfg.colorSpace) + " cam#: " + 
		std::to_string(i) + " " + camid );
		
		// setup dma memory 
		for (StreamConfiguration &config : *rcams_[i].cfg)
		{
			Stream *stream = config.stream();
			std::vector<std::unique_ptr<FrameBuffer>> fb;

			for (unsigned int ib = 0; ib < config.bufferCount; ib++)
			{
				std::string name("RaceCam_" + std::string(it->key()) + "_" + std::to_string(ib));
				libcamera::UniqueFD fd = dma_heap_.alloc(name.c_str(), config.frameSize);

				if (!fd.isValid())
					throw std::runtime_error("failed to allocate capture buffers for stream");

				std::vector<FrameBuffer::Plane> plane(1);
				plane[0].fd = libcamera::SharedFD(std::move(fd));
				plane[0].offset = 0;
				plane[0].length = config.frameSize;

				fb.push_back(std::make_unique<FrameBuffer>(plane));
				void *memory = mmap(NULL, config.frameSize, PROT_READ | PROT_WRITE, MAP_SHARED, plane[0].fd.get(), 0);
				mapped_buffers_[fb.back().get()].push_back(
						libcamera::Span<uint8_t>(static_cast<uint8_t *>(memory), config.frameSize));
			}
			rcams_[i].fbuf[stream] = std::move(fb);
		}

		// set libcamera controls per input json 
		auto camcntls = rcams_[i].cam->controls();
//		DEBUG_PRINT("%s", "\n");
//		jsonWrite(std::cout, it->value());
//		auto cfgcntls = getCfgValue("/Controls", it->value()).get_object();
		json::value cv = getCfgValue("/Controls", it->value());
		if (!cv.is_null() && cv.is_object()) {
			json::object cfgcntls = cv.as_object();
//		DEBUG_PRINT("%s", "\n"); 
			for(auto cfgit = cfgcntls.begin(); cfgit != cfgcntls.end(); ++cfgit)
			{
				if (cfgit->value().is_null()) continue;
			
				unsigned int cntlnum = getCntlID(cfgit->key());  
				auto cim = camcntls.find(cntlnum);
				ControlValue cv = toCntlVal(cfgit->value(), cim->first);
				if (!cv.isNone()) {
					if (cim != camcntls.end())
				{
						rcams_[i].cntls.set(cntlnum, cv); 
					} else {
						logger_.Log(LogLevel::ERROR, "cntlID not found in cam controls!");
					}
				
				} else {
					logger_.Log(LogLevel::ERROR, std::string("Control ") + std::string(cfgit->key().data(), cfgit->key().size()) + " skipped toCntlVal() failed!");
				}
			} 
		}
	
		i++;
	}
};

void RCam::startCams(const json::object& cams)
{
	DEBUG_PRINT("%s", "\n");
	for(auto it = cams.begin(); it != cams.end(); ++it) {
		unsigned int i = std::distance(cams.begin(), it);
		makeRequests(i);

		if (rcams_[i].cam->start(&rcams_[i].cntls))
			throw std::runtime_error("failed to start camera");

//		rcams_[i].cntls.clear();  //why clear? don't clear and reuse control list for cam restart if stopped WEK check this and see it better to rebuild 
		rcams_[i].stat = RCrunning;
	
		rcams_[i].cam->requestCompleted.connect(this, &RCam::requestComplete);
		
		for (std::unique_ptr<Request> &request : rcams_[i].req)
		{
			if (rcams_[i].cam->queueRequest(request.get()) < 0)
				throw std::runtime_error("Failed to queue request");
		}
	}
	cameras_running_ = true;
};

void RCam::initOutputs(void)
{
	DEBUG_PRINT("%s", "\n");
	avdevice_register_all();

	json::value av = getCfgValue("/Audio");
	if (!av.is_null()) {
		json::object aparms = av.get_object();
		audio_ = true;
		initAudioInCodec(aparms);
	}
	
	json::object jo = getCfgValue("/Outputs").get_object();
    for (auto it = jo.begin(); it != jo.end(); ++it) {
		bool raw = (it->key() == "Raw" ? true : false);
		json::object jo = it->value().get_object();
		const char *format = nullptr;
		auto fv = getCfgValue("/ContainerFromat", jo);
		if (!fv.is_null())
			format = json::value_to<std::string>(fv).c_str();
			
		time_t ts = time({});
		char timeStr[std::size("_yyyy_mm_dd_hh_mm_ss")];
		std::strftime(std::data(timeStr), std::size(timeStr),
                  "_%Y_%m_%d_%H_%M_%S", localtime(&ts));
	
		auto dv = getCfgValue("/Destination", jo);
		std::string dest;
		if (dv.is_null()) {
			throw std::runtime_error("no /Destination found for " + std::string(raw ? "raw" : "composite"));
		}
		if (dv.is_string()) {
			dest = json::value_to<std::string>(dv);	
			dest.append(timeStr, std::size(timeStr)-1);
	//WEK make container type a parameter see below make file ext match /ContainerFromat 
	//TODO gives timebase warning on mov but not mkv
			dest.append(".mkv");
	//TODO Setup an appropriate stream/container format.
		} else {
			if (dv.is_object()) {
				auto tv = getCfgValue("/YouTubeTitle", dv);
				std::string title;
				if (!tv.is_null()) title = json::value_to<std::string>(tv);
				if (title.empty()) {
					throw std::runtime_error("/YouTubeTitle is empty");
				} else {
					title.append(timeStr, std::size(timeStr)-1);
					format = "flv"; 
					auto pv = getCfgValue("/YouTubePrivacy", dv);
					std::string publish = pv.is_null() ? "private" : json::value_to<std::string>(pv);	
					yt_strm s = YouTube(srcpath).StartStrm(title, publish); 
					strmID_ = s.b_id;
					dest = s.strmurl;
				}
			}
		}
       
		AVFormatContext *fmt_ctx;
		avformat_alloc_output_context2(&fmt_ctx, nullptr, format, dest.c_str());
		if (!fmt_ctx)
			throw std::runtime_error("libav: cannot allocate output context, try setting with --libav-format");
		all_fmt_ctx_.push_back(fmt_ctx);
		format_opened_.insert({fmt_ctx, false});
//	out_fmt_ctx_->debug = FF_FDEBUG_TS;

		if (raw) {
			json::array sv = getCfgValue("/Streams", jo).get_array();
			for (auto& value : sv) {
				initVideoStream(fmt_ctx, value);
			}
		} else {
			json::value lv = getCfgValue("/Layers", jo);
			initVideoStream(fmt_ctx, lv);
		}
	
		if (audio_) {
			json::value aparms = getCfgValue("/Audio", jo);
			audio_out_codec_ctxs_.push_back(initAudioOutCodec(fmt_ctx, aparms));
		}		
	}
	
	video_thread_ = std::thread(&RCam::videoThread, this);
	if (audio_) audio_thread_ = std::thread(&RCam::audioThread, this);

}

void RCam::freeOutputs()
{
	DEBUG_PRINT("%s", "\n");

	if (audio_) {
		audio_ = false;
		audio_thread_.join();
	}

	msg_queue_.Post(Msg(MsgType::Quit)); // need to seed a messsge to stop the get waiting
	video_thread_.join();

	for (auto i : all_fmt_ctx_) { 
		if (i->oformat != nullptr) {
			if (outputReady(i)) av_write_trailer(i);
			avio_closep(&i->pb); 
		}
		if (i->iformat != nullptr) {
			avformat_close_input(&i);
		}
		avformat_free_context(i);
	}
	format_opened_.clear();
	all_fmt_ctx_.clear();
    
	for (auto i = codec_to_stream_.cbegin(); i != codec_to_stream_.cend();) {
		AVCodecContext *ptr = i->first; 
		avcodec_free_context(&ptr);
		codec_to_stream_.erase(i++);
	}
	
	for (AVFilterGraph *fg : fgctx_) {
		avfilter_graph_free(&fg);
	}
	fgctx_.clear();
	
	if (!strmID_.empty()) {
		YouTube(srcpath).StopStrm(strmID_); 
		strmID_.clear();
	}
}

AVStream *RCam::addStream(AVFormatContext *fmt_ctx, const AVCodecContext *codec_ctx)
{
	DEBUG_PRINT("%s", "\n");
	AVStream *strm = avformat_new_stream(fmt_ctx, codec_ctx->codec);
	if (!strm)
		throw std::runtime_error("libav: cannot allocate stream for vidout output context");
	if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
	// The avi stream context seems to need the video stream time_base set to
	// 1/framerate to report the correct framerate in the container file.
	//
	// This seems to be a limitation/bug in ffmpeg:
	// https://github.com/FFmpeg/FFmpeg/blob/3141dbb7adf1e2bd5b9ff700312d7732c958b8df/libavformat/avienc.c#L527
		if (!strncmp(fmt_ctx->oformat->name, "avi", 3)) {
		//TODO where to get framerate time base?
		//	strm->time_base = { 1000, (int)(options->framerate_a[cam].value_or(DEF_FRAMERATE) * 1000) };
		}
		else
			strm->time_base = codec_ctx->time_base;
		strm->avg_frame_rate = strm->r_frame_rate = codec_ctx->framerate;
	} else if (codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
			strm->time_base = codec_ctx->time_base;
		} else throw std::runtime_error("libav: Media type is not Video or Audio");
	avcodec_parameters_from_context(strm->codecpar, codec_ctx);
	return strm;
}

void RCam::initAudioInCodec(const json::object& aparms)
{
	DEBUG_PRINT("%s", "\n");
	json::value val;
	val = getCfgValue("/Source", aparms);
	//WEK bug why alsa does not work
//	std::string audio_source = val.is_null() ? "alsa" : json::value_to<std::string>(val);	//to short
	std::string audio_source = val.is_null() ? "pulse" : json::value_to<std::string>(val); //works
	
	AVInputFormat *input_fmt = (AVInputFormat *)av_find_input_format(audio_source.c_str());

	assert(audio_in_fmt_ctx_ == nullptr);

	int ret;
	AVDictionary *format_opts = nullptr;
	
	val = getCfgValue("/Channels", aparms);
	int audio_channels = val.is_null() ? 0 : json::value_to<int>(val);
	
	if (audio_channels != 0)
		ret = av_dict_set_int(&format_opts, "channels", audio_channels, 0);

	val = getCfgValue("/Device", aparms);
	std::string audio_device = val.is_null() ? "plughw:RacecamMic" : json::value_to<std::string>(val);
	
	ret = avformat_open_input(&audio_in_fmt_ctx_, audio_device.c_str(), input_fmt, &format_opts);
	if (ret < 0) {
		av_dict_free(&format_opts);
		throw std::runtime_error("libav: cannot open " + audio_source + " input device " + audio_device);
	}
	all_fmt_ctx_.push_back(audio_in_fmt_ctx_);

	av_dict_free(&format_opts);

	avformat_find_stream_info(audio_in_fmt_ctx_, nullptr);

	AVStream *strmAudioIn = nullptr;
	for (unsigned int i = 0; i < audio_in_fmt_ctx_->nb_streams; i++) {
		if (audio_in_fmt_ctx_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
			strmAudioIn = audio_in_fmt_ctx_->streams[i];
			break;
		}
	}

	if (!strmAudioIn)
		throw std::runtime_error("libav: couldn't find a audio stream.");

	const AVCodec *codec_in = avcodec_find_decoder(strmAudioIn->codecpar->codec_id);
	audio_in_codec_ctx_ = avcodec_alloc_context3(codec_in);

	avcodec_parameters_to_context(audio_in_codec_ctx_, strmAudioIn->codecpar);
	// usec timebase
	audio_in_codec_ctx_->time_base = { 1, 1000 * 1000 };

	ret = avcodec_open2(audio_in_codec_ctx_, codec_in, nullptr);
	if (ret < 0)
		throw std::runtime_error("libav: unable to open audio in codec: " + std::to_string(ret));
}

AVCodecContext *RCam::initAudioOutCodec(AVFormatContext *fmt_ctx, const json::value& aparms)
{
	DEBUG_PRINT("%s", "\n");
	json::value val;
	val = getCfgValue("/CodecOut", aparms);
	std::string audio_codec= val.is_null() ? "aac" : json::value_to<std::string>(val);
	
	const AVCodec *codec_out = avcodec_find_encoder_by_name(audio_codec.c_str());
	if (!codec_out)
		throw std::runtime_error("libav: cannot find audio encoder " + audio_codec);

	auto out_codec_ctx = avcodec_alloc_context3(codec_out);
	if (!out_codec_ctx)
		throw std::runtime_error("libav: cannot allocate audio in context");
		
	AVStream *strmAudioIn = nullptr;
	for (unsigned int i = 0; i < audio_in_fmt_ctx_->nb_streams; i++) {
		if (audio_in_fmt_ctx_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
			strmAudioIn = audio_in_fmt_ctx_->streams[i];
			break;
		}
	}

	if (!strmAudioIn)
		throw std::runtime_error("libav: couldn't find a audio stream.");

	av_channel_layout_default(&out_codec_ctx->ch_layout, strmAudioIn->codecpar->ch_layout.nb_channels);

	val = getCfgValue("/SampleRate", aparms);
	unsigned int audio_samplerate = val.is_null() ? 0 : json::value_to<unsigned int>(val);
	
	out_codec_ctx->sample_rate = audio_samplerate ? audio_samplerate : strmAudioIn->codecpar->sample_rate;
	
//	out_codec_ctx->sample_fmt = codec_out->sample_fmts[0];
#if LIBAVCODEC_VERSION_MAJOR < 61
	out_codec_ctx->sample_fmt = codec_out->sample_fmts[0];
#else
	const enum AVSampleFormat *sample_fmts = nullptr;
	avcodec_get_supported_config(out_codec_ctx, codec_out, AV_CODEC_CONFIG_SAMPLE_FORMAT, 0,
								 (const void **)&sample_fmts, nullptr);
	if (!sample_fmts)
		throw std::runtime_error("libav: no supported sample formats for audio codec");
	else
		out_codec_ctx->sample_fmt = sample_fmts[0];
#endif
	
	val = getCfgValue("/BitsPerSecond", aparms);
	int audio_bitrate = val.is_null() ? 32000 : json::value_to<int>(val);
	
	out_codec_ctx->bit_rate = audio_bitrate;
	// usec timebase
	out_codec_ctx->time_base = { 1, 1000 * 1000 };
	
	if (fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) out_codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	
	int ret = avcodec_open2(out_codec_ctx, codec_out, nullptr);
	if (ret < 0)
		throw std::runtime_error("libav: unable to open audio codec: " + std::to_string(ret));
	
	AVStream *strm = addStream(fmt_ctx, out_codec_ctx);
	
	codec_to_stream_.insert({out_codec_ctx, {fmt_ctx, strm}});
	 
	return out_codec_ctx;
}

void RCam::stopCams(const json::object& cams)
{
	DEBUG_PRINT("%s", "\n");
	for(auto it = cams.begin(); it != cams.end(); ++it) {
		unsigned int i = std::distance(cams.begin(), it);
		{
			// don't want QueueRequest to run asynchronously while we stop the camera.
			std::lock_guard<std::mutex> lock(camera_stop_mutex_);
			if (rcams_[i].stat == RCrunning) {
				rcams_[i].stat = RCstopping;
				if (rcams_[i].cam->stop())
					throw std::runtime_error("failed to stop camera");
			}
		}
		
		cameras_running_ = false;

		if (rcams_[i].cam)
			rcams_[i].cam->requestCompleted.disconnect(this, &RCam::requestComplete);
		
		bool reqdone = true;
		for (auto it = rcams_[i].req.begin(); it != rcams_[i].req.end(); ++it) {
			if (it->get()->status() != Request::Status::RequestCancelled)
				reqdone = false;
		}
		if (reqdone) rcams_[i].stat = RCconfigured;

		
		// An application might be holding a CompletedRequest, so queueRequest will get
		// called to delete it later, but we need to know not to try and re-queue it.
//		completed_requests_.clear();
		msg_queue_.Clear();
		
		rcams_[i].req.clear();
	}
};

void RCam::freeCams(const json::object& cams)
{
	DEBUG_PRINT("%s", "\n");
	for(auto it = cams.begin(); it != cams.end(); ++it) {
		unsigned int i = std::distance(cams.begin(), it);
		if (rcams_[i].stat != RCavailable)
			rcams_[i].cam->release();
		rcams_[i].stat = RCavailable;

		rcams_[i].cntls.clear();
		rcams_[i].cam.reset();
	}
};

void RCam::makeRequests(int cam)
{
	DEBUG_PRINT("%s", "\n");
	std::map<Stream *, std::queue<FrameBuffer *>> free_buffers;
	
  	for (auto &kv : rcams_[cam].fbuf) {
		free_buffers[kv.first] = {};
		for (auto &b : kv.second)
			free_buffers[kv.first].push(b.get());
	}

	while (true) {
		for (StreamConfiguration &config : *rcams_[cam].cfg) {
			Stream *stream = config.stream();
			//WEK dual stream -- make request for all streams
			if (stream == rcams_[cam].cfg->at(0).stream()) {
				if (free_buffers[stream].empty()) {
					return;
				}
				std::unique_ptr<Request> request = rcams_[cam].cam->createRequest(cam);
				if (!request)
					throw std::runtime_error("failed to make request");
				rcams_[cam].req.push_back(std::move(request));
			}
			else if (free_buffers[stream].empty())
				throw std::runtime_error("concurrent streams need matching numbers of buffers");

			FrameBuffer *buffer = free_buffers[stream].front();
			free_buffers[stream].pop();
			if (rcams_[cam].req.back()->addBuffer(stream, buffer) < 0)
				throw std::runtime_error("failed to add buffer to request");
		}
	}
};

void RCam::requestComplete(Request *request)
{
	DEBUG_PRINT("%s", "\n");
	if (request->status() == Request::RequestCancelled) {
		// If the request is cancelled while the camera is still running, it indicates
		// a hardware timeout. Let the application handle this error.
		if (rcams_[request->cookie()].stat == RCrunning) {
			msg_queue_.Post(Msg(MsgType::Timeout));
		}
		return;
	}

	struct dma_buf_sync dma_sync {};
	dma_sync.flags = DMA_BUF_SYNC_START | DMA_BUF_SYNC_READ;
	for (auto const &buffer_map : request->buffers()) {
		auto it = mapped_buffers_.find(buffer_map.second);
		if (it == mapped_buffers_.end())
			throw std::runtime_error("failed to identify request complete buffer");

		int ret = ::ioctl(buffer_map.second->planes()[0].fd.get(), DMA_BUF_IOCTL_SYNC, &dma_sync);
		if (ret)
			throw std::runtime_error("failed to sync dma buf on request complete");
	}

	CompletedRequest *r = new CompletedRequest(request);
	CompletedRequestPtr payload(r, [this](CompletedRequest *cr) { this->queueRequest(cr); });

	msg_queue_.Post(Msg(MsgType::RequestComplete, std::move(payload)));
}

void RCam::videoThread()
{
	DEBUG_PRINT("%s", "\n");
	
	AVFrame *frame = av_frame_alloc();
	if (!frame)
		throw std::runtime_error("libav: could not allocate AVFrame");
		
//	int fcnt = 0;
//	int64_t lpts = 0;

	while (!stop_threads_) {
		Msg msg = msg_queue_.Get();
		if (msg.type == MsgType::Timeout) {
			logger_.Log(LogLevel::WARN, "Device timeout detected, attempting a restart!!!");
			continue;
		}
		if (msg.type == MsgType::Quit) {
			stop_threads_ = true;
			continue;
		}
		else if (msg.type != MsgType::RequestComplete)
			throw std::runtime_error("unrecognised message!");
		
		CompletedRequestPtr &cr = std::get<CompletedRequestPtr>(msg.payload);
		//WEK dual stream loop thru all streams
		Stream *stream = rcams_[cr->request->cookie()].cfg->at(0).stream();
		StreamConfiguration const &cfg = stream->configuration();
		FrameBuffer *buffer = cr->buffers[stream];

		auto it = mapped_buffers_.find(buffer);
		if (it == mapped_buffers_.end())
			throw std::runtime_error("failed to find buffer in mapped_buffers_");

		auto planes = it->second;

		libcamera::Span span = planes[0];
	
		void *mem = span.data();
		if (!buffer || !mem)
			throw std::runtime_error("no buffer to encode");

		auto ts = cr->metadata.get(controls::SensorTimestamp);
		
		uint64_t timestamp_ns = ts ? *ts : buffer->metadata().timestamp;
//		std::cout << std::to_string(timestamp_ns) << std::endl;
		if (!video_start_ts_)
			video_start_ts_ = timestamp_ns / 1000;
		//WEK dual stream should opaque be CameraStream
		frame->opaque = (void *) &cam_fgctx_[cr->request->cookie()];
		
		// to ffmpeg frame and repixel if needed
		
		if (cfg.pixelFormat == libcamera::formats::YUV420) {
			frame->format = AV_PIX_FMT_YUV420P;
			frame->linesize[0] = cfg.stride; 
			frame->linesize[1] = frame->linesize[2] = cfg.stride >> 1;
		} else if (cfg.pixelFormat == libcamera::formats::YUYV) {
			frame->format = AV_PIX_FMT_YUYV422;
			frame->linesize[0] = cfg.stride;
			frame->linesize[1] = frame->linesize[2] = 0;
		} else {
			throw std::runtime_error("unhandled libcamera pixel format!");
		}
			
//		frame->format = AV_PIX_FMT_YUV420P;  //TODO use lookup func	
		frame->width = cfg.size.width;
		frame->height = cfg.size.height;
		
		
//		frame->linesize[0] = cfg.stride; //TODO use av_image_fill_linesize()??	
//		frame->linesize[1] = frame->linesize[2] = cfg.stride >> 1;
		


//WEK fill_linesize does not return alinged size for CSI cameras
//		av_image_fill_linesizes(frame->linesize, AV_PIX_FMT_YUV420P, cfg.size.width);

		frame->pts = (timestamp_ns / 1000) - video_start_ts_;
		
//		std::cout << 
//			frame->linesize[0] << " " << frame->linesize[1] << 
//			" " << frame->linesize[2] << 
//			" " << frame->pts <<
//			" " << frame->pts - lpts / 1000 <<
//			" " << fcnt++ << std::endl;
//		lpts = frame->pts;
//		fcnt++;
		
	//TODO use av_buffer_default_free() if RCam::releaseBuffer not needed 
		frame->buf[0] = av_buffer_create((uint8_t *)mem, span.size(), &av_buffer_default_free, NULL, 0);
		assert(frame->buf[0]);
		av_image_fill_pointers(frame->data, AV_PIX_FMT_YUV420P, frame->height, frame->buf[0]->data, frame->linesize); //TODO use lookup func
		av_frame_make_writable(frame);
		
		// done frame to ffmpeg
		//WEK dual stream should opaque be CameraStream
		for (AVFilterContext *fc : cam_fgctx_[cr->request->cookie()]) {
			filterFrame(fc, frame);
		}

	}

//	std::cout << "Frame count: " << fcnt << std::endl;
	
	frame->buf[0] = NULL;
	av_frame_unref(frame);
	av_frame_free(&frame);

// flush any valid encoder
	for (const auto& c : codec_to_stream_)
		if (c.first->codec_type == AVMEDIA_TYPE_VIDEO) encodeFrame(c.first, nullptr);
}

void RCam::audioThread() 
{
	DEBUG_PRINT("%s", "\n");
	// WEK fail if logger_.Log used here but works anywhere after swr_alloc_set_opts2??????
	const AVSampleFormat required_fmt = audio_out_codec_ctxs_.front()->sample_fmt;
	int ret;

	uint32_t out_channels = audio_out_codec_ctxs_.front()->ch_layout.nb_channels;

	SwrContext *conv;
	AVAudioFifo *fifo;

	ret = swr_alloc_set_opts2(&conv, &audio_out_codec_ctxs_.front()->ch_layout, required_fmt,
		audio_out_codec_ctxs_.front()->sample_rate, &audio_in_codec_ctx_->ch_layout,
		audio_in_codec_ctx_->sample_fmt, audio_in_codec_ctx_->sample_rate, 0, nullptr);

	if (ret < 0)
		throw std::runtime_error("libav: cannot create swr context");

	// 2 seconds FIFO buffer
	fifo = av_audio_fifo_alloc(required_fmt, audio_out_codec_ctxs_.front()->ch_layout.nb_channels,
		audio_out_codec_ctxs_.front()->sample_rate * 2);

	swr_init(conv);

	AVPacket *in_pkt = av_packet_alloc();
	AVPacket *out_pkt = av_packet_alloc();
	AVFrame *in_frame = av_frame_alloc();
	uint8_t **samples = nullptr;
	int sample_linesize = 0;
	
	int max_output_samples = av_rescale_rnd(audio_out_codec_ctxs_.front()->frame_size,
		audio_out_codec_ctxs_.front()->sample_rate, audio_in_codec_ctx_->sample_rate, AV_ROUND_UP);
	ret = av_samples_alloc_array_and_samples(&samples, &sample_linesize, out_channels, 
		max_output_samples, required_fmt, 0);

	if (ret < 0)
		throw std::runtime_error("libav: failed to alloc sample array");
	
	std::chrono::steady_clock::time_point syncstart;

	while (!stop_threads_) {
		// Audio In
		ret = av_read_frame(audio_in_fmt_ctx_, in_pkt);
		if (ret < 0)
			throw std::runtime_error("libav: cannot read audio in frame");

		ret = avcodec_send_packet(audio_in_codec_ctx_, in_pkt);
		if (ret < 0)
			throw std::runtime_error("libav: cannot send pkt for decoding audio in");

		ret = avcodec_receive_frame(audio_in_codec_ctx_, in_frame);
		if (ret && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
			throw std::runtime_error("libav: error getting decoded audio in frame");

		// Audio Resample/Conversion
		int num_output_samples = av_rescale_rnd(swr_get_delay(conv, 
			audio_in_codec_ctx_->sample_rate) + in_frame->nb_samples, 
			audio_out_codec_ctxs_.front()->sample_rate, 
			audio_in_codec_ctx_->sample_rate, AV_ROUND_UP);
		
		if (num_output_samples > max_output_samples) {
			av_freep(&samples[0]);
			max_output_samples = num_output_samples;
			ret = av_samples_alloc_array_and_samples(&samples, &sample_linesize, out_channels, max_output_samples,
													 required_fmt, 0);
			if (ret < 0)
				throw std::runtime_error("libav: failed to alloc sample array");
		}

		ret = swr_convert(conv, samples, num_output_samples, (const uint8_t **)in_frame->extended_data,
						  in_frame->nb_samples);
		if (ret < 0)
			throw std::runtime_error("libav: swr_convert failed");

//		if (!cameras_running_ && !abort_audio_) av_audio_fifo_reset(fifo);
//TODO might need only one ref of stop_thread_ per function / is stop_thread_ even needed??
// how to handle mutli cam running flag
// replace cameras_running with a func that returns bool // true if any running and false of all stopped
//		if (!cameras_running_ && !stop_threads_) 
//			av_audio_fifo_reset(fifo); 
		if (!cameras_running_ && !stop_threads_) 
			syncstart = std::chrono::steady_clock::now();
		constexpr std::chrono::milliseconds delay{250}; 
		if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - syncstart) < delay)
			av_audio_fifo_reset(fifo);

		if (av_audio_fifo_space(fifo) < num_output_samples)
			av_audio_fifo_drain(fifo, num_output_samples);

		av_audio_fifo_write(fifo, (void **)samples, num_output_samples);

		av_frame_unref(in_frame);
		av_packet_unref(in_pkt);

		// Not yet ready to generate encoded audio!
		if (!output_ready_) {
			continue;
		}

		// Audio Out
		while (av_audio_fifo_size(fifo) >= audio_out_codec_ctxs_.front()->frame_size) {
			AVFrame *out_frame = av_frame_alloc();
			out_frame->nb_samples = audio_out_codec_ctxs_.front()->frame_size;

			av_channel_layout_copy(&out_frame->ch_layout, &audio_out_codec_ctxs_.front()->ch_layout);

			out_frame->format = required_fmt;
			out_frame->sample_rate = audio_out_codec_ctxs_.front()->sample_rate;

			av_frame_get_buffer(out_frame, 0);
			av_audio_fifo_read(fifo, (void **)out_frame->data, audio_out_codec_ctxs_.front()->frame_size);

			AVRational num = { 1, out_frame->sample_rate };
			int64_t ts = av_rescale_q(audio_samples_, num, audio_out_codec_ctxs_.front()->time_base);

			out_frame->pts = ts;

			audio_samples_ += audio_out_codec_ctxs_.front()->frame_size;
		
			for ( auto cctx : audio_out_codec_ctxs_) {
				encodeFrame(cctx, out_frame);
			}
			av_frame_free(&out_frame);
		}
	}

	// Flush the encoder
	for ( auto cctx : audio_out_codec_ctxs_) {
		encodeFrame(cctx, nullptr);
	}

	swr_free(&conv);
	av_freep(&samples[0]);
	av_audio_fifo_free(fifo);

	av_packet_free(&in_pkt);
	av_packet_free(&out_pkt);
	av_frame_free(&in_frame);
}

void RCam::encodeFrame(AVCodecContext *codec_ctx, AVFrame *frame)
{
	DEBUG_PRINT("%s", "\n");
	assert(codec_ctx);
	int ret = avcodec_send_frame(codec_ctx, frame);

	if (ret < 0)
		throw std::runtime_error("libav: error encoding frame: " + std::to_string(ret));
		
	AVPacket *pkt = av_packet_alloc();
	
	ret = 0;
	while (ret >= 0) {
		ret = avcodec_receive_packet(codec_ctx, pkt);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
			av_packet_unref(pkt);
			break;
		}
		else if (ret < 0)
			throw std::runtime_error("libav: error receiving packet: " + std::to_string(ret));

		// Initialise the ouput mux on the first received video packet, as we may need
		// to copy global header data from the encoder.
		auto c = codec_to_stream_.find(codec_ctx);
		if (c == codec_to_stream_.end()) 
			throw std::runtime_error("encodeFrame(AVCodecContext *, AVFrame *) codec_to_stream_ not found!");
		AVFormatContext *fmt_ctx = c->second.fmt_ctx;
		AVStream *strm = c->second.strm;
	
		if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO && !outputReady(codec_ctx)) {
			openFormat(codec_ctx);
			output_ready_ = allFormatsOpen();
		}
		
		pkt->pos = -1;
		pkt->duration = 0;
		pkt->stream_index = strm->index;
		av_packet_rescale_ts(pkt, codec_ctx->time_base,  
			strm->time_base); 
					
		{std::scoped_lock<std::mutex> lock(output_mutex_);
		// cp_pkt is now blank (av_interleaved_write_frame() takes ownership of
		// its contents and resets pkt), so that no unreferencing is necessary.
		// This would be different if one used av_write_frame().
		ret = av_interleaved_write_frame(fmt_ctx, pkt);}
		if (ret < 0) {
			char err[AV_ERROR_MAX_STRING_SIZE];
			av_strerror(ret, err, sizeof(err));
			throw std::runtime_error("libav: error writing output: " + std::string(err));
		}  

	} 
	av_packet_free(&pkt); 
}

bool RCam::outputReady(AVFormatContext *fctx)
{
	DEBUG_PRINT("%s", "\n");
	auto f = format_opened_.find(fctx);
	if (f == format_opened_.end()) 
		throw std::runtime_error("outputReady(AVFormatContext *) format_opened_ not found!");
		
	return f->second;
} 

bool RCam::outputReady(AVCodecContext *cctx)
{
	DEBUG_PRINT("%s", "\n");
	auto c = codec_to_stream_.find(cctx);
	if (c == codec_to_stream_.end()) 
		throw std::runtime_error("outputReady(AVCodecContext *) codec_to_stream_ not found!");
		
	return outputReady(c->second.fmt_ctx);
	//TODO add lookup format open??
} 

void RCam::openFormat(AVCodecContext *codec_ctx) 
{
	DEBUG_PRINT("%s", "\n");
	int ret;

	auto st = codec_to_stream_.find(codec_ctx);
	if (st == codec_to_stream_.end())
		throw std::runtime_error("openFormat(AVCodecContext **) codec_to_stream_ not found!");
	CodecStream cs = st->second;
	
	avcodec_parameters_from_context(cs.strm->codecpar, codec_ctx);

	char err[64];
	std::string url = cs.fmt_ctx->url;
	ret = avio_open2(&cs.fmt_ctx->pb, cs.fmt_ctx->url, AVIO_FLAG_WRITE, nullptr, nullptr);
	if (ret < 0) {
		av_strerror(ret, err, sizeof(err));
		throw std::runtime_error("libav: unable to open output mux for " + url + ": " + err);
	}

	ret = avformat_write_header(cs.fmt_ctx, nullptr);
	if (ret < 0) {
		av_strerror(ret, err, sizeof(err));
		throw std::runtime_error("libav: unable write output mux header for " + url + ": " + err);
	}

	auto fo = format_opened_.find(cs.fmt_ctx);
	if (fo == format_opened_.end())
		throw std::runtime_error("openFormat(AVCodecContext **) format_opened_ not found!");
	fo->second = true;	
}

void RCam::queueRequest(CompletedRequest *completed_request)
{
	DEBUG_PRINT("%s", "\n");
	BufferMap buffers(std::move(completed_request->buffers));

	// This function may run asynchronously so needs protection from the
	// camera stopping at the same time.
	std::lock_guard<std::mutex> stop_lock(camera_stop_mutex_);

	Request *request = completed_request->request;
	delete completed_request;
	assert(request);

	if (rcams_[request->cookie()].stat != RCrunning) return;
	
	for (auto const &p : buffers) {
		struct dma_buf_sync dma_sync {};
		dma_sync.flags = DMA_BUF_SYNC_END | DMA_BUF_SYNC_READ;

		auto it = mapped_buffers_.find(p.second);
		if (it == mapped_buffers_.end())
			throw std::runtime_error("failed to identify queue request buffer");

		int ret = ::ioctl(p.second->planes()[0].fd.get(), DMA_BUF_IOCTL_SYNC, &dma_sync);
		if (ret)
			throw std::runtime_error("failed to sync dma buf on queue request");

		if (request->addBuffer(p.first, p.second) < 0)
			throw std::runtime_error("failed to add buffer to request in QueueRequest");
	}
	{
		std::lock_guard<std::mutex> lock(control_mutex_);
		request->controls() = std::move(rcams_[request->cookie()].cntls);
	}

	if (rcams_[request->cookie()].cam->queueRequest(request) < 0)
		throw std::runtime_error("failed to queue request");
}

void RCam::initVideoStream(AVFormatContext *fmt_ctx, json::value& parms)
{
	DEBUG_PRINT("%s", "\n");
	char args[512];
	int ret = 0;
	AVFilterContext *fgctxSrc = nullptr; //always Source at that point in time
	AVFilterContext *fgctxDst = nullptr; //always Destionation at that point in time
	AVFilterContext *fgctxTmp = nullptr; //the saved end of filter chain at end of each loop so overlay if used can have it for main and source for overlay

	AVFilterGraph *fg = avfilter_graph_alloc();
	std::vector<AVFilterContext *> fctxin {};
	StreamInfo rsi {};
	std::string name;
	bool raw {false};
	
	int layer = 0;
	json::array stream_def {};

	if (parms.is_object()) {
		raw = true;
		stream_def.push_back(parms);
	} else {
		if (!parms.is_array()) {
			throw std::runtime_error("initStream() parms not array or object!");
			}
		stream_def = parms.get_array();
	}
	
	std::string sstr = raw ? "_s" + std::to_string(layer) : "_l" + std::to_string(layer);
	std::string lstr = raw ? " stream: " + std::to_string(layer) : " layer: " + std::to_string(layer);

	if (stream_def.empty()) 
		throw std::runtime_error("initStream() parms is empty!");

	
	for (auto it = stream_def.begin();it < stream_def.end();it++, layer++) {
		json::value cam = getCfgValue("/Source", *it);
		if (cam.is_null()) 
//			throw std::runtime_error("Source is not found for layer " + std::to_string(layer) + "!");  //here
			throw std::runtime_error("Source is not found for layer " + lstr + "!");  

		//WEK dual stream -- use both cam and stream
		StreamConfiguration const &streamcfg = rcams_[json::value_to<int>(cam)].cfg->at(0); 
		StreamInfo si = getStreamInfo(streamcfg, json::value_to<int>(cam));
		if (!layer) rsi = si;
		snprintf(args, sizeof(args), "video_size=%dx%d:pix_fmt=%d:time_base=1/1000000:pixel_aspect=0/1", 
			si.w, si.h, si.pix); //TODO make pixel converter
//		name = "in" + std::to_string(layer);  //here
		name = "in" + sstr;  
		ret = avfilter_graph_create_filter(&fgctxSrc, avfilter_get_by_name("buffer"), name.c_str(), args, NULL, fg);
//		if (ret < 0) throw std::runtime_error("libav: untable to create buffer for layer " + std::to_string(layer) + "!");  //WEK fix message so layer/stream
		if (ret < 0) throw std::runtime_error("libav: untable to create buffer for" + lstr + "!"); 
		
		//WEK dual stream CameraStream
		cam_fgctx_[json::value_to<int>(cam)].push_back(fgctxSrc);
		fctxin.push_back(fgctxSrc);
		
		//start
		if (streamcfg.pixelFormat != libcamera::formats::YUV420) {
			snprintf(args, sizeof(args), "pix_fmts=%s", av_get_pix_fmt_name(AV_PIX_FMT_YUV420P));
	//		std::string arg {"format=YUV420"};
//			name =  "format" + std::to_string(layer);  //here
			name =  "format" + sstr;  
			ret = avfilter_graph_create_filter(&fgctxDst, avfilter_get_by_name("format"), name.c_str(), args, NULL, fg);
	//		if (ret < 0) throw std::runtime_error("libav: unable to create format for layer " + std::to_string(layer) + "!");  //here
			if (ret < 0) throw std::runtime_error("libav: unable to create format for" +lstr + "!");  
			ret = avfilter_link(fgctxSrc, 0, fgctxDst, 0);
	//		if (ret < 0) throw std::runtime_error("libav: unable to link in to format for layer " + std::to_string(layer) + "!");  //here
			if (ret < 0) throw std::runtime_error("libav: unable to link in to format for" + lstr + "!");  
			fgctxSrc = fgctxDst; 
			rsi.pix = AV_PIX_FMT_YUV420P;		
		}
		//end

		json::value fp = getCfgValue("/Crop", *it);
		if (!fp.is_null()) {
			Rectangle r{};
			if (fromArray(r, fp)) {
				snprintf(args, sizeof(args), "%d:%d:%d:%d", r.width, r.height, r.x, r.y);
				if (!layer) {
					rsi.w = r.width;
					rsi.h = r.height;
				}
	//			name =  "crop" + std::to_string(layer); //here 
				name =  "crop" + sstr; 
				ret = avfilter_graph_create_filter(&fgctxDst, avfilter_get_by_name("crop"), name.c_str(), args, NULL, fg);
	//			if (ret < 0) throw std::runtime_error("libav: unable to create crop for layer " + std::to_string(layer) + "!"); //here
				if (ret < 0) throw std::runtime_error("libav: unable to create crop for" + lstr + "!"); 
				ret = avfilter_link(fgctxSrc, 0, fgctxDst, 0);
	//			if (ret < 0) throw std::runtime_error("libav: unable to link in to crop for layer " + std::to_string(layer) + "!"); //here
				if (ret < 0) throw std::runtime_error("libav: unable to link in to crop for" + lstr + "!"); 
				fgctxSrc = fgctxDst; 
			}
		}
		fp = getCfgValue("/Scale", *it);
		if (!fp.is_null()) {
			Size s{}; 
			if (fromArray(s, fp)) {
				snprintf(args, sizeof(args), "%d:%d", s.width, s.height);
				if (!layer) {
					rsi.w = s.width;
					rsi.h = s.height;
				}
	//			name = "scale" +  std::to_string(layer); //here
				name = "scale" +  sstr; 
				ret = avfilter_graph_create_filter(&fgctxDst, avfilter_get_by_name("scale"), name.c_str(), args, NULL, fg);
		//		if (ret < 0) throw std::runtime_error("libav: unable to create scaler for layer " + std::to_string(layer) + "!"); //here
				if (ret < 0) throw std::runtime_error("libav: unable to create scaler for" + lstr + "!"); 
				ret = avfilter_link(fgctxSrc, 0, fgctxDst, 0);
	//			if (ret < 0) throw std::runtime_error("libav: unable to link in to scaler for layer " + std::to_string(layer) + "!"); //here
				if (ret < 0) throw std::runtime_error("libav: unable to link in to scaler for" + lstr + "!"); 
				fgctxSrc = fgctxDst; 
			}
		}
		fp = getCfgValue("/Overlay", *it);
		if (!fp.is_null()) {
			if (!layer) {
				logger_.Log(LogLevel::ERROR, "overlay invalid on layer 0!");
			} else { 
				Point p{};
				if (fromArray(p, fp)) {
					snprintf(args, sizeof(args), "x=%d:y=%d:eval=init", p.x, p.y);
		//			name = "overlay" + std::to_string(layer);   //here
					name = "overlay" + sstr;
					ret = avfilter_graph_create_filter(&fgctxDst, avfilter_get_by_name("overlay"), name.c_str(), args, NULL, fg);
		//			if (ret < 0) throw std::runtime_error("libav: unable to create overlay for layer " + std::to_string(layer) + "!"); //here
					if (ret < 0) throw std::runtime_error("libav: unable to create overlay for" + lstr + "!"); 
					ret = avfilter_link(fgctxTmp, 0, fgctxDst, 0);
		//			if (ret < 0) throw std::runtime_error("libav: unable to link in to main for layer " + std::to_string(layer) + "!"); //here
					if (ret < 0) throw std::runtime_error("libav: unable to link in to main for" + lstr + "!"); 
					ret = avfilter_link(fgctxSrc, 0, fgctxDst, 1);
		//			if (ret < 0) throw std::runtime_error("libav: unable to link in to overlay for layer " + std::to_string(layer) + "!"); //here
					if (ret < 0) throw std::runtime_error("libav: unable to link in to overlay for" + lstr + "!"); 
					fgctxSrc = fgctxDst; 
				}
			} 
		}
		fgctxTmp = fgctxSrc;
	}			

	ret = avfilter_graph_create_filter(&fgctxDst, avfilter_get_by_name("buffersink"), "sink", NULL, NULL, fg);
	if (ret < 0) throw std::runtime_error("libav: untable to create sink buffer");

	enum AVPixelFormat pf_enum[] = {AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE};  //TODO use lookup func?
	ret = av_opt_set_int_list(fgctxDst, "pix_fmts", pf_enum, AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
	if (ret < 0) throw std::runtime_error("libav: untable to set sink format list"); 
	ret = avfilter_link(fgctxSrc, 0, fgctxDst, 0);
    if (ret < 0) throw std::runtime_error("libav: link overlay filter to sink buffer failed!");
    ret = avfilter_graph_config(fg, NULL);
    if (ret < 0) throw std::runtime_error("libav: configure filter graph failed!");
    fgctx_.push_back(fg);
 
	json::value vcv = getCfgValue("/VideoCodec", parms);
	std::string codec_name = (vcv.is_null() ? (raw ? "yuv4" : "libx264") : json::value_to<std::string>(vcv));
	
	//TODO make setting type unique (and for h264 normal and low latancy)
	const AVCodec *codec = avcodec_find_encoder_by_name(codec_name.c_str());
	if (!codec)
		throw std::runtime_error("libav: cannot find video encoder " + codec_name);
		
	auto codec_ctx = avcodec_alloc_context3(codec);
	
	if (!codec_ctx)
		throw std::runtime_error("libav: Cannot allocate video context");

	codec_ctx->width = rsi.w;
	codec_ctx->height = rsi.h;
	// usec timebase
	codec_ctx->time_base = { 1, 1000 * 1000 };
	codec_ctx->sw_pix_fmt = rsi.pix;  // WEK unused can be removed?
	codec_ctx->pix_fmt = rsi.pix;


	codec_ctx->color_primaries = rsi.cp;
	codec_ctx->color_trc = rsi.ct;
	codec_ctx->colorspace = rsi.cs;
	codec_ctx->color_range = rsi.cr;
	
	auto bv = getCfgValue("/BitStream", parms);
	if (!bv.is_null()) {
		int64_t br = json::value_to<int64_t>(bv);
		codec_ctx->rc_max_rate = br;
	}

	//std::optional<float> framerate_a[MAX_CAMS]; from opts
//	codec->framerate = { (int)(options->framerate_a[cam].value_or(DEF_FRAMERATE) * 1000), 1000 };
	std::string loc = getLocation(rcams_[rsi.cam].cam);
	json::value val = getCfgValue("/Cameras/" + loc + "/Controls/FrameDurationLimits");
	if (!val.is_null()) {
		if (val.is_array()) {
			if (val.at(0) == val.at(1))
				codec_ctx->framerate = {(1000000 / json::value_to<int>(val.at(0))),1000};
			else 
				throw std::runtime_error("FrameDurationLimits not set to fixed FPS!");
		} 
		else
			throw std::runtime_error("FrameDurationLimits not array!");
	} 
	else 
		codec_ctx->framerate = {30, 1000};
	
	if ("libx264" == codec_name) {
//		codec_ctx->max_b_frames = 1;
		codec_ctx->me_range = 16;
		codec_ctx->me_cmp = 1; // No chroma ME
		codec_ctx->me_subpel_quality = 0;
		codec_ctx->thread_count = 0;
		
		auto ll = getCfgValue("/LowLatency", parms);
		if (!ll.is_null() && ll.as_bool()) {
			codec_ctx->thread_type = FF_THREAD_SLICE;
			codec_ctx->slices = 4;
			codec_ctx->refs = 1;
			av_opt_set(codec_ctx->priv_data, "preset", "ultrafast", 0);
			av_opt_set(codec_ctx->priv_data, "tune", "zerolatency", 0);
		} else {
			codec_ctx->thread_type = FF_THREAD_FRAME;
			codec_ctx->slices = 1;
			codec_ctx->max_b_frames = 1;
			av_opt_set(codec_ctx->priv_data, "preset", "superfast", 0);
			av_opt_set(codec_ctx->priv_data, "partitions", "i8x8,i4x4", 0);
		}
		
		av_opt_set(codec_ctx->priv_data, "weightp", "none", 0);
		av_opt_set(codec_ctx->priv_data, "weightb", "0", 0);
		av_opt_set(codec_ctx->priv_data, "motion-est", "dia", 0);
		av_opt_set(codec_ctx->priv_data, "sc_threshold", "0", 0);
		av_opt_set(codec_ctx->priv_data, "rc-lookahead", "0", 0);
		av_opt_set(codec_ctx->priv_data, "mixed_ref", "0", 0);
	 

		codec_ctx->profile = FF_PROFILE_UNKNOWN;
		auto pv = getCfgValue("/CodecProfile", parms);
		if (!pv.is_null()) {
			std::string profile = json::value_to<std::string>(pv);
			const AVCodecDescriptor *desc = avcodec_descriptor_get(codec_ctx->codec_id);
			for (const AVProfile *cp = desc->profiles; cp && cp->profile != FF_PROFILE_UNKNOWN; cp++) {
				if (!strncasecmp(profile.c_str(), cp->name, profile.size())) {
					codec_ctx->profile = cp->profile;
					break;
				}
			}
			if (codec_ctx->profile == FF_PROFILE_UNKNOWN)
				throw std::runtime_error("libav: no such profile " + profile);
		} 
	
	//std::string level; from opts
/*	codec->level = options->level.empty() ? FF_LEVEL_UNKNOWN : std::stof(options->level) * 10;
	codec->level = options->level_a[cam].value_or(codec->level); */
		json::value pl = getCfgValue("ProfileLevel", parms);
		if (pl.is_null())
			codec_ctx->level =	FF_LEVEL_UNKNOWN;
		else
			codec_ctx->level = json::value_to<int>(pl);


//unsigned int intra; from opts	
//	codec->gop_size = options->intra ? options->intra : (int)(options->framerate_a[cam].value_or(DEF_FRAMERATE));
		json::value gop = getCfgValue("GOPSize", parms);
		if (!gop.is_null())
			codec_ctx->gop_size = json::value_to<int>(gop);

//Bitrate bitrate; from opts 
/*	if (options->bitrate)
		codec->bit_rate = options->bitrate.bps(); */
	
		json::value br = getCfgValue("BitRate", parms);
		if (!br.is_null())
			codec_ctx->bit_rate = json::value_to<int64_t>(br);
	
//TODO have generic encoder options???
//std::string libav_video_codec_opts;
/*	if (!options->libav_video_codec_opts.empty())
	{
		const std::string &opts = options->libav_video_codec_opts;
		for (std::string::size_type i = 0, n = 0; i != std::string::npos; i = n)
		{
			n = opts.find(';', i);
			const std::string opt = opts.substr(i, n - i);
			if (n != std::string::npos)
				n++;
			if (opt.empty())
				continue;
			std::string::size_type kn = opt.find('=');
			const std::string key = opt.substr(0, kn);
			const std::string value = (kn != std::string::npos) ? opt.substr(kn + 1) : "";
			int ret = av_opt_set(codec, key.c_str(), value.c_str(), AV_OPT_SEARCH_CHILDREN);
			if (ret < 0)
			{
				char err[AV_ERROR_MAX_STRING_SIZE];
				av_strerror(ret, err, sizeof(err));
				throw std::runtime_error("libav: codec option error " + opt + ": " + err);
			}
		}
	} */
	//WEK lossless 27.2 time bigger file and 15% more cpu :( 
/*	if (raw) {
		codec_ctx->qmin = 0;
		codec_ctx->qmax = 0;
	} */
	}
	if (fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
		codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	} 
	
	int rc = avcodec_open2(codec_ctx, codec, nullptr);
	if (rc < 0)
		throw std::runtime_error("libav: unable to open video codec: " + std::to_string(rc));
		
	AVStream *strm = addStream(fmt_ctx, codec_ctx);
		
	codec_to_stream_.insert({codec_ctx, {fmt_ctx, strm}});
	for (AVFilterContext *fctx : fctxin) {
		filter_codec_ctx_.insert({fctx, {fgctxDst, codec_ctx}});
    }	
} 

//TODO take only cam number
StreamInfo RCam::getStreamInfo(const StreamConfiguration& cfg, const int camera)
{
	DEBUG_PRINT("%s", "\n");
	std::cout << "Stream config: " << cfg.toString() << 
	" " << cfg.colorSpace->toString() << std::endl;
	
	StreamInfo si;
	si.cam = camera;
	si.w = cfg.size.width;
	si.h = cfg.size.height;

//	si.pix = AV_PIX_FMT_YUV420P; 
	static const std::map< PixelFormat , AVPixelFormat> pix_map = {
		{ libcamera::formats::YUV420 ,AV_PIX_FMT_YUV420P },
		{ libcamera::formats::YUYV ,AV_PIX_FMT_YUYV422 },  //segfault wo sensor | unable to open raw codec
	//	{ libcamera::formats::YUYV ,AV_PIX_FMT_UYVY422 }, // unable to open raw codec
	//	{ libcamera::formats::YUYV ,AV_PIX_FMT_YUV422P },  //segfault wo sensor | runs with raw but color crap | segment fault with format filter
	//	{ libcamera::formats::YUYV ,AV_PIX_FMT_YUV422P16 },  // unable to open raw codec
	//	{ libcamera::formats::YUYV ,AV_PIX_FMT_YUV420P },  // runs with raw but color crap but best Y'
	//	{ libcamera::formats::MJPEG ,AV_PIX_FMT_YUVJ420P },
	//	{ libcamera::formats::YUYV ,AV_PIX_FMT_YUV420P }
//		{ libcamera::formats::SRGGB10_CSI2P ,AV_PIX_FMT_SGRBG10P }
	}; 
	auto it_pf = pix_map.find(cfg.pixelFormat);
		if (it_pf == pix_map.end())
			throw std::runtime_error("libav: no match for pixel format " + cfg.pixelFormat.toString());
		si.pix = it_pf->second;
	

	if (cfg.colorSpace) {
		using libcamera::ColorSpace; 

		static const std::map<ColorSpace::Primaries, AVColorPrimaries> pri_map = {
			{ ColorSpace::Primaries::Smpte170m, AVCOL_PRI_SMPTE170M },
			{ ColorSpace::Primaries::Rec709, AVCOL_PRI_BT709 },
			{ ColorSpace::Primaries::Rec2020, AVCOL_PRI_BT2020 },
		};

		static const std::map<ColorSpace::TransferFunction, AVColorTransferCharacteristic> tf_map = {
			{ ColorSpace::TransferFunction::Linear, AVCOL_TRC_LINEAR },
			{ ColorSpace::TransferFunction::Srgb, AVCOL_TRC_IEC61966_2_1 },
			{ ColorSpace::TransferFunction::Rec709, AVCOL_TRC_BT709 },
		};

		static const std::map<ColorSpace::YcbcrEncoding, AVColorSpace> cs_map = {
			{ ColorSpace::YcbcrEncoding::None, AVCOL_SPC_UNSPECIFIED },
			{ ColorSpace::YcbcrEncoding::Rec601, AVCOL_SPC_SMPTE170M },
			{ ColorSpace::YcbcrEncoding::Rec709, AVCOL_SPC_BT709 },
			{ ColorSpace::YcbcrEncoding::Rec2020, AVCOL_SPC_BT2020_CL },
		};

		
		auto it_p = pri_map.find(cfg.colorSpace->primaries);
		if (it_p == pri_map.end())
			throw std::runtime_error("libav: no match for colour primaries in " + cfg.colorSpace->toString());
		si.cp = it_p->second;

		auto it_tf = tf_map.find(cfg.colorSpace->transferFunction);
		if (it_tf == tf_map.end())
			throw std::runtime_error("libav: no match for transfer function in " + cfg.colorSpace->toString());
		si.ct = it_tf->second;

		auto it_cs = cs_map.find(cfg.colorSpace->ycbcrEncoding);
		if (it_cs == cs_map.end())
			throw std::runtime_error("libav: no match for ycbcr encoding in " + cfg.colorSpace->toString());
		si.cs = it_cs->second;

		si.cr =
			cfg.colorSpace->range == ColorSpace::Range::Full ? AVCOL_RANGE_JPEG : AVCOL_RANGE_MPEG;
	}
	return si;

}

void RCam::filterFrame(AVFilterContext *fctx, AVFrame *frame)
{
	DEBUG_PRINT("%s", "\n");
	assert(fctx);
	int ret;

	ret = av_buffersrc_add_frame_flags(fctx, frame, AV_BUFFERSRC_FLAG_KEEP_REF);
	if (ret < 0) throw std::runtime_error("libav: error sending frame to filter: " + std::to_string(ret));
	AVFrame *fframe = av_frame_alloc();
	FilterCtxs fc = filter_codec_ctx_[fctx];
	do {	
		ret = av_buffersink_get_frame(fc.outfctx, fframe);

		if (!(ret >= 0 || ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)) 
			throw std::runtime_error("libav: error getting flitered frame: " + std::to_string(ret));
		if (ret >= 0) {
			encodeFrame(fc.cctx, fframe);
			av_frame_unref(fframe); 
		}
	} while (!(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF));
	av_frame_free(&fframe);
}

bool RCam::allFormatsOpen()
{
	DEBUG_PRINT("%s", "\n");
	for (const auto& pair : format_opened_) {
		if (!pair.second) return false;
	}
	return true;
}

