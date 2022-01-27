#include "racecamCommon.h"

int64_t last_pts = 0;
int64_t adjust_pts = 1;

void hvs_input_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
  if (buffer->user_data)
    {
    cairo_surface_destroy(buffer->user_data);
    } 
  mmal_buffer_header_release(buffer);
} 
int create_video_stream(RACECAM_STATE *state)
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
  MMAL_STATUS_T status = MMAL_SUCCESS;
   
    // Setup for sensor specific parameters, only set W/H settings if zero on entry
  int max_width = 0, max_height = 0, i;
  for (i = 0; i < 2; i++)
    {
    get_sensor_defaults(state->common_settings[i].cameraNum, state->common_settings[i].camera_name, &max_width, &max_height);
    if (state->common_settings[i].cam.width > max_width || state->common_settings[i].cam.height > max_height)
      {
      log_error("Resolution larger than sensor %dX%d for camera %d\n", max_width, max_height, state->common_settings[i].cameraNum);
      return -1;
      }
    state->camera_component[i] = create_camera_component(state, i);
    if (!state->camera_component[i])
      { 
      log_error("%s: Failed to create %d camera %d component", __func__, i, state->common_settings[i].cameraNum);
      return -1;
      }
    }
	
	//setup hvs inputs format
  state->hvs[0].format = state->camera_component[MAIN_CAMERA]->output[MMAL_CAMERA_VIDEO_PORT]->format;
  state->hvs[1].format = state->camera_component[OVERLAY_CAMERA]->output[MMAL_CAMERA_VIDEO_PORT]->format;


  MMAL_ES_FORMAT_T *text = mmal_format_alloc();
  mmal_format_copy(text, state->hvs[0].format); 

  text->encoding = MMAL_ENCODING_BGRA;
  text->es->video.width = VCOS_ALIGN_UP(state->common_settings[MAIN_CAMERA].cam.width, 32);
  text->es->video.height = VCOS_ALIGN_UP(state->common_settings[MAIN_CAMERA].cam.height, 16);
  text->es->video.crop.x = 0;
  text->es->video.crop.y = 0;
  text->es->video.crop.width = state->common_settings[MAIN_CAMERA].cam.width;
  text->es->video.crop.height = state->common_settings[MAIN_CAMERA].cam.height;

  state->hvs[2].format = text; 
					
  state->hvs_component = create_hvs_component(state);
  mmal_format_free(text);

  if (!state->hvs_component)
    {
    log_error("%s: Failed to create hvs component", __func__);
    destroy_component(&state->camera_component[MAIN_CAMERA]);
    destroy_component(&state->camera_component[OVERLAY_CAMERA]);
    return -1;
    } 
     
  if ((status = connect_ports(state->camera_component[MAIN_CAMERA]->output[MMAL_CAMERA_VIDEO_PORT],
      state->hvs_component->input[0], &state->hvs_main_connection)) != MMAL_SUCCESS)
    {
    log_error("%s: Failed to connect camera video port to hvs input", __func__); 
    state->hvs_main_connection = NULL;
    return -1;
    } 
	
  if ((status = connect_ports(state->camera_component[OVERLAY_CAMERA]->output[MMAL_CAMERA_VIDEO_PORT],
      state->hvs_component->input[1], &state->hvs_ovl_connection)) != MMAL_SUCCESS)
    {
    log_error("%s: Failed to connect camera2 video port to hvs input", __func__); 
    state->hvs_ovl_connection = NULL;
    return -1;
    } 
	
	
//    call pool func 	
  state->hvs_component->input[2]->buffer_num = state->hvs_component->input[2]->buffer_num_min+1;
  state->hvs_component->input[2]->buffer_size = state->hvs_component->input[2]->buffer_size_min;
  state->hvs_textin_pool = mmal_pool_create(state->hvs_component->input[2]->buffer_num, state->hvs_component->input[2]->buffer_size);

  if ((status = mmal_port_enable(state->hvs_component->input[2], hvs_input_callback)) != MMAL_SUCCESS)
    {
    log_error("%s: Failed to enable hvs text input", __func__); 
    return -1;
    } 	

  state->splitter_component = create_splitter_component(&state->splitter_component, state->hvs_component->output[0]->format);
  if (!state->splitter_component)
    {
    log_error("failed to create splitter");
    return -1;
    }

  status = connect_ports(state->hvs_component->output[0], state->splitter_component->input[0], &state->splitter_connection);	
  if (status != MMAL_SUCCESS)
    {
    log_error("%s: Failed to connect hvs to splitter input", __func__); 
    state->splitter_connection = NULL;
    return -1;
    }


// should the format be copied from HVS or splitter???
  if (state->output_state[FILE_STRM].run_state)
    {
    state->encoder_component[FILE_STRM] = create_encoder_component(state, state->hvs_component->output[0]->format);
    if (!state->encoder_component[FILE_STRM]) 
      {
      return -1;
      }
    
    status = connect_ports(state->splitter_component->output[0], state->encoder_component[FILE_STRM]->input[0], &state->encoder_connection[FILE_STRM]);
    if (status != MMAL_SUCCESS)
      {
      log_error("%s: Failed to connect hvs to encoder input", __func__); 
      state->encoder_connection[FILE_STRM] = NULL;
      return -1;
      }

      /* Create pool of buffer headers for the output port to consume */
    state->encoder_pool[FILE_STRM] = mmal_port_pool_create(state->encoder_component[FILE_STRM]->output[0], 
      state->encoder_component[FILE_STRM]->output[0]->buffer_num, state->encoder_component[FILE_STRM]->output[0]->buffer_size);
    if (!state->encoder_pool[FILE_STRM])
      {
      log_error("Failed to create buffer header pool for encoder output port %d", FILE_STRM);
      return -1;
      }

    state->userdata[FILE_STRM].pool = state->encoder_pool[FILE_STRM];
    state->userdata[FILE_STRM].s_time = 0;
	
    state->encoder_component[FILE_STRM]->output[0]->userdata = (struct MMAL_PORT_USERDATA_T *)&state->userdata[FILE_STRM];

    status = mmal_port_enable(state->encoder_component[FILE_STRM]->output[0], encoder_buffer_callback);
    if (status) 
      {
      log_error("enable port failed");
      }

    int q, num = mmal_queue_length(state->encoder_pool[FILE_STRM]->queue);    
    for (q=0; q<num; q++)
      {
      MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get(state->encoder_pool[FILE_STRM]->queue);
      if (!buffer) log_error("Unable to get a required buffer %d:%d from pool queue", FILE_STRM, q);
      if (mmal_port_send_buffer(state->encoder_component[FILE_STRM]->output[0], buffer)!= MMAL_SUCCESS)
      log_error("Unable to send a buffer to encoder output port (%d:%d)", FILE_STRM, q);
      }
    }
  	
  if (state->output_state[URL_STRM].run_state)
    {
      // create url encoder and  pool 
    state->encoder_component[URL_STRM] = create_encoder_component(state, state->hvs_component->output[0]->format);
    if (!state->encoder_component[URL_STRM]) 
      {
      return -1;
      }
    
    status = connect_ports(state->splitter_component->output[1], state->encoder_component[URL_STRM]->input[0], &state->encoder_connection[URL_STRM]);
    if (status != MMAL_SUCCESS)
      {
      log_error("%s: Failed to connect hvs to encoder input", __func__); 
      state->encoder_connection[URL_STRM] = NULL;
      return -1;
      }

      /* Create pool of buffer headers for the output port to consume */
    state->encoder_pool[URL_STRM] = mmal_port_pool_create(state->encoder_component[URL_STRM]->output[0], 
      state->encoder_component[URL_STRM]->output[0]->buffer_num, state->encoder_component[URL_STRM]->output[0]->buffer_size);
    if (!state->encoder_pool[URL_STRM])
      {
      log_error("Failed to create buffer header pool for encoder output port %d", FILE_STRM);
      return -1;
      }

    state->userdata[URL_STRM].pool = state->encoder_pool[URL_STRM];
    state->userdata[URL_STRM].s_time = 0;
	
    state->encoder_component[URL_STRM]->output[0]->userdata = (struct MMAL_PORT_USERDATA_T *)&state->userdata[URL_STRM];

    status = mmal_port_enable(state->encoder_component[URL_STRM]->output[0], encoder_buffer_callback);
    if (status) 
      {
      log_error("enable port failed");
      }

    int q, num = mmal_queue_length(state->encoder_pool[URL_STRM]->queue);    
    for (q=0; q<num; q++)
      {
      MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get(state->encoder_pool[URL_STRM]->queue);
      if (!buffer) log_error("Unable to get a required buffer %d:%d from pool queue", URL_STRM, q);
      if (mmal_port_send_buffer(state->encoder_component[URL_STRM]->output[0], buffer)!= MMAL_SUCCESS)
      log_error("Unable to send a buffer to encoder output port (%d:%d)", URL_STRM, q);
      }
    }
	      
  if (state->preview_mode)
    {
    state->preview_component = create_preview_component(state);
    if (!state->preview_component)
      {
      log_error("failed to create preview");
      return -1;
      }
    switch (state->preview_mode)
      {
      case PREVIEW_MAIN:
	status = connect_ports(state->camera_component[MAIN_CAMERA]->output[MMAL_CAMERA_PREVIEW_PORT], 
		state->preview_component->input[0], &state->preview_connection);
	break;
      case PREVIEW_OVRL:
	status = connect_ports(state->camera_component[OVERLAY_CAMERA]->output[MMAL_CAMERA_PREVIEW_PORT], 
		state->preview_component->input[0], &state->preview_connection);
	break;
      case PREVIEW_COMP:
	status = connect_ports(state->splitter_component->output[2], 
		state->preview_component->input[0], &state->preview_connection);
	break;
      }
    if (status != MMAL_SUCCESS)
      {
      log_error("%s: Failed to connect preview input", __func__); 
      state->preview_connection = NULL;
      return -1;
      } 
    } 
  return 0;
}

void destroy_video_stream(RACECAM_STATE *state)
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__); 
  if (state->preview_mode)
    {
    if (state->preview_connection) mmal_connection_destroy(state->preview_connection);
    if (state->preview_component) check_disable_port(state->preview_component->input[0]);
    destroy_component(&state->preview_component);
    } 
	
	if (state->splitter_component) check_disable_port(state->splitter_component->output[2]);
	if (state->splitter_connection) mmal_connection_destroy(state->splitter_connection);
	
  if (state->output_state[FILE_STRM].run_state)
		{
		if (state->encoder_component[FILE_STRM]) check_disable_port(state->encoder_component[FILE_STRM]->output[0]);
		if (state->encoder_connection[FILE_STRM]) mmal_connection_destroy(state->encoder_connection[FILE_STRM]);
		if (state->encoder_pool[FILE_STRM])
			{
			mmal_port_pool_destroy(state->encoder_component[FILE_STRM]->output[0], state->encoder_pool[FILE_STRM]);
			}
		destroy_component(&state->encoder_component[FILE_STRM]);
		}
	
  if (state->output_state[URL_STRM].run_state)
    {
    if (state->encoder_component[URL_STRM]) check_disable_port(state->encoder_component[URL_STRM]->output[0]);
		if (state->encoder_connection[URL_STRM]) mmal_connection_destroy(state->encoder_connection[URL_STRM]);
		if (state->encoder_pool[URL_STRM])
			{
			mmal_port_pool_destroy(state->encoder_component[URL_STRM]->output[0], state->encoder_pool[URL_STRM]);
			}
		destroy_component(&state->encoder_component[URL_STRM]);
    }
	
	destroy_component(&state->splitter_component);

      // Disable all our ports that are not handled by connections
  if (state->camera_component[MAIN_CAMERA]) 
    {
    check_disable_port(state->camera_component[MAIN_CAMERA]->output[MMAL_CAMERA_PREVIEW_PORT]);  
    check_disable_port(state->camera_component[MAIN_CAMERA]->output[MMAL_CAMERA_CAPTURE_PORT]);
    }
  if (state->camera_component[OVERLAY_CAMERA]) 
    {
    check_disable_port(state->camera_component[OVERLAY_CAMERA]->output[MMAL_CAMERA_PREVIEW_PORT]); 
    check_disable_port(state->camera_component[OVERLAY_CAMERA]->output[MMAL_CAMERA_CAPTURE_PORT]);
    }  

  if (state->hvs_main_connection) mmal_connection_destroy(state->hvs_main_connection);
  if (state->hvs_ovl_connection) mmal_connection_destroy(state->hvs_ovl_connection);
  if (state->hvs_component->input[2]->is_enabled) mmal_port_disable(state->hvs_component->input[2]);

	if (state->hvs_textin_pool)
		{
		mmal_port_pool_destroy(state->hvs_component->input[2], state->hvs_textin_pool);
		} 
  if (state->hvs_component) mmal_component_disable(state->hvs_component);
  if (state->camera_component[MAIN_CAMERA]) mmal_component_disable(state->camera_component[MAIN_CAMERA]);
  if (state->camera_component[OVERLAY_CAMERA]) mmal_component_disable(state->camera_component[OVERLAY_CAMERA]);

  destroy_component(&state->hvs_component);
  destroy_component(&state->camera_component[MAIN_CAMERA]);
  destroy_component(&state->camera_component[OVERLAY_CAMERA]);

  return;
}

void check_output_status(RACECAM_STATE *state)
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
  if (state->current_mode <= 0) return;
  int running = 0, stopped = 0, i;
  for (i=0; i<MAX_NUMBER_OF_STREAMS; i++)
    {
    if (state->output_state[i].run_state) running++;
    if (state->output_state[i].run_state < 0)
      {
      stopped++;
      if (state->encoder_component[i]) check_disable_port(state->encoder_component[i]->output[0]);
      if (state->encoder_connection[i]) mmal_connection_destroy(state->encoder_connection[i]);
      if (state->encoder_pool[i])
        {
        mmal_port_pool_destroy(state->encoder_component[i]->output[0], state->encoder_pool[i]);
        }
      destroy_component(&state->encoder_component[i]);
      }
    }
  if (stopped)
    {
    if (stopped == running && state->current_mode > 0) state->current_mode *= -1;
    }
  
}

int create_video_preview(RACECAM_STATE *state)
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
  MMAL_STATUS_T status = MMAL_SUCCESS;
     
    // Setup for sensor specific parameters, only set W/H settings if zero on entry
  int max_width = 0, max_height = 0, i;
  for (i = 0; i < 2; i++)
    {
    get_sensor_defaults(state->common_settings[i].cameraNum, state->common_settings[i].camera_name, &max_width, &max_height);

    if (state->common_settings[i].cam.width > max_width || state->common_settings[i].cam.height > max_height)
      {
      log_error("Resolution larger than sensor %dX%d for camera %d\n", max_width, max_height, state->common_settings[i].cameraNum);
      return -1;
      }

      state->camera_component[i] = create_camera_component(state, i);
      if (!state->camera_component[i])
	{ 
	log_error("%s: Failed to create %d camera %d component", __func__, i, state->common_settings[i].cameraNum);
	return -1;
	}
    }
  if (state->preview_mode == PREVIEW_COMP)
    {
	//setup hvs inputs format
    state->hvs[0].format = state->camera_component[MAIN_CAMERA]->output[MMAL_CAMERA_PREVIEW_PORT]->format;
    state->hvs[1].format = state->camera_component[OVERLAY_CAMERA]->output[MMAL_CAMERA_PREVIEW_PORT]->format;

    state->hvs[2].enable = MMAL_FALSE;
				
    state->hvs_component = create_hvs_component(state);

    if (!state->hvs_component)
      {
      log_error("%s: Failed to create hvs component", __func__);
      destroy_component(&state->camera_component[MAIN_CAMERA]);
      destroy_component(&state->camera_component[OVERLAY_CAMERA]);
      return -1;
      } 
     
    if ((status = connect_ports(state->camera_component[MAIN_CAMERA]->output[MMAL_CAMERA_PREVIEW_PORT],
      state->hvs_component->input[0], &state->hvs_main_connection)) != MMAL_SUCCESS)
      {
      log_error("%s: Failed to connect camera video port to hvs input", __func__); 
      state->hvs_main_connection = NULL;
      return -1;
      } 
	
    if ((status = connect_ports(state->camera_component[OVERLAY_CAMERA]->output[MMAL_CAMERA_PREVIEW_PORT],
      state->hvs_component->input[1], &state->hvs_ovl_connection)) != MMAL_SUCCESS)
      {
      log_error("%s: Failed to connect camera2 video port to hvs input", __func__); 
      state->hvs_ovl_connection = NULL;
      return -1;
      } 
    }
	
  state->preview_component = create_preview_component(state);
  if (!state->preview_component)
    {
    log_error("failed to create preview");
    return -1;
    }
  switch (state->preview_mode)
    {
    case PREVIEW_MAIN:
      status = connect_ports(state->camera_component[MAIN_CAMERA]->output[MMAL_CAMERA_PREVIEW_PORT], 
        state->preview_component->input[0], &state->preview_connection);
      break;
    case PREVIEW_OVRL:
      status = connect_ports(state->camera_component[OVERLAY_CAMERA]->output[MMAL_CAMERA_PREVIEW_PORT], 
        state->preview_component->input[0], &state->preview_connection);
      break;
    case PREVIEW_COMP:
      status = connect_ports(state->hvs_component->output[0], 
        state->preview_component->input[0], &state->preview_connection);
      break;
    }
  if (status != MMAL_SUCCESS)
    {
    log_error("%s: Failed to connect preview input", __func__); 
    state->preview_connection = NULL;
    return -1;
    } 
    
  return 0;
}

void destroy_video_preview(RACECAM_STATE *state)
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
  if (state->preview_connection) mmal_connection_destroy(state->preview_connection);
  if (state->preview_component) check_disable_port(state->preview_component->input[0]);

  destroy_component(&state->preview_component);

      // Disable all our ports that are not handled by connections
  if (state->camera_component[MAIN_CAMERA]) 
    {
    check_disable_port(state->camera_component[MAIN_CAMERA]->output[MMAL_CAMERA_PREVIEW_PORT]);  
    check_disable_port(state->camera_component[MAIN_CAMERA]->output[MMAL_CAMERA_CAPTURE_PORT]);
    }
  if (state->camera_component[OVERLAY_CAMERA]) 
    {
    check_disable_port(state->camera_component[OVERLAY_CAMERA]->output[MMAL_CAMERA_PREVIEW_PORT]); 
    check_disable_port(state->camera_component[OVERLAY_CAMERA]->output[MMAL_CAMERA_CAPTURE_PORT]);
    }  
   
  if (state->hvs_component) mmal_component_disable(state->hvs_component);
  if (state->camera_component[MAIN_CAMERA]) mmal_component_disable(state->camera_component[MAIN_CAMERA]);
  if (state->camera_component[OVERLAY_CAMERA]) mmal_component_disable(state->camera_component[OVERLAY_CAMERA]);

  if (state->hvs_component) destroy_component(&state->hvs_component);
  destroy_component(&state->camera_component[MAIN_CAMERA]);
  destroy_component(&state->camera_component[OVERLAY_CAMERA]);

  return;
}

void *adjust_q(void *arg)
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
  ADJUST_Q_STATE *q_state;
  q_state = (ADJUST_Q_STATE *) arg;
  int *ptr_status=q_state->running;
  int atlimit=0;
  int avg_q=0;

  while (*ptr_status > 0)
    {
    vcos_sleep(Q_WAIT);
    if (q_state->current_sample == NUM_SAMPLES)
      {
      q_state->current_sample = 0;
      MMAL_STATUS_T status;
      MMAL_PARAMETER_UINT32_T param = {{ MMAL_PARAMETER_VIDEO_ENCODE_INITIAL_QUANT, sizeof(param)}, 0};
      status = mmal_port_parameter_get(q_state->port, &param.hdr);
      if (status != MMAL_SUCCESS) {log_error("Unable to get current QP");}
      if (param.value > q_state->min_q && param.value < MAX_Q)
        {
        atlimit = 0;
        avg_q = 0;
        int i;
        for (i=0 ; i<NUM_SAMPLES ; i++)
          {
          avg_q += q_state->samples[i];
          }
        avg_q /= NUM_SAMPLES;
        avg_q = (avg_q/Q_FACTOR)-1;
        param.value += avg_q;
        if (param.value > MAX_Q) param.value = MAX_Q;
        status = mmal_port_parameter_set(q_state->port, &param.hdr);
        if (status == MMAL_SUCCESS)
          {
          log_status("New QP %d", param.value);
          }
        else
          {
          log_error("Unable to reset QP %s", mmal_status_to_string(status));
          }
        }
      else
        {
        if (!atlimit) 
          {
          atlimit = 1;
          log_warning("at limit %d", param.value);
          }
        }
      }
    else
      {
      q_state->samples[q_state->current_sample] = queue_length(q_state->queue);
      q_state->current_sample++;
      }
    }
}

// ALSA stuff
int allocate_alsa(RACECAM_STATE *state)
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
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

  err = snd_pcm_open(&state->pcmhnd, state->adev, SND_PCM_STREAM_CAPTURE, 0);  // not SND_PCM_ASYNC 
// err = snd_pcm_open(&state->pcmhnd, state->adev, SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK);  

  if (err < 0) 
    {
    log_error("%s open error: %s", state->adev, snd_strerror(err));
    return -1;
    }

  snd_pcm_hw_params_alloca(&params);
  err = snd_pcm_hw_params_any(state->pcmhnd, params);
  if (err < 0) 
    {
    log_error("Broken configuration for this PCM: no configurations available");
    return -1;
    }

  err = snd_pcm_hw_params_set_access(state->pcmhnd, params, SND_PCM_ACCESS_RW_INTERLEAVED);

  if (err < 0) 
    {
    log_error("Access type not available");
    return -1;
    }
  err = snd_pcm_hw_params_set_format(state->pcmhnd, params, hwparams.format);
  if (err < 0) 
    {
    log_error("Sample format non available");
    return -1;
    }
  err = snd_pcm_hw_params_set_channels(state->pcmhnd, params, hwparams.channels);
  if (err < 0) 
    {
    log_error("Channels count non available");
    return -1;
    }
  err = snd_pcm_hw_params_set_rate_near(state->pcmhnd, params, &hwparams.rate, 0);
  assert(err >= 0);

  err = snd_pcm_hw_params(state->pcmhnd, params);
  if (err < 0) 
    {
    log_error("Unable to install hw params");
    return -1;
    }
		
  snd_pcm_hw_params_get_period_size(params, &chunk_size, 0);
  snd_pcm_hw_params_get_buffer_size(params, &buffer_size);
  if (chunk_size == buffer_size) 
    {
    log_error("Can't use period equal to buffer size (%lu == %lu)", 
      chunk_size, buffer_size);
    return -1;
    }

  bits_per_sample = snd_pcm_format_physical_width(hwparams.format);
  bits_per_frame = bits_per_sample * hwparams.channels;
  state->bufsize = chunk_size * bits_per_frame / 8;
  
  state->pcmbuf = (u_char *)malloc(state->bufsize);
  if (state->pcmbuf == NULL)  
    {
    log_error("not enough memory");
    return -1;
    }
  state->rlbufs = (u_char *)malloc(state->bufsize); 
  if (state->rlbufs == NULL) 
    {
    log_error("not enough memory");
    return -1;
    }
  return 0;
}

int free_alsa(RACECAM_STATE *state)
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
  if (state->pcmhnd) 
    {
    snd_pcm_close(state->pcmhnd);
    state->pcmhnd = NULL;
    }

  free(state->pcmbuf); 
  free(state->rlbufs);
  
} 

void xrun(snd_pcm_t *handle)
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
  snd_pcm_status_t *status;
  int res;
  snd_pcm_status_alloca(&status);
  if ((res = snd_pcm_status(handle, status))<0) 
    {
    log_error("status error: %s", snd_strerror(res));
    exit(-2);
    }
	
  if (snd_pcm_status_get_state(status) == SND_PCM_STATE_XRUN) 
    {
    if ((res = snd_pcm_prepare(handle))<0) 
      {
      log_error("xrun: prepare error: %s", snd_strerror(res));
      exit(-2);
      }
    return;		
    } 
  if (snd_pcm_status_get_state(status) == SND_PCM_STATE_DRAINING) 
    {
    {
    log_error("capture stream format change? attempting recover...");
    if ((res = snd_pcm_prepare(handle))<0) 
      {
      log_error("xrun(DRAINING): prepare error: %s", snd_strerror(res));
      exit(-2);
      }
    return;
    }
    }
  log_error("read/write error, state = %s", snd_pcm_state_name(snd_pcm_status_get_state(status)));
  exit(-2);
}

void suspend(snd_pcm_t *handle)
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
  int res;
  while ((res = snd_pcm_resume(handle)) == -EAGAIN)
    sleep(1);	
  if (res < 0)
    {
    if ((res = snd_pcm_prepare(handle)) < 0) 
      {
      log_error("suspend: prepare error: %s", snd_strerror(res));
      exit(-3);
      }
    }
}

void read_pcm(RACECAM_STATE *state)
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
// *  currently setup for blocking (0) reads [see allocate_alsa snd_pcm_open()]
// * so all the logic is included even wait that would be unneed unless
// * open set too non-blocked (SND_PCM_NONBLOCK) 

  snd_pcm_t *handle = state->pcmhnd;
  u_char *data_in = state->pcmbuf;
  ssize_t r; 
  size_t result = 0;
  size_t count=256;
  
	while (count > 0) 
    {
    r = snd_pcm_readi(handle, data_in, count);
    
    if (r == -EAGAIN || (r >= 0 && (size_t)r < count)) 
      {
      log_error("ALSA in wait");
      snd_pcm_wait(handle, 100);
      }
    else if (r == -EPIPE) 
      {
      log_error("ALSA xrun");
      xrun(handle);
      } 
    else if (r == -ESTRPIPE)
      {
      log_error("ALSA suspend");
      suspend(handle);
      } 
    else if (r < 0) 
      {
      log_error("read error: %s", snd_strerror(r));
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
  u_char *lptr=state->rlbufs;
  u_char *rptr=state->rlbufs+(state->bufsize/2);
  data_in = state->pcmbuf;
  u_char *data_out[2] = {lptr,rptr};

  x=512;  // number of right and left samples
  for (i=0; i < x; ++i) {
    for (s=0;s < 4; ++s) {
      if (lr) {*rptr = *data_in; ++rptr;}
      else {*lptr = *data_in; ++lptr;}
      ++data_in;}
      if (lr) {lr=0;}
      else {lr=1;}}	

// data_out[0] = data_in;
// data_out[1] = NULL;
  int status=av_audio_fifo_write(state->fifo, (void **)data_out, r);
//  int status=av_audio_fifo_write(state->fifo, (void **)data_in, r);
  if (status < 0)
    {
    log_error("fifo write failed!");
    }
  else
    if (status != r) 
      {
      log_error("fifo did not write all! to write %d written %d", r, status);
      }
  
  encode_queue_audio(state, 0);

}

int allocate_audio_encode(RACECAM_STATE *state) 
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
  int status=0;
  
  last_pts = 0;
  adjust_pts = 1;
//  setup RAW codec and context
// AVCodec *raw_codec = avcodec_find_encoder(AV_CODEC_ID_PCM_S32LE);
  AVCodec *raw_codec = avcodec_find_encoder(AV_CODEC_ID_PCM_S32LE_PLANAR);  // AV_CODEC_ID_PCM_S32LE maybe interleaved ??
  if (!raw_codec)
    {
    log_error("PCM_S32_LE codec id not found!");
    return -1;
    }	

  state->rawctx = avcodec_alloc_context3(raw_codec); 
  if (!state->rawctx) 
    {
    log_error("Could not alloc RAW context");
    return -1;
    }
    
  state->rawctx->channels       = DEFAULT_CHANNELS_IN;    //use default channels???
  state->rawctx->channel_layout = av_get_default_channel_layout(DEFAULT_CHANNELS_IN);    //use default channels???
  state->rawctx->sample_rate    = DEFAULT_SPEED;
  state->rawctx->sample_fmt     = raw_codec->sample_fmts[0];  // AV_SAMPLE_FMT_S32
  state->rawctx->bit_rate       = 2822400;  // or 64000
  state->rawctx->time_base.num  = 1;
  state->rawctx->time_base.den  = DEFAULT_SPEED;
  state->rawctx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;   // Allow the use of the experimental AAC encoder.
  
  
  //  setup AAC codec and stream context
  AVCodec *aac_codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
  if (!aac_codec)
    {
    log_error("AAC codec id not found!");
    return -1;
    }	
 
  state->audctx = avcodec_alloc_context3(aac_codec); 
   
  if (!state->audctx)
    {
    log_error("Could not alloc an encoding context");
    return -1;
    }

  state->audctx->channels       = DEFAULT_CHANNELS_IN;     //use default channels???
  state->audctx->channel_layout = av_get_default_channel_layout(DEFAULT_CHANNELS_IN);  //use default channels???
  state->audctx->sample_rate    = DEFAULT_SPEED;
  state->audctx->sample_fmt     = aac_codec->sample_fmts[0];
  state->audctx->bit_rate       = 64000;
  state->audctx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;  // Allow the use of the experimental AAC encoder.
  
  if ((status = avcodec_open2(state->audctx, aac_codec, NULL) < 0)) 
    {
    log_error("Could not open output codec (error '%s')", av_err2str(status));
    return -1;
    }

//  setup resampler context
  state->swrctx = swr_alloc_set_opts(NULL, av_get_default_channel_layout(state->audctx->channels), state->audctx->sample_fmt,
  state->audctx->sample_rate, av_get_default_channel_layout(state->rawctx->channels), state->rawctx->sample_fmt,
  state->rawctx->sample_rate, 0, NULL);
  if (!state->swrctx)  
    {
    log_error("Could not allocate resample context");
    return -1;
    }
  
  if ((status = swr_init(state->swrctx)) < 0) 
    {
    log_error("Could not open resample context");
    swr_free(&state->swrctx);
    return -1;
    }

// setup fifo sample queue
  if (!(state->fifo = av_audio_fifo_alloc(AV_SAMPLE_FMT_S32P, DEFAULT_CHANNELS_IN, 1)))
//  if (!(state->fifo = av_audio_fifo_alloc(AV_SAMPLE_FMT_S32, DEFAULT_CHANNELS_IN, 1))) 
    {
    log_error("Could not allocate FIFO");
    return -1;
    }
    
  return 0; 
}

void free_audio_encode(RACECAM_STATE *state)
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
	
  if (state->fifo) {av_audio_fifo_free(state->fifo);}

  if (state->swrctx) {swr_init(state->swrctx);}
  
  if (state->rawctx) {avcodec_free_context(&state->rawctx);}
  if (state->audctx) {avcodec_free_context(&state->audctx);}
}

void encode_queue_audio(RACECAM_STATE *state, int flush)
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__); 
  int status=0, send_status=0, recieve_status=0, min_frame=state->audctx->frame_size; 
  AVAudioFifo *fifo = state->fifo;

  SwrContext *resample_ctx = state->swrctx;
  AVCodecContext *aac_codec_ctx = state->audctx;

  float sample_const=44.1;
   
  if  ((!flush) &&  av_audio_fifo_size(fifo) < state->audctx->frame_size)  
    return;

  if (flush) min_frame = 0;
 
  AVFrame *infrm=av_frame_alloc();	
  if (!infrm) {log_error("unable to allocate in frame!");}

  infrm->channel_layout=state->rawctx->channel_layout;
  infrm->sample_rate=state->rawctx->sample_rate;
  infrm->format=state->rawctx->sample_fmt;
  infrm->nb_samples=state->audctx->frame_size; 
    
  status=av_frame_get_buffer(infrm, 0);  
  if (status) {log_error("unable to allocate in frame data! %d %s", status, av_err2str(status));}

  AVFrame *outfrm=av_frame_alloc();	
  if (!outfrm) {log_error("unable to allocate out frame!");}
  
  outfrm->channel_layout=state->audctx->channel_layout;
  outfrm->sample_rate=state->audctx->sample_rate;
  outfrm->format=state->audctx->sample_fmt;
  outfrm->nb_samples=state->audctx->frame_size;
  
  status=av_frame_get_buffer(outfrm, 0);
  if (status) {log_error("unable to allocate out frame data!  %d %s", status, av_err2str(status));}  
  
  do{
    infrm->pts = infrm->pkt_dts = outfrm->pts = outfrm->pkt_dts = state->sample_cnt/sample_const;
    status = av_audio_fifo_read(fifo, (void **)infrm->data, state->audctx->frame_size);
    if (status < 0) 
      {
      log_error("fifo read failed! %d %s", status, av_err2str(status));
      return;
      }
    else
      {
      infrm->nb_samples = status;
      state->sample_cnt += status;
      }
	
    status = swr_convert_frame(resample_ctx, outfrm, infrm);
    if (status) {log_error("Frame convert %d (error '%s')", status, av_err2str(status));}

    AVPacket packet;	
    av_init_packet(&packet); 
    packet.data = NULL;
    packet.size = 0;
    
    if (flush && infrm->nb_samples == 0)
      {
      flush=0;
      status = avcodec_send_frame(aac_codec_ctx, NULL); 
      }
    else
      status = avcodec_send_frame(aac_codec_ctx, outfrm); 
    if (status < 0 && !(status == AVERROR(EAGAIN))) 
      {
      log_error("Could not send packet for encoding (error '%s')", av_err2str(send_status));
       goto cleanup;
      }
    int continue_recieve = 1;
    do{
      recieve_status = avcodec_receive_packet(aac_codec_ctx, &packet);
          
      if ((recieve_status == AVERROR(EAGAIN)) || (recieve_status == AVERROR_EOF)) 
        continue_recieve = 0;
      else  
        {
        if (recieve_status < 0) 
          {
          log_error("Could not encode frame (error '%s')", av_err2str(recieve_status));  
          goto cleanup;
          } 
          if (packet.pts < 0)
            {
            int64_t save_pts = packet.pts;
            packet.pts = last_pts+adjust_pts;
            adjust_pts += (save_pts/sample_const*-1);
            }
          else
            {
            packet.pts += adjust_pts;
           }
          last_pts = packet.pts;
        int i;
        for (i=0;i<2;i++) 
          {
          if (state->output_state[i].queue)
            {
            status = queue_frame(state->output_state[i].queue, packet.data, packet.size, AUDIO_DATA, packet.pts, packet.flags);
            if (status)
              {
              log_error("Queue audio failed queue=%d", i);
              goto cleanup;
              }
            }
            
          }
        }
      } while (continue_recieve);
    } while (av_audio_fifo_size(fifo) > min_frame || flush);
cleanup:
  if (outfrm) {av_frame_free(&outfrm);}
  if (infrm) {av_frame_free(&infrm);}
  return; 
}

// OUTPUT stuff
int allocate_fmtctx(OUTPUT_STATE *o_state)  
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
  int status=0;
  RACECAM_STATE *state=o_state->r_state;
  
//  setup format context and io context
  avformat_alloc_output_context2(&o_state->fmtctx, NULL, "flv", NULL);
  if (!o_state->fmtctx) 
    {
    log_error("Could not allocate output format context");
    return -1;
    }

  if (!(o_state->fmtctx->url = av_strdup(o_state->dest))) 
    {
    log_error("Could not copy dest.");
    return -1;
    }
    
// Setup  H264 codec
  AVCodec *h264_codec = avcodec_find_encoder(AV_CODEC_ID_H264);
  if (!h264_codec)
    {
    log_error("H264 codec id not found!");
    return -1;
    }	
  AVStream *h264_video_strm = avformat_new_stream(o_state->fmtctx, h264_codec);
  if (!h264_video_strm) 
    {
    log_error("Could not allocate H264 stream");
    return -1;
    }
    
  AVCodecContext *vctx = avcodec_alloc_context3(h264_codec); 
  if (!vctx) 
    {
    log_error("Could not alloc an video encoding context");
    return -1;
    }	

  vctx->codec_id = AV_CODEC_ID_H264;  //try MPEG4 for fvlcheck issue 
  vctx->bit_rate = 0;
  vctx->qmin = state->quantisationMin;
  vctx->qmax = 39;
  vctx->width = vctx->coded_width  = state->common_settings[MAIN_CAMERA].cam.width;
  vctx->height = vctx->coded_height = state->common_settings[MAIN_CAMERA].cam.height;
  
  vctx->sample_rate = state->framerate;
  vctx->gop_size = state->intraperiod;                 
  vctx->pix_fmt = AV_PIX_FMT_YUV420P; 
  
  o_state->fmtctx->oformat->flags |= AVFMT_NOTIMESTAMPS;  // new 
  
  if (o_state->fmtctx->oformat->flags & AVFMT_GLOBALHEADER) { // Some container formats (like MP4) require global headers to be present.
    vctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;}
  
  status = avcodec_parameters_from_context(h264_video_strm->codecpar, vctx);
  if (status < 0) 
    {
    log_error("Could not initialize stream parameters");
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
    log_error("AAC codec id not found!");
    return -1;
    }	

  AVStream *aac_audio_strm = avformat_new_stream(o_state->fmtctx, aac_codec);
  if (!aac_audio_strm) 
    {
    log_error("Could not allocate AAC stream");
    return -1;
    }
        
  AVCodecContext *actx = avcodec_alloc_context3(aac_codec);
  if (!actx)
    {
    log_error("Could not alloc an encoding context");
    return -1;
    }
  
  actx->channels       = DEFAULT_CHANNELS_IN;
  actx->channel_layout = av_get_default_channel_layout(DEFAULT_CHANNELS_IN);
  actx->sample_rate    = DEFAULT_SPEED;
  actx->sample_fmt     = aac_codec->sample_fmts[0];
  actx->bit_rate       = 64000;
  actx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;  // Allow the use of the experimental AAC encoder.

  aac_audio_strm->time_base.den = DEFAULT_SPEED;   // Set the sample rate for the container
  aac_audio_strm->time_base.num = 1;
  
    
  if (o_state->fmtctx->oformat->flags & AVFMT_GLOBALHEADER)  // Some container formats (like MP4) require global headers to be present.
    o_state->fmtctx->oformat->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	
  if ((status = avcodec_open2(actx, aac_codec, NULL) < 0)) 
    {
    log_error("Could not open output codec (error '%s')\n", av_err2str(status));
    return -1;
    }
  
  status = avcodec_parameters_from_context(aac_audio_strm->codecpar, actx);
  if (status < 0) 
    {
    log_error("Could not initialize stream parameters");
    return -1;
    }
   
    avcodec_free_context(&actx);
		
  AVDictionary *options = NULL;
  if (!(memcmp(o_state->dest, "file:", 5)))
    {
    if ((status = av_dict_set(&options, "rtmp_live", "live", 0)) < 0)
      {
      log_error("rtmp live option: %s", av_err2str(status));
      }
    }

  if ((status =  avio_open2(&o_state->fmtctx->pb, o_state->dest, AVIO_FLAG_WRITE, NULL, &options)))
    {
    log_error("Could not open output file '%s' (error '%s')", o_state->dest, av_err2str(status));
    return -1;
    }
  if (options) {av_dict_free (&options);}

//  write flv header 
  o_state->fmtctx->start_time_realtime=0; 

  status = avformat_write_header(o_state->fmtctx, NULL);  // null if AVDictionary is unneeded????
  if (status < 0)
    {
    log_error("Write ouput header failed! STATUS %d", status);
    return -1;
    }

//  av_dump_format(o_state->fmtctx, 1, "stderr", 1);  //why not working
  return 0;
  
}

int free_fmtctx(OUTPUT_STATE *o_state)
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
  int status=0;
  if (o_state->fmtctx) 
    {
    if (o_state->fmtctx->pb && o_state->fmtctx->pb->seekable == 1)
      {
      status = av_write_trailer(o_state->fmtctx); 
      if (status < 0) {log_error("Write ouput trailer failed! STATUS %d", status);}
      }  
    status = avio_closep(&o_state->fmtctx->pb);	
    if (status < 0)
      {
      log_error("Could not close output file (error '%s')", av_err2str(status));
      return -1; 
      }
    avformat_free_context(o_state->fmtctx);
    o_state->fmtctx=NULL;
    } 
    
  return 0;
}

int32_t convert_flag(int32_t flags)
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
  if (flags & MMAL_BUFFER_HEADER_FLAG_KEYFRAME)
    return AV_PKT_FLAG_KEY+AV_PKT_FLAG_TRUSTED;
  else
    return AV_PKT_FLAG_TRUSTED;
}

queue_frame_s *append_frame(queue_frame_s *dst, queue_frame_s *src)
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
  int old_size=dst->size;
  int new_size=dst->size + src->size;
  queue_frame_s *new_frame = NULL;
  new_frame = realloc(dst, new_size+sizeof(queue_frame_s));

  if (!new_frame)
    {
    log_error("frame could not be reallocated");
    return NULL;
    }
  new_frame;
  memcpy(&new_frame->data+old_size, &src->data, src->size);
  new_frame->size = new_size;
  return new_frame;
}

int write_packet(OUTPUT_STATE *o_state)
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
  queue_frame_s *frame = get_tail(o_state->queue);
  if (!frame) log_error("no frame in queue");

  int status; 
  AVPacket packet;
  av_init_packet(&packet); 
  packet.data = NULL;
  packet.size = 0;
  
  switch (frame->type)
    {
    case AUDIO_DATA:
      {
      packet.pts = frame->pts;
      packet.dts = AV_NOPTS_VALUE;
      packet.data = &frame->data;
      packet.size = frame->size;
      packet.stream_index = 1;
      packet.flags = frame->flag;
      packet.duration=0;
      packet.pos=-1;
      status = av_write_frame(o_state->fmtctx, &packet);
      if (status) log_error("Could not audio write frame to context (error '%s')", av_err2str(status));
      free_frame(o_state->queue);
      return status;
      break;
      }
    case VIDEO_DATA:
      {
      if(frame->flag & MMAL_BUFFER_HEADER_FLAG_CODECSIDEINFO)
	{
	log_error("skipped due to CODECSIDEINFO flag %d", frame->flag);
	free_frame(o_state->queue);
	return 0;
	}
      if(frame->flag & MMAL_BUFFER_HEADER_FLAG_CONFIG)
	{
	if (o_state->side_data_frame) 
	  {
	  if (o_state->side_data_frame)
	    {
	    free(o_state->side_data_frame);
	    o_state->side_data_frame = NULL;
	    }
	  else
	    {
	    log_error("non NULL");
	    }
	  }
	o_state->side_data_frame = frame;
	unqueue_frame(o_state->queue);
	return 0;
	}
  if (frame->flag & MMAL_BUFFER_HEADER_FLAG_FRAME_END) 
	{ //write frame
	packet.dts = AV_NOPTS_VALUE;
	packet.duration=0;
	packet.pos=-1;
	packet.stream_index=0;
	uint8_t *side_data = NULL;
	if (o_state->side_data_frame)
	  {
	  side_data = av_packet_new_side_data(&packet, AV_PKT_DATA_NEW_EXTRADATA, o_state->side_data_frame->size);
	  if (!side_data) 
	    {
	    log_error("add side data failed %s", AVERROR(ENOMEM));
            return -1;
	    }

	  memcpy(packet.side_data->data, &o_state->side_data_frame->data, o_state->side_data_frame->size);
	  } 
	if (o_state->save_partial_frame)
	  {
	  queue_frame_s *new_frame = append_frame(o_state->save_partial_frame, frame);
	  if(new_frame) 
	    {
	    o_state->save_partial_frame = new_frame;
	    }
	  else
	    {
	    log_error("append frame failed");
	    return -1;
	    }
	  packet.pts = o_state->save_partial_frame->pts;
	  packet.data = &o_state->save_partial_frame->data;
	  packet.size = o_state->save_partial_frame->size;
	  packet.flags = convert_flag(o_state->save_partial_frame->flag);
	  status=av_write_frame(o_state->fmtctx, &packet);
	  if (status)
	    {
	    log_error("video frame write error  %d %s", status, av_err2str(status));
	    return -1;	  
	    }
	  else
	    {
	    if (packet.side_data) av_packet_free_side_data(&packet);
	    if (o_state->save_partial_frame)
	      {
	      free(o_state->save_partial_frame);
	      o_state->save_partial_frame = NULL;
	      }
	    else
	      {
	      log_error("non NULL");
	      }
	    free_frame(o_state->queue);
	    return 0;
	    }
	  }
	else
	  {  
	  packet.pts = frame->pts;
	  packet.data = &frame->data;
	  packet.size = frame->size;
	  packet.flags = convert_flag(frame->flag);
	  status=av_write_frame(o_state->fmtctx, &packet);
	  if (status)
	    {
	    log_error("video frame write error  %d %s", status, av_err2str(status));
	    return -1;	  
	    }
	  else
	    {
	    if (packet.side_data) {av_packet_free_side_data(&packet);}
	    free_frame(o_state->queue);
	    return 0;
	    }
	  }
	}
      else
	{ //save frame
	if (o_state->save_partial_frame)
	  {
	  queue_frame_s *new_frame = append_frame(o_state->save_partial_frame, frame);
	  if(new_frame)
	    {
	    o_state->save_partial_frame = new_frame;
	    }
	  else
	    {
	    log_error("append frame failed");
	    return -1;
	    } 
	  if (o_state->queue)
	    {
	    free_frame(o_state->queue);
	    }
	  else
	    {
	    log_error("invalid queue");
	    }
	  return 0;
	  }
	else
	  {
	  o_state->save_partial_frame = frame;
	  unqueue_frame(o_state->queue);
	  }
	}
      break;
      }
    case END_OF_QUEUE:
      {
      if (o_state->side_data_frame) free(o_state->side_data_frame);
      o_state->side_data_frame = NULL;
      if (o_state->save_partial_frame) free(o_state->save_partial_frame);
      o_state->save_partial_frame = NULL;
      o_state->run_state = STOPPING_WRITE; 
      free_frame(o_state->queue);
      break;
      }
    default:
      {
      log_error("invalid queue type");
      return -1;
      }
  } 
}

void *write_stream(void *arg)
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
  OUTPUT_STATE *o_state;
  o_state = (OUTPUT_STATE *) arg;
  
  log_debug("here1 %s ", o_state->dest);
  if (!(strncmp(o_state->dest, "rtmp://", 7)))
    {
    int keep_waiting = 9000, not_done_waiting=1;
    while (not_done_waiting)  
      {

  //    if (system("ping -c 1 google.com > /dev/null 2>&1")) 
      if (system("ping -c 1 8.8.8.8 > /dev/null 2>&1")) 
  //    if (system("ping -c 1 10.10.10.0 > /dev/null 2>&1")) // to test failed ping
        {
        if (o_state->run_state == STOPPING_WRITE)
          {
          not_done_waiting=0;
          log_error("End of write signaled before newtork available");
          o_state->run_state = STOPPED;
          return NULL;
          }
        keep_waiting--;
        vcos_sleep(10);
        }
      else
      	{
        not_done_waiting=0;
        }
      }
    if (!keep_waiting)
      {
      log_error("Stopping URL stream as no network found!");
      o_state->run_state = STOPPED;
      while(queue_length(o_state->queue))
        {
        free_frame(o_state->queue);
        }
      }
    }
    
  log_debug("here2");
  if (allocate_fmtctx(o_state) < 0)
    {
    o_state->run_state = STOPPED;
    while(queue_length(o_state->queue))
      {
      free_frame(o_state->queue);
      }
    }

  while (o_state->run_state)
    {
    empty_wait(o_state->queue);
    write_packet(o_state);
    if (o_state->run_state == STOPPING_WRITE)
      {
      if (queue_length(o_state->queue))
        {
        log_status("stopping but queue not empty %d %s", queue_length(o_state->queue), o_state->dest);
        }
      else
        {
        o_state->run_state = STOPPED;
        }
      }
    }
  free_fmtctx(o_state);
  if (free_queue(o_state->queue)) {log_error("Free queue failed %s", o_state->dest);} 
}

//MMAL stuff
MMAL_COMPONENT_T *create_camera_component(RACECAM_STATE *state, int camera_type)
{
   log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
   MMAL_COMPONENT_T *camera = NULL;
   MMAL_ES_FORMAT_T *format;
   MMAL_PORT_T *preview_port = NULL, *video_port = NULL, *still_port = NULL;
   MMAL_STATUS_T status;

   status = mmal_component_create(MMAL_COMPONENT_DEFAULT_CAMERA, &camera);

   if (status != MMAL_SUCCESS)
   {
      log_error("Failed to create camera component");
      goto error;
   }

  status = raspicamcontrol_set_stereo_mode(camera->output[0], &state->camera_parameters.stereo_mode);
   status += raspicamcontrol_set_stereo_mode(camera->output[1], &state->camera_parameters.stereo_mode);
   status += raspicamcontrol_set_stereo_mode(camera->output[2], &state->camera_parameters.stereo_mode);


   if (status != MMAL_SUCCESS)
   {
      log_error("Could not set stereo mode : error %d", status);
      goto error;
   }

   MMAL_PARAMETER_INT32_T camera_num =
   {{MMAL_PARAMETER_CAMERA_NUM, sizeof(camera_num)}, state->common_settings[camera_type].cameraNum};

   status = mmal_port_parameter_set(camera->control, &camera_num.hdr);

   if (status != MMAL_SUCCESS)
   {
      log_error("Could not select camera : error %d", status);
      goto error;
   }

   if (!camera->output_num)
   {
      status = MMAL_ENOSYS;
      log_error("Camera doesn't have output ports");
      goto error;
   }

   status = mmal_port_parameter_set_uint32(camera->control, MMAL_PARAMETER_CAMERA_CUSTOM_SENSOR_CONFIG, state->common_settings[camera_type].sensor_mode);

   if (status != MMAL_SUCCESS)
   {
      log_error("Could not set sensor mode : error %d", status);
      goto error;
   }

   preview_port = camera->output[MMAL_CAMERA_PREVIEW_PORT];
   video_port = camera->output[MMAL_CAMERA_VIDEO_PORT];
   still_port = camera->output[MMAL_CAMERA_CAPTURE_PORT];

   // Enable the camera, and tell it its control callback function
   status = mmal_port_enable(camera->control, default_camera_control_callback);

   if (status != MMAL_SUCCESS)
   {
      log_error("Unable to enable control port : error %d", status);
      goto error;
   }

   //  set up the camera configuration
   {
      MMAL_PARAMETER_CAMERA_CONFIG_T cam_config =
      {
         { MMAL_PARAMETER_CAMERA_CONFIG, sizeof(cam_config) },
         .max_stills_w = state->common_settings[camera_type].cam.width,
         .max_stills_h = state->common_settings[camera_type].cam.height,
         .stills_yuv422 = 0,
         .one_shot_stills = 0,
         .max_preview_video_w = state->common_settings[camera_type].cam.width,
         .max_preview_video_h = state->common_settings[camera_type].cam.height,
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
   format->es->video.width = VCOS_ALIGN_UP(state->common_settings[camera_type].cam.width, 32);
   format->es->video.height = VCOS_ALIGN_UP(state->common_settings[camera_type].cam.height, 16);
   format->es->video.crop.x = 0;
   format->es->video.crop.y = 0;
   format->es->video.crop.width = state->common_settings[camera_type].cam.width;
   format->es->video.crop.height = state->common_settings[camera_type].cam.height;
   format->es->video.frame_rate.num = state->framerate;
   format->es->video.frame_rate.den = VIDEO_FRAME_RATE_DEN;
   
   status = mmal_port_format_commit(preview_port);

   if (status != MMAL_SUCCESS)
   {
      log_error("camera viewfinder format couldn't be set");
      goto error;
   }

  
   // Set the encode format on the video  port
   format = video_port->format;
   format->encoding_variant = MMAL_ENCODING_I420;
   format->encoding = MMAL_ENCODING_OPAQUE;
   format->es->video.width = VCOS_ALIGN_UP(state->common_settings[camera_type].cam.width, 32);
   format->es->video.height = VCOS_ALIGN_UP(state->common_settings[camera_type].cam.height, 16);
   format->es->video.crop.x = 0;
   format->es->video.crop.y = 0;
   format->es->video.crop.width = state->common_settings[camera_type].cam.width;
   format->es->video.crop.height = state->common_settings[camera_type].cam.height;
   format->es->video.frame_rate.num = state->framerate;
   format->es->video.frame_rate.den = VIDEO_FRAME_RATE_DEN;

   status = mmal_port_format_commit(video_port);

   if (status != MMAL_SUCCESS)
   {
      log_error("camera video format couldn't be set");
      goto error;
   }
   

   if (video_port->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM)
      video_port->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;

   // Set the encode format on the still  port
   format = still_port->format;

   format->encoding = MMAL_ENCODING_OPAQUE;
   format->encoding_variant = MMAL_ENCODING_I420;
   format->es->video.width = VCOS_ALIGN_UP(state->common_settings[camera_type].cam.width, 32);
   format->es->video.height = VCOS_ALIGN_UP(state->common_settings[camera_type].cam.height, 16);
   format->es->video.crop.x = 0;
   format->es->video.crop.y = 0;
   format->es->video.crop.width = state->common_settings[camera_type].cam.width;
   format->es->video.crop.height = state->common_settings[camera_type].cam.height;
   format->es->video.frame_rate.num = 0;
   format->es->video.frame_rate.den = 1;

   status = mmal_port_format_commit(still_port);

   if (status != MMAL_SUCCESS)
   {
      log_error("camera still format couldn't be set");
      goto error;
   }

  // Ensure there are enough buffers to avoid dropping frames 
   if (still_port->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM)
      still_port->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;


   // Enable component 
   status = mmal_component_enable(camera);
   if (status != MMAL_SUCCESS)
   {
      log_error("camera component couldn't be enabled");
      goto error;
   }

   // Note: this sets lots of parameters that were not individually addressed before.
   raspicamcontrol_set_all_parameters(camera, &state->camera_parameters, camera_type);

   return camera;
   
error:
   if (camera)
      mmal_component_destroy(camera);

   return NULL;
} 

MMAL_COMPONENT_T *create_hvs_component(RACECAM_STATE *state)
{
   log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);   
   MMAL_COMPONENT_T *hvs = NULL; 
   MMAL_STATUS_T status;
           
   status = mmal_component_create("vc.ril.hvs", &hvs);
   
   if (status != MMAL_SUCCESS)
   {
      log_error("Unable to create hvs component");
      goto error;
   }
   
   if (!hvs->input_num || !hvs->output_num)
   {
      status = MMAL_ENOSYS;
      log_error("HVS doesn't have input/output ports");
      goto error;
   }

   int i;
   for (i=0;i<4;i++)
      {
      if (state->hvs[i].enable)   
         {
         status = mmal_port_parameter_set(hvs->input[i], &state->hvs[i].param.hdr);
         if (status != MMAL_SUCCESS)
            {
            log_error("Unable to set displayregion hvs input port:%d %d", i, status);
            goto error;
            }
         // Commit the port changes to the hvs input port'
         mmal_format_copy(hvs->input[i]->format, state->hvs[i].format); 
         status = mmal_port_format_commit(hvs->input[i]);
         if (status != MMAL_SUCCESS)
            {
            log_error("Unable to set format on hvs ovl input port");
            goto error;
            }
         }
      }
    
   // Commit the port changes to the hvs output port
   hvs->output[0]->format->encoding = MMAL_ENCODING_RGB24;
   hvs->output[0]->format->es->video.width = VCOS_ALIGN_UP(state->common_settings[0].cam.width, 32);
   hvs->output[0]->format->es->video.height = VCOS_ALIGN_UP(state->common_settings[0].cam.height, 16);
   hvs->output[0]->format->es->video.crop.width = state->common_settings[0].cam.width;
   hvs->output[0]->format->es->video.crop.height = state->common_settings[0].cam.height;
   hvs->output[0]->format->es->video.frame_rate.num = state->framerate;
   hvs->output[0]->format->es->video.frame_rate.den = VIDEO_FRAME_RATE_DEN;
 
   status = mmal_port_format_commit(hvs->output[0]);

   if (status != MMAL_SUCCESS)
   {
      log_error("Unable to set format on hvs output port");
      goto error;
   }
   
   //  Enable component
   status = mmal_component_enable(hvs);

   if (status != MMAL_SUCCESS)
   {
      log_error("Unable to enable hvs component");
      goto error;
   }

   return hvs;

error:
   if (hvs)
      mmal_component_destroy(hvs);

   return NULL;
}

MMAL_COMPONENT_T *create_encoder_component(RACECAM_STATE *state, MMAL_ES_FORMAT_T *format)
{
   log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
   MMAL_COMPONENT_T *encoder = 0;
   MMAL_PORT_T *encoder_input = NULL, *encoder_output = NULL;
   MMAL_STATUS_T status;
   MMAL_POOL_T *pool;
   
   status = mmal_component_create(MMAL_COMPONENT_DEFAULT_VIDEO_ENCODER, &encoder);

   if (status != MMAL_SUCCESS)
   {
      log_error("Unable to create video encoder component");
      goto error;
   }

   if (!encoder->input_num || !encoder->output_num)
   {
      status = MMAL_ENOSYS;
      log_error("Video encoder doesn't have input/output ports");
      goto error;
   }

   encoder_input = encoder->input[0];
   encoder_output = encoder->output[0];

   // We want same format on input and output ??
   mmal_format_copy(encoder_output->format, encoder_input->format);
     
   mmal_format_copy(encoder_input->format, format);
      
   status = mmal_port_format_commit(encoder_input);

   if (status != MMAL_SUCCESS)
   {
      log_error("Unable to set format on video encoder input port");
      goto error;
   }

   encoder_output->format->encoding = state->encoding;
   encoder_output->format->bitrate = state->bitrate;
   
   encoder_output->buffer_size = encoder_output->buffer_size_recommended;
   if (encoder_output->buffer_size < encoder_output->buffer_size_min)
      encoder_output->buffer_size = encoder_output->buffer_size_min;
      
   encoder_output->buffer_num = encoder_output->buffer_num_recommended;
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
      log_error("Unable to set format on video encoder output port");
      goto error;
   }

   MMAL_PARAMETER_VIDEO_RATECONTROL_T paramrc = {{ MMAL_PARAMETER_RATECONTROL, sizeof(paramrc)}, MMAL_VIDEO_RATECONTROL_DEFAULT};
   status = mmal_port_parameter_set(encoder_output, &paramrc.hdr);
   if (status != MMAL_SUCCESS)
   {
      log_error("Unable to set ratecontrol");
      goto error;
   }

   MMAL_PARAMETER_UINT32_T param = {{ MMAL_PARAMETER_INTRAPERIOD, sizeof(param)}, state->intraperiod};
   status = mmal_port_parameter_set(encoder_output, &param.hdr);
   if (status != MMAL_SUCCESS)
   {
      log_error("Unable to set intraperiod");
      goto error;
   }

   MMAL_PARAMETER_UINT32_T param1 = {{ MMAL_PARAMETER_VIDEO_ENCODE_INITIAL_QUANT, sizeof(param1)}, state->quantisationParameter};
   status = mmal_port_parameter_set(encoder_output, &param1.hdr);
   if (status != MMAL_SUCCESS)
   {
      log_error("Unable to set initial QP");
      goto error;
   }

   MMAL_PARAMETER_UINT32_T param2 = {{ MMAL_PARAMETER_VIDEO_ENCODE_MIN_QUANT, sizeof(param2)}, state->quantisationMin};
   status = mmal_port_parameter_set(encoder_output, &param2.hdr);
   if (status != MMAL_SUCCESS)
   {
      log_error("Unable to set min QP");
      goto error;
   }

   MMAL_PARAMETER_UINT32_T param3 = {{ MMAL_PARAMETER_VIDEO_ENCODE_MAX_QUANT, sizeof(param3)}, 39};
   status = mmal_port_parameter_set(encoder_output, &param3.hdr);
   if (status != MMAL_SUCCESS)
   {
      log_error("Unable to set max QP");
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
      log_error("Unable to set H264 profile");
      goto error;
   }

   if (mmal_port_parameter_set_boolean(encoder_input, MMAL_PARAMETER_VIDEO_IMMUTABLE_INPUT, state->immutableInput) != MMAL_SUCCESS)
   {
      log_error("Unable to set immutable input flag");
   }


   //set INLINE HEADER flag to generate SPS and PPS for every IDR if requested
   if (mmal_port_parameter_set_boolean(encoder_output, MMAL_PARAMETER_VIDEO_ENCODE_INLINE_HEADER, state->bInlineHeaders) != MMAL_SUCCESS)
   {
      log_error("failed to set INLINE HEADER FLAG parameters");
   }

      //set flag for add SPS TIMING
   if (mmal_port_parameter_set_boolean(encoder_output, MMAL_PARAMETER_VIDEO_ENCODE_SPS_TIMING, MMAL_FALSE) != MMAL_SUCCESS)
   {
      log_error("failed to set SPS TIMINGS FLAG parameters");
   }
 
   //  Enable component
   status = mmal_component_enable(encoder);

   if (status != MMAL_SUCCESS)
   {
      log_error("Unable to enable video encoder component");
      goto error;
   }

   return encoder;

error:
   if (encoder)
      mmal_component_destroy(encoder);

   return NULL;
}


MMAL_COMPONENT_T *create_preview_component(RACECAM_STATE *state)
{
   log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
   MMAL_COMPONENT_T *preview = 0;
   MMAL_PORT_T *preview_port = NULL;
   MMAL_STATUS_T status;

 
   status = mmal_component_create(MMAL_COMPONENT_DEFAULT_VIDEO_RENDERER,
                                     &preview);

   if (status != MMAL_SUCCESS)
      {
      log_error("Unable to create preview component");
      goto error;
      }

   if (!preview->input_num)
      {
      status = MMAL_ENOSYS;
      log_error("No input ports found on component");
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
   param.display_num = 0;   // 0 for DSI, 2 for HDMI0, 7 for HDMI1

   status = mmal_port_parameter_set(preview_port, &param.hdr);

   if (status != MMAL_SUCCESS && status != MMAL_ENOSYS)
      {
      log_error("unable to set preview port parameters (%u)", status);
      goto error;
      }


   /* Enable component */
   status = mmal_component_enable(preview);

   if (status != MMAL_SUCCESS)
   {
      log_error("Unable to enable preview/null sink component (%u)", status);
      goto error;
   }

   return preview;

error:

   if (preview)
      mmal_component_destroy(preview);

   return NULL;
}

MMAL_STATUS_T connect_ports(MMAL_PORT_T *output_port, MMAL_PORT_T *input_port, MMAL_CONNECTION_T **connection)
{
   log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
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

MMAL_COMPONENT_T *create_splitter_component(MMAL_COMPONENT_T **splitter_pptr, MMAL_ES_FORMAT_T *format)
{
   log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
   MMAL_PORT_T *splitter_output = NULL;
   MMAL_STATUS_T status;
   int i;

//   move to format copy of pasted format
   if (format == NULL)
      {
      status = MMAL_ENOSYS;
      log_error("Format not passed to create splitter component\n");
      goto error;
      } 

   /* Create the component */
   status = mmal_component_create(MMAL_COMPONENT_DEFAULT_VIDEO_SPLITTER, splitter_pptr);
   MMAL_COMPONENT_T *splitter=*splitter_pptr;

   if (status != MMAL_SUCCESS)
      {
      log_error("Failed to create splitter component\n");
      goto error;
      }

   if (!splitter->input_num)
      {
      status = MMAL_ENOSYS;
      log_error("Splitter doesn't have any input port\n");
      goto error;
      }

   if (splitter->output_num < 2)
      {
      status = MMAL_ENOSYS;
      log_error("Splitter doesn't have enough output ports\n");
      goto error;
      }

   /* Ensure there are enough buffers to avoid dropping frames: */
   mmal_format_copy(splitter->input[0]->format, format);

   if (splitter->input[0]->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM)
      splitter->input[0]->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;

   status = mmal_port_format_commit(splitter->input[0]);

   if (status != MMAL_SUCCESS)
      {
      log_error("Unable to set format on splitter input port\n");
      goto error;
      }

   /* Splitter can do format conversions, configure format for its output port: */
   for (i = 0; i < splitter->output_num; i++)
      {
      mmal_format_copy(splitter->output[i]->format, splitter->input[0]->format);
      status = mmal_port_format_commit(splitter->output[i]);
      if (status != MMAL_SUCCESS)
         {
         log_error("Unable to set format on splitter output port %d\n", i);
         goto error;
         }
      }

   /* Enable component */
   status = mmal_component_enable(splitter);

   if (status != MMAL_SUCCESS)
      {
      log_error("splitter component couldn't be enabled\n");
      goto error;
      }

   return splitter;

error:

   if (splitter)
      mmal_component_destroy(splitter);

   return NULL;
}


void check_disable_port(MMAL_PORT_T *port)
{
   log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
   if (port && port->is_enabled)
      mmal_port_disable(port);
}

void get_sensor_defaults(int camera_num, char *camera_name, int *width, int *height )
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
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
        log_error("Cannot read camera info, keeping the defaults for OV5647");
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
    log_error("Failed to create camera_info component");
    }

   // default to OV5647 if nothing detected..
  if (*width == 0)
    *width = 2592;
  if (*height == 0)
    *height = 1944; 
}

void encoder_buffer_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
  MMAL_BUFFER_HEADER_T *new_buffer;
  int bytes_written;
  int64_t calc_pts;
	
  PORT_USERDATA *pData = (PORT_USERDATA *)port->userdata;
  QUEUE_STATE *queue = pData->queue;
  
  if (buffer->pts && (!pData->s_time))
    {
    pData->s_time = buffer->pts-1000;
    }
  if (buffer->pts) 
    calc_pts = (buffer->pts-pData->s_time)/1000;
  else
    calc_pts = 0;

  mmal_buffer_header_mem_lock(buffer);
  if (queue_frame(queue, buffer->data+buffer->offset, buffer->length, VIDEO_DATA, calc_pts, buffer->flags))
    {
    log_error("Queue video data failed!\n");
    }
  else
    { 
    bytes_written = buffer->length;
    }

  mmal_buffer_header_mem_unlock(buffer);
  if (bytes_written != buffer->length)
    {
    log_error("Failed to write buffer data (%d from %d)- aborting", bytes_written, buffer->length);
    }

  mmal_buffer_header_release(buffer);
  if (port->is_enabled)
    {
    MMAL_STATUS_T status;
    new_buffer = mmal_queue_get(pData->pool->queue);
    if (new_buffer) status = mmal_port_send_buffer(port, new_buffer);
    if (!new_buffer || status != MMAL_SUCCESS) log_error("Unable to return a buffer to the encoder port");
    }
}

void destroy_component(MMAL_COMPONENT_T **comp_ptr)
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
  MMAL_COMPONENT_T *comp=*comp_ptr;
  MMAL_STATUS_T status;
  if (comp)
    {
    status=mmal_component_destroy(comp);
    if (status == MMAL_SUCCESS)
      comp = NULL;
    else
      log_error("component not destroyed :%s", comp->name);
    }
} 

void default_status(RACECAM_STATE *state)
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
  if (!state)
    {
    vcos_assert(0);
    return;
    }

   // Default everything to zero
  memset(state, 0, sizeof(RACECAM_STATE));
   
  strncpy(state->common_settings[0].camera_name, "(Unknown)", MMAL_PARAMETER_CAMERA_INFO_MAX_STR_LEN);
  strncpy(state->common_settings[1].camera_name, "(Unknown)", MMAL_PARAMETER_CAMERA_INFO_MAX_STR_LEN);

   // Now set anything non-zero
	state->common_settings[0].cameraNum = 0;
	state->common_settings[1].cameraNum = 1;
  state->common_settings[0].sensor_mode = 5;
  state->common_settings[1].sensor_mode = 5;
  state->timeout = -1; 
  state->common_settings[0].cam.width = 1920;    
  state->common_settings[0].cam.height = 1080; 
  state->common_settings[1].cam.width = 480;    
  state->common_settings[1].cam.height = 270;    
  state->encoding = MMAL_ENCODING_H264;
  state->bitrate = 0; // 0 for variable bit rate
  state->intraperiod = 15;    // Not set
  state->quantisationParameter = 30;
  state->quantisationMin = 20;
  state->immutableInput = 1;
  state->profile = MMAL_VIDEO_PROFILE_H264_HIGH;
  state->level = MMAL_VIDEO_LEVEL_H264_41;
  state->bInlineHeaders = 0;
  state->framerate=30;

  // Set up the camera_parameters to default
  state->camera_parameters.sharpness = 0;
  state->camera_parameters.contrast = 0;
  state->camera_parameters.brightness = 50;
  state->camera_parameters.saturation = 25;
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
  state->camera_parameters.hflip[0] = state->camera_parameters.vflip[0] = 0;
  state->camera_parameters.hflip[1] = state->camera_parameters.vflip[1] = 0;
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


