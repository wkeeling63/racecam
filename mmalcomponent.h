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

#include <libavformat/avformat.h>
#include "libswresample/swresample.h"
#include "libavutil/audio_fifo.h"
#include <alsa/asoundlib.h>

enum mode_enum {
  NOT_RUNNING,
  CLICK_RECORD,
  SWITCH_RECORD,
  PREVIEW,
  EXITING=-1};

typedef struct
{
   char camera_name[MMAL_PARAMETER_CAMERA_INFO_MAX_STR_LEN]; // Name of the camera sensor
   int width;                          /// Requested width of image
   int height;                         /// requested height of image
   MMAL_RECT_T ovl;                    /// overlay location
   int cameraNum;                      /// Camera number
   int sensor_mode;                    /// Sensor mode. 0=auto. Check docs/forum for modes selected by other values.

} RASPICOMMONSETTINGS_PARAMETERS;

typedef struct RASPIVID_STATE_S RASPIVID_STATE;

typedef struct
{
   RASPIVID_STATE *pstate;              /// pointer to our state in case required in callback
   AVPacket *vpckt;
   u_char *vbuf;
   int vbuf_ptr;
   sem_t *mutex;         
   int64_t wtargettime;
   int64_t wvariance;
      
} PORT_USERDATA;

typedef struct
{
   AVFormatContext   *fmtctx;
   AVIOContext       *ioctx;
   AVCodecContext    *audctx;
} FORMAT_CTX;

typedef struct
{
   AVAudioFifo       *fifo;
   SwrContext        *swrctx;
   AVFrame           *infrm;
   AVFrame           *outfrm;
   AVCodecContext    *rawctx;
   AVCodecContext    *audctx;
   snd_pcm_t         *pcmhnd;
   u_char            *pcmbuf;
   u_char            *rlbufs;
   size_t            bufsize;
   int64_t           audio_sample_cnt;
   int64_t           start_time;
} AENCODE_CTX;

struct RASPIVID_STATE_S
{
   RASPICOMMONSETTINGS_PARAMETERS common_settings;     /// Common settings
   int timeout;                        /// Time taken before frame is grabbed and app then shuts down. Units are milliseconds
   MMAL_FOURCC_T encoding;             /// Requested codec video encoding (MJPEG or H264)
   int bitrate;                        /// Requested bitrate
   int framerate;                      /// Requested frame rate (fps)
   int intraperiod;                    /// Intra-refresh period (key frame rate)
   int quantisationParameter;          /// Quantisation parameter - quality. Set bitrate 0 and set this for variable bitrate
   int quantisationMin;  
   int quantisationMax;  
   int bInlineHeaders;                  /// Insert inline headers to stream (SPS, PPS)
   int immutableInput;                 /// Flag to specify whether encoder works in place or creates a new buffer. Result is preview can display either
                                       /// the camera output or the encoder output (with compression artifacts)
   int profile;                        /// H264 profile to use for encoding
   int level;                          /// H264 level to use for encoding
   int waitMethod;         //needed??            /// Method for switching between pause and capture
   int recording;            // 0=stopped 1=GUIRecording 2=SwitchRecording

   RASPICAM_CAMERA_PARAMETERS camera_parameters; /// Camera setup parameters

   MMAL_COMPONENT_T *camera_component;    /// Pointer to the camera component
   MMAL_COMPONENT_T *camera2_component;    /// Pointer to the camera component
   MMAL_COMPONENT_T *encoder_component;   /// Pointer to the encoder component
   MMAL_COMPONENT_T *hvs_component;   /// Pointer to the encoder component
   MMAL_COMPONENT_T *preview_component;   /// Pointer to the preview component
   MMAL_CONNECTION_T *hvs_main_connection; /// Pointer to the connection to hvs main
   MMAL_CONNECTION_T *hvs_ovl_connection; /// Pointer to the connection to hvs ovl
   MMAL_CONNECTION_T *encoder_connection; /// Pointer to the connection to encoder
   MMAL_CONNECTION_T *preview_connection; /// Pointer to the connection to preview

   MMAL_POOL_T *encoder_pool; /// Pointer to the pool of buffers used by encoder output port
   MMAL_POOL_T *hvs_pool; /// Pointer to the pool of buffers used by hvs output port
   MMAL_POOL_T *hvs_textin_pool; /// Pointer to the input pool of buffers for text overlay
   
   FORMAT_CTX  urlctx;
   FORMAT_CTX  filectx;
   AENCODE_CTX encodectx;

   PORT_USERDATA callback_data;        /// Used to move data to the encoder callback

   int mode;    // 1=click_record, 2=switch_record, 3=preview -1=exiting)

   int frame;  
   int64_t lasttime;
   char gps;

   MMAL_BOOL_T addSPSTiming;
   int slices;
};
MMAL_STATUS_T create_camera_component(RASPIVID_STATE *state);
void destroy_camera_component(RASPIVID_STATE *state);
MMAL_STATUS_T create_hvs_component(RASPIVID_STATE *state);
void destroy_hvs_component(RASPIVID_STATE *state);
MMAL_STATUS_T create_encoder_component(RASPIVID_STATE *state);
void destroy_encoder_component(RASPIVID_STATE *state);
MMAL_STATUS_T create_preview_component(RASPIVID_STATE *state);
void destroy_preview_component(RASPIVID_STATE *state);
MMAL_STATUS_T connect_ports(MMAL_PORT_T *output_port, MMAL_PORT_T *input_port, MMAL_CONNECTION_T **connection);
void check_disable_port(MMAL_PORT_T *port);
void get_sensor_defaults(int camera_num, char *camera_name, int *width, int *height );
void default_status(RASPIVID_STATE *state);
