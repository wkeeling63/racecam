#include <gtk/gtk.h>

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include "libavformat/avio.h"
#include "libavutil/audio_fifo.h"
#include "libavutil/avassert.h"
#include "libavutil/avstring.h"
#include "libavutil/frame.h"
#include "libavutil/opt.h"
#include "libswresample/swresample.h"
#include <libavutil/channel_layout.h>
#include <libavutil/mathematics.h>
#include <libavutil/timestamp.h>

#include "interface/mmal/mmal.h"
#include "interface/mmal/mmal_logging.h"
#include "interface/mmal/mmal_buffer.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_util_params.h"
#include "interface/mmal/util/mmal_default_components.h"
#include "interface/mmal/util/mmal_connection.h"
#include "interface/mmal/mmal_parameters_camera.h"

#include <bcm2835.h>

#include "raspiCamUtilities.h"
#include "mmalcomponent.h"
#include "GPSUtil.h"

#define DEFAULT_FORMAT		SND_PCM_FORMAT_S32_LE
#define DEFAULT_SPEED 		44100
#define DEFAULT_CHANNELS_IN	2
#define BUFFER_SIZE			262144
#define STOP 0
#define START 1
// write target time in micro seconds 250000=.25 second
#define TARGET_TIME 250000

#define GPIO_MODEM_LED	RPI_BPLUS_GPIO_J8_07 
#define GPIO_LED			  RPI_BPLUS_GPIO_J8_13 
#define GPIO_SWT			  RPI_BPLUS_GPIO_J8_15
#define GPIO_PWR_LED	  RPI_BPLUS_GPIO_J8_16

 /* typedef struct FLVFileposition {
        int64_t keyframe_position;
        double keyframe_timestamp;
        struct FLVFileposition *next;
    } FLVFileposition;

typedef struct FLVContext {
       AVClass *av_class;
       int     reserved;
       int64_t duration_offset;
       int64_t filesize_offset;
       int64_t duration;
       int64_t delay;      ///< first dts delay (needed for AVC & Speex)
   
       int64_t datastart_offset;
       int64_t datasize_offset;
       int64_t datasize;
       int64_t videosize_offset;
       int64_t videosize;
       int64_t audiosize_offset;
       int64_t audiosize;
   
       int64_t metadata_size_pos;
       int64_t metadata_totalsize_pos;
       int64_t metadata_totalsize;
       int64_t keyframe_index_size;
   
       int64_t lasttimestamp_offset;
       double lasttimestamp;
       int64_t lastkeyframetimestamp_offset;
       double lastkeyframetimestamp;
       int64_t lastkeyframelocation_offset;
       int64_t lastkeyframelocation;
   
       int acurframeindex;
       int64_t keyframes_info_offset;
   
       int64_t filepositions_count;
       FLVFileposition *filepositions;
       FLVFileposition *head_filepositions;
   
       AVCodecParameters *audio_par;
       AVCodecParameters *video_par;
       double framerate;
       AVCodecParameters *data_par;
   
       int flags;
   } FLVContext; */
  
typedef struct{
  GtkWidget *label;
  float *val;
  char *format;
  float *min;
  float *max;
  float incv;
  } limit;
  
typedef struct{
  GtkWidget *button;
  char *status;
  } check;
  
typedef struct{
  GtkWidget *draw_area;
  short int *x;
  short int *y;
  float *size;
  float *o_x;
  float *o_y;
  } ovrl; 
  
typedef struct{
  float *val;
  ovrl *ol;
  float *min;
  float *max;
  float *xymin;
  float *xymax;
  float incv;
  } draw;
  
struct {
  char url[64];    // rtmp://a.rtmp.youtube.com/live2/<key>
  char write_url;
  char file[64];
  char write_file;
  float file_keep;      
  char adev[18];  // dmic_sv
  short int main_size;  // 2: 854x480 1: 1280x720 0: 1920x1080
  float ovrl_size;
  float ovrl_x;
  float ovrl_y;
  char fmh;
  char fmv;
  char foh;
  char fov;
  char channels;
  float qmin;
  float qcur;
  float qmax;
  float fps;
  float ifs;
  char cam;
  char gps;
  } iparms;

GtkWidget *stop_win, *stop_button, *message1, *message2;

char gpio_init=0;

GtkWidget *m_layout;
static const char *m_kbd_path = "/usr/local/bin/matchbox-keyboard";
static const char *m_kbd_str;
static guint m_kbd_xid;
static guint m_kbd_pid;

unsigned long     kb_xid;

/* Backing pixmap for drawing area */
static GdkPixmap *pixmap = NULL;

void cleanup_children(int s)
{
  kill(-getpid(), 15);  /* kill every one in our process group  */
  exit(0);
}

void install_signal_handlers(void)
{
  signal (SIGCHLD, SIG_IGN);  /* kernel can deal with zombies  */
  signal (SIGINT, cleanup_children);
  signal (SIGQUIT, cleanup_children);
  signal (SIGTERM, cleanup_children);
}

unsigned long launch_keyboard(void)
{
  int    i = 0, fd[2];
  int    stdout_pipe[2];
  int    stdin_pipe[2];
  char   buf[256], c;
  size_t n;

  unsigned long result;

//  printf("Launching keyboard from: %s\r\n",m_kbd_path);

  pipe (stdout_pipe);
  pipe (stdin_pipe);

  switch (fork ())
    {
    case 0:
      {
	/* Close the Child process' STDOUT */
	close(1);
	dup(stdout_pipe[1]);
	close(stdout_pipe[0]);
	close(stdout_pipe[1]);
	
	execlp ("/bin/sh", "sh", "-c", "matchbox-keyboard --xid fi", NULL);
      }
    case -1:
      perror ("### Failed to launch 'matchbox-keyboard --xid', is it installed? ### ");
      exit(-1);
    }

  /* Parent */

  /* Close the write end of STDOUT */
  close(stdout_pipe[1]);

  /* FIXME: This could be a little safer... */
  do 
    {
      n = read(stdout_pipe[0], &c, 1);
      if (n == 0 || c == '\n')
	break;
      buf[i++] = c;
    } 
  while (i < 256);

  buf[i] = '\0';
  result = atol (buf);

  close(stdout_pipe[0]);

  return result;
}

static void hvs_input_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
	if (buffer->user_data)
		{
		cairo_surface_destroy(buffer->user_data);
		}
	mmal_buffer_header_release(buffer);
}

static void encoder_buffer_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
	MMAL_BUFFER_HEADER_T *new_buffer;
	static int64_t framecnt=0;
	static int64_t pts = -1;
	
	PORT_USERDATA *pData = (PORT_USERDATA *)port->userdata;
  RASPIVID_STATE *pstate = pData->pstate;

	if (pData)
		{
		int bytes_written = buffer->length;
		if (buffer->length)
			{
			mmal_buffer_header_mem_lock(buffer);
			if(buffer->flags & MMAL_BUFFER_HEADER_FLAG_CODECSIDEINFO)
				{
				bytes_written = buffer->length;
				fprintf(stderr, "skipped due to flag %d \n", buffer->flags);
				}
			else
				{			
				AVPacket *packet=pData->vpckt;
//        static AVPacket packet;
//        av_init_packet(&packet);
        
				int status=0;
				if (buffer->pts != MMAL_TIME_UNKNOWN && buffer->pts != pData->pstate->lasttime)
					{
					if (pData->pstate->frame == 0) 
						pData->pstate->starttime = buffer->pts;
					pData->pstate->lasttime = buffer->pts;
					pts = buffer->pts - pData->pstate->starttime;
					pData->pstate->frame++;
					}	
				if (buffer->flags & MMAL_BUFFER_HEADER_FLAG_FRAME_END) 
					{
					if (buffer->flags & MMAL_BUFFER_HEADER_FLAG_KEYFRAME)
						{
						packet->flags=AV_PKT_FLAG_KEY+AV_PKT_FLAG_TRUSTED;
						}
					else
						{
						packet->flags=AV_PKT_FLAG_TRUSTED;
						}
					if (pData->vbuf_ptr == 0)
						{
						packet->data=buffer->data;
						packet->size=buffer->length;
						} 
					else
						{
						memcpy(pData->vbuf+pData->vbuf_ptr, buffer->data+buffer->offset, buffer->length);
						pData->vbuf_ptr += buffer->length;
						packet->data=pData->vbuf;
						packet->size=pData->vbuf_ptr;
						pData->vbuf_ptr=0;
						}
					packet->dts = packet->pts = pts/1000;
					if (buffer->flags & MMAL_BUFFER_HEADER_FLAG_CONFIG)
						{
						if (packet->side_data) {av_packet_free_side_data(packet);}
						uint8_t *side_data = NULL;
						side_data = av_packet_new_side_data(packet, AV_PKT_DATA_NEW_EXTRADATA, buffer->length);
						if (!side_data) {
							fprintf(stderr, "%s\n", AVERROR(ENOMEM));
              exit(-4);
							}
						memcpy(side_data, buffer->data+buffer->offset, buffer->length);
						}
					int64_t wstart = get_microseconds64();
					sem_wait(pData->mutex);
					if (pstate->urlctx.fmtctx) status=av_write_frame(pstate->urlctx.fmtctx, packet);
          if (pstate->filectx.fmtctx) status+=av_write_frame(pstate->filectx.fmtctx, packet);					
          sem_post(pData->mutex);
          pData->wvariance += (get_microseconds64() - wstart) - pData->wtargettime;

					if (status)
						{
						fprintf(stderr, "video frame write error or flush %d %s\n", status, av_err2str(status));
						bytes_written = 0;
						}
					else 
						{
						++framecnt;
						bytes_written = buffer->length;
						}				
					}

				else
					{
					if (buffer->length >  BUFFER_SIZE - pData->vbuf_ptr) 
						{
						fprintf(stderr, "save vbuf to small\n");
						}
					else
						{
						memcpy(pData->vbuf+pData->vbuf_ptr, buffer->data+buffer->offset, buffer->length);
						pData->vbuf_ptr+=buffer->length;
						bytes_written = buffer->length;	
						}
					}
//        av_packet_unref(&packet);
				}

				mmal_buffer_header_mem_unlock(buffer);
				if (bytes_written != buffer->length)
					{
					vcos_log_error("Failed to write buffer data (%d from %d)- aborting", bytes_written, buffer->length);
					}
			}
		}
	else
		{
		vcos_log_error("Received a encoder buffer callback with no state");
		}

	mmal_buffer_header_release(buffer);
	if (port->is_enabled)
		{
		MMAL_STATUS_T status;
		new_buffer = mmal_queue_get(pData->pstate->encoder_pool->queue);
		if (new_buffer)
			status = mmal_port_send_buffer(port, new_buffer);
		if (!new_buffer || status != MMAL_SUCCESS)
			vcos_log_error("Unable to return a buffer to the encoder port");
		}
}

void parms_to_state(RASPIVID_STATE *state)
{
  default_status(state);
  switch (iparms.main_size)    // 2: 854x480 1: 1280x720 0: 1920x1080
  {
  case 0:
    state->common_settings.width = 1920;
    state->common_settings.height = 1080;
    break;
  case 1:
    state->common_settings.width = 1280;
    state->common_settings.height = 720;
    break;
  default:
    state->common_settings.width = 854;
    state->common_settings.height = 480;
  }
    
	state->common_settings.ovl.width = state->common_settings.width*iparms.ovrl_size;
	state->common_settings.ovl.height = state->common_settings.height*iparms.ovrl_size;
	state->common_settings.ovl.x = state->common_settings.width*iparms.ovrl_x;
	state->common_settings.ovl.y = state->common_settings.height*iparms.ovrl_y;
	state->common_settings.cameraNum = iparms.cam;
	state->camera_parameters.vflip = iparms.fmv;
	state->camera_parameters.hflip = iparms.fmh;
  
  state->quantisationParameter=iparms.qcur;
  state->quantisationMin=iparms.qmin;
  state->quantisationMax=iparms.qmax;
  state->framerate=iparms.fps;
  state->intraperiod=iparms.fps*iparms.ifs;
  
  state->gps=iparms.gps;
}

void adjust_q(RASPIVID_STATE *state)
{
  int64_t *write_variance = &state->callback_data.wvariance;
  int64_t *write_target_time = &state->callback_data.wtargettime;
  int fps=state->framerate;
  static int atQlimit=MMAL_FALSE;
  
  if (*write_variance > (*write_target_time*fps*4) || *write_variance < (*write_target_time*fps*-4))
  {
    MMAL_STATUS_T status;
    MMAL_PARAMETER_UINT32_T param = {{ MMAL_PARAMETER_VIDEO_ENCODE_INITIAL_QUANT, sizeof(param)}, 0};
    status = mmal_port_parameter_get(state->encoder_component->output[0], &param.hdr);
    if (status != MMAL_SUCCESS) {vcos_log_error("Unable to get current QP");}
    if (*write_variance < 0 && param.value > state->quantisationMin)
      {
      param.value--;
      atQlimit = MMAL_FALSE;
      fprintf(stdout, "Quantization %d\n", param.value);
      status = mmal_port_parameter_set(state->encoder_component->output[0], &param.hdr);
      if (status != MMAL_SUCCESS) {vcos_log_error("Unable to reset QP");}
      *write_variance = 0;
      }
    else 
      {
      if (*write_variance > 0 && param.value < state->quantisationMax)
        {
        param.value++;
        atQlimit = MMAL_FALSE;
        fprintf(stdout, "Quantization %d\n", param.value);
        status = mmal_port_parameter_set(state->encoder_component->output[0], &param.hdr);
        if (status != MMAL_SUCCESS) {vcos_log_error("Unable to reset QP");}
        *write_variance = 0;
        }
      else
        {
        if ((param.value == state->quantisationMax || param.value == state->quantisationMin) && !(atQlimit))
          {
          fprintf(stdout, "Quantization at limit %d\n", param.value);
          atQlimit = MMAL_TRUE;
          }
        *write_variance = 0;
        }
      }
    }
}

void send_text(int speed, RASPIVID_STATE *state)
{
  static int last_speed=-2, font_space;
  MMAL_BUFFER_HEADER_T *buffer_header=NULL;
  int font_size=state->common_settings.height/21;
  if (last_speed == -2) 
    {
    cairo_surface_t *temp_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, TEXTW, TEXTH);
    cairo_t *temp_context =  cairo_create(temp_surface);
    cairo_rectangle(temp_context, 0, 0, TEXTW, TEXTH);
    cairo_select_font_face(temp_context, "cairo:serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(temp_context, font_size);
    cairo_text_extents_t extents;
    cairo_text_extents(temp_context, "888 mph", &extents);
    font_space=TEXTW-extents.x_advance;
    cairo_destroy(temp_context);
    cairo_surface_destroy(temp_surface);
    }

  if (last_speed != speed)
    {
    if ((buffer_header = mmal_queue_get(state->hvs_textin_pool->queue)) != NULL)
      {
      if (speed < 0)
        {
        buffer_header->length=buffer_header->alloc_size=0;
        buffer_header->user_data=NULL;
        } 
      else
        {
        cairo_surface_t *image=cairo_text(speed, font_size, font_space);	
        buffer_header->data=cairo_image_surface_get_data(image);
        buffer_header->length=buffer_header->alloc_size=
        cairo_image_surface_get_height(image)*cairo_image_surface_get_stride(image);
        } 
      buffer_header->cmd=buffer_header->offset=0;
      int status=mmal_port_send_buffer(state->hvs_component->input[2], buffer_header);
      if (status) printf("buffer send of text overlay failed\n");
      }
    last_speed=speed;
    }
}

int allocate_fmtctx(char *dest, FORMAT_CTX *fctx, RASPIVID_STATE *state) 
// int allocate_fmtctx(char *dest, RASPIVID_STATE *state) 
{
  int status=0;
//  printf("%p %p\n", fctx, &state->filectx);
/*  FORMAT_CTX *fctx;
  if (memcmp(dest, "file:", 5)) {fctx=&state->filectx;}
  else {fctx=&state->urlctx;} */
  
	AVDictionary *options = NULL;
//  setup format context and io context

	avformat_alloc_output_context2(&fctx->fmtctx, NULL, "flv", NULL);
	if (!fctx->fmtctx) 
		{
		fprintf(stderr, "Could not allocate output format context\n");
		return -1;
		}
	if (!(fctx->fmtctx->url = av_strdup(dest))) 
		{
        fprintf(stderr, "Could not copy url.\n");
        return -1;
		}
// Setup  H264 codec
	AVCodec *h264_codec = avcodec_find_encoder(AV_CODEC_ID_H264);
	if (!h264_codec)
		{
		fprintf(stderr, "H264 codec id not found!\n");
		return -1;
		}	
	AVStream *h264_video_strm = avformat_new_stream(fctx->fmtctx, NULL);
	if (!h264_video_strm) 
		{
		fprintf(stderr, "Could not allocate H264 stream\n");
		return -1;
		}
        
	fctx->vidctx = avcodec_alloc_context3(h264_codec); 
	if (!fctx->vidctx) 
		{
		fprintf(stderr, "Could not alloc an video encoding context\n");
		return -1;
		}	

  fctx->vidctx->codec_id = AV_CODEC_ID_H264;
	fctx->vidctx->bit_rate = 0;
  fctx->vidctx->qmin = state->quantisationMin;
	fctx->vidctx->qmax = state->quantisationMax;
  fctx->vidctx->width = fctx->vidctx->coded_width  = state->common_settings.width;
  fctx->vidctx->height = fctx->vidctx->coded_height = state->common_settings.height;
  
  fctx->vidctx->sample_rate = state->framerate;
	fctx->vidctx->gop_size = state->intraperiod;                  
	fctx->vidctx->pix_fmt = AV_PIX_FMT_YUV420P; 
	status = avcodec_parameters_from_context(h264_video_strm->codecpar, fctx->vidctx);
	if (status < 0) 
		{
		fprintf(stderr, "Could not initialize stream parameters\n");
		return -1;
		}
    
  h264_video_strm->time_base.den = state->framerate;   // Set the sample rate for the container
	h264_video_strm->time_base.num = 1;
	h264_video_strm->avg_frame_rate.num = state->framerate;   // Set the sample rate for the container
	h264_video_strm->avg_frame_rate.den = 1;
	h264_video_strm->r_frame_rate.num = state->framerate;   // Set the sample rate for the container
	h264_video_strm->r_frame_rate.den = 1;

	if (fctx->fmtctx->oformat->flags & AVFMT_GLOBALHEADER) { // Some container formats (like MP4) require global headers to be present.
		fctx->vidctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;}
		
	if ((status = av_dict_set(&options, "rtmp_live", "live", 0)) < 0) {
        fprintf(stderr, "rtmp live option: %s\n", av_err2str(status));}
    	
  if (memcmp(dest, "file:", 5)) options=NULL;
  if ((status = avio_open2(&fctx->ioctx, dest, AVIO_FLAG_WRITE, NULL, &options)))
		{
		fprintf(stderr, "Could not open output file '%s' (error '%s')\n", dest, av_err2str(status));
		return -1;
		}
        
	fctx->fmtctx->pb = fctx->ioctx;
		
//  setup AAC codec and stream context
	AVCodec *aac_codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
	if (!aac_codec)
		{
		fprintf(stderr, "AAC codec id not found!\n");
		return -1;
		}	
	AVStream *aac_audio_strm = avformat_new_stream(fctx->fmtctx, NULL);
	if (!aac_audio_strm) 
		{
		fprintf(stderr, "Could not allocate AAC stream\n");
		return -1;
		}
        
	fctx->audctx = avcodec_alloc_context3(aac_codec); 
	if (!fctx->audctx) 
		{
		fprintf(stderr, "Could not alloc an encoding context\n");
		return -1;
		}
	fctx->audctx->channels       = iparms.channels;
	fctx->audctx->channel_layout = av_get_default_channel_layout(iparms.channels);
	fctx->audctx->sample_rate    = DEFAULT_SPEED;
	fctx->audctx->sample_fmt     = aac_codec->sample_fmts[0];
	fctx->audctx->bit_rate       = 64000;
	fctx->audctx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;  // Allow the use of the experimental AAC encoder.

	aac_audio_strm->time_base.den = DEFAULT_SPEED;   // Set the sample rate for the container
	aac_audio_strm->time_base.num = 1;
    
	if (fctx->fmtctx->oformat->flags & AVFMT_GLOBALHEADER)  // Some container formats (like MP4) require global headers to be present.
		fctx->audctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        
	if ((status = avcodec_open2(fctx->audctx, aac_codec, NULL) < 0)) 
		{
		fprintf(stderr, "Could not open output codec (error '%s')\n", av_err2str(status));
		return -1;
		}
	status = avcodec_parameters_from_context(aac_audio_strm->codecpar, fctx->audctx);
	if (status < 0) 
		{
		fprintf(stderr, "Could not initialize stream parameters\n");
		return -1;
		}
//  write flv header 
	fctx->fmtctx->start_time_realtime=get_microseconds64();  // flv_frmtctx->start_time_realtime=0;  // 0 should user system clock
/*	status = avformat_init_output(fctx->fmtctx, &options);  // null if AVDictionary is unneeded????
	if (status < 0)
		{
		fprintf(stderr, "Write ouput header failed! STATUS %d\n", status);
		return -1;
		}  */

	status = avformat_write_header(fctx->fmtctx, NULL);  // null if AVDictionary is unneeded????
	if (status < 0)
		{
		fprintf(stderr, "Write ouput header failed! STATUS %d\n", status);
		return -1;
		}
//  av_dump_format(fctx->fmtctx, 0, "stdout", 1);
  return 0;
}

int free_fmtctx(FORMAT_CTX *fctx)
{
  int status=0;

	if (fctx->vidctx) {avcodec_free_context(&fctx->vidctx);}
	if (fctx->audctx) {avcodec_free_context(&fctx->audctx);}
		
	if (fctx->fmtctx)
		{
		if (fctx->ioctx && fctx->ioctx->seekable == 1)
			{
			status = av_write_trailer(fctx->fmtctx);  
			if (status < 0) {fprintf(stderr, "Write ouput trailer failed! STATUS %d\n", status);}
			}  
		status = avio_close(fctx->ioctx);	
		if (status < 0)
			{
			fprintf(stderr, "Could not close output file (error '%s')\n", av_err2str(status));
			return -1; 
			}
		avformat_free_context(fctx->fmtctx);
    fctx->fmtctx=NULL;
		}
	return 0;
}

int allocate_audio_encode(AENCODE_CTX *actx) // need aacctx, rawctx,resamplectx, fifoctx
{
  int status=0;
//  setup RAW codec and context
	AVCodec *raw_codec = avcodec_find_encoder(AV_CODEC_ID_PCM_S32LE_PLANAR);
	if (!raw_codec)
		{
		fprintf(stderr, "PCM_S32_LE codec id not found!\n");
		return -1;
		}	
	actx->rawctx = avcodec_alloc_context3(raw_codec); 
	if (!actx->rawctx) 
		{
		fprintf(stderr, "Could not alloc RAW context\n");
		return -1;
		}
    
	actx->rawctx->channels       = DEFAULT_CHANNELS_IN;
	actx->rawctx->channel_layout = av_get_default_channel_layout(DEFAULT_CHANNELS_IN);
	actx->rawctx->sample_rate    = DEFAULT_SPEED;
	actx->rawctx->sample_fmt     = raw_codec->sample_fmts[0];  // AV_SAMPLE_FMT_S32
	actx->rawctx->bit_rate       = 2822400;  // or 64000
	actx->rawctx->time_base.num  = 1;
	actx->rawctx->time_base.den  = DEFAULT_SPEED;
	actx->rawctx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;   // Allow the use of the experimental AAC encoder.
    
//  setup resampler context
	actx->swrctx = swr_alloc_set_opts(NULL, av_get_default_channel_layout(actx->audctx->channels), actx->audctx->sample_fmt,
		actx->audctx->sample_rate, av_get_default_channel_layout(actx->rawctx->channels), actx->rawctx->sample_fmt,
		actx->rawctx->sample_rate, 0, NULL);
	if (!actx->swrctx) 
		{
		fprintf(stderr, "Could not allocate resample context\n");
		return -1;
		}
	if ((status = swr_init(actx->swrctx)) < 0) 
		{
		fprintf(stderr, "Could not open resample context\n");
		swr_free(&actx->swrctx);
		return -1;
		}

// setup fifo sample queue
	if (!(actx->fifo = av_audio_fifo_alloc(AV_SAMPLE_FMT_S32P, DEFAULT_CHANNELS_IN, 1))) 
		{
		fprintf(stderr, "Could not allocate FIFO\n");
		return -1;
		}
    
// allocate and init work frames
	actx->infrm=av_frame_alloc();	
	if (!actx->infrm) {fprintf(stderr, "unable to allocate in frame!\n");}

	actx->infrm->channel_layout=actx->rawctx->channel_layout;
	actx->infrm->sample_rate=actx->rawctx->sample_rate;
	actx->infrm->format=actx->rawctx->sample_fmt;
	actx->infrm->nb_samples=actx->audctx->frame_size;  
    
	status=av_frame_get_buffer(actx->infrm, 0);  
	if (status) {fprintf(stderr, "unable to allocate in frame data! %d %s\n", status, av_err2str(status));}
    
	actx->outfrm=av_frame_alloc();	
	if (!actx->outfrm) {fprintf(stderr, "unable to allocate out frame!\n");}
	actx->outfrm->channel_layout=actx->audctx->channel_layout;
	actx->outfrm->sample_rate=actx->audctx->sample_rate;
	actx->outfrm->format=actx->audctx->sample_fmt;
	actx->outfrm->nb_samples=actx->audctx->frame_size;

	status=av_frame_get_buffer(actx->outfrm, 0);
	if (status) {fprintf(stderr, "unable to allocate out frame data!\n");}

	return 0; 
}

void free_audio_encode(AENCODE_CTX *actx)
{
  if (actx->outfrm) {av_frame_free(&actx->outfrm);}
	if (actx->infrm) {av_frame_free(&actx->infrm);}
	
	if (actx->fifo) {av_audio_fifo_free(actx->fifo);}

	if (actx->swrctx) {swr_init(actx->swrctx);}
  
  if (actx->rawctx) {avcodec_free_context(&actx->rawctx);}
}

int create_video_stream(RASPIVID_STATE *state)
{
  MMAL_STATUS_T status = MMAL_SUCCESS;
	MMAL_PORT_T *camera_video_port = NULL;
	MMAL_PORT_T *camera2_video_port = NULL;
	MMAL_PORT_T *hvs_main_input_port = NULL;
	MMAL_PORT_T *hvs_ovl_input_port = NULL;
	MMAL_PORT_T *hvs_text_input_port = NULL;
	MMAL_PORT_T *hvs_output_port = NULL;
   
    // Setup for sensor specific parameters, only set W/H settings if zero on entry
	int cam = state->common_settings.cameraNum, cam2 = 0, max_width = 0, max_height = 0;
	get_sensor_defaults(state->common_settings.cameraNum, state->common_settings.camera_name,
                       &max_width, &max_height);
                        

  if (state->common_settings.width > max_width || state->common_settings.height > max_height || 
		state->common_settings.ovl.width > max_width || state->common_settings.ovl.height > max_height)
    {
		fprintf(stdout, "Resolution larger than sensor %dX%d\n", max_width, max_height);
		return -1;
    }
		
  state->camera_parameters.stereo_mode.mode = MMAL_STEREOSCOPIC_MODE_NONE;

	if (!cam) {cam2 = 1;}

	if ((status = create_camera_component(state)) != MMAL_SUCCESS)
		{
		vcos_log_error("%s: Failed to create main camera %d component", __func__, cam);
		return -1;
		}
		
	int save_width=state->common_settings.width, save_height=state->common_settings.height;
	state->common_settings.width = state->common_settings.ovl.width;
	state->common_settings.height = state->common_settings.ovl.height;
	state->common_settings.cameraNum = cam2;
	state->camera_parameters.vflip = iparms.fov;
	state->camera_parameters.hflip = iparms.foh;
     
	if ((status = create_camera_component(state)) != MMAL_SUCCESS)
		{
		vcos_log_error("%s: Failed to create overlay camera %d component", __func__, cam2);
		return -1;
		}

	state->common_settings.width = save_width;
	state->common_settings.height = save_height;
	state->common_settings.cameraNum = cam;
	state->camera_parameters.vflip = iparms.fmv;
	state->camera_parameters.hflip = iparms.fmh;

  if ((status = create_hvs_component(state)) != MMAL_SUCCESS)
		{
		vcos_log_error("%s: Failed to create hvs component", __func__);
		destroy_camera_component(state);
		return -1;
		} 
   
  camera_video_port   = state->camera_component->output[MMAL_CAMERA_VIDEO_PORT];
  camera2_video_port   = state->camera2_component->output[MMAL_CAMERA_VIDEO_PORT];

	hvs_main_input_port = state->hvs_component->input[0];
  hvs_ovl_input_port  = state->hvs_component->input[1];
  hvs_text_input_port  = state->hvs_component->input[2];
  hvs_output_port     = state->hvs_component->output[0];
     
  if ((status = connect_ports(camera_video_port, hvs_main_input_port, &state->hvs_main_connection)) != MMAL_SUCCESS)
    {
		vcos_log_error("%s: Failed to connect camera video port to hvs input", __func__); 
		state->hvs_main_connection = NULL;
		return -1;
    }
	
	if ((status = connect_ports(camera2_video_port, hvs_ovl_input_port, &state->hvs_ovl_connection)) != MMAL_SUCCESS)
    {
		vcos_log_error("%s: Failed to connect camera2 video port to hvs input", __func__); 
		state->hvs_ovl_connection = NULL;
		return -1;
    } 
	
	hvs_text_input_port->buffer_num = hvs_text_input_port->buffer_num_min+1;
	hvs_text_input_port->buffer_size = hvs_text_input_port->buffer_size_min;
	state->hvs_textin_pool = mmal_pool_create(hvs_text_input_port->buffer_num, hvs_text_input_port->buffer_size);

	if ((status = mmal_port_enable(hvs_text_input_port, hvs_input_callback)) != MMAL_SUCCESS)
    {
		vcos_log_error("%s: Failed to enable hvs text input", __func__); 
		return -1;
    } 	
    
	return 0;
}

int allocate_alsa(AENCODE_CTX *actx)
{
	snd_pcm_hw_params_t *params;
	snd_pcm_uframes_t buffer_size = 0;
  snd_pcm_uframes_t chunk_size = 0;
  snd_pcm_uframes_t buffer_frames = 0;
  size_t bits_per_sample, bits_per_frame;
  size_t chunk_bytes;
  struct {
	snd_pcm_format_t format;
	unsigned int channels;
	unsigned int rate;
  } hwparams, rhwparams; 
  
  rhwparams.format = DEFAULT_FORMAT;
	rhwparams.rate = DEFAULT_SPEED;
	rhwparams.channels = DEFAULT_CHANNELS_IN;
	chunk_size = 1024;
	hwparams = rhwparams;

	int err;

	snd_pcm_info_t *info;

	snd_pcm_info_alloca(&info);

  err = snd_pcm_open(&actx->pcmhnd, iparms.adev, SND_PCM_STREAM_CAPTURE, 0);
	if (err < 0) {
		fprintf(stdout, "%s open error: %s\n", iparms.adev, snd_strerror(err));
		return -1;
	}

  snd_pcm_hw_params_alloca(&params);
	err = snd_pcm_hw_params_any(actx->pcmhnd, params);
	if (err < 0) 
		{
		fprintf(stderr, "Broken configuration for this PCM: no configurations available\n");
    return -1;
		}

  err = snd_pcm_hw_params_set_access(actx->pcmhnd, params, SND_PCM_ACCESS_RW_INTERLEAVED);

	if (err < 0) 
		{
		fprintf(stderr, "Access type not available\n");
    return -1;
		}
	err = snd_pcm_hw_params_set_format(actx->pcmhnd, params, hwparams.format);
	if (err < 0) 
		{
		fprintf(stderr, "Sample format non available\n");
    return -1;
		}
	err = snd_pcm_hw_params_set_channels(actx->pcmhnd, params, hwparams.channels);
	if (err < 0) 
		{
		fprintf(stderr, "Channels count non available\n");
    return -1;
		}
	err = snd_pcm_hw_params_set_rate_near(actx->pcmhnd, params, &hwparams.rate, 0);
	assert(err >= 0);

	err = snd_pcm_hw_params(actx->pcmhnd, params);
	if (err < 0) 
		{
		fprintf(stderr, "Unable to install hw params\n");
    return -1;
		}
		
	snd_pcm_hw_params_get_period_size(params, &chunk_size, 0);
	snd_pcm_hw_params_get_buffer_size(params, &buffer_size);
	if (chunk_size == buffer_size) 
		{
		fprintf(stderr, "Can't use period equal to buffer size (%lu == %lu)\n", 
		      chunk_size, buffer_size);
    return -1;
		}

	bits_per_sample = snd_pcm_format_physical_width(hwparams.format);
	bits_per_frame = bits_per_sample * hwparams.channels;
	actx->bufsize = chunk_size * bits_per_frame / 8;
//  printf("bufsize %d\n", actx->bufsize);

	actx->pcmbuf = (u_char *)malloc(actx->bufsize);
	if (actx->pcmbuf == NULL) 
		{
		fprintf(stderr, "not enough memory\n");
    return -1;
		}
	actx->rlbufs = (u_char *)malloc(actx->bufsize);
	if (actx->rlbufs == NULL) 
		{
		fprintf(stderr, "not enough memory\n");
    return -1;
		}
  return 0;
}

int free_alsa(AENCODE_CTX *actx)
{
  if (actx->pcmhnd) {
		snd_pcm_close(actx->pcmhnd);
		actx->pcmhnd = NULL;}
	free(actx->pcmbuf);
	free(actx->rlbufs);
} 

int toggle_stream(RASPIVID_STATE *state, int run_status)
{
  if (state->encodectx.pcmhnd && run_status)
    {
    snd_pcm_drop(state->encodectx.pcmhnd);
    snd_pcm_prepare(state->encodectx.pcmhnd);
    snd_pcm_start(state->encodectx.pcmhnd);
    } 

  bcm2835_gpio_write(GPIO_LED, run_status);
  bcm2835_gpio_write(GPIO_MODEM_LED, run_status);
  
	mmal_port_parameter_set_boolean(state->camera_component->output[MMAL_CAMERA_VIDEO_PORT], MMAL_PARAMETER_CAPTURE, run_status);
	mmal_port_parameter_set_boolean(state->camera2_component->output[MMAL_CAMERA_VIDEO_PORT], MMAL_PARAMETER_CAPTURE, run_status);

}

void destroy_video_stream(RASPIVID_STATE *state)
{
      // Disable all our ports that are not handled by connections
  if (state->camera_component) {
		check_disable_port(state->camera_component->output[MMAL_CAMERA_PREVIEW_PORT]);  
		check_disable_port(state->camera_component->output[MMAL_CAMERA_CAPTURE_PORT]);}
	if (state->camera2_component) {
		check_disable_port(state->camera2_component->output[MMAL_CAMERA_PREVIEW_PORT]); 
		check_disable_port(state->camera2_component->output[MMAL_CAMERA_CAPTURE_PORT]);}  
   
	if (state->hvs_main_connection)
		mmal_connection_destroy(state->hvs_main_connection);
	if (state->hvs_ovl_connection)
		mmal_connection_destroy(state->hvs_ovl_connection);
	if (state->hvs_component->input[2]->is_enabled)
		mmal_port_disable(state->hvs_component->input[2]);

    // Disable and destroy components 
	if (state->hvs_component)
		mmal_component_disable(state->hvs_component);
	if (state->camera_component)
		mmal_component_disable(state->camera_component);
	if (state->camera2_component)
		mmal_component_disable(state->camera2_component);

	destroy_hvs_component(state);
	destroy_camera_component(state);

	return;
}
int write_audio(RASPIVID_STATE *state)
{
	int status; 
  AVFrame *infrm = state->encodectx.infrm;
  AVFrame *outfrm = state->encodectx.outfrm;
  AVAudioFifo *fifo = state->encodectx.fifo;
  int64_t start_time = state->encodectx.start_time;
  AVFormatContext *urlctx = state->urlctx.fmtctx;
  AVFormatContext *filectx = state->filectx.fmtctx;
  SwrContext *resample_ctx = state->encodectx.swrctx;
  AVCodecContext *aac_codec_ctx = state->encodectx.audctx;
  sem_t *mutex = state->callback_data.mutex;
	AVPacket packet;
	int64_t save_pts=0, calc_pts;

	while (av_audio_fifo_size(fifo) >= infrm->nb_samples)
	{
  outfrm->pts = outfrm->pkt_dts = save_pts = get_microseconds64()/1000-state->encodectx.start_time;
	status = av_audio_fifo_read(fifo, (void **)infrm->data, infrm->nb_samples);
	if (status < 0) 
		{
		fprintf(stderr, "fifo read failed! %d %s\n", status, av_err2str(status));
		return -1;
		}
	
	status = swr_convert_frame(resample_ctx, outfrm, infrm);
	if (status) {fprintf(stderr, "Frame convert %d (error '%s')\n", status, av_err2str(status));}
	
	av_init_packet(&packet); // Set the packet data and size so that it is recognized as being empty. 
	packet.data = NULL;
	packet.size = 0;

	status = avcodec_send_frame(aac_codec_ctx, outfrm);  
	if (status == AVERROR_EOF) // The encoder signals that it has nothing more to encode.
		{
		status = 0;
		fprintf(stderr, "EOF at send frame\n");
		goto cleanup;
		}
	 else 
		if (status < 0)
			{
			fprintf(stderr, "Could not send packet for encoding (error '%s')\n", av_err2str(status));
			return status;
			}
      
	status = avcodec_receive_packet(aac_codec_ctx, &packet);
	if (status == AVERROR(EAGAIN)) // If the encoder asks for more data to be able to provide an encoded frame, return indicating that no data is present.
		{
		status = 0;
		} 
	else 
		if (status == AVERROR_EOF) // If the last frame has been encoded, stop encoding.
			{
			status = 0;
			fprintf(stderr, "EOF at receive packet\n");
			goto cleanup;
			} 
		else 
			if (status < 0) 
				{
				fprintf(stderr, "Could not encode frame (error '%s')\n", av_err2str(status));  //get this if not loaded frame
				goto cleanup;
    			} 
			else 
				{
				packet.duration=0;
				packet.pos=-1;
				packet.dts=packet.pts=save_pts-250;
				packet.stream_index = 1;
				sem_wait(mutex);
				if (urlctx) status = av_write_frame(urlctx, &packet);
        if (filectx) status += av_write_frame(filectx, &packet);
				sem_post(mutex); 
				if (status < 0) 
					{
					fprintf(stderr, "Could not audio write frame (error '%s')\n", av_err2str(status));
					goto cleanup;
					}
				}
	}
	return status;


cleanup:
	av_packet_unref(&packet);
	return status;
}

void flush_audio(RASPIVID_STATE *state)
{
  state->encodectx.infrm->nb_samples=1;
  write_audio(state);
  int rc=0, write=0;
	if (state->encodectx.audctx)
		{
		AVPacket packet;
		av_init_packet(&packet); 
		packet.data = NULL;
		packet.size = 0;
		avcodec_send_frame(state->encodectx.audctx, NULL); 
		rc = avcodec_receive_packet(state->encodectx.audctx, &packet);
		while (!rc) {
			packet.pts = packet.dts = get_microseconds64()/1000-state->encodectx.start_time;
      if (state->urlctx.fmtctx) write = av_write_frame(state->urlctx.fmtctx, &packet);
      if (state->filectx.fmtctx) write += av_write_frame(state->filectx.fmtctx, &packet);
      if (write) printf("Flush audio write status %d\n", write);
			rc = avcodec_receive_packet(state->encodectx.audctx, &packet);
			};
		} 
}
void xrun(snd_pcm_t *handle)
{
	snd_pcm_status_t *status;
	int res;
	snd_pcm_status_alloca(&status);
	if ((res = snd_pcm_status(handle, status))<0) 
		{
		fprintf(stderr, "status error: %s\n", snd_strerror(res));
    exit(-2);
		}
	
	if (snd_pcm_status_get_state(status) == SND_PCM_STATE_XRUN) 
		{
		if ((res = snd_pcm_prepare(handle))<0) 
			{
			fprintf(stderr, "xrun: prepare error: %s\n", snd_strerror(res));
      exit(-2);
			}
		return;		/* ok, data should be accepted again */
		} 
		if (snd_pcm_status_get_state(status) == SND_PCM_STATE_DRAINING) 
			{
				{
				fprintf(stderr, "capture stream format change? attempting recover...\n");
				if ((res = snd_pcm_prepare(handle))<0) 
					{
					fprintf(stderr, "xrun(DRAINING): prepare error: %s\n", snd_strerror(res));
          exit(-2);
					}
				return;
				}
			}
		fprintf(stderr, "read/write error, state = %s\n", snd_pcm_state_name(snd_pcm_status_get_state(status)));
    exit(-2);
}
/* I/O suspend handler */
void suspend(snd_pcm_t *handle)
{
	int res;
	while ((res = snd_pcm_resume(handle)) == -EAGAIN)
		sleep(1);	/* wait until suspend flag is released */
	if (res < 0) {
		if ((res = snd_pcm_prepare(handle)) < 0) {
			fprintf(stderr, "suspend: prepare error: %s\n", snd_strerror(res));
      exit(-3);
		}
	}
}
/*
 *  read function
 */
int read_pcm(RASPIVID_STATE *state)
{
  snd_pcm_status_t *pcm_status;
	snd_pcm_status_alloca(&pcm_status);
  snd_pcm_t *handle = state->encodectx.pcmhnd;
  u_char *data_in = state->encodectx.pcmbuf;
  AVAudioFifo *fifo = state->encodectx.fifo;
	ssize_t r; 
	size_t result = 0;
  size_t count=256;
	
	while (count > 0) 
		{
		r = snd_pcm_readi(handle, data_in, count);

		if (r == -EAGAIN || (r >= 0 && (size_t)r < count)) 
			{
			fprintf(stderr, "wait\n");
			snd_pcm_wait(handle, 100);
			}
		else if (r == -EPIPE) 
			{
      fprintf(stderr, "xrun\n");
			xrun(handle);
			} 
		else if (r == -ESTRPIPE)
			{
      fprintf(stderr, "suspend\n");
			suspend(handle);
			} 
		else if (r < 0) 
			{
			fprintf(stderr, "read error: %s\n", snd_strerror(r));
      exit(-5);	
			}
			if (r > 0) 
				{
				result += r;
				count -= r;
				data_in += r * 8;  
				}
		}

	size_t i;   
	int s, x, lr=0;
	u_char *lptr=state->encodectx.rlbufs;
  u_char *rptr=state->encodectx.rlbufs+(state->encodectx.bufsize/2);
//  rptr+=1024;
  data_in = state->encodectx.pcmbuf;
  u_char *data_out[2] = {lptr,rptr};

  x=512;  // number of right and left samples
	for (i=0; i < x; ++i) {
		for (s=0;s < 4; ++s) {
			if (lr) {*rptr = *data_in; ++rptr;}
			else {*lptr = *data_in; ++lptr;}
			++data_in;}
			if (lr) {lr=0;}
			else {lr=1;}}

	int status;		

  status=av_audio_fifo_write(fifo, (void **)data_out, r);
	if (status < 0)
		{
		fprintf(stderr, "fifo write failed!\n");
		}
	else
		if (status != r) 
			{
			fprintf(stderr, "fifo did not write all! to write %d written %d\n", r, status);
			}
  write_audio(state);
  return result;
}

void *gps_thread(void *argp)
{
   GPS_T *gps = (GPS_T *)argp;
   gps->active=1;
   gps->speed=-1; 
  
   open_gps(gps);
    
   while (gps->active) 
      {  
      read_gps(gps);
      vcos_sleep(100);
      }
 
   close_gps(gps);
}

void *record_thread(void *argp)
{
  RASPIVID_STATE *state = (RASPIVID_STATE *)argp;
  parms_to_state (state);
  state->callback_data.pstate = state;
  state->encodectx.start_time = get_microseconds64()/1000;
  state->recording=1;
  	
  AVPacket video_packet;
	av_init_packet(&video_packet);
	video_packet.stream_index=0;
	video_packet.duration=0;
	video_packet.pos=-1;
  state->callback_data.vpckt=&video_packet;

  GPS_T gps_data;
  pthread_t gps_tid;
  if (state->gps) 
    {
    pthread_create(&gps_tid, NULL, gps_thread, (void *)&gps_data);
    }
  // set wtargettime ????? correct ????
  state->callback_data.wtargettime = TARGET_TIME/state->framerate;
  // allocate video buffer
	state->callback_data.vbuf = (u_char *)malloc(BUFFER_SIZE);
	if (state->callback_data.vbuf == NULL) 
		{
		fprintf(stderr, "not enough memory vbuf\n");
    goto err_gps;
		}
  // allocate mutex
	sem_t def_mutex;
	sem_init(&def_mutex, 0, 1);
	state->callback_data.mutex=&def_mutex;
  
  int length;
  char str[64];
  //	if file allocate_fmtctx (file, ctx)
  if (iparms.write_file)
    {
    time_t time_uf;
    struct tm *time_fmt;
    time(&time_uf);
    time_fmt = localtime(&time_uf);
    strcpy(str, "file:");
    length=strlen(str);
    strcpy(str+length, iparms.file);
    length=strlen(str);
    strftime(str+length, 20,"%Y-%m-%d_%H_%M_%S", time_fmt);
    length=strlen(str);
    strcpy(str+length, ".flv");
    if (allocate_fmtctx(str, &state->filectx, state))
      {
      printf("Allocate %s context failed\n", str);
      goto err_file;
      }
    state->encodectx.audctx=state->filectx.audctx;
    }
  //	if url allocate_fmtctx (url, ctx)  
  if (iparms.write_url)
    {
    strcpy(str, "rtmp://");
    length=strlen(str);
    strcpy(str+length, iparms.url);
    printf("%s\n", str);
    if (allocate_fmtctx(str, &state->urlctx, state))
      {
      printf("Allocate %s context failed\n", str);
      goto err_url;
      }
    state->encodectx.audctx=state->urlctx.audctx;
    } 
  //	allocate_audio_encode(encodectx)
  if (allocate_audio_encode(&state->encodectx)) goto err_aencode;
  //	allocate_alsa(encodectx)
  if (allocate_alsa(&state->encodectx)) goto err_alsa;
  //	create_video_stream(state)
  if (create_video_stream(state)) goto err_vstream;
  //	create_encoder(state)
  if (create_encoder_component(state)) goto err_encoder;
//  g_signal_emit_by_name(stop_ptr, "clicked");
//  printf("after emit\n");
//  goto err_encoder;
  //	connect encoder
  MMAL_STATUS_T status; 
  status = connect_ports(state->hvs_component->output[0], state->encoder_component->input[0], &state->encoder_connection);
  if (status != MMAL_SUCCESS)
    {
		vcos_log_error("%s: Failed to connect hvs to encoder input", __func__); 
		state->encoder_connection = NULL;
    goto err_audio;
 // change to goto label   
//    exit(120);
    }
  // Set up our userdata - this is passed though to the callback where we need the information.
	state->encoder_component->output[0]->userdata = (struct MMAL_PORT_USERDATA_T *)&state->callback_data;

    // Enable the encoder output port and tell it its callback function
	status = mmal_port_enable(state->encoder_component->output[0], encoder_buffer_callback);
	if (status) 
		{
		fprintf(stderr, "enable port failed\n");
		}
	
	// Send all the buffers to the encoder output port
	int num = mmal_queue_length(state->encoder_pool->queue);    
	int q;
	for (q=0; q<num; q++)
		{
		MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get(state->encoder_pool->queue);
		if (!buffer)
			vcos_log_error("Unable to get a required buffer %d from pool queue", q);
		if (mmal_port_send_buffer(state->encoder_component->output[0], buffer)!= MMAL_SUCCESS)
			vcos_log_error("Unable to send a buffer to encoder output port (%d)", q);
		}
 
  //	toggle_stream
  toggle_stream(state, START);
    
  while (state->recording) 
      {
//      printf("here10\n");
      // read_PCM
      read_pcm(state);
      // adjust_Q
      adjust_q(state);
      // send_text
      if (state->gps) 
        {
        send_text(gps_data.speed, state);
        }
 /*     // run time message
      printf("message\n");
      if (message1)
        {
        int64_t raw_time=(get_microseconds64()/1000)-state->encodectx.start_time;
  //      printf("%lld\n", raw_time);
  //      printf("in if\n");
        char buf[9];
        int hours=0, mins=0, secs=0;
        raw_time = raw_time/1000;
        secs = raw_time % 60;
        raw_time = (raw_time-secs)/60;
        mins = raw_time%60;
        hours = (raw_time-mins)/60;       
        sprintf(buf, "%2d:%02d:%02d", hours, mins, secs);
        printf("%s\n", buf);
        gtk_label_set_text (GTK_LABEL(message1), buf);
        gtk_widget_show_all(stop_win);
        }  */
      
  //    vcos_sleep(50);
      }
      
  // toggle_stream(state, int)
  toggle_stream(state, STOP);

  //	disconnect encoder
  if (state->encoder_component)
		check_disable_port(state->encoder_component->output[0]);
	if (state->encoder_connection)
		mmal_connection_destroy(state->encoder_connection);
  // write_audio
err_audio:
  flush_audio(state);
err_encoder:
  // destroy_encoder
  destroy_encoder_component(state);
err_vstream:
  // destroy_video_stream
  destroy_video_stream(state);
err_alsa:
  // free_alsa
  free_alsa(&state->encodectx);
err_aencode:
  // free_audio_encode 
  free_audio_encode(&state->encodectx);
err_url:  
  // if url free_fmtctx (url_ctx)
  if (state->urlctx.fmtctx) free_fmtctx(&state->urlctx);
err_file:
  // if file free_fmtctx (file_ctx)
  if (state->filectx.fmtctx) free_fmtctx(&state->filectx);
  // free  vbuf 
  free(state->callback_data.vbuf);
  sem_destroy(&def_mutex);
err_gps:
    if (state->gps) 
      {
      gps_data.active=0;
      pthread_join(gps_tid, NULL);
      }
  av_packet_unref(&video_packet);
}

int read_parms(void)
  {
  FILE *parm_file;
  parm_file=fopen("/home/pi/racecam.ini", "rb");
  if (parm_file)
    {
    size_t cnt=0;
    cnt=fread(&iparms, 1, sizeof(iparms), parm_file);
    if (cnt!=sizeof(iparms)) return 1;
    if (fclose(parm_file)) return 1;
    }
  else
    {
    return 1;
    } 
  return 0;
}

int write_parms(void)
  {
  FILE *parm_file;
  parm_file=fopen("/home/pi/racecam.ini", "wb");
  size_t cnt=0;
  cnt=fwrite(&iparms, 1, sizeof(iparms), parm_file);
  if (cnt!=sizeof(iparms)) return 1;
  if (fclose(parm_file)) return 1;
  return 0;
  }
  

void inc_val_lbl(GtkWidget *widget, gpointer data)
{
  limit *lmt=data;
  if (*lmt->val < *lmt->max)
    {
    (*lmt->val)+=lmt->incv;
    char buf[3];
    sprintf(buf, lmt->format, *lmt->val);
    gtk_label_set_text (GTK_LABEL(lmt->label), buf);
    }
}

void dec_val_lbl(GtkWidget *widget, gpointer data)
{
  limit *lmt=data;
  if (*lmt->val > *lmt->min)
    {
    (*lmt->val)-=lmt->incv;
    char buf[3];
    sprintf(buf, lmt->format, *lmt->val);
    gtk_label_set_text (GTK_LABEL(lmt->label), buf);
    }
}

void swap_cam(GtkWidget *widget, gpointer data)
{
  char *cptr=data;
  if (*cptr)
    {
    gtk_button_set_label (GTK_BUTTON(widget), "Rear/Front");
    *cptr = 0;
    }
  else
    {
    gtk_button_set_label (GTK_BUTTON(widget), "Front/Rear");
    *cptr = 1;
    }
}

void check_status(GtkWidget *widget, gpointer data)
{
  check *chk=data;
  *chk->status = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(chk->button));  
}

void check_res0(GtkWidget *widget, gpointer data)
{
  if(gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(data))) iparms.main_size=0;  
}

void check_res1(GtkWidget *widget, gpointer data)
{
  if(gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(data))) iparms.main_size=1;  
}

void check_res2(GtkWidget *widget, gpointer data)
{
  if(gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(data))) iparms.main_size=2;  
}

void check_mono(GtkWidget *widget, gpointer data)
{
  if(gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(data))) iparms.channels=1;
}

void check_stereo(GtkWidget *widget, gpointer data)
{
  if(gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(data))) iparms.channels=2; 
}

void draw_it(ovrl *ol)
{
  GtkWidget *widget=ol->draw_area;
  gdk_draw_rectangle (pixmap, widget->style->black_gc, TRUE, 0, 0, *ol->x, *ol->y); 		
  int ox=(*ol->x)*(*ol->o_x);
  int oy=(*ol->y)*(*ol->o_y);
  int ow=(*ol->x)*(*ol->size);
  int oh=(*ol->y)*(*ol->size);
  gdk_draw_rectangle (pixmap, widget->style->white_gc, TRUE, ox, oy, ow, oh); 
  gtk_widget_queue_draw_area (widget, 0, 0, *ol->x, *ol->y); 
}

void inc_size(GtkWidget *widget, gpointer data)
{
  draw *dptr=data;
  if (*dptr->val < *dptr->max) 
    {
    *dptr->val += dptr->incv;
    *dptr->ol->o_x -= dptr->incv/2;
    if (*dptr->ol->o_x < *dptr->xymin) *dptr->ol->o_x += *dptr->xymin-*dptr->ol->o_x;  
    if ((*dptr->ol->o_x+*dptr->val) > *dptr->xymax) *dptr->ol->o_x += (*dptr->xymax-*dptr->val)-*dptr->ol->o_x;
    *dptr->ol->o_y -= dptr->incv/2;
    if (*dptr->ol->o_y < *dptr->xymin) *dptr->ol->o_y += *dptr->xymin-*dptr->ol->o_y;  
    if ((*dptr->ol->o_y+*dptr->val) > *dptr->xymax) *dptr->ol->o_y += (*dptr->xymax-*dptr->val)-*dptr->ol->o_y;
    draw_it(dptr->ol);
    }
}

void dec_size(GtkWidget *widget, gpointer data)
{
  draw *dptr=data; 
  if (*dptr->val > *dptr->min) 
    {
    *dptr->val -= dptr->incv;
    *dptr->ol->o_x += dptr->incv/2;
    *dptr->ol->o_y += dptr->incv/2;
    draw_it(dptr->ol);
    }
}

void inc_xy(GtkWidget *widget, gpointer data)
{
  draw *dptr=data;
  if (*dptr->val < (*dptr->max-*dptr->ol->size)) 
    {
    *dptr->val += dptr->incv;
    if(*dptr->val > (*dptr->max-*dptr->ol->size)) *dptr->val += (*dptr->max-*dptr->ol->size)-*dptr->val;
    draw_it(dptr->ol);
    }
}

void dec_xy(GtkWidget *widget, gpointer data)
{
  draw *dptr=data;
  if (*dptr->val > *dptr->min) 
    {
    *dptr->val -= dptr->incv;
    if(*dptr->val < *dptr->min) *dptr->val += *dptr->min-*dptr->val;
    draw_it(dptr->ol);
    }
}

void cancel_clicked(GtkWidget *widget, gpointer data)
{
  if (read_parms()) printf("read failed\n");
  gtk_widget_destroy(data);
  gtk_main_quit ();
}

void save_clicked(GtkWidget *widget, gpointer data)
{
  if (write_parms()) printf("write failed\n");
}

void done_clicked(GtkWidget *widget, gpointer data)
{
  if (write_parms()) printf("write failed\n");
  gtk_widget_destroy(data);
  gtk_main_quit ();
}

void stop_clicked(GtkWidget *widget, gpointer data)
{
  message1=NULL;
  gtk_widget_destroy(data);
  gtk_main_quit ();
}

void text_cb(GtkWidget *widget, gpointer data)
{
  char *cptr=data;
  const gchar *text;
  text = gtk_entry_get_text (GTK_ENTRY (widget));
  strcpy(cptr, text);
}

void widget_destroy(GtkWidget *widget, gpointer data)
{
   gtk_widget_destroy(widget);
//   gtk_widget_destroy(m_layout);
   gtk_main_quit ();
}

/* Create a new backing pixmap of the appropriate size */
static gboolean configure_event(GtkWidget *widget, GdkEventConfigure *event, gpointer data)
{
  ovrl *ol=data;
  
  if (pixmap)
    g_object_unref (pixmap);
    
  pixmap = gdk_pixmap_new (widget->window,
          *ol->x,		//	   widget->allocation.width,
          *ol->y,		//	   widget->allocation.height,
			   -1);
  draw_it(data);

  return TRUE; 
}

/* Redraw the screen from the backing pixmap */
static gboolean expose_event( GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
  gdk_draw_drawable (widget->window,
		     widget->style->fg_gc[gtk_widget_get_state (widget)],
		     pixmap,
		     event->area.x, event->area.y,
		     event->area.x, event->area.y,
		     event->area.width, event->area.height);

  return FALSE; 
}

void setup_clicked(GtkWidget *widget, gpointer data)
{
    
  char num_buf[3];
  char intf[6]="%2.0f"; 
  char fltf[6]="%1.1f";
  float fps_min=20, fps_max=60, q_min=25, q_max=40, ifs_min=.1, ifs_max=2, fk_min=1, fk_max=25;
  limit fk_lmt = {0, &iparms.file_keep, intf, &fk_min, &fk_max, 1};
  limit fps_lmt = {0, &iparms.fps, intf, &fps_min, &fps_max, 1};
  limit ifs_lmt = {0, &iparms.ifs, fltf, &ifs_min, &ifs_max, .1};
  limit qmin_lmt = {0, &iparms.qmin, intf, &q_min, &iparms.qcur, 1};
  limit qcur_lmt = {0, &iparms.qcur, intf, &iparms.qmin, &iparms.qmax, 1};
  limit qmax_lmt = {0, &iparms.qmax, intf, &iparms.qcur, &q_max, 1};
  check url_check = {0, &iparms.write_url};
  check file_check = {0, &iparms.write_file};
  check fmh_check = {0, &iparms.fmh};
  check fmv_check = {0, &iparms.fmv};
  check foh_check = {0, &iparms.foh};
  check fov_check = {0, &iparms.fov};
  check gps_check = {0, &iparms.gps};
  short int sample_x=448, sample_y=252;
  float size_min=.1, size_max=.5, xymin=.005, xymax=.999;
  ovrl da_ovrl = {0, &sample_x, &sample_y, &iparms.ovrl_size, &iparms.ovrl_x, &iparms.ovrl_y};
  draw dsize = {&iparms.ovrl_size, &da_ovrl, &size_min, &size_max, &xymin, &xymax, .01};
  draw dx = {&iparms.ovrl_x, &da_ovrl, &xymin, &xymax, &xymin, &xymax, .05};
  draw dy = {&iparms.ovrl_y, &da_ovrl, &xymin, &xymax, &xymin, &xymax, .05};
   
  gtk_widget_hide(data);
  /* setup window */
  GtkWidget *setup_win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (G_OBJECT (setup_win), "destroy", G_CALLBACK (cancel_clicked), setup_win);
  gtk_window_set_default_size (GTK_WINDOW (setup_win), 736, 480);
  gtk_container_set_border_width (GTK_CONTAINER (setup_win), 20);
  
  GtkWidget *vbox = gtk_vbox_new(FALSE, 5);
  gtk_container_add (GTK_CONTAINER (setup_win), vbox);
  
  GtkWidget *notebk = gtk_notebook_new();
  gtk_box_pack_start (GTK_BOX(vbox), notebk, TRUE, TRUE, 2);
  
  GtkWidget *hbox = gtk_hbox_new(TRUE, 5);
  gtk_box_pack_start (GTK_BOX(vbox), hbox, FALSE, TRUE, 2);
  /* setup window  cancel/save/done buttons */
  GtkWidget *button = gtk_button_new_with_label ("Cancel");
  g_signal_connect(button, "clicked", G_CALLBACK(cancel_clicked), setup_win);
  gtk_box_pack_start (GTK_BOX(hbox), button, TRUE, TRUE, 2);
  
  button = gtk_button_new_with_label ("Save");
  g_signal_connect(button, "clicked", G_CALLBACK(save_clicked), NULL);
  gtk_box_pack_start (GTK_BOX(hbox), button, TRUE, TRUE, 2);
  
  button = gtk_button_new_with_label ("Done");
  g_signal_connect(button, "clicked", G_CALLBACK(done_clicked), setup_win);
  gtk_box_pack_start (GTK_BOX(hbox), button, TRUE, TRUE, 2);
  
  GtkWidget *label1 = gtk_label_new("Destinations/Audio");
  GtkWidget *vbox1 = gtk_vbox_new(FALSE, 5);

  /* setup window tab1 setup */
  gtk_notebook_append_page(GTK_NOTEBOOK(notebk), vbox1, label1);
  
  GtkWidget *table = gtk_table_new (3, 4, FALSE);
  gtk_box_pack_start (GTK_BOX(vbox1), table, FALSE, TRUE, 5);
  
  /* setup window  URL stuff */
  url_check.button=gtk_check_button_new_with_label ("Stream to URL RTMP://");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(url_check.button), iparms.write_url);
  g_signal_connect(url_check.button, "toggled", G_CALLBACK(check_status), &url_check);
  gtk_table_attach_defaults (GTK_TABLE (table), url_check.button, 0, 1, 0, 1);
  
  GtkEntryBuffer *buf1= gtk_entry_buffer_new(iparms.url, -1);
  GtkWidget *wptr=gtk_entry_new_with_buffer(buf1);
  g_signal_connect (wptr, "changed", G_CALLBACK (text_cb), &iparms.url);
  gtk_table_attach_defaults (GTK_TABLE (table), wptr, 1, 4, 0, 1);
  
  /* setup window file stuff */
  file_check.button=gtk_check_button_new_with_label ("Stream to file   FILE:");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(file_check.button), iparms.write_file);
  g_signal_connect(file_check.button, "toggled", G_CALLBACK(check_status), &file_check);
  gtk_table_attach_defaults (GTK_TABLE (table), file_check.button, 0, 1, 1, 2);
  
  GtkEntryBuffer *buf2=gtk_entry_buffer_new(iparms.file, -1);
  wptr=gtk_entry_new_with_buffer(buf2);
  g_signal_connect (wptr, "changed", G_CALLBACK (text_cb), &iparms.file);
  gtk_table_attach_defaults (GTK_TABLE (table), wptr, 1, 3, 1, 2);
  
  /* file keep stuff */
  hbox = gtk_hbox_new(FALSE, 0);
  
  gtk_table_attach_defaults (GTK_TABLE (table), hbox, 3, 4, 1, 2);
  wptr = gtk_label_new ("Keep files ");
  gtk_box_pack_start (GTK_BOX(hbox), wptr, FALSE, FALSE, 5);
  
  wptr = gtk_button_new();
  g_signal_connect(wptr, "clicked", G_CALLBACK(dec_val_lbl), &fk_lmt);
  gtk_button_set_image(GTK_BUTTON(wptr), gtk_image_new_from_stock(GTK_STOCK_REMOVE, GTK_ICON_SIZE_BUTTON));
  gtk_box_pack_start (GTK_BOX(hbox), wptr, FALSE, FALSE, 0);
  
  sprintf(num_buf, "%2.0f", iparms.file_keep);
  fk_lmt.label = gtk_label_new (num_buf);
  gtk_box_pack_start (GTK_BOX(hbox), fk_lmt.label, FALSE, FALSE, 0);
    
  wptr = gtk_button_new();
  g_signal_connect(wptr, "clicked", G_CALLBACK(inc_val_lbl), &fk_lmt);
  gtk_button_set_image(GTK_BUTTON(wptr), gtk_image_new_from_stock(GTK_STOCK_ADD, GTK_ICON_SIZE_BUTTON));
  gtk_box_pack_start (GTK_BOX(hbox), wptr, FALSE, FALSE, 0);
   
  /* setup window ALSA device */ 
  wptr = gtk_label_new ("ALSA device name");
  gtk_table_attach_defaults (GTK_TABLE (table), wptr, 0, 1, 2, 3);
  
  GtkEntryBuffer *buf3=gtk_entry_buffer_new (iparms.adev, -1);
  wptr=gtk_entry_new_with_buffer(buf3);
  g_signal_connect (wptr, "changed", G_CALLBACK (text_cb), &iparms.adev);
  gtk_table_attach_defaults (GTK_TABLE (table), wptr, 1, 2, 2, 3);
  
  /* setup window ALSA device  stereo/mono */ 
  wptr=gtk_radio_button_new_with_label (NULL, "Stereo");
  g_signal_connect(wptr, "clicked", G_CALLBACK(check_stereo), wptr);
  GSList *channel_grp=gtk_radio_button_get_group (GTK_RADIO_BUTTON(wptr));
  if (iparms.channels == 2) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (wptr), TRUE);
  gtk_table_attach_defaults (GTK_TABLE (table), wptr, 2, 3, 2, 3);

  wptr=gtk_radio_button_new_with_label (channel_grp, "Mono");
  g_signal_connect(wptr, "clicked", G_CALLBACK(check_mono), wptr);
  if (iparms.channels == 1) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (wptr), TRUE);
  gtk_table_attach_defaults (GTK_TABLE (table), wptr, 3, 4, 2, 3);

  /* setup window keyboard socket stuff */ 
  GtkWidget *socket_box = gtk_event_box_new ();
  gtk_widget_show (socket_box);
  
  wptr = gtk_socket_new ();

  gtk_container_add (GTK_CONTAINER (socket_box), wptr);
  gtk_box_pack_start (GTK_BOX (vbox1), GTK_WIDGET(socket_box), TRUE, TRUE, 0);

  gtk_socket_add_id(GTK_SOCKET(wptr), kb_xid); 
   /* FIXME: handle "plug-added" & "plug-removed" signals for socket */
  
  /* setup window tab2 setup */
  wptr = gtk_label_new ("Video/Encoding");
  vbox = gtk_vbox_new(FALSE, 5);
  gtk_notebook_append_page(GTK_NOTEBOOK(notebk), vbox, wptr);
  
  /* setup window Resolution */
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start (GTK_BOX(vbox), hbox, FALSE, TRUE, 2);
  
  wptr=gtk_radio_button_new_with_label (NULL, "1920x1080");
  if (iparms.main_size == 0) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (wptr), TRUE);
  g_signal_connect(wptr, "clicked", G_CALLBACK(check_res0), wptr);
  gtk_box_pack_start (GTK_BOX(hbox), wptr, FALSE, TRUE, 2);
  
  wptr=gtk_radio_button_new_with_label (gtk_radio_button_get_group (GTK_RADIO_BUTTON(wptr)), "1280x720");
  if (iparms.main_size == 1) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (wptr), TRUE);
  g_signal_connect(wptr, "clicked", G_CALLBACK(check_res1), wptr);
  gtk_box_pack_start (GTK_BOX(hbox), wptr, FALSE, TRUE, 2);
  
  wptr=gtk_radio_button_new_with_label (gtk_radio_button_get_group (GTK_RADIO_BUTTON(wptr)), "854x480");
  if (iparms.main_size == 2) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (wptr), TRUE);
  g_signal_connect(wptr, "clicked", G_CALLBACK(check_res2), wptr);
  gtk_box_pack_start (GTK_BOX(hbox), wptr, FALSE, TRUE, 2);
  
  /* setup window FPS */
  gtk_box_pack_start (GTK_BOX(hbox), gtk_label_new (" FPS "), FALSE, TRUE, 2);
  
  wptr = gtk_button_new();
  g_signal_connect(wptr, "clicked", G_CALLBACK(dec_val_lbl), &fps_lmt);
  gtk_button_set_image(GTK_BUTTON(wptr), gtk_image_new_from_stock(GTK_STOCK_REMOVE, GTK_ICON_SIZE_BUTTON));
  gtk_box_pack_start (GTK_BOX(hbox), wptr, FALSE, TRUE, 2);
  
  sprintf(num_buf, "%2.0f", iparms.fps);
  fps_lmt.label = gtk_label_new (num_buf);
  gtk_box_pack_start (GTK_BOX(hbox), fps_lmt.label, FALSE, TRUE, 2);
  
  wptr = gtk_button_new();
  g_signal_connect(wptr, "clicked", G_CALLBACK(inc_val_lbl), &fps_lmt);
  gtk_button_set_image(GTK_BUTTON(wptr), gtk_image_new_from_stock(GTK_STOCK_ADD, GTK_ICON_SIZE_BUTTON));
  gtk_box_pack_start (GTK_BOX(hbox), wptr, FALSE, TRUE, 2);
  
  /* setup window I frames */
  gtk_box_pack_start (GTK_BOX(hbox), gtk_label_new ("Key in secs"), FALSE, TRUE, 2);
  
  wptr = gtk_button_new();
  g_signal_connect(wptr, "clicked", G_CALLBACK(dec_val_lbl), &ifs_lmt);
  gtk_button_set_image(GTK_BUTTON(wptr), gtk_image_new_from_stock(GTK_STOCK_REMOVE, GTK_ICON_SIZE_BUTTON));
  gtk_box_pack_start (GTK_BOX(hbox), wptr, FALSE, TRUE, 2);
  
  sprintf(num_buf, "%1.1f", iparms.ifs);
  ifs_lmt.label = gtk_label_new (num_buf);
  gtk_box_pack_start (GTK_BOX(hbox), ifs_lmt.label, FALSE, TRUE, 2);
  
  wptr = gtk_button_new();
  g_signal_connect(wptr, "clicked", G_CALLBACK(inc_val_lbl), &ifs_lmt);
  gtk_button_set_image(GTK_BUTTON(wptr), gtk_image_new_from_stock(GTK_STOCK_ADD, GTK_ICON_SIZE_BUTTON));
  gtk_box_pack_start (GTK_BOX(hbox), wptr, FALSE, TRUE, 2);
  
  /* setup Quantization */ 
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start (GTK_BOX(vbox), hbox, FALSE, TRUE, 2);
  
  gtk_box_pack_start (GTK_BOX(hbox), gtk_label_new ("Quantization Min "), FALSE, TRUE, 2);
  
  wptr = gtk_button_new();
  g_signal_connect(wptr, "clicked", G_CALLBACK(dec_val_lbl), &qmin_lmt);
  gtk_button_set_image(GTK_BUTTON(wptr), gtk_image_new_from_stock(GTK_STOCK_REMOVE, GTK_ICON_SIZE_BUTTON));
  gtk_box_pack_start (GTK_BOX(hbox), wptr, FALSE, TRUE, 2);
  
  sprintf(num_buf, "%2.0f", iparms.qmin);
  qmin_lmt.label = gtk_label_new (num_buf);
  gtk_box_pack_start (GTK_BOX(hbox), qmin_lmt.label, FALSE, TRUE, 2);
  
  wptr = gtk_button_new();
  g_signal_connect(wptr, "clicked", G_CALLBACK(inc_val_lbl), &qmin_lmt);
  gtk_button_set_image(GTK_BUTTON(wptr), gtk_image_new_from_stock(GTK_STOCK_ADD, GTK_ICON_SIZE_BUTTON));
  gtk_box_pack_start (GTK_BOX(hbox), wptr, FALSE, TRUE, 2);
  
  gtk_box_pack_start (GTK_BOX(hbox), gtk_label_new (" Initial "), FALSE, TRUE, 2);
  
  wptr = gtk_button_new();
  g_signal_connect(wptr, "clicked", G_CALLBACK(dec_val_lbl), &qcur_lmt);
  gtk_button_set_image(GTK_BUTTON(wptr), gtk_image_new_from_stock(GTK_STOCK_REMOVE, GTK_ICON_SIZE_BUTTON));
  gtk_box_pack_start (GTK_BOX(hbox), wptr, FALSE, TRUE, 2);
  
  sprintf(num_buf, "%2.0f", iparms.qcur);
  qcur_lmt.label = gtk_label_new (num_buf);
  gtk_box_pack_start (GTK_BOX(hbox), qcur_lmt.label, FALSE, TRUE, 2);
  
  wptr = gtk_button_new();
  g_signal_connect(wptr, "clicked", G_CALLBACK(inc_val_lbl), &qcur_lmt);
  gtk_button_set_image(GTK_BUTTON(wptr), gtk_image_new_from_stock(GTK_STOCK_ADD, GTK_ICON_SIZE_BUTTON));
  gtk_box_pack_start (GTK_BOX(hbox), wptr, FALSE, TRUE, 2);
    
  gtk_box_pack_start (GTK_BOX(hbox), gtk_label_new (" Max "), FALSE, TRUE, 2);
  
  wptr = gtk_button_new();
  g_signal_connect(wptr, "clicked", G_CALLBACK(dec_val_lbl), &qmax_lmt);
  gtk_button_set_image(GTK_BUTTON(wptr), gtk_image_new_from_stock(GTK_STOCK_REMOVE, GTK_ICON_SIZE_BUTTON));
  gtk_box_pack_start (GTK_BOX(hbox), wptr, FALSE, TRUE, 2);
  
  sprintf(num_buf, "%2.0f", iparms.qmax);
  qmax_lmt.label = gtk_label_new (num_buf);
  gtk_box_pack_start (GTK_BOX(hbox), qmax_lmt.label, FALSE, TRUE, 2);
  
  wptr = gtk_button_new();
  g_signal_connect(wptr, "clicked", G_CALLBACK(inc_val_lbl), &qmax_lmt);
  gtk_button_set_image(GTK_BUTTON(wptr), gtk_image_new_from_stock(GTK_STOCK_ADD, GTK_ICON_SIZE_BUTTON)); 
  gtk_box_pack_start (GTK_BOX(hbox), wptr, FALSE, TRUE, 2);
  
  gps_check.button=gtk_check_button_new_with_label ("GPS spd");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gps_check.button), iparms.gps);
  g_signal_connect(gps_check.button, "toggled", G_CALLBACK(check_status), &gps_check);
  gtk_box_pack_start (GTK_BOX(hbox), gps_check.button, FALSE, TRUE, 2);
    
  /* setup window Flip check boxes */ 
  vbox1 = gtk_vbox_new(FALSE, 5);
  hbox = gtk_hbox_new(FALSE, 5);
  table = gtk_table_new (2, 3, FALSE);
  gtk_box_pack_start (GTK_BOX(vbox1), table, FALSE, TRUE, 2);
  gtk_box_pack_start (GTK_BOX(hbox), vbox1, FALSE, TRUE, 2);
  gtk_box_pack_start (GTK_BOX(vbox), hbox, FALSE, TRUE, 2);

  gtk_table_attach_defaults (GTK_TABLE (table), gtk_label_new ("Flip main   "), 0, 1, 0, 1);
  
  fmh_check.button=gtk_check_button_new_with_label ("horiz.");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(fmh_check.button), iparms.fmh);
  g_signal_connect(fmh_check.button, "toggled", G_CALLBACK(check_status), &fmh_check);
  gtk_table_attach_defaults (GTK_TABLE (table), fmh_check.button, 1, 2, 0, 1);
  
  fmv_check.button=gtk_check_button_new_with_label ("vert.");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(fmv_check.button), iparms.fmv);
  g_signal_connect(fmv_check.button, "toggled", G_CALLBACK(check_status), &fmv_check);
  gtk_table_attach_defaults (GTK_TABLE (table), fmv_check.button, 2, 3, 0, 1);
  
  gtk_table_attach_defaults (GTK_TABLE (table), gtk_label_new ("Flip overlay"), 0, 1, 1, 2);
  
  foh_check.button=gtk_check_button_new_with_label ("horiz.");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(foh_check.button), iparms.foh);
  g_signal_connect(foh_check.button, "toggled", G_CALLBACK(check_status), &foh_check);
  gtk_table_attach_defaults (GTK_TABLE (table), foh_check.button, 1, 2, 1, 2);
  
  fov_check.button=gtk_check_button_new_with_label ("vert.");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(fov_check.button), iparms.fov);
  g_signal_connect(fov_check.button, "toggled", G_CALLBACK(check_status), &fov_check);
  gtk_table_attach_defaults (GTK_TABLE (table), fov_check.button, 2, 3, 1, 2);
  
  /* setup window Overlay  */ 
  table = gtk_table_new (4, 4, FALSE);
  gtk_box_pack_start (GTK_BOX(vbox1), table, FALSE, FALSE, 0);
  
  gtk_table_attach_defaults (GTK_TABLE (table), gtk_label_new ("Overlay size "), 0, 1, 0, 1);
  
  wptr = gtk_button_new();
  gtk_button_set_image(GTK_BUTTON(wptr), gtk_image_new_from_stock(GTK_STOCK_REMOVE, GTK_ICON_SIZE_BUTTON));
  g_signal_connect(wptr, "clicked", G_CALLBACK(dec_size), &dsize);
  gtk_table_attach_defaults (GTK_TABLE (table), wptr, 1, 2, 0, 1);
  
  wptr = gtk_button_new();
  gtk_button_set_image(GTK_BUTTON(wptr), gtk_image_new_from_stock(GTK_STOCK_ADD, GTK_ICON_SIZE_BUTTON));
  g_signal_connect(wptr, "clicked", G_CALLBACK(inc_size), &dsize);
  gtk_table_attach_defaults (GTK_TABLE (table), wptr, 3, 4, 0, 1);
  
  gtk_table_attach_defaults (GTK_TABLE (table), gtk_label_new ("Overlay move"), 0, 1, 2, 3);
  
  wptr = gtk_button_new();
  gtk_button_set_image(GTK_BUTTON(wptr), gtk_image_new_from_stock(GTK_STOCK_GO_UP, GTK_ICON_SIZE_BUTTON));
  g_signal_connect(wptr, "clicked", G_CALLBACK(dec_xy), &dy);
  gtk_table_attach_defaults (GTK_TABLE (table), wptr, 2, 3, 1, 2);
  
  wptr = gtk_button_new();
  gtk_button_set_image(GTK_BUTTON(wptr), gtk_image_new_from_stock(GTK_STOCK_GO_BACK, GTK_ICON_SIZE_BUTTON));
  g_signal_connect(wptr, "clicked", G_CALLBACK(dec_xy), &dx);
  gtk_table_attach_defaults (GTK_TABLE (table), wptr, 1, 2, 2, 3);
  
  wptr = gtk_button_new();
  gtk_button_set_image(GTK_BUTTON(wptr), gtk_image_new_from_stock(GTK_STOCK_GO_FORWARD, GTK_ICON_SIZE_BUTTON));
  g_signal_connect(wptr, "clicked", G_CALLBACK(inc_xy), &dx);
  gtk_table_attach_defaults (GTK_TABLE (table), wptr, 3, 4, 2, 3);

  wptr = gtk_button_new();
  gtk_button_set_image(GTK_BUTTON(wptr), gtk_image_new_from_stock(GTK_STOCK_GO_DOWN, GTK_ICON_SIZE_BUTTON));
  g_signal_connect(wptr, "clicked", G_CALLBACK(inc_xy), &dy);
  gtk_table_attach_defaults (GTK_TABLE (table), wptr, 2, 3, 3, 4);
  
  /* setup window Switch camera */ 
  GtkWidget *hbox1 = gtk_hbox_new(FALSE, 5);
  gtk_box_pack_start (GTK_BOX(vbox1), hbox1, FALSE, TRUE, 2);    
  gtk_box_pack_start (GTK_BOX(hbox1), gtk_label_new ("Swap cameras"), FALSE, TRUE, 2);
  wptr = gtk_button_new();
  g_signal_connect(wptr, "clicked", G_CALLBACK(swap_cam), &iparms.cam);
  if (iparms.cam)
    gtk_button_set_label (GTK_BUTTON(wptr), "Front/Rear");
  else
    gtk_button_set_label (GTK_BUTTON(wptr), "Rear/Front");
  gtk_box_pack_start (GTK_BOX(hbox1), wptr, FALSE, TRUE, 2);
  
  /* Create the drawing area */
  da_ovrl.draw_area = gtk_drawing_area_new ();
  int x=*da_ovrl.x, y=*da_ovrl.y;
  gtk_widget_set_size_request (GTK_WIDGET (da_ovrl.draw_area), x, y);
  g_signal_connect (da_ovrl.draw_area, "expose_event", G_CALLBACK (expose_event), &da_ovrl);
  g_signal_connect (da_ovrl.draw_area, "configure_event", G_CALLBACK (configure_event), &da_ovrl);
  gtk_box_pack_start (GTK_BOX (hbox), da_ovrl.draw_area, TRUE, TRUE, 0);
  gtk_widget_show (da_ovrl.draw_area); 

  gtk_widget_show_all(setup_win);
  gtk_main();
  gtk_widget_show_all(data);

}

void stop_window(gpointer data)
{
  gtk_widget_hide(data);
  /* stop window */
  GtkWidget *wptr = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  stop_win=wptr;
  gtk_window_set_decorated (GTK_WINDOW(wptr), FALSE); 
//  gtk_window_fullscreen (GTK_WINDOW(wptr));
  /* stop button */
//  GtkWidget *stop_button = gtk_button_new_with_label ("Stop");
  stop_button = gtk_button_new();
//  GtkWidget *image = gtk_image_new_from_file ("/home/pi/Pictures/stop_sign.png");
//  gtk_button_set_image(GTK_BUTTON(stop_button), image);
  gtk_button_set_image(GTK_BUTTON(stop_button), gtk_image_new_from_stock(GTK_STOCK_STOP, GTK_ICON_SIZE_DIALOG));
  GtkWidget *vbox = gtk_vbox_new(FALSE, 5);
  gtk_box_pack_start (GTK_BOX(vbox), stop_button, TRUE, TRUE, 0);
//  message1 = gtk_label_new ("");
//  gtk_box_pack_start (GTK_BOX(vbox), message1, TRUE, TRUE, 0);
//  message2 = gtk_label_new ("");
//  gtk_box_pack_start (GTK_BOX(vbox), message2, TRUE, TRUE, 0);

  g_signal_connect(stop_button, "clicked", G_CALLBACK(stop_clicked), wptr);
  gtk_container_add (GTK_CONTAINER (wptr), vbox);
  
  gtk_widget_show_all(wptr);
  
  gtk_main();

  gtk_widget_show_all(data);
}

void preview_clicked(GtkWidget *widget, gpointer data)
{
  MMAL_STATUS_T status = MMAL_SUCCESS;
  RASPIVID_STATE preview_state;
  parms_to_state (&preview_state);
  preview_state.callback_data.pstate = &preview_state;
  
  create_video_stream(&preview_state);
  create_preview_component(&preview_state);
  if ((status = connect_ports(preview_state.hvs_component->output[0], 
    preview_state.preview_component->input[0], &preview_state.preview_connection)) != MMAL_SUCCESS)
    {
		vcos_log_error("%s: Failed to connect hvs to encoder input", __func__); 
		preview_state.preview_connection = NULL;
    goto error;
    } 

  toggle_stream(&preview_state, START);
    
  stop_window(data);

  toggle_stream(&preview_state, STOP);
  
  if (preview_state.preview_connection)
		mmal_connection_destroy(preview_state.preview_connection);

error:  
  destroy_preview_component(&preview_state);
  destroy_video_stream(&preview_state);
}

void record_clicked(GtkWidget *widget, gpointer data)
{
  RASPIVID_STATE record_state;
  pthread_t record_tid;
  pthread_create(&record_tid, NULL, record_thread, (void *)&record_state);

  stop_window(data);
  
  record_state.recording=0;
  pthread_join(record_tid, NULL);

}

int main(int argc, char **argv)
{
  if (bcm2835_init()) 
    {
    gpio_init = 1;    
    bcm2835_gpio_fsel(GPIO_SWT, BCM2835_GPIO_FSEL_INPT);
    bcm2835_gpio_fsel(GPIO_LED, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(GPIO_MODEM_LED, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(GPIO_PWR_LED, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_write(GPIO_PWR_LED, HIGH);
    }
	else 
    {
    printf ("bcm2835 init failed\n");
    }
  if (read_parms())
    {
    strcpy(iparms.url, "a.rtmp.youtube.com/live2/g9td-pva2-fwgy-suv1-9gkz");
    strcpy(iparms.file, "filename.flv");
    strcpy(iparms.adev, "dmic_sv");
    iparms.fmh=iparms.fmv=iparms.foh=iparms.fov=iparms.main_size=0;
    iparms.cam=iparms.channels=1;
    iparms.ovrl_size=iparms.ovrl_x=iparms.ovrl_y=.5;
    iparms.fps=30;
    iparms.ifs=2;
    iparms.qmin=25;
    iparms.qcur=28;
    iparms.qmax=40;
    iparms.write_url=iparms.write_file=1;
    iparms.file_keep=18;
    if (write_parms()) printf("write failed\n");
    } 
    
  gtk_init (&argc, &argv);

  install_signal_handlers();

  kb_xid = launch_keyboard();

  if (!kb_xid)
    {
      perror ("### 'matchbox-keyboard --xid', failed to return valid window ID. ### ");
      exit(-1);
    }
    
  /*Main Window */
  GtkWidget *main_win = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  gtk_window_set_default_size (GTK_WINDOW (main_win), 640, 480);
  gtk_container_set_border_width (GTK_CONTAINER (main_win), 20);

  g_signal_connect (G_OBJECT (main_win), "destroy", G_CALLBACK (widget_destroy), NULL);

  GtkWidget *vbox = gtk_vbox_new(FALSE, 20);
  gtk_container_add (GTK_CONTAINER (main_win), vbox);

  GtkWidget *button = gtk_button_new_with_label ("Setup");
  g_signal_connect(button, "clicked", G_CALLBACK(setup_clicked), main_win);
  gtk_box_pack_start (GTK_BOX(vbox), button, TRUE, TRUE, 2);
  button = gtk_button_new_with_label ("Perview");
  g_signal_connect(button, "clicked", G_CALLBACK(preview_clicked), main_win);
  gtk_box_pack_start (GTK_BOX(vbox), button, TRUE, TRUE, 2);
  button = gtk_button_new_with_label ("Record");
  g_signal_connect(button, "clicked", G_CALLBACK(record_clicked), main_win);
  gtk_box_pack_start (GTK_BOX(vbox), button, TRUE, TRUE, 2);
  
  gtk_widget_show_all (main_win);

  gtk_main ();
  
  if (gpio_init) {
		bcm2835_gpio_write(GPIO_LED, LOW);
    bcm2835_gpio_write(GPIO_MODEM_LED, LOW);
    bcm2835_gpio_write(GPIO_PWR_LED, LOW);
		bcm2835_close();}

  kill(-getpid(), 15);
  

  return 0;
}
