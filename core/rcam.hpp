/*  racecam capture class header
 * rcam.hpp
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

#include <condition_variable>
#include <sys/mman.h>

//#include <queue>
//#include <mutex>
//#include <condition_variable>
//#include <memory>
//#include <functional>
//#include <cstring>
//#include <iostream>
//include <thread>
//#include <atomic>

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

//#include <libavcodec/packet.h>
}

#pragma once

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
	
struct AVPacketDeleter {
    void operator()(AVPacket* pkt) const {
        if (pkt) av_packet_free(&pkt);
    }
};
using AVPacketPtr = std::unique_ptr<AVPacket, AVPacketDeleter>;

enum class NetworkState {
    Normal,
    Slow
};
class PacketQueue {public:
    explicit PacketQueue(Logger& log, unsigned int maxCapacity = 60, double highWatermarkPct = 0.70, double lowWatermarkPct = 0.20) 
        : logger_(log),
		maxCapacity_(maxCapacity),
        highWatermark_(static_cast<unsigned int>(maxCapacity * highWatermarkPct)),
        lowWatermark_(static_cast<unsigned int>(maxCapacity * lowWatermarkPct)),
        currentState_(NetworkState::Normal),
        shutdown_(false) {}

    ~PacketQueue() { shutdown(); }

    // Register a callback function to handle changes in network congestion status
    void setCongestionCallback(std::function<void(NetworkState)> cb) {
//		fprintf(stderr, "%s:%d:%s\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);
        std::lock_guard<std::mutex> lock(mutex_);
        congestionCallback_ = cb;
    }

    bool push(const AVPacket* srcPkt) {
        std::function<void(NetworkState)> callbackToExecute = nullptr;
        {
//			fprintf(stderr, "%s:%d:%s\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);
            std::unique_lock<std::mutex> lock(mutex_);
            
            if (queue_.size() >= maxCapacity_ || shutdown_) {
				logger_.Log(LogLevel::WARN, "Unable to queue packet for output" + std::string((queue_.size() >= maxCapacity_) ? " at max capacity" : " shutting down"));
                return false; 
            }

			// Allocate and clone the packet safely via smart pointers
			AVPacketPtr clonedPkt(av_packet_alloc());
			if (av_packet_ref(clonedPkt.get(), srcPkt) < 0) {
				return false;
			}

            queue_.push(std::move(clonedPkt));

            // Check if we just crossed into the high-watermark congestion zone
            if (queue_.size() >= highWatermark_ && currentState_ == NetworkState::Normal) {
                currentState_ = NetworkState::Slow;
                callbackToExecute = congestionCallback_;
            }

            cv_.notify_one();
        }

        // Execute callback outside the lock to prevent encoder thread deadlocks
        if (callbackToExecute) {
            callbackToExecute(NetworkState::Slow);
        }
        return true;
    }

    AVPacketPtr pop() {
        std::function<void(NetworkState)> callbackToExecute = nullptr;
        AVPacketPtr pkt = nullptr;

        {
//			fprintf(stderr, "%s:%d:%s\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] { return !queue_.empty() || shutdown_; });

            if (queue_.empty() && shutdown_) return nullptr;

            pkt = std::move(queue_.front());
            queue_.pop();

            // Check if the queue has drained safely below the low-watermark threshold
            if (queue_.size() <= lowWatermark_ && currentState_ == NetworkState::Slow) {
                currentState_ = NetworkState::Normal;
                callbackToExecute = congestionCallback_;
            }
        }

        if (callbackToExecute) {
            callbackToExecute(NetworkState::Normal);
        }
        return pkt;
    }

    void flush() {
//		fprintf(stderr, "%s:%d:%s\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);
        std::unique_lock<std::mutex> lock(mutex_);
        shutdown_ = true; 
        cv_.notify_all(); 
        cv_.wait(lock, [this] { return queue_.empty(); });
    }
    void shutdown() {
//		fprintf(stderr, "%s:%d:%s\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);
        std::unique_lock<std::mutex> lock(mutex_);
        shutdown_ = true;
        while (!queue_.empty()) queue_.pop();
        cv_.notify_all();
    }
private:
	Logger& logger_;
    std::queue<AVPacketPtr> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    unsigned int maxCapacity_;
    unsigned int highWatermark_;
    unsigned int lowWatermark_;
    NetworkState currentState_;
    std::function<void(NetworkState)> congestionCallback_;
    bool shutdown_;
};

class NetworkWriter {public:
    NetworkWriter(Logger& log, AVFormatContext* outCtx, size_t maxQueueCapacity = 60)
        : logger_(log), outCtx_(outCtx), queue_(log, maxQueueCapacity), running_(false) {}

    ~NetworkWriter() { stop(); }

    void start() {
//		fprintf(stderr, "%s:%d:%s\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);
        if (running_.load()) return;
        running_.store(true);
        workerThread_ = std::thread(&NetworkWriter::networkLoop, this);
    }

    void stop() {
//		fprintf(stderr, "%s:%d:%s\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);
        if (!running_.load()) return;
        queue_.flush();
        running_.store(false);
        if (workerThread_.joinable()) workerThread_.join();
    }
    
    void setRate(unsigned int r) {
//		fprintf(stderr, "%s:%d:%s\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);
		normalrate_ = r;
		slowrate_ = r * .4;
		targetrate_.store(r);
		if (r)
			queue_.setCongestionCallback([this](NetworkState ns) {
				unsigned int tr = targetrate_.load();
				if (ns == NetworkState::Normal && tr != normalrate_) {
					logger_.Log(LogLevel::INFO, "Set bit_rate back to normal: " + std::to_string(normalrate_));
					targetrate_.store(normalrate_);
				return;
				}
				if (ns == NetworkState::Slow && tr != slowrate_) {
					logger_.Log(LogLevel::INFO, "Set bit_rate back to slow: " + std::to_string(slowrate_));
					targetrate_.store(slowrate_);
				return;
				}
			});
	}
	
	unsigned int newRate(unsigned int br) {
//		fprintf(stderr, "%s:%d:%s\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);
		if (!br) return false;
		unsigned int r = targetrate_.load();
		if (r != br) return r;
		else return false;
	}

    bool enqueuePacket(const AVPacket* pkt) {
//		fprintf(stderr, "%s:%d:%s\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);
        return queue_.push(pkt);
    }

private:
    void networkLoop() {
//		fprintf(stderr, "%s:%d:%s\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);
        while (running_.load()) {
            AVPacketPtr pkt = queue_.pop();
            if (!pkt) break;
			int ret = 0;
            ret = av_interleaved_write_frame(outCtx_, pkt.get());
            if (ret < 0) {
                std::cerr << "[Error] Network write failed: " << ret << "\n";
            }
        }
    }
    
    Logger& logger_;
    AVFormatContext* outCtx_;
    PacketQueue queue_;
    std::thread workerThread_;
    std::atomic<bool> running_;
    std::atomic<unsigned int> targetrate_ {0};
    unsigned int normalrate_ {0};
    unsigned int slowrate_ {0};
};

using NetworkWriterPtr = std::shared_ptr<NetworkWriter>; 

typedef struct {
	AVFormatContext *fmt_ctx = nullptr;
	AVStream *strm = nullptr;
	NetworkWriterPtr queue = nullptr;
} CodecOutput; 

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
	AVCodecContext *initAudioOutCodec(AVFormatContext *, const NetworkWriterPtr&, const json::value&);
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
	void initVideoStream(AVFormatContext *, const NetworkWriterPtr&, json::value&);
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
	std::map<AVCodecContext *,CodecOutput> codec_to_output_; //WEK make this a vector of CodecOuput when change audio to one codec
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
