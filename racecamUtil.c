#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <memory.h>
#include <sysexits.h> 

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <semaphore.h>
 
#include "bcm_host.h"
#include "interface/vcos/vcos.h"

#include "interface/mmal/mmal.h"
#include "interface/mmal/mmal_logging.h"
#include "interface/mmal/mmal_buffer.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_util_params.h"
#include "interface/mmal/util/mmal_default_components.h"
#include "interface/mmal/util/mmal_connection.h"
#include "interface/mmal/mmal_parameters_camera.h"

#include "raspiCamUtilities.h"
#include "racecamUtil.h"
#include "GPSUtil.h"

#include <stdbool.h>

MMAL_STATUS_T create_camera_component(RASPIVID_STATE *state)
{
   MMAL_COMPONENT_T *camera = 0;
   MMAL_ES_FORMAT_T *format;
   MMAL_PORT_T *preview_port = NULL, *video_port = NULL, *still_port = NULL;
   MMAL_STATUS_T status;

   status = mmal_component_create(MMAL_COMPONENT_DEFAULT_CAMERA, &camera);

   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("Failed to create camera component");
      goto error;
   }

   status = raspicamcontrol_set_stereo_mode(camera->output[0], &state->camera_parameters.stereo_mode);
   status += raspicamcontrol_set_stereo_mode(camera->output[1], &state->camera_parameters.stereo_mode);
   status += raspicamcontrol_set_stereo_mode(camera->output[2], &state->camera_parameters.stereo_mode);

   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("Could not set stereo mode : error %d", status);
      goto error;
   }

   MMAL_PARAMETER_INT32_T camera_num =
   {{MMAL_PARAMETER_CAMERA_NUM, sizeof(camera_num)}, state->common_settings.cameraNum};

   status = mmal_port_parameter_set(camera->control, &camera_num.hdr);

   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("Could not select camera : error %d", status);
      goto error;
   }

   if (!camera->output_num)
   {
      status = MMAL_ENOSYS;
      vcos_log_error("Camera doesn't have output ports");
      goto error;
   }

   status = mmal_port_parameter_set_uint32(camera->control, MMAL_PARAMETER_CAMERA_CUSTOM_SENSOR_CONFIG, state->common_settings.sensor_mode);

   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("Could not set sensor mode : error %d", status);
      goto error;
   }

   preview_port = camera->output[MMAL_CAMERA_PREVIEW_PORT];
   video_port = camera->output[MMAL_CAMERA_VIDEO_PORT];
   still_port = camera->output[MMAL_CAMERA_CAPTURE_PORT];

   // Enable the camera, and tell it its control callback function
   status = mmal_port_enable(camera->control, default_camera_control_callback);

   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("Unable to enable control port : error %d", status);
      goto error;
   }

   //  set up the camera configuration
   {
      MMAL_PARAMETER_CAMERA_CONFIG_T cam_config =
      {
         { MMAL_PARAMETER_CAMERA_CONFIG, sizeof(cam_config) },
         .max_stills_w = state->common_settings.width,
         .max_stills_h = state->common_settings.height,
         .stills_yuv422 = 0,
         .one_shot_stills = 0,
         .max_preview_video_w = state->common_settings.width,
         .max_preview_video_h = state->common_settings.height,
         .num_preview_video_frames = 3 + vcos_max(0, (state->framerate-30)/10),
         .stills_capture_circular_buffer_height = 0,
         .fast_preview_resume = 0,
         .use_stc_timestamp = MMAL_PARAM_TIMESTAMP_MODE_RAW_STC
      };
      mmal_port_parameter_set(camera->control, &cam_config.hdr);
   }

   format = preview_port->format;

   format->encoding = MMAL_ENCODING_OPAQUE;
   format->encoding_variant = MMAL_ENCODING_I420;
   format->es->video.width = VCOS_ALIGN_UP(state->common_settings.width, 32);
   format->es->video.height = VCOS_ALIGN_UP(state->common_settings.height, 16);
   format->es->video.crop.x = 0;
   format->es->video.crop.y = 0;
   format->es->video.crop.width = state->common_settings.width;
   format->es->video.crop.height = state->common_settings.height;
   format->es->video.frame_rate.num = state->framerate;
   format->es->video.frame_rate.den = VIDEO_FRAME_RATE_DEN;
   
   status = mmal_port_format_commit(preview_port);

   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("camera viewfinder format couldn't be set");
      goto error;
   }

   // Set the encode format on the video  port
   format = video_port->format;
   format->encoding_variant = MMAL_ENCODING_I420;
   format->encoding = MMAL_ENCODING_OPAQUE;
   format->es->video.width = VCOS_ALIGN_UP(state->common_settings.width, 32);
   format->es->video.height = VCOS_ALIGN_UP(state->common_settings.height, 16);
   format->es->video.crop.x = 0;
   format->es->video.crop.y = 0;
   format->es->video.crop.width = state->common_settings.width;
   format->es->video.crop.height = state->common_settings.height;
   format->es->video.frame_rate.num = state->framerate;
   format->es->video.frame_rate.den = VIDEO_FRAME_RATE_DEN;

   status = mmal_port_format_commit(video_port);

   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("camera video format couldn't be set");
      goto error;
   }
   

   if (video_port->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM)
      video_port->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;

   // Set the encode format on the still  port
   format = still_port->format;

   format->encoding = MMAL_ENCODING_OPAQUE;
   format->encoding_variant = MMAL_ENCODING_I420;
   format->es->video.width = VCOS_ALIGN_UP(state->common_settings.width, 32);
   format->es->video.height = VCOS_ALIGN_UP(state->common_settings.height, 16);
   format->es->video.crop.x = 0;
   format->es->video.crop.y = 0;
   format->es->video.crop.width = state->common_settings.width;
   format->es->video.crop.height = state->common_settings.height;
   format->es->video.frame_rate.num = 0;
   format->es->video.frame_rate.den = 1;

   status = mmal_port_format_commit(still_port);

   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("camera still format couldn't be set");
      goto error;
   }

  // Ensure there are enough buffers to avoid dropping frames 
   if (still_port->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM)
      still_port->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;


   // Enable component 
   status = mmal_component_enable(camera);

   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("camera component couldn't be enabled");
      goto error;
   }

   // Note: this sets lots of parameters that were not individually addressed before.
   raspicamcontrol_set_all_parameters(camera, &state->camera_parameters);

   if (!state->camera_component)
   {
      state->camera_component = camera;
   }
   else
   {
      state->camera2_component = camera;
   }

   return status;
   
error:

   if (camera)
      mmal_component_destroy(camera);

   return status;
} 

void destroy_camera_component(RASPIVID_STATE *state)
{
   if (state->camera_component)
   {
      mmal_component_destroy(state->camera_component);
      state->camera_component = NULL;
   }
   if (state->camera2_component)
   {
      mmal_component_destroy(state->camera2_component);
      state->camera_component = NULL;
   }
} 

MMAL_STATUS_T create_hvs_component(RASPIVID_STATE *state)
{
      
   MMAL_COMPONENT_T *hvs = 0; 
   MMAL_PORT_T *hvs_main_input = NULL, *hvs_ovl_input, *hvs_txt_input, *hvs_output = NULL;
   MMAL_STATUS_T status;
   MMAL_POOL_T *pool;
   int opacity = 255;
           
   status = mmal_component_create("vc.ril.hvs", &hvs);
   
   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("Unable to create hvs component");
      goto error;
   }
   
   if (!hvs->input_num || !hvs->output_num)
   {
      status = MMAL_ENOSYS;
      vcos_log_error("HVS doesn't have input/output ports");
      goto error;
   }

   hvs_main_input = hvs->input[0];
   hvs_ovl_input = hvs->input[1];
   hvs_txt_input = hvs->input[2];
   hvs_output = hvs->output[0];
  
   mmal_format_copy(hvs_main_input->format, state->camera_component->output[MMAL_CAMERA_VIDEO_PORT]->format); 

   hvs_output->format->encoding = MMAL_ENCODING_RGB24;

   MMAL_DISPLAYREGION_T param = {{MMAL_PARAMETER_DISPLAYREGION, sizeof(param)}, MMAL_DISPLAY_SET_DUMMY};
   param.set = MMAL_DISPLAY_SET_DEST_RECT | MMAL_DISPLAY_SET_LAYER | MMAL_DISPLAY_SET_ALPHA;
   param.dest_rect.width = state->common_settings.width;
   param.dest_rect.height = state->common_settings.height;
   param.layer = 0;
   param.alpha = opacity;
 
   status = mmal_port_parameter_set(hvs_main_input, &param.hdr);
   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("Unable to set displayregion hvs main input");
      goto error;
   }
    
   // Commit the port changes to the hvs main input port
   status = mmal_port_format_commit(hvs_main_input);

   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("Unable to set format on hvs main input port");
      goto error;
   }

   param.set =  MMAL_DISPLAY_SET_FULLSCREEN | MMAL_DISPLAY_SET_DEST_RECT | MMAL_DISPLAY_SET_LAYER | MMAL_DISPLAY_SET_ALPHA;
   param.fullscreen = MMAL_FALSE;
   param.dest_rect.x = state->common_settings.ovl.x;  
   param.dest_rect.y = state->common_settings.ovl.y;
   param.dest_rect.width = state->common_settings.ovl.width;
   param.dest_rect.height = state->common_settings.ovl.height;
   param.layer=  1;
   param.alpha = opacity | MMAL_DISPLAY_ALPHA_FLAGS_DISCARD_LOWER_LAYERS;
      
   status = mmal_port_parameter_set(hvs_ovl_input, &param.hdr);
   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("Unable to set displayregion hvs overlay input");
      goto error;
   }
    
   // Commit the port changes to the hvs ovl input port
   status = mmal_port_format_commit(hvs_ovl_input);

   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("Unable to set format on hvs ovl input port");
      goto error;
   }
   
   // text overlay
   param.set =  MMAL_DISPLAY_SET_FULLSCREEN | MMAL_DISPLAY_SET_DEST_RECT | MMAL_DISPLAY_SET_LAYER | MMAL_DISPLAY_SET_ALPHA;
   param.fullscreen = MMAL_FALSE;
   param.dest_rect.x = 0;
   param.dest_rect.y = state->common_settings.height-TEXTH;
   param.dest_rect.width = TEXTW;
   param.dest_rect.height = TEXTH;
   param.layer=  2;
   param.alpha = opacity;
      
   status = mmal_port_parameter_set(hvs_txt_input, &param.hdr);
   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("Unable to set displayregion hvs text input");
      goto error;
   }
   
   // Commit the port changes to the hvs text input port
   hvs_txt_input->format->encoding = MMAL_ENCODING_BGRA;
   hvs_txt_input->format->es->video.width = TEXTW;
   hvs_txt_input->format->es->video.height = TEXTH;
   hvs_txt_input->format->es->video.crop.x = 0;
   hvs_txt_input->format->es->video.crop.y = 0;
   hvs_txt_input->format->es->video.crop.width = 0;
   hvs_txt_input->format->es->video.crop.height = 0;
  
   // Commit the port changes to the hvs text input port
   status = mmal_port_format_commit(hvs_txt_input);

   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("Unable to set format on hvs ovl text port");
      goto error;
   }
   
   // Commit the port changes to the hvs output port
   hvs_output->format->encoding = MMAL_ENCODING_RGB24;
   hvs_output->format->es->video.width = VCOS_ALIGN_UP(state->common_settings.width, 32);
   hvs_output->format->es->video.height = VCOS_ALIGN_UP(state->common_settings.height, 16);
   hvs_output->format->es->video.crop.x = 0;
   hvs_output->format->es->video.crop.y = 0;
   hvs_output->format->es->video.crop.width = state->common_settings.width;
   hvs_output->format->es->video.crop.height = state->common_settings.height;
   hvs_output->format->es->video.frame_rate.num = state->framerate;
   hvs_output->format->es->video.frame_rate.den = VIDEO_FRAME_RATE_DEN;
   
   status = mmal_port_format_commit(hvs_output);

   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("Unable to set format on hvs output port");
      goto error;
   }
   
   //  Enable component
   status = mmal_component_enable(hvs);

   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("Unable to enable hvs component");
      goto error;
   }

   /* Create pool of buffer headers for the output port to consume */
   pool = mmal_port_pool_create(hvs_output, hvs_output->buffer_num, hvs_output->buffer_size);

   if (!pool)
   {
      vcos_log_error("Failed to create buffer header pool for hvs output port %s", hvs_output->name);
   }

   state->hvs_pool = pool;
   state->hvs_component = hvs;

   return status;

error:
   if (hvs)
      mmal_component_destroy(hvs);

   state->hvs_component = NULL;

   return status;
}

void destroy_hvs_component(RASPIVID_STATE *state)
{
   // Get rid of any port buffers first
   if (state->hvs_pool)
   {
      mmal_port_pool_destroy(state->hvs_component->output[0], state->hvs_pool);
   }

   if (state->hvs_component)
   {
      mmal_component_destroy(state->hvs_component);
      state->hvs_component = NULL;
   }
}


MMAL_STATUS_T create_encoder_component(RASPIVID_STATE *state)
{
   MMAL_COMPONENT_T *encoder = 0;
   MMAL_PORT_T *encoder_input = NULL, *encoder_output = NULL;
   MMAL_STATUS_T status;
   MMAL_POOL_T *pool;
   
   status = mmal_component_create(MMAL_COMPONENT_DEFAULT_VIDEO_ENCODER, &encoder);

   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("Unable to create video encoder component");
      goto error;
   }

   if (!encoder->input_num || !encoder->output_num)
   {
      status = MMAL_ENOSYS;
      vcos_log_error("Video encoder doesn't have input/output ports");
      goto error;
   }

   encoder_input = encoder->input[0];
   encoder_output = encoder->output[0];

   // We want same format on input and output ??
   mmal_format_copy(encoder_output->format, encoder_input->format);
     
   mmal_format_copy(encoder_input->format, state->hvs_component->output[0]->format);
      
   status = mmal_port_format_commit(encoder_input);

   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("Unable to set format on video encoder input port");
      goto error;
   }

   encoder_output->format->encoding = state->encoding;
   encoder_output->format->bitrate = state->bitrate;
   
   encoder_output->buffer_size = encoder_output->buffer_size_recommended;
   if (encoder_output->buffer_size < encoder_output->buffer_size_min)
      encoder_output->buffer_size = encoder_output->buffer_size_min;
      
   encoder_output->buffer_num = encoder_output->buffer_num_recommended;
// remove when hang is resolved
//   encoder_output->buffer_num = 100;  // if hang not fix on real Pi hardware
   if (encoder_output->buffer_num < encoder_output->buffer_num_min)
      encoder_output->buffer_num = encoder_output->buffer_num_min;
     
   // set the frame rate on output to 0, to ensure it gets
   // updated correctly from the input framerate when port connected
   encoder_output->format->es->video.frame_rate.num = 0;
   encoder_output->format->es->video.frame_rate.den = 1;

   // Commit the port changes to the output port
   status = mmal_port_format_commit(encoder_output);
   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("Unable to set format on video encoder output port");
      goto error;
   }

   MMAL_PARAMETER_VIDEO_RATECONTROL_T paramrc = {{ MMAL_PARAMETER_RATECONTROL, sizeof(paramrc)}, MMAL_VIDEO_RATECONTROL_DEFAULT};
   status = mmal_port_parameter_set(encoder_output, &paramrc.hdr);
   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("Unable to set ratecontrol");
      goto error;
   }

   MMAL_PARAMETER_UINT32_T param = {{ MMAL_PARAMETER_INTRAPERIOD, sizeof(param)}, state->intraperiod};
   status = mmal_port_parameter_set(encoder_output, &param.hdr);
   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("Unable to set intraperiod");
      goto error;
   }

   MMAL_PARAMETER_UINT32_T param1 = {{ MMAL_PARAMETER_VIDEO_ENCODE_INITIAL_QUANT, sizeof(param1)}, state->quantisationParameter};
   status = mmal_port_parameter_set(encoder_output, &param1.hdr);
   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("Unable to set initial QP");
      goto error;
   }

   MMAL_PARAMETER_UINT32_T param2 = {{ MMAL_PARAMETER_VIDEO_ENCODE_MIN_QUANT, sizeof(param2)}, state->quantisationMin};
   status = mmal_port_parameter_set(encoder_output, &param2.hdr);
   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("Unable to set min QP");
      goto error;
   }

   MMAL_PARAMETER_UINT32_T param3 = {{ MMAL_PARAMETER_VIDEO_ENCODE_MAX_QUANT, sizeof(param3)}, state->quantisationMax};
   status = mmal_port_parameter_set(encoder_output, &param3.hdr);
   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("Unable to set max QP");
      goto error;
   }

   MMAL_PARAMETER_VIDEO_PROFILE_T  paramvp;
   paramvp.hdr.id = MMAL_PARAMETER_PROFILE;
   paramvp.hdr.size = sizeof(paramvp);
   paramvp.profile[0].profile = state->profile;
   paramvp.profile[0].level = state->level;
   status = mmal_port_parameter_set(encoder_output, &paramvp.hdr);
   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("Unable to set H264 profile");
      goto error;
   }

   if (mmal_port_parameter_set_boolean(encoder_input, MMAL_PARAMETER_VIDEO_IMMUTABLE_INPUT, state->immutableInput) != MMAL_SUCCESS)
   {
      vcos_log_error("Unable to set immutable input flag");
   }


   //set INLINE HEADER flag to generate SPS and PPS for every IDR if requested
   if (mmal_port_parameter_set_boolean(encoder_output, MMAL_PARAMETER_VIDEO_ENCODE_INLINE_HEADER, state->bInlineHeaders) != MMAL_SUCCESS)
   {
      vcos_log_error("failed to set INLINE HEADER FLAG parameters");
   }

      //set flag for add SPS TIMING
   if (mmal_port_parameter_set_boolean(encoder_output, MMAL_PARAMETER_VIDEO_ENCODE_SPS_TIMING, state->addSPSTiming) != MMAL_SUCCESS)
   {
      vcos_log_error("failed to set SPS TIMINGS FLAG parameters");
   }
 
   //  Enable component
   status = mmal_component_enable(encoder);

   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("Unable to enable video encoder component");
      goto error;
   }

   /* Create pool of buffer headers for the output port to consume */
   pool = mmal_port_pool_create(encoder_output, encoder_output->buffer_num, encoder_output->buffer_size);

   if (!pool)
   {
      vcos_log_error("Failed to create buffer header pool for encoder output port %s", encoder_output->name);
   }

   state->encoder_pool = pool;
   state->encoder_component = encoder;
      
   return status;

error:
   if (encoder)
      mmal_component_destroy(encoder);

   state->encoder_component = NULL;

   return status;
}

void destroy_encoder_component(RASPIVID_STATE *state)
{
   // Get rid of any port buffers first
   if (state->encoder_pool)
   {
      mmal_port_pool_destroy(state->encoder_component->output[0], state->encoder_pool);
   }

   if (state->encoder_component)
   {
      mmal_component_destroy(state->encoder_component);
      state->encoder_component = NULL;
   }
}


MMAL_STATUS_T create_preview_component(RASPIVID_STATE *state)
{
   MMAL_COMPONENT_T *preview = 0;
   MMAL_PORT_T *preview_port = NULL;
   MMAL_STATUS_T status;

 
   status = mmal_component_create(MMAL_COMPONENT_DEFAULT_VIDEO_RENDERER,
                                     &preview);

   if (status != MMAL_SUCCESS)
      {
      vcos_log_error("Unable to create preview component");
      goto error;
      }

   if (!preview->input_num)
      {
      status = MMAL_ENOSYS;
      vcos_log_error("No input ports found on component");
      goto error;
      }

   preview_port = preview->input[0];

   MMAL_DISPLAYREGION_T param;
   param.hdr.id = MMAL_PARAMETER_DISPLAYREGION;
   param.hdr.size = sizeof(MMAL_DISPLAYREGION_T);

   param.set = MMAL_DISPLAY_SET_LAYER;
   param.layer = 2;

   param.set |= MMAL_DISPLAY_SET_ALPHA;
   param.alpha = 255; 

   param.set |= MMAL_DISPLAY_SET_FULLSCREEN;
   param.fullscreen = 1;
 
   param.set |= MMAL_DISPLAY_SET_NUM;
   param.display_num = 0;

   status = mmal_port_parameter_set(preview_port, &param.hdr);

   if (status != MMAL_SUCCESS && status != MMAL_ENOSYS)
      {
      vcos_log_error("unable to set preview port parameters (%u)", status);
      goto error;
      }


   /* Enable component */
   status = mmal_component_enable(preview);

   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("Unable to enable preview/null sink component (%u)", status);
      goto error;
   }

   state->preview_component = preview;

   return status;

error:

   if (preview)
      mmal_component_destroy(preview);

   return status;
}


/**
 * Destroy the preview component
 *
 * @param state Pointer to state control struct
 *
 */
void destroy_preview_component(RASPIVID_STATE *state)
{
   if (state->preview_component)
   {
      mmal_component_destroy(state->preview_component);
      state->preview_component = NULL;
   }
}


MMAL_STATUS_T connect_ports(MMAL_PORT_T *output_port, MMAL_PORT_T *input_port, MMAL_CONNECTION_T **connection)
{
   MMAL_STATUS_T status;

   status =  mmal_connection_create(connection, output_port, input_port, MMAL_CONNECTION_FLAG_TUNNELLING | MMAL_CONNECTION_FLAG_ALLOCATION_ON_INPUT);
   if (status == MMAL_SUCCESS)
   {
      status =  mmal_connection_enable(*connection);
      if (status != MMAL_SUCCESS)
         mmal_connection_destroy(*connection);
   }

   return status;
}

void check_disable_port(MMAL_PORT_T *port)
{
   if (port && port->is_enabled)
      mmal_port_disable(port);
}

void get_sensor_defaults(int camera_num, char *camera_name, int *width, int *height )
{
   MMAL_COMPONENT_T *camera_info;
   MMAL_STATUS_T status;

   // Default to the OV5647 setup
   strncpy(camera_name, "OV5647", MMAL_PARAMETER_CAMERA_INFO_MAX_STR_LEN);

   // Try to get the camera name and maximum supported resolution
   status = mmal_component_create(MMAL_COMPONENT_DEFAULT_CAMERA_INFO, &camera_info);
   if (status == MMAL_SUCCESS)
   {
      MMAL_PARAMETER_CAMERA_INFO_T param;
      param.hdr.id = MMAL_PARAMETER_CAMERA_INFO;
      param.hdr.size = sizeof(param)-4;  // Deliberately undersize to check firmware version
      status = mmal_port_parameter_get(camera_info->control, &param.hdr);

      if (status != MMAL_SUCCESS)
      {
         // Running on newer firmware
         param.hdr.size = sizeof(param);
         status = mmal_port_parameter_get(camera_info->control, &param.hdr);
         if (status == MMAL_SUCCESS && param.num_cameras > camera_num)
         {
            // Take the parameters from the first camera listed.
            if (*width == 0)
               *width = param.cameras[camera_num].max_width;
            if (*height == 0)
               *height = param.cameras[camera_num].max_height;
            strncpy(camera_name, param.cameras[camera_num].camera_name, MMAL_PARAMETER_CAMERA_INFO_MAX_STR_LEN);
            camera_name[MMAL_PARAMETER_CAMERA_INFO_MAX_STR_LEN-1] = 0;
         }
         else
            vcos_log_error("Cannot read camera info, keeping the defaults for OV5647");
      }
      else
      {
         // Older firmware
         // Nothing to do here, keep the defaults for OV5647
      }

      mmal_component_destroy(camera_info);
   }
   else
   {
      vcos_log_error("Failed to create camera_info component");
   }

   // default to OV5647 if nothing detected..
   if (*width == 0)
      *width = 2592;
   if (*height == 0)
      *height = 1944;
}
void default_status(RASPIVID_STATE *state)
{
   if (!state)
   {
      vcos_assert(0);
      return;
   }

   // Default everything to zero
   memset(state, 0, sizeof(RASPIVID_STATE));
   
   strncpy(state->common_settings.camera_name, "(Unknown)", MMAL_PARAMETER_CAMERA_INFO_MAX_STR_LEN);
   // We dont set width and height since these will be specific to the app being built.

//   state->common_settings.cameraNum = 1;
   state->common_settings.sensor_mode = 5;

   // Now set anything non-zero
   state->timeout = -1; 
   state->common_settings.width = 1920;    
   state->common_settings.height = 1080;     
   state->encoding = MMAL_ENCODING_H264;
   state->bitrate = 0; // 0 for variable bit rate
   state->intraperiod = 15;    // Not set
   state->quantisationParameter = 30;
   state->quantisationMin = 20;
   state->quantisationMax = 40;
   state->immutableInput = 1;
   state->profile = MMAL_VIDEO_PROFILE_H264_HIGH;
   state->level = MMAL_VIDEO_LEVEL_H264_41;
   state->waitMethod = 0;     //remove
   state->bInlineHeaders = 0;
   state->frame = 0;             //remove??
   state->addSPSTiming = MMAL_FALSE;
   state->slices = 1;
   state->mode=NOT_RUNNING;

   // Set up the camera_parameters to default
   state->camera_parameters.sharpness = 0;
   state->camera_parameters.contrast = 0;
   state->camera_parameters.brightness = 50;
   state->camera_parameters.saturation = 0;
   state->camera_parameters.ISO = 0;                    // 0 = auto
   state->camera_parameters.videoStabilisation = 0;
   state->camera_parameters.exposureCompensation = 0;
   state->camera_parameters.exposureMode = MMAL_PARAM_EXPOSUREMODE_AUTO;
   state->camera_parameters.flickerAvoidMode = MMAL_PARAM_FLICKERAVOID_OFF;
   state->camera_parameters.exposureMeterMode = MMAL_PARAM_EXPOSUREMETERINGMODE_SPOT;  //from MMAL_PARAM_EXPOSUREMETERINGMODE_AVERAGE
   state->camera_parameters.awbMode = MMAL_PARAM_AWBMODE_AUTO;
   state->camera_parameters.imageEffect = MMAL_PARAM_IMAGEFX_NONE;
   state->camera_parameters.colourEffects.enable = 0;
   state->camera_parameters.colourEffects.u = 128;
   state->camera_parameters.colourEffects.v = 128;
   state->camera_parameters.rotation = 0;
   state->camera_parameters.hflip = state->camera_parameters.vflip = 0;
   state->camera_parameters.roi.x = state->camera_parameters.roi.y = 0.0;
   state->camera_parameters.roi.w = state->camera_parameters.roi.h = 1.0;
   state->camera_parameters.shutter_speed = 0;          // 0 = auto
   state->camera_parameters.awb_gains_r = 0;      // Only have any function if AWB OFF is used.
   state->camera_parameters.awb_gains_b = 0;
   state->camera_parameters.drc_level = MMAL_PARAMETER_DRC_STRENGTH_OFF;
   state->camera_parameters.stats_pass = MMAL_FALSE;
   state->camera_parameters.enable_annotate = 0;
   state->camera_parameters.annotate_string[0] = '\0';
   state->camera_parameters.annotate_text_size = 0;	//Use firmware default
   state->camera_parameters.annotate_text_colour = -1;   //Use firmware default
   state->camera_parameters.annotate_bg_colour = -1;     //Use firmware default
   state->camera_parameters.stereo_mode.mode = MMAL_STEREOSCOPIC_MODE_NONE;
   state->camera_parameters.stereo_mode.decimate = MMAL_FALSE;
   state->camera_parameters.stereo_mode.swap_eyes = MMAL_FALSE;
}

int allocate_audio_encode(AENCODE_CTX *actx) 
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
  int save_vflip=state->camera_parameters.vflip, save_hflip=state->camera_parameters.hflip;
  state->common_settings.width = state->common_settings.ovl.width;
  state->common_settings.height = state->common_settings.ovl.height;
  state->common_settings.cameraNum = cam2;
  state->camera_parameters.vflip = state->vflip_o;
  state->camera_parameters.hflip = state->hflip_o;
     
  if ((status = create_camera_component(state)) != MMAL_SUCCESS)
    {
    vcos_log_error("%s: Failed to create overlay camera %d component", __func__, cam2);
    return -1;
    }

  state->common_settings.width = save_width;
  state->common_settings.height = save_height;
  state->common_settings.cameraNum = cam;
  state->camera_parameters.vflip = save_vflip; 
  state->camera_parameters.hflip = save_hflip; 

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

void destroy_video_stream(RASPIVID_STATE *state)
{
      // Disable all our ports that are not handled by connections
  if (state->camera_component) 
    {
    check_disable_port(state->camera_component->output[MMAL_CAMERA_PREVIEW_PORT]);  
    check_disable_port(state->camera_component->output[MMAL_CAMERA_CAPTURE_PORT]);
    }
  if (state->camera2_component) 
    {
    check_disable_port(state->camera2_component->output[MMAL_CAMERA_PREVIEW_PORT]); 
    check_disable_port(state->camera2_component->output[MMAL_CAMERA_CAPTURE_PORT]);
    }  
   
  if (state->hvs_main_connection) mmal_connection_destroy(state->hvs_main_connection);
  if (state->hvs_ovl_connection) mmal_connection_destroy(state->hvs_ovl_connection);
  if (state->hvs_component->input[2]->is_enabled) mmal_port_disable(state->hvs_component->input[2]);

    // Disable and destroy components 
  if (state->hvs_component) mmal_component_disable(state->hvs_component);
  if (state->camera_component) mmal_component_disable(state->camera_component);
  if (state->camera2_component) mmal_component_disable(state->camera2_component);

  destroy_hvs_component(state);
  destroy_camera_component(state);

  return;
}

void hvs_input_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
  if (buffer->user_data)
    {
    cairo_surface_destroy(buffer->user_data);
    }
  mmal_buffer_header_release(buffer);
}

int allocate_alsa(AENCODE_CTX *actx)
{
  snd_pcm_hw_params_t *params;
  snd_pcm_uframes_t buffer_size = 0;
  snd_pcm_uframes_t chunk_size = 0;
  snd_pcm_uframes_t buffer_frames = 0;
  size_t bits_per_sample, bits_per_frame;
  size_t chunk_bytes;
  struct 
    {
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
  
  err = snd_pcm_open(&actx->pcmhnd, actx->adev, SND_PCM_STREAM_CAPTURE, 0);
//err = snd_pcm_open(&actx->pcmhnd, actx->adev, SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK);

  if (err < 0) 
    {
    fprintf(stdout, "%s open error: %s\n", actx->adev, snd_strerror(err));
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
  if (actx->pcmhnd) 
    {
    snd_pcm_close(actx->pcmhnd);
    actx->pcmhnd = NULL;
    }
  free(actx->pcmbuf);
  free(actx->rlbufs);
} 

int allocate_fmtctx(char *dest, FORMAT_CTX *fctx, RASPIVID_STATE *state) 
{
  int status=0;
  
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
    
  AVCodecContext *vctx = avcodec_alloc_context3(h264_codec); 
  if (!vctx) 
    {
    fprintf(stderr, "Could not alloc an video encoding context\n");
    return -1;
    }	

  vctx->codec_id = AV_CODEC_ID_H264;
  vctx->bit_rate = 0;
  vctx->qmin = state->quantisationMin;
  vctx->qmax = state->quantisationMax;
  vctx->width = vctx->coded_width  = state->common_settings.width;
  vctx->height = vctx->coded_height = state->common_settings.height;
  
  vctx->sample_rate = state->framerate;
  vctx->gop_size = state->intraperiod;                  
  vctx->pix_fmt = AV_PIX_FMT_YUV420P; 
  
  if (fctx->fmtctx->oformat->flags & AVFMT_GLOBALHEADER) { // Some container formats (like MP4) require global headers to be present.
    vctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;}
  
  status = avcodec_parameters_from_context(h264_video_strm->codecpar, vctx);
  if (status < 0) 
    {
    fprintf(stderr, "Could not initialize stream parameters\n");
    return -1;
    }
    
  avcodec_free_context(&vctx);
  
  h264_video_strm->time_base.den = state->framerate;   // Set the sample rate for the container
  h264_video_strm->time_base.num = 1;
  h264_video_strm->avg_frame_rate.num = state->framerate;   // Set the sample rate for the container
  h264_video_strm->avg_frame_rate.den = 1;
  h264_video_strm->r_frame_rate.num = state->framerate;   // Set the sample rate for the container
  h264_video_strm->r_frame_rate.den = 1;

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

//  fctx->audctx->channels       = iparms.channels;
  fctx->audctx->channels       = state->achannels;
//  fctx->audctx->channel_layout = av_get_default_channel_layout(iparms.channels);
   fctx->audctx->channel_layout = av_get_default_channel_layout(state->achannels);
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
		
  if ((status = av_dict_set(&options, "rtmp_live", "live", 0)) < 0) {
    fprintf(stderr, "rtmp live option: %s\n", av_err2str(status));}
    	
  if (memcmp(dest, "file:", 5)) options=NULL;
  if ((status =  avio_open2(&fctx->ioctx, dest, AVIO_FLAG_WRITE, NULL, &options)))
    {
    fprintf(stderr, "Could not open output file '%s' (error '%s')\n", dest, av_err2str(status));
    return -1;
    }
        
  fctx->fmtctx->pb = fctx->ioctx;
//  write flv header 
  fctx->fmtctx->start_time_realtime=0; 

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
  if (fctx->fmtctx)
    {
    if (fctx->ioctx && fctx->ioctx->seekable == 1)
      {
      status = av_write_trailer(fctx->fmtctx); 
      if (status < 0) {fprintf(stderr, "Write ouput trailer failed! STATUS %d\n", status);}
      }  
    status = avio_closep(&fctx->ioctx);	
    if (status < 0)
      {
      fprintf(stderr, "Could not close output file (error '%s')\n", av_err2str(status));
      return -1; 
      }

    avformat_free_context(fctx->fmtctx);
    fctx->fmtctx=NULL;
    } 
  if (fctx->audctx) {avcodec_free_context(&fctx->audctx);}
    
  return 0;
}

int write_audio(RASPIVID_STATE *state, int samples)
{
  int status; 
  AVFrame *infrm = state->encodectx.infrm;
  AVFrame *outfrm = state->encodectx.outfrm;
  AVAudioFifo *fifo = state->encodectx.fifo;
  AVFormatContext *urlctx = state->urlctx.fmtctx;
  AVFormatContext *filectx = state->filectx.fmtctx;
  SwrContext *resample_ctx = state->encodectx.swrctx;
  AVCodecContext *aac_codec_ctx = state->encodectx.audctx;
  sem_t *mutex = state->callback_data.mutex;
  AVPacket packet;
  int64_t save_pts=0, calc_pts; 
  float sample_const=44.1;
  
  while (av_audio_fifo_size(fifo) >= samples)
    {
    outfrm->pts = outfrm->pkt_dts = save_pts = state->encodectx.audio_sample_cnt/sample_const;
    status = av_audio_fifo_read(fifo, (void **)infrm->data, AUDIO_SIZE);
    if (status < 0) 
      {
      fprintf(stderr, "fifo read failed! %d %s\n", status, av_err2str(status));
      return -1;
      }
    else
      {
      state->encodectx.audio_sample_cnt += status;
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
      if (status == AVERROR(EAGAIN) || packet.pts < 0) // If the encoder asks for more data to be able to provide an encoded frame, return indicating that no data is present.
	{
	status = 0;
	goto cleanup;
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
	    packet.stream_index = 1;
	    int url_status=0, file_status=0;
	    sem_wait(mutex);
	    if (urlctx) url_status = av_write_frame(urlctx, &packet);
	    if (url_status) fprintf(stderr, "Could not audio write frame to url (error '%s')\n", av_err2str(url_status));
	    if (filectx) file_status = av_write_frame(filectx, &packet);
	    if (file_status) fprintf(stderr, "Could not audio write frame to file (error '%s')\n", av_err2str(file_status));
	    sem_post(mutex); 
	    if (url_status || file_status) goto cleanup;
	    }
    }
    return status;
    
cleanup:
    av_packet_unref(&packet);
    return status; 
}

void flush_audio(RASPIVID_STATE *state)
{
  write_audio(state, 1);
  int rc=0, url_status=0, file_status=0;
    if (state->encodectx.audctx)
      {
      AVPacket packet;
      av_init_packet(&packet); 
      avcodec_send_frame(state->encodectx.audctx, NULL); 
      rc = avcodec_receive_packet(state->encodectx.audctx, &packet);
      while (!rc) 
	{
	packet.stream_index = 1;
	packet.duration=0;
	if (state->urlctx.fmtctx) url_status = av_write_frame(state->urlctx.fmtctx, &packet);
	if (url_status) printf("Flush audio write to url status %d\n", url_status);
	if (state->filectx.fmtctx) file_status = av_write_frame(state->filectx.fmtctx, &packet);
	if (file_status) printf("Flush audio write to file status %d\n", file_status);
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
    return;		
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

void suspend(snd_pcm_t *handle)
{
  int res;
  while ((res = snd_pcm_resume(handle)) == -EAGAIN)
    sleep(1);	
  if (res < 0)
    {
    if ((res = snd_pcm_prepare(handle)) < 0) 
      {
      fprintf(stderr, "suspend: prepare error: %s\n", snd_strerror(res));
      exit(-3);
      }
    }
}

int read_pcm(RASPIVID_STATE *state)
{

// *  currently setup for blocking (0) reads [see allocate_alsa snd_pcm_open()]
 //* so all the logic is included even wait that would be unneed unless
// * open set too non-blocked (SND_PCM_NONBLOCK) 

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
//    printf("pcm %d\n", r);

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
  write_audio(state, AUDIO_SIZE);
  return result;
}

void toggle_stream(RASPIVID_STATE *state, int run_status)
{
  if (state->encodectx.pcmhnd && run_status)
    {
    snd_pcm_drop(state->encodectx.pcmhnd);
    snd_pcm_prepare(state->encodectx.pcmhnd);
    snd_pcm_start(state->encodectx.pcmhnd);
    state->encodectx.audio_sample_cnt = 0;
    } 

  bcm2835_gpio_write(GPIO_LED, run_status);
  bcm2835_gpio_write(GPIO_MODEM_LED, run_status);
  
  mmal_port_parameter_set_boolean(state->camera_component->output[MMAL_CAMERA_VIDEO_PORT], MMAL_PARAMETER_CAPTURE, run_status);
  mmal_port_parameter_set_boolean(state->camera2_component->output[MMAL_CAMERA_VIDEO_PORT], MMAL_PARAMETER_CAPTURE, run_status);

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

void adjust_q(RASPIVID_STATE *state, char *msg)
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
      if (msg) sprintf(msg, "Quantization %d", param.value);
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
        if (msg) sprintf(msg, "Quantization %d", param.value);
        status = mmal_port_parameter_set(state->encoder_component->output[0], &param.hdr);
        if (status != MMAL_SUCCESS) {vcos_log_error("Unable to reset QP");}
        *write_variance = 0;
        }
      else
        {
        if ((param.value == state->quantisationMax || param.value == state->quantisationMin) && !(atQlimit))
          {
          if (msg) sprintf(msg, "Quantization %d at limit", param.value);
          atQlimit = MMAL_TRUE;
          }
        *write_variance = 0;
        }
      }
    }
}

void encoder_buffer_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
  MMAL_BUFFER_HEADER_T *new_buffer;
  int64_t pts = 0;
  static int64_t start_pts=0;
	
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
	if (buffer->pts != pData->pstate->lasttime)
	  {
	    if (pData->pstate->frame == 0) 
	    {
	    start_pts = buffer->pts;
	    }
	  pData->pstate->lasttime = buffer->pts;
	  pts = buffer->pts - start_pts;
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
	    if (!side_data) 
	      {
	      fprintf(stderr, "%s\n", AVERROR(ENOMEM));
              exit(-4);
	      }
	    memcpy(side_data, buffer->data+buffer->offset, buffer->length);
	    }
	  int64_t wstart = get_microseconds64();
	  int url_status=0, file_status=0;
	  sem_wait(pData->mutex);
	  if (pstate->urlctx.fmtctx) url_status=av_write_frame(pstate->urlctx.fmtctx, packet);
	  if (url_status) fprintf(stderr, "video frame write error to url %d %s\n", url_status, av_err2str(url_status));
          if (pstate->filectx.fmtctx) file_status=av_write_frame(pstate->filectx.fmtctx, packet);					
	  if (file_status) fprintf(stderr, "video frame write error to file %d %s\n", file_status, av_err2str(file_status));
          sem_post(pData->mutex);
          pData->wvariance += (get_microseconds64() - wstart) - pData->wtargettime;

	  if (url_status || file_status) 
	    {
	    bytes_written = 0;
	    }
	  else 
	    {
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
    if (new_buffer) status = mmal_port_send_buffer(port, new_buffer);
    if (!new_buffer || status != MMAL_SUCCESS) vcos_log_error("Unable to return a buffer to the encoder port");
    }
}
