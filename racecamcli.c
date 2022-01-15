/*
 *  racecamcli.c command line vesion of racecam.c
 * This program creates a 2 camera components and tunnels them to the
 * Hardware Video Scaler (HVS) component to overlay the second camera 
 * frame on the frame of the main camera. The HVS output is tunneled to 
 * the encoder to create a video H.264 stream.  The video stream is 
 * created using MMAL api and run on the GPU.  The audio stream is 
 * created from ALSA api using Adafruit I2S MEMS Microphone 
 * (SPH0645LM4H).  The audio stream is encoded to ACC using FFPMEG aps 
 * and both streams are added to flash video container by FFMPEG api.
 *  
 */
 
#include <getopt.h> 

#include "racecamCommon.h"

#include <bcm2835.h>

#include "GPSUtil.h"

enum {STOP, START};

#define GPIO_MODEM_LED	RPI_BPLUS_GPIO_J8_18 
#define GPIO_LED	RPI_BPLUS_GPIO_J8_13 
#define GPIO_SWT	RPI_BPLUS_GPIO_J8_15
#define GPIO_PWR_LED	RPI_BPLUS_GPIO_J8_16

RACECAM_STATE *pstate;

int timelimit = 0;
int gps_enabled = 0;  
char file[64];
char parm_file[64] = "/home/pi/racecamcli";
char url[64];

static void usage(char *command)
{
	log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
	printf(
("Usage: %s [OPTION]"
"\nProgram paramaters:\n"
"-?, --help                 these messages\n"
"-d, --duration=#           interrupt after # seconds - defaults to 0 use GPIO switch\n"
"-g, --gps                  enable gps speed overlay - defaults to disables\n"
"\nAudio paramaters:\n"
"-D, --device=NAME          select PCM by name\n"  
"-n, --number=#             number of audio channels defaults to 2\n"
"\nCamera paramaters:\n"
"-c, --camera=#             main camera # - defaults to 0\n"
"-r, --resolution=#         0=1920x1080 1=1280x720 2=854x480 - defaults to 0\n"
"-s, --overlaysize%=#       overlay size % of main size (99% to 1%) defaults to 25%\n"
"-l, --location=#           overlay location 0=BottomLeft, 1=TopLeft, 2=TopCenter, 3=TopRight, 4=BottomRight,\n" 
"                           5=BottomCenter - Defaults to BottomLeft\n"
"-f, --flip=h.v:h.v         flip image of camera - example 1.1:0.1 flips the main camera vertically &\n" 
"                           horizontally and 2nd camera vertically\n"
"-F, --fps=#                frames per second #\n"
"-p, --preview              preview mode 0=none, 1=main, 2=overlay 3=composite Defaults to 3\n"
"\nEncoder paramaters:\n"
"-q, --quantisation=#:#     quantisation Init:Min parameters\n"
"-i, --intraframe=#         intra key frame rate # frame\n" 
"\nOutput stream location - atleast one must be selected - both can be used to save locally and remotely\n"
"                           if neither is supplied then defaults to file of the name /home/pi/rqacecamcli\n"
"-o, --outputfile=PATH/FILE local output file ie /home/pi/racecamcli - the name will be appended with\n"
"                           timespace and file extention (.flv)\n" 
"-u,  --url=URL/KEY         url output location and key\n" )   
		, command);
} 

static void parse_params(int argc, char *argv[], RACECAM_STATE *state)
{
	log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
	int option_index, resolution = 0, overlay_percent = 33, overlay_location = 0;
	char *command;
	static const char short_options[] = "?D:d:r:c:s:q:i:n:l:f:F:o:u:gp:";
	static const struct option long_options[] = {
		{"help", 0, 0, '?'},
		{"device", 1, 0, 'D'},
		{"duration", 1, 0,'d'},
		{"resolution", 1, 0, 'r'},
		{"camera", 1, 0, 'c'},
		{"overlaysize%", 1, 0, 's'},
		{"quantisation", 1, 0, 'q'},
		{"intraframe", 1, 0, 'i'},
		{"number", 1, 0, 'n'},
		{"location", 1, 0, 'l'},
		{"flip", 1, 0, 'f'},
		{"fps", 1, 0, 'F'},
		{"outputfile", 1, 0, 'o'},
		{"url", 1, 0, 'u'},
		{"gps", 0, 0, 'g'},
		{"preview", 1, 0, 'p'},
		{0, 0, 0, 0}
		};

	int c, i, num_parms = 0, cam_parm = 0, badparm=0;

	command = argv[0];
	
	while ((c = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
	switch (c) {
		case '?':
			usage(command);
			exit(EINVAL);
		case 'D':
			strncpy(state->adev, optarg, 17);
			break;
		case 'd':
			timelimit = strtol(optarg, NULL, 0);
			if (timelimit < 0)
				{
				log_error("Duration %d invalid\n", timelimit);
				badparm=1;
				}
			break;
		case 'r':
			resolution = strtol(optarg, NULL, 0);
			if (resolution < 0 || resolution > 2)
				{
 				log_error("Resolution %d invalid\n", resolution);
				badparm=1;
				}
			break;		
		case 's':
			overlay_percent = strtol(optarg, NULL, 0);
			if (overlay_percent > 99 || overlay_percent < 1)
				{
				log_error("Overlay size %% must be between 1 and 99 you selected %d\%\n", overlay_percent);
				badparm=1;
				break;
				}
			break;
		case 'l':
			overlay_location = strtol(optarg, NULL, 0);
			if (overlay_location > 5 || overlay_location < 0)
				{
				log_error("Overlay location must be between 0 and 5 you selected %d\n", overlay_location);
				badparm=1;
				}
			break;
		case 'f':
			num_parms = sscanf(optarg, "%d.%d:%d.%d", state->camera_parameters.hflip[0], state->camera_parameters.vflip[0], 
			state->camera_parameters.hflip[1], state->camera_parameters.vflip[1]);
            switch(num_parms) {
				case EOF:
					log_error("filp sscanf failed\n");
					badparm=1;
					break;
				case 4:
					if (state->camera_parameters.hflip[0] < 0 || state->camera_parameters.hflip[0] > 1 || 
						state->camera_parameters.vflip[0] < 0 || state->camera_parameters.vflip[0] > 1 ||
						state->camera_parameters.hflip[1] < 0 || state->camera_parameters.hflip[1] > 1 || 
						state->camera_parameters.vflip[1] < 0 || state->camera_parameters.vflip[1] > 1) 
						{
						log_error("flip flag must be 1 or 0 (true or false) you selected %d.%d:%d.%d\n", state->camera_parameters.hflip[0],
						state->camera_parameters.vflip[0], state->camera_parameters.hflip[1], state->camera_parameters.vflip[1]); 
						badparm=1;}
					break;
				default:
					log_error("flip does not have 4 params\n");
					badparm=1;
					break;}
				break;
		case 'c':
			cam_parm = strtol(optarg, NULL, 0);
			if (cam_parm > 1 || cam_parm < 0) {
				log_error("Camera must be 0 or 1 you selected %d\n", cam_parm);
				badparm=1;}
			else
				{
				state->common_settings[0].cameraNum = state->common_settings[1].cameraNum = 1;
				state->common_settings[cam_parm].cameraNum = 0; 
				}
			break;
		case 'q':
			num_parms = sscanf(optarg, "%d:%d", &state->quantisationParameter, &state->quantisationMin); 
            switch(num_parms) {
				case EOF:
					log_error("quantisation sscanf failed\n");
					badparm=1;
					break;
				case 2:
					if (state->quantisationMin > 39 || state->quantisationMin < 10) {
						log_error("Min quantisation parameter must be 20 to 40 you selected %d\n", state->quantisationMin);
						badparm=1;}
					if (state->quantisationParameter > 39 || state->quantisationParameter < state->quantisationMin) {
						log_error("Init quantisation parameters must be between Min and Max you selected %d\n", state->quantisationParameter);
						badparm=1;}
				break;
				default:
					log_error("Quantisation does not have 3 params\n");
					badparm=1;
					break;}
			break;
		case 'n':
			state->achannels = strtol(optarg, NULL, 0);
			if (state->achannels > 2 || state->achannels < 1) {
				log_error("number of audio channels must be 1 or 2  you selected %d\n", state->achannels);
				badparm=1;}
			break; 
		case 'i':
			state->intraperiod = strtol(optarg, NULL, 0);
			if (state->intraperiod > 60) {
				log_error("Intra frame rate should be < 61  you selected %d\n", state->intraperiod);
				badparm=1;}
			break;
		case 'F':
			state->framerate = strtol(optarg, NULL, 0);
			if (state->framerate < 22 || state->framerate > 30) {
				log_error("frames per second must be > 21 and < 31 you selected %d\n", state->framerate);
				badparm=1;}
			break;
		case 'g':
			gps_enabled = 1;
			break;
		case 'o':
			strcpy(parm_file, optarg);
//			state->selected[FILE_STRM] = 1;
			state->output_state[FILE_STRM].run_state = RECORDING;
			break;
		case 'u':
			strcpy(url, "rtmp://");
			strcpy(url+7, optarg);
//			state->selected[URL_STRM]=1;
			state->output_state[URL_STRM].run_state = RECORDING;
			break;
		case 'p':
			state->preview_mode = strtol(optarg, NULL, 0);
			if (state->preview_mode  < 0 || state->preview_mode  > 3) {
				log_error("preview must be 0, 1, 2, or 3 you selected %d\n", state->preview_mode);
				badparm=1;}
			break;
		default:
			log_error("Try `%s --help' for more information.\n", command);
			exit(EINVAL);
		}
	}

	if (badparm) {exit(EINVAL);}
	
//	if (!(state->selected[FILE_STRM] || state->selected[URL_STRM]))
	if (!(state->output_state[FILE_STRM].run_state || state->output_state[URL_STRM].run_state))
		{
		log_status("Defaulting to file /home/pi/racecamcli");
//		state->selected[FILE_STRM] = 1;
		state->output_state[FILE_STRM].run_state = RECORDING;
		}
//	if (state->selected[FILE_STRM])
	if (state->output_state[FILE_STRM].run_state)
		{
		int length = 0;	
		time_t time_uf;
		struct tm *time_fmt;
		time(&time_uf);
		time_fmt = localtime(&time_uf);
		strcpy(file, "file:");
		length=strlen(file);
		strcpy(file+length, parm_file);
		length=strlen(file);
		strftime(file+length, 20,"%Y-%m-%d_%H_%M_%S", time_fmt);
		length=strlen(file);
		strcpy(file+length, ".flv");
		}
		
	switch (resolution)    // 2: 854x480 1: 1280x720 0: 1920x1080
		{
		case 0:
			state->common_settings[0].cam.width = 1920;
			state->common_settings[0].cam.height = 1080;
			break;
		case 1:
			state->common_settings[0].cam.width = 1280;
			state->common_settings[0].cam.height = 720;
			break;
		default:
			state->common_settings[0].cam.width = 854;
			state->common_settings[0].cam.height = 480;
			break;
		}
			
	state->common_settings[1].cam.width = state->common_settings[0].cam.width * overlay_percent / 100;
	state->common_settings[1].cam.height = state->common_settings[0].cam.height * overlay_percent / 100;

	switch (overlay_location)   // 0=BL 1=TL 2=TC 3=TR 4=BR 5=BC 
		{
		case 0:
			state->common_settings[1].cam.x=0;  
			state->common_settings[1].cam.y=state->common_settings[0].cam.height-state->common_settings[1].cam.height;
			break;
		case 1:
			state->common_settings[1].cam.x=0;  
			state->common_settings[1].cam.y=0;
			break;
		case 2:
			state->common_settings[1].cam.x=(state->common_settings[0].cam.width/2)-(state->common_settings[1].cam.width/2);  
			state->common_settings[1].cam.y=0;
			break;
		case 3:
			state->common_settings[1].cam.x=state->common_settings[0].cam.width-state->common_settings[1].cam.width;  
			state->common_settings[1].cam.y=0;
			break;
		case 4:
			state->common_settings[1].cam.x=state->common_settings[0].cam.width-state->common_settings[1].cam.width;  
			state->common_settings[1].cam.y=state->common_settings[0].cam.height-state->common_settings[1].cam.height;
			break;
		default:
			state->common_settings[1].cam.x=(state->common_settings[0].cam.width/2)-(state->common_settings[1].cam.width/2);  
			state->common_settings[1].cam.y=state->common_settings[0].cam.height-state->common_settings[1].cam.height;
			break;
		}
	
	for (i=0;i<4;i++)
		{
		state->hvs[i].enable = MMAL_TRUE;
		state->hvs[i].param.hdr.id = MMAL_PARAMETER_DISPLAYREGION;
		state->hvs[i].param.hdr.size = sizeof(state->hvs[i].param);
		state->hvs[i].param.layer = i;
		}

	state->hvs[0].param.set = MMAL_DISPLAY_SET_DEST_RECT | MMAL_DISPLAY_SET_LAYER | MMAL_DISPLAY_SET_ALPHA;
	state->hvs[0].param.dest_rect.width = state->common_settings[MAIN_CAMERA].cam.width;
 	state->hvs[0].param.dest_rect.height = state->common_settings[MAIN_CAMERA].cam.height;
 	state->hvs[0].param.alpha = 255;
 
 	state->hvs[1].param.set =  MMAL_DISPLAY_SET_FULLSCREEN | MMAL_DISPLAY_SET_DEST_RECT | MMAL_DISPLAY_SET_LAYER | MMAL_DISPLAY_SET_ALPHA;
  	state->hvs[1].param.fullscreen = MMAL_FALSE;
  	state->hvs[1].param.dest_rect.x = state->common_settings[OVERLAY_CAMERA].cam.x;  
  	state->hvs[1].param.dest_rect.y = state->common_settings[OVERLAY_CAMERA].cam.y;
  	state->hvs[1].param.dest_rect.width = state->common_settings[OVERLAY_CAMERA].cam.width;
  	state->hvs[1].param.dest_rect.height = state->common_settings[OVERLAY_CAMERA].cam.height;
  	state->hvs[1].param.alpha = 255 | MMAL_DISPLAY_ALPHA_FLAGS_DISCARD_LOWER_LAYERS;

  	state->hvs[2].param.set =  MMAL_DISPLAY_SET_FULLSCREEN | MMAL_DISPLAY_SET_DEST_RECT | MMAL_DISPLAY_SET_LAYER | MMAL_DISPLAY_SET_ALPHA;
    state->hvs[2].param.fullscreen = MMAL_FALSE;
	state->hvs[2].param.dest_rect.x = 0;
	state->hvs[2].param.dest_rect.y = 0;
	state->hvs[2].param.dest_rect.width = state->common_settings[MAIN_CAMERA].cam.width;
	state->hvs[2].param.dest_rect.height = state->common_settings[MAIN_CAMERA].cam.height;
	state->hvs[2].param.alpha = 255;
 
	state->hvs[3].enable = MMAL_FALSE;
}

static void signal_handler(int sig)
{
	log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
	static int64_t kill_time = -1;
	
	if (!pstate->current_mode) 
		{
		log_error("not recording Exiting killed!!\n");
		exit(EXIT_FAILURE);	
		}
	
	if (kill_time == -1) 
		{
		kill_time = get_microseconds64() + 500;
		pstate->current_mode = CANCELLED;  // -1 is stop killed
		return;
		}
	else
		if (get_microseconds64() < kill_time)
			{
			return;
			}
		
	log_error("wait time for killing has passed Exiting killed!!\n");
	exit(EXIT_FAILURE);	
}

int main(int argc, char *argv[])
{
// set message levels as needed 	
//	logger_set_log_level(LOG_MAX_LEVEL_ERROR_WARNING_STATUS_DEBUG);	
	logger_set_log_level(LOG_MAX_LEVEL_ERROR_WARNING_STATUS);	
	logger_set_out_stdout();
// AV_LOG_ QUIET, PANIC, FATAL, ERROR, WARNING, INFO, VERBOSE, DEBUG and TRACE
	av_log_set_level(AV_LOG_ERROR);

	log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
	
	RACECAM_STATE state;
	int url_selected = 0, file_selected = 0;

	default_status(&state);
	pstate = state.output_state[FILE_STRM].r_state = state.output_state[URL_STRM].r_state = &state;

	//non state params 
	char alsa_dev[17] = "dmic_sv";
	
	// set state defauts not  set in default_status()
	state.adev = alsa_dev;  //??  how to get from parse_parms to alsa_state???
	state.preview_mode = 3;

	parse_params(argc, argv, &state);

//	if (state.selected[FILE_STRM])
	if (state.output_state[FILE_STRM].run_state)
		{
		// setup states
		log_debug("setup file stream");
		state.output_state[FILE_STRM].dest = file;
		state.output_state[FILE_STRM].run_state = WRITING;
		file_selected = 1;
		state.userdata[FILE_STRM].queue = state.output_state[FILE_STRM].queue = alloc_queue();
		}
		
//	if (state.selected[URL_STRM])
	if (state.output_state[URL_STRM].run_state)
		{
		// setup states
		log_debug("setup URL stream");
		state.output_state[URL_STRM].dest = url;
		state.output_state[URL_STRM].run_state = WRITING;
		url_selected = 1;
		state.userdata[URL_STRM].queue = state.output_state[URL_STRM].queue = alloc_queue();
		}

	int gpio_init = 0;
	if (bcm2835_init()) 
		{
		gpio_init = 1; 
		bcm2835_gpio_fsel(GPIO_SWT, BCM2835_GPIO_FSEL_INPT);
		bcm2835_gpio_set_pud(GPIO_SWT, BCM2835_GPIO_PUD_UP);
		bcm2835_gpio_fsel(GPIO_LED, BCM2835_GPIO_FSEL_OUTP);
		bcm2835_gpio_fsel(GPIO_MODEM_LED, BCM2835_GPIO_FSEL_OUTP);
		bcm2835_gpio_fsel(GPIO_PWR_LED, BCM2835_GPIO_FSEL_OUTP);
		bcm2835_gpio_write(GPIO_PWR_LED, HIGH);
		}
	else 
		{
		log_error("bcm2835 init failed\n");
		} 
		
	int64_t stop_pts = timelimit*1000;
		
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGABRT, signal_handler); 
	
//	if not time limited wait for switch to start
	if (!timelimit)
		{
		log_status("No duration specified -- using switch for start/stop");
		while (bcm2835_gpio_lev(GPIO_SWT))
			{
			vcos_sleep(1000);
			}
		} 

	if (allocate_audio_encode(&state))
		{
		goto err_aencode;
		}
 
	if (allocate_alsa(&state)) 
		{ 
		goto err_alsa;
		}

	if (create_video_stream(&state)) 
		{
		goto err_vstream;
		}

	GPS_T gps_data;
//	gps_data.active = &state.current_mode;
	pthread_t gps_tid;	
	if (gps_enabled) 
		{
		gps_data.active = SENDING;
		gps_data.text_size = state.common_settings[MAIN_CAMERA].cam.height/20;
		gps_data.text.width = state.common_settings[MAIN_CAMERA].cam.width;
		gps_data.text.height =  state.common_settings[MAIN_CAMERA].cam.height;
		gps_data.text.x = 2000;
		gps_data.text.y = 2000;
		gps_data.t_queue = state.hvs_textin_pool->queue;
		gps_data.t_port = state.hvs_component->input[2];
		pthread_create(&gps_tid, NULL, gps_thread, (void *)&gps_data);
		}  

	pthread_t file_tid;
// 	if (state.selected[FILE_STRM])
 	if (state.output_state[FILE_STRM].run_state)
		{
		pthread_create(&file_tid, NULL, write_stream, (void *)&state.output_state[FILE_STRM]);
		}  
		
	pthread_t url_tid, adjq_tid;
// 	if (state.selected[URL_STRM])
 	if (state.output_state[URL_STRM].run_state)
		{
		pthread_create(&url_tid, NULL, write_stream, (void *)&state.output_state[URL_STRM]);
		state.adjust_q_state.queue = state.userdata[URL_STRM].queue;
	//	state.adjust_q_state.running = ADJUSTING;
		state.adjust_q_state.running = &state.output_state[URL_STRM].run_state;
		state.adjust_q_state.port = state.encoder_component[URL_STRM]->output[0];
		state.adjust_q_state.min_q = state.quantisationMin;
		pthread_create(&adjq_tid, NULL, adjust_q, (void *)&state.adjust_q_state);
		}  


	int64_t start_time = get_microseconds64()/1000;
	
	state.current_mode=RECORDING;

    snd_pcm_drop(state.pcmhnd);
    snd_pcm_prepare(state.pcmhnd);
    snd_pcm_start(state.pcmhnd);
    state.sample_cnt = 0;
	mmal_port_parameter_set_boolean(state.camera_component[MAIN_CAMERA]->output[MMAL_CAMERA_VIDEO_PORT], MMAL_PARAMETER_CAPTURE, START);
	mmal_port_parameter_set_boolean(state.camera_component[OVERLAY_CAMERA]->output[MMAL_CAMERA_VIDEO_PORT], MMAL_PARAMETER_CAPTURE, START);
	
	while (state.current_mode > 0 ) 
		{
		//should flash LEDs?
		if (gpio_init)
			{
			bcm2835_gpio_write(GPIO_LED, HIGH);
			bcm2835_gpio_write(GPIO_MODEM_LED, HIGH);	
			}

		read_pcm(&state);
			
		if (timelimit)
			{
			if (stop_pts < ((get_microseconds64()/1000) - start_time))
				{
				state.current_mode = STOPPED;
				}
			}
		else
			{
			// 0 closed and 1 open 
			if (bcm2835_gpio_lev(GPIO_SWT)) state.current_mode = STOPPED;
			}
		}

	mmal_port_parameter_set_boolean(state.camera_component[MAIN_CAMERA]->output[MMAL_CAMERA_VIDEO_PORT], MMAL_PARAMETER_CAPTURE, STOP);
	mmal_port_parameter_set_boolean(state.camera_component[OVERLAY_CAMERA]->output[MMAL_CAMERA_VIDEO_PORT], MMAL_PARAMETER_CAPTURE, STOP);

	encode_queue_audio(&state, 1); // flush audio encoder
	
	if (gps_enabled) 
		{
		gps_data.active=DONE;
		pthread_join(gps_tid, NULL);
		} 

/*	if (state.userdata[URL_STRM].queue)
		{
		state.adjust_q_state.running = STOPPED;
		} */

err_vstream:
	destroy_video_stream(&state);
	
	if (state.output_state[FILE_STRM].queue)
		{
		if (queue_end(state.output_state[FILE_STRM].queue)) {log_error("End queue file stream failed");}
		if (state.output_state[FILE_STRM].run_state == WRITING) state.output_state[FILE_STRM].run_state = STOPPING_WRITE;
		}

	if (state.output_state[URL_STRM].queue)
		{
		if (queue_end(state.output_state[URL_STRM].queue)) {log_error("End queue url stream failed");}
		if (state.output_state[URL_STRM].run_state == WRITING) state.output_state[URL_STRM].run_state = STOPPING_WRITE;
		}
		
err_alsa:
	free_alsa(&state);
	
err_aencode:
	free_audio_encode(&state);

//	log_debug("here");
//	if (state.selected[FILE_STRM]) 
//	if (state.output_state[FILE_STRM].run_state) 
	if (file_selected) 
		{
		pthread_join(file_tid, NULL);
		}  
		
//	log_debug("here2");
//	if (state.selected[URL_STRM]) 
//	if (state.output_state[URL_STRM].run_state) 
	if (url_selected) 
		{
		pthread_join(url_tid, NULL);
		pthread_join(adjq_tid, NULL);
		}  	
//	log_debug("here3");
	if (gpio_init) {
		bcm2835_gpio_write(GPIO_LED, LOW);
		bcm2835_gpio_write(GPIO_MODEM_LED, LOW);
		bcm2835_gpio_write(GPIO_PWR_LED, LOW);
		bcm2835_close();} 
//	log_debug("here4");
	if (state.current_mode == CANCELLED) return EXIT_FAILURE;
		
	return EXIT_SUCCESS;  
}
