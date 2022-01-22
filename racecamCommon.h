#ifndef RACECAMCOMMON_H_
#define RACECAMCOMMON_H_

#include "interface/mmal/mmal.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_util_params.h"
#include "interface/mmal/util/mmal_default_components.h"
#include "interface/mmal/util/mmal_connection.h"

#include <alsa/asoundlib.h>

#include <libavformat/avformat.h>
#include "libswresample/swresample.h"
#include "libavutil/audio_fifo.h"

#include "raspiCamUtilities.h"

#include "racecamQueue.h"
#include "racecamLogger.h"
#include <cairo/cairo.h>


#define NUM_SAMPLES 10
#define MAX_Q 39
#define Q_FACTOR 3
#define Q_WAIT 500 // in ms 1000 - 1 second

#define MAX_NUMBER_OF_CAMERAS 2
#define MAIN_CAMERA 0
#define OVERLAY_CAMERA 1

// Port configuration for the splitter component
#define SPLITTER_OUTPUT_PORT 0
#define SPLITTER_PREVIEW_PORT 1

// Standard port setting for the camera component
#define MMAL_CAMERA_PREVIEW_PORT 0
#define MMAL_CAMERA_VIDEO_PORT 1
#define MMAL_CAMERA_CAPTURE_PORT 2

// Video format information
// 0 implies variable
#define VIDEO_FRAME_RATE_NUM 25
#define VIDEO_FRAME_RATE_DEN 1

/// Video render needs at least 2 buffers.
#define VIDEO_OUTPUT_BUFFERS_NUM 3

#define DEFAULT_FORMAT		SND_PCM_FORMAT_S32_LE
#define DEFAULT_SPEED 		44100
#define DEFAULT_CHANNELS_IN	2

enum encoder_emun {
   FILE_STRM,
   URL_STRM,
   MAX_NUMBER_OF_STREAMS};

enum mode_enum {
  CANCELLED = -99,
  STOPPING_WRITE = -4,
  STOPPING_PREVIEW,
  STOPPING_RECORD,
  STOPPING_SWITCH,
  STOPPED,
  SWITCHING,
  RECORDING,
  PREVIEWING,
  WRITING,
  SELECTED};
  
enum preview_emun {
   NO_PREVIEW,
   PREVIEW_MAIN,
   PREVIEW_OVRL,
   PREVIEW_COMP};
      
typedef struct ADJUST_Q_STATE_S
  {
  int *running;
  QUEUE_STATE *queue;
  MMAL_PORT_T *port;
  int min_q;
  int samples[NUM_SAMPLES];
  int current_sample;
  } ADJUST_Q_STATE;
  
typedef struct
{
	QUEUE_STATE *queue;
	MMAL_POOL_T *pool;
	int64_t s_time;
} PORT_USERDATA;

typedef struct
{
   char camera_name[MMAL_PARAMETER_CAMERA_INFO_MAX_STR_LEN]; // Name of the camera sensor
   MMAL_RECT_T cam;                    /// camera origin and size
   int cameraNum;                      /// Camera number
   int sensor_mode;                    /// Sensor mode. 0=auto. Check docs/forum for modes selected by other values.

} RASPICOMMONSETTINGS_PARAMETERS;

typedef struct
{
   char enable;
   MMAL_DISPLAYREGION_T param;
   MMAL_ES_FORMAT_T *format;
} HVS_PARMS_T; 

typedef struct
{
   int              run_state;
   char              *dest;
    void    *r_state;  
   AVFormatContext   *fmtctx;
   QUEUE_STATE       *queue;
   queue_frame_s     *save_partial_frame;  // will need to be set to null on 
   queue_frame_s     *side_data_frame; // will need to be set to null on 
} OUTPUT_STATE;

typedef struct RACECAM_STATE_S
  {
  MMAL_COMPONENT_T *camera_component[MAX_NUMBER_OF_CAMERAS];    /// Pointer to the camera component
  MMAL_COMPONENT_T *hvs_component;   /// Pointer to the HVS component
  MMAL_COMPONENT_T *encoder_component[MAX_NUMBER_OF_STREAMS];   /// Pointer to the encoder components
  MMAL_COMPONENT_T *splitter_component;
  MMAL_COMPONENT_T *preview_component;   /// Pointer to the preview component

  MMAL_CONNECTION_T *hvs_main_connection; /// Pointer to the connection to hvs main
  MMAL_CONNECTION_T *hvs_ovl_connection; /// Pointer to the connection to hvs ovl
  MMAL_CONNECTION_T *splitter_connection;
  MMAL_CONNECTION_T *encoder_connection[MAX_NUMBER_OF_STREAMS]; /// Pointer to the connection to encoder
  MMAL_CONNECTION_T *preview_connection; /// Pointer to the connection to preview

  MMAL_POOL_T *hvs_textin_pool; /// Pointer to the input pool of buffers for text overlay
  MMAL_POOL_T *encoder_pool[MAX_NUMBER_OF_STREAMS]; /// Pointer to the pool of buffers used by encoder output port

  snd_pcm_t         *pcmhnd;
  int64_t           sample_cnt;
  u_char            *pcmbuf;
  u_char            *rlbufs;
  size_t            bufsize;
  char              *adev;
  AVAudioFifo       *fifo;
  SwrContext        *swrctx;
  AVCodecContext    *rawctx;
  AVCodecContext    *audctx;
  
  ADJUST_Q_STATE adjust_q_state;
    
  OUTPUT_STATE output_state[MAX_NUMBER_OF_STREAMS];
  PORT_USERDATA userdata[MAX_NUMBER_OF_STREAMS];

  RASPICOMMONSETTINGS_PARAMETERS common_settings[MAX_NUMBER_OF_CAMERAS];     /// Common settings
  HVS_PARMS_T hvs[4];
  int timeout;                        /// Time taken before frame is grabbed and app then shuts down. Units are milliseconds
  MMAL_FOURCC_T encoding;             /// Requested codec video encoding (MJPEG or H264)
  int bitrate;                        /// Requested bitrate
  int framerate;                      /// Requested frame rate (fps)
  int intraperiod;                    /// Intra-refresh period (key frame rate)
  int quantisationParameter;          /// Quantisation parameter - quality. Set bitrate 0 and set this for variable bitrate
  int quantisationMin;   
  int bInlineHeaders;                  /// Insert inline headers to stream (SPS, PPS)
  int immutableInput;                 /// Flag to specify whether encoder works in place or creates a new buffer. Result is preview can display either
                                       /// the camera output or the encoder output (with compression artifacts)
  int profile;                        /// H264 profile to use for encoding
  int level;                          /// H264 level to use for encoding
  int current_mode;            // 0=stopped 1=GUIRecording 2=SwitchRecording

   RASPICAM_CAMERA_PARAMETERS camera_parameters; /// Camera setup parameters

   int   achannels;
 
  int preview_mode;
  } RACECAM_STATE;

int create_video_stream(RACECAM_STATE *state);
void destroy_video_stream(RACECAM_STATE *state);
void check_output_status(RACECAM_STATE *state);
int create_video_preview(RACECAM_STATE *state);
void destroy_video_preview(RACECAM_STATE *state);
void *adjust_q(void *arg);

//ALSA
int allocate_alsa(RACECAM_STATE *);  
int free_alsa(RACECAM_STATE *);  
void read_pcm(RACECAM_STATE *);
int allocate_audio_encode(RACECAM_STATE *);
void free_audio_encode(RACECAM_STATE *);
void encode_queue_audio(RACECAM_STATE *, int flush);

//OUTPUT
void *write_stream(void *arg);

//MMAL
void check_disable_port(MMAL_PORT_T *);
MMAL_STATUS_T connect_ports(MMAL_PORT_T *, MMAL_PORT_T *, MMAL_CONNECTION_T **);
MMAL_COMPONENT_T *create_camera_component(RACECAM_STATE *state, int camera_type);
MMAL_COMPONENT_T *create_encoder_component(RACECAM_STATE *,MMAL_ES_FORMAT_T *format);
MMAL_COMPONENT_T *create_hvs_component(RACECAM_STATE *);
MMAL_COMPONENT_T *create_preview_component(RACECAM_STATE *);
MMAL_COMPONENT_T *create_splitter_component(MMAL_COMPONENT_T **splitter, MMAL_ES_FORMAT_T *format);
void default_status(RACECAM_STATE *);
void destroy_component(MMAL_COMPONENT_T **splitter);
void encoder_buffer_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
void get_sensor_defaults(int, char *, int *, int *);

#endif /* RACECAMCOMMON_H_ */
