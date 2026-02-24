/*  racecam capture class header
* rcam.hpp
*/ 

#include <condition_variable>
#include <sys/mman.h>

#include <libcamera/control_ids.h>

#include <boost/json.hpp>  

#include "core/dma_heaps.hpp"
#include "rcamshared.hpp"
#include "core/youtube.hpp"

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavdevice/avdevice.h"
#include "libavutil/audio_fifo.h"
#include "libavutil/imgutils.h"
#include "libswresample/swresample.h"
#include "libavfilter/buffersrc.h"
#include "libavfilter/buffersink.h"
}

#pragma once

// #define DEBUG 1
#ifdef DEBUG
    #define DEBUG_PRINT(fmt, ...) \
	fprintf(stderr, "%s:%d:%s" fmt, __FILE__, __LINE__, __PRETTY_FUNCTION__, ##__VA_ARGS__) 
#else
    #define DEBUG_PRINT(fmt, ...)
#endif

//constexpr size_t MAX_SIZE = std::numeric_limits<size_t>::max(); 
#define CLEARSCREEN std::cout << "\033[2J\033[1;1H" << std::flush;
#define CLEARPREVLINE std::cout << "\033[1F\033[2K" << std::flush;
#define CLEARPREVLINES std::cout << "\033[1F\033[2K\033[1F\033[2K" << std::flush;

#define LOC std::string(__FILE__) + ":" + std::string(__PRETTY_FUNCTION__) + ":" + std::to_string(__LINE__)
//#define LOC std::string(__FILE__) + ":" + std::string(__FUNCTION__) + ":" + std::to_string(__LINE__)

using namespace libcamera;
namespace json = boost::json;

typedef struct {
	int w;
	int h;
	AVPixelFormat pix;
	AVColorPrimaries cp;
	AVColorTransferCharacteristic ct;
	AVColorSpace cs;
	AVColorRange cr;
} StreamInfo;

typedef enum {
	RCunknown,
	RCavailable, 
	RCaquired,
	RCconfigured,
	RCrunning,
	RCstopping,
} CamStat;

typedef struct {
	std::shared_ptr<Camera> cam = {};
	std::string loc = {};
	CamStat stat = RCunknown;
	std::unique_ptr<CameraConfiguration> cfg = {};
	std::map<Stream *, std::vector<std::unique_ptr<FrameBuffer>>> fbuf = {}; 
	std::vector<std::unique_ptr<Request>> req = {};
	ControlList cntls = {}; 
	uint64_t lts = {0};  
} RCamStruct;

typedef struct {
	AVFormatContext *fmt_ctx = nullptr;
	AVStream *strm = nullptr;
} CodecStream; 

typedef struct {
	AVFilterContext *outfctx = nullptr;
	AVCodecContext *cctx = nullptr;	
} FilterCtxs;

struct CompletedRequest
{
	using BufferMap = libcamera::Request::BufferMap;
	using ControlList = libcamera::ControlList;
	using Request = libcamera::Request;

	CompletedRequest(Request *r)
		: buffers(r->buffers()),  metadata(r->metadata()), request(r)
	{
		r->reuse();
	}
	BufferMap buffers;
	ControlList metadata;
	Request *request;
}; 

using CompletedRequestPtr = std::shared_ptr<CompletedRequest>; 

	enum class MsgType
	{
		RequestComplete,
		Timeout,
		Quit
	};

	typedef std::variant<CompletedRequestPtr> MsgPayload;
	struct Msg
	{
		Msg(MsgType const &t) : type(t) {}
		template <typename T>
		Msg(MsgType const &t, T p) : type(t), payload(std::forward<T>(p))
		{
		}
		MsgType type;
		MsgPayload payload;
	}; 
	
class RCam : public RCamShared
{
public:
	using CameraManager = libcamera::CameraManager;
	using Camera = libcamera::Camera;
	using BufferMap = Request::BufferMap;
	
//WEK bug default cfg not working;
	RCam(Logger& lptr, std::string const& cfg = "racecam_config.json");
	virtual ~RCam(){};  
//	virtual ~RCam();   

	void InitCapture();  
	void FreeCapture();  
	
protected:

private:
	template <typename T>
	class MessageQueue
	{
	public:
		template <typename U>
		void Post(U &&msg)
		{
			std::unique_lock<std::mutex> lock(mutex_);
			queue_.push(std::forward<U>(msg));
			cond_.notify_one();
		}
		T Get()
		{
			std::unique_lock<std::mutex> lock(mutex_);
			cond_.wait(lock, [this] { return !queue_.empty(); });
			T msg = std::move(queue_.front());
			queue_.pop();
			return msg;
		}
		void Clear()
		{
			std::unique_lock<std::mutex> lock(mutex_);
			queue_ = {};
		}

	private:
		std::queue<T> queue_;
		std::mutex mutex_;
		std::condition_variable cond_;
	};
	void initCams(const json::object&);
	void startCams(const json::object&);
	void initOutputs(void);
	void freeOutputs();
	void initContainer(const bool, const json::object&);
	AVStream *addStream(AVFormatContext *, const AVCodecContext *);
	void initAudioInCodec(const json::object&);
	AVCodecContext *initAudioOutCodec(AVFormatContext *, const json::value&);
	void stopCams(const json::object&);
	void freeCams(const json::object&);
	void makeRequests(int cam);
	void requestComplete(Request *);
	void queueRequest(CompletedRequest *);
	void rcamCommon();
	void videoThread();
	void audioThread();
	void encodeFrame(AVCodecContext *, AVFrame *);
	bool outputReady(AVFormatContext *);
	bool outputReady(AVCodecContext *);
	void openFormat(AVCodecContext *); 
	unsigned int getCntlID(const std::string cntlname);
	void initVideoStream(AVFormatContext *, json::value&);
	StreamInfo getStreamInfo(const StreamConfiguration&);
	void filterFrame(AVFilterContext *, AVFrame *);
	bool allFormatsOpen();
	
	std::array<RCamStruct, 5> rcams_;
	std::map<FrameBuffer *, std::vector<libcamera::Span<uint8_t>>> mapped_buffers_;
	DmaHeap dma_heap_;
	MessageQueue<Msg> msg_queue_;
	std::mutex completed_requests_mutex_;
	std::mutex camera_stop_mutex_; 
	std::mutex control_mutex_;
	uint64_t sequence_ = 0;
	std::vector<AVFormatContext *> all_fmt_ctx_;
	std::map<AVCodecContext *, CodecStream> codec_to_stream_;
	std::map<int, std::vector<AVFilterContext *>> camstrm_fgctx_;
	AVFormatContext *audio_in_fmt_ctx_ = nullptr;
	AVCodecContext *audio_in_codec_ctx_ = nullptr;
	std::vector<AVCodecContext *> audio_out_codec_ctxs_; 
	std::atomic<bool> stop_threads_ = false;
	std::thread video_thread_;
	std::thread audio_thread_;
	std::queue<AVFrame *> frame_queue_;
	std::condition_variable video_cv_;
	bool cameras_running_ = false;
	std::atomic<bool> output_ready_ = false;
	std::mutex output_mutex_;
	std::map<AVFormatContext *, bool> format_opened_;
	uint64_t audio_samples_ = 0;
	uint64_t video_start_ts_ = 0;
	std::vector<AVFilterGraph *> fgctx_ {};
	std::map<AVFilterContext *, FilterCtxs> filter_codec_ctx_;
	bool audio_ = false;
	std::string strmID_ {};
};
