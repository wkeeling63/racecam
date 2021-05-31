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
#include <string.h>
#include <ctype.h>
//#include <alsa/asoundlib.h>
#include <sys/time.h>
#include <sys/signal.h>

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <sysexits.h>
#include <sys/socket.h>
#include <time.h>
#include <semaphore.h>
#include <stdbool.h>

//#include <libavformat/avformat.h>
//#include <libavcodec/avcodec.h>
//#include "libavformat/avio.h"
//#include "libavutil/audio_fifo.h"
//#include "libavutil/avassert.h"
//#include "libavutil/avstring.h"
//#include "libavutil/frame.h"
//#include "libavutil/opt.h"
//#include "libswresample/swresample.h"
//#include <libavutil/channel_layout.h>
//#include <libavutil/mathematics.h>
//#include <libavutil/timestamp.h>

#include "bcm_host.h"
#include "interface/vcos/vcos.h"

#include "interface/mmal/mmal.h"
#include "interface/mmal/mmal_logging.h"
//#include "interface/mmal/mmal_buffer.h"
//#include "interface/mmal/util/mmal_util.h"
//#include "interface/mmal/util/mmal_util_params.h"
//#include "interface/mmal/util/mmal_default_components.h"
#include "interface/mmal/util/mmal_connection.h"
//#include "interface/mmal/mmal_parameters_camera.h"

//#include <bcm2835.h>

#include "raspiCamUtilities.h"
#include "racecamUtil.h"
#include "GPSUtil.h"

int not_killable = 0;

//static void prg_exit(int code);

static void usage(char *command)
{
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
							"5=BottomCenter - Defaults to BottomLeft\n"
"-f, --flip=h.v:h.v         flip image of camera - example 1.1:0.1 flips the main camera vertically &\n" 
"                           horizontally and 2nd camera vertically\n"
"-F, --fps=#                frames per second #\n"
"\nEncoder paramaters:\n"
"-q, --quantisation=#:#:#   quantisation Init:Min:Max parameters\n"
"-i, --intraframe=#         intra key frame rate # frame\n" 
"\nOutput stream location - atleast one must be selected - both can be used to save locally and remotely\n"
"                           if neither is supplied then defaults to file of the name /home/pi/rqacecamcli\n"
"-o, --outputfile=PATH/FILE local output file ie /home/pi/racecamcli - the name will be appended with\n"
"                           timespace and file extention (.flv)\n"
"-u, --url=URL/KEY          url output location and key\n" )   
		, command);
} 

/*
 *	Subroutine to clean up before exit.
 */
/*static void close_it(void)  
{
//	fprintf(stdout, "\n");
	// stop gps thread
	gps_data.active=0;
	pthread_join(tid, NULL); 
	
	sem_t *psem=pstate->callback_data.mutex;	
	close_components(pstate);
	close_avapi();
	sem_destroy(psem);
	if (gpio_init) {
		bcm2835_gpio_write(GPIO_LED, LOW);
		bcm2835_close();}
	if (handle) {
		snd_pcm_close(handle);
		handle = NULL;}
	free(audiobuf);
	free(rlbufs);
	snd_config_update_free_global();
	fprintf(stdout, "%s PiPIPflv ending\n", get_time_str(datestr));
	
}
static void prg_exit(int code)  
{
	close_it();
	exit(code);
} */
static void signal_handler(int sig)
{
// logic to wait for stream to stop and exit ????

	if (not_killable)
		{
		not_killable=-1;
		return;
		}

//	prg_exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
	RASPIVID_STATE state;
	default_status(&state);

	//non state params 
	int timelimit = 0, resolution = 0, badparm=0;
	int overlay_percent = 25, overlay_location = 0;
	char *command;
	int gps_enabled = 0, stream_file = 0, stream_url = 0;
	char file[64];
	char parm_file[64] = "/home/pi/racecamcli";
	char url[64];
	char alsa_dev[17] = "dmic_sv";
	state.encodectx.adev = alsa_dev;

	int option_index;
	static const char short_options[] = "?D:d:r:c:s:q:i:n:l:f:F:o:u:g";
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
		{0, 0, 0, 0}
		};

	int c, num_parms = 0;

	command = argv[0];
	
	while ((c = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
	switch (c) {
		case '?':
			usage(command);
			return 0;
		case 'D':
			strncpy(state.encodectx.adev, optarg, 17);
			break;
		case 'd':
			timelimit = strtol(optarg, NULL, 0);
			if (timelimit < 0)
				{
				fprintf(stdout, "Duration %d invalid\n", timelimit);
				badparm=1;
				}
			break;
		case 'r':
			resolution = strtol(optarg, NULL, 0);
			if (resolution < 0 || resolution > 2)
				{
 				fprintf(stdout, "Resolution %d invalid\n", resolution);
				badparm=1;
				}
			break;		
		case 's':
			overlay_percent = strtol(optarg, NULL, 0);
			if (overlay_percent > 99 || overlay_percent < 1)
				{
				fprintf(stdout, "Overlay size %% must be between 1 and 99 %d\%\n", overlay_percent);
				badparm=1;
				break;
				}
			break;
		case 'l':
			overlay_location = strtol(optarg, NULL, 0);
			if (overlay_location > 6 || overlay_location < 0)
				{
				fprintf(stdout, "Overlay location must be between 0 and 5 %d\%\n", overlay_location);
				badparm=1;
				}
			break;
		case 'f':
			num_parms = sscanf(optarg, "%d.%d:%d.%d", &state.camera_parameters.hflip, &state.camera_parameters.vflip, 
			&state.hflip_o, &state.vflip_o);
            printf("here\n");
            switch(num_parms) {
				case EOF:
					fprintf(stderr, "filp sscanf failed\n");
					badparm=1;
					break;
				case 4:
					if (state.camera_parameters.hflip < 0 || state.camera_parameters.hflip > 1 || 
						state.camera_parameters.vflip < 0 || state.camera_parameters.vflip > 1 ||
						state.hflip_o  < 0 || state.hflip_o > 1 ||
						state.vflip_o < 0 || state.vflip_o > 1) 
						{
						fprintf(stdout, "flip flag must be 1 or 0 (true or false) %d.%d:%d.%d\n", state.camera_parameters.hflip,
						state.camera_parameters.vflip, state.hflip_o, state.vflip_o);
						badparm=1;}
					break;
				default:
					fprintf(stdout, "flip does not have 4 params\n");
					badparm=1;
					break;}
				break;
		case 'c':
			state.common_settings.cameraNum = strtol(optarg, NULL, 0);
			if (state.common_settings.cameraNum > 1 || state.common_settings.cameraNum < 0) {
				fprintf(stdout, "Camera must be 0 or 1 %d\n", state.common_settings.cameraNum);
				badparm=1;}
			break;
		case 'q':
			num_parms = sscanf(optarg, "%d:%d:%d", &state.quantisationParameter, &state.quantisationMin, 
			&state.quantisationMax);
            switch(num_parms) {
				case EOF:
					fprintf(stderr, "quantisation sscanf failed\n");
					badparm=1;
					break;
				case 3:
					if (state.quantisationMin > 40 || state.quantisationMin < 20) {
						fprintf(stdout, "Min quantisation parameter must be 20 to 40 %d\n", state.quantisationMin);
						badparm=1;}
					if (state.quantisationMax > 40 || state.quantisationMax < 20) {
						fprintf(stdout, "Max quantisation parameter must be 20 to 40 %d\n", state.quantisationMax);
						badparm=1;}
					if (state.quantisationParameter > state.quantisationMax || state.quantisationParameter < state.quantisationMin) {
						fprintf(stdout, "Init quantisation parameters must be Min and Max %d\n", state.quantisationParameter);
						badparm=1;}
				break;
				default:
					fprintf(stdout, "Quantisation does not have 3 params\n");
					badparm=1;
					break;}
			break;
		case 'n':
			state.achannels = strtol(optarg, NULL, 0);
			if (state.achannels > 2 || state.achannels < 1) {
				fprintf(stdout, "number of audio channels must be 1 or 2 %d\n", state.achannels);
				badparm=1;}
			break;
		case 'i':
			state.intraperiod = strtol(optarg, NULL, 0);
			if (state.intraperiod > 60) {
				fprintf(stdout, "Intra frame rate should be < 61  %d\n", state.intraperiod);
				badparm=1;}
			break;
		case 'F':
			state.framerate = strtol(optarg, NULL, 0);
			if (state.framerate < 22 || state.framerate > 30) {
				fprintf(stdout, "frames per second must be > 21 and < 31 %d\n", state.framerate);
				badparm=1;}
			break;
		case 'g':
			gps_enabled = 1;
			break;
		case 'o':
//			strcpy(parm_file, "file:");
			strcpy(parm_file, optarg);
			stream_file = 1;
			break;
		case 'u':
			strcpy(url, "rtmp://");
			strcpy(url+7, optarg);
			break;
		default:
			fprintf(stdout, "Try `%s --help' for more information.\n", command);
			return 1;
		}
	}

	if (badparm) {return EINVAL;}
	
	switch (resolution)    // 2: 854x480 1: 1280x720 0: 1920x1080
		{
		case 0:
			state.common_settings.width = 1920;
			state.common_settings.height = 1080;
			break;
		case 1:
			state.common_settings.width = 1280;
			state.common_settings.height = 720;
			break;
		default:
			state.common_settings.width = 854;
			state.common_settings.height = 480;
			break;
		}
			
	state.common_settings.ovl.width = state.common_settings.width * overlay_percent / 100;
	state.common_settings.ovl.height = state.common_settings.height * overlay_percent / 100;

	switch (overlay_location)   // 0=BL 1=TL 2=TC 3=TR 4=BR 5=BC 
		{
		case 0:
			state.common_settings.ovl.x=0;  
			state.common_settings.ovl.y=state.common_settings.height-state.common_settings.ovl.height;
			break;
		case 1:
			state.common_settings.ovl.x=0;  
			state.common_settings.ovl.y=0;
			break;
		case 2:
			state.common_settings.ovl.x=state.common_settings.width-(state.common_settings.ovl.width/2);  
			state.common_settings.ovl.y=0;
			break;
		case 3:
			state.common_settings.ovl.x=state.common_settings.width-state.common_settings.ovl.width;  
			state.common_settings.ovl.y=0;
			break;
		case 4:
			state.common_settings.ovl.x=state.common_settings.width-state.common_settings.ovl.width;  
			state.common_settings.ovl.y=state.common_settings.height-state.common_settings.ovl.height;
			break;
		default:
			state.common_settings.ovl.x=state.common_settings.width-(state.common_settings.ovl.width/2);  
			state.common_settings.ovl.y=state.common_settings.height-state.common_settings.ovl.height;
			break;
		}
		
	if (!(stream_file || stream_url))
		{
		printf("Defaulting to file /home/pi/racecamcli\n");
		stream_file = 1;
		}
	if (stream_file)
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
		printf ("bcm2835 init failed\n");
		}
		
	int64_t stop_pts = timelimit*1000;
		
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGABRT, signal_handler); 
	
	state.callback_data.pstate = &state;
	state.encodectx.start_time = get_microseconds64()/1000;
	state.recording=not_killable=1;
	state.lasttime = state.frame = 0;
  	
	AVPacket video_packet;
	av_init_packet(&video_packet);
	video_packet.stream_index=0;
	video_packet.duration=0;
	video_packet.pos=-1;
	state.callback_data.vpckt=&video_packet;
	
	GPS_T gps_data;
	pthread_t gps_tid;	
	if (gps_enabled) 
		{
		pthread_create(&gps_tid, NULL, gps_thread, (void *)&gps_data);
		}

// if timelimit == 0 loop and wait for switch to close_gps
//	int switch_state = bcm2835_gpio_lev(GPIO_SWT);
	if (!timelimit)
		while (bcm2835_gpio_lev(GPIO_SWT))
		{
		vcos_sleep(1000);
		}
//			{
//			switch_state = bcm2835_gpio_lev(GPIO_SWT);  */
//  a

	state.callback_data.wtargettime = TARGET_TIME/state.framerate;
// allocate video buffer
	state.callback_data.vbuf = (u_char *)malloc(BUFFER_SIZE);
	if (state.callback_data.vbuf == NULL) 
		{
		fprintf(stderr, "not enough memory vbuf\n");
		goto err_gps;
		}
// allocate mutex
	sem_t def_mutex;
	sem_init(&def_mutex, 0, 1);
	state.callback_data.mutex=&def_mutex;
 
	if (stream_file)
		{
		if (allocate_fmtctx(file, &state.filectx, &state))
			{
			printf("Allocate %s context failed\n", file);
			goto err_file;
			}
		state.encodectx.audctx=state.filectx.audctx;
		} 
	if (stream_url)
		{
		if (allocate_fmtctx(url, &state.urlctx, &state))
			{
			printf("Allocate %s context failed\n", url);
			goto err_url;
			}
		state.encodectx.audctx=state.urlctx.audctx;
		} 
	if (allocate_audio_encode(&state.encodectx)) {state.recording=-1; goto err_aencode;}
	if (allocate_alsa(&state.encodectx)) {state.recording=-1; goto err_alsa;}
	if (create_video_stream(&state)) {state.recording=-1; goto err_vstream;}
	if (create_encoder_component(&state)) {state.recording=-1; goto err_encoder;}

	MMAL_STATUS_T status; 
	status = connect_ports(state.hvs_component->output[0], state.encoder_component->input[0], &state.encoder_connection);
	if (status != MMAL_SUCCESS)
		{
		vcos_log_error("%s: Failed to connect hvs to encoder input", __func__); 
		state.encoder_connection = NULL;
		state.recording=-1;
		goto err_audio;
		}

	state.encoder_component->output[0]->userdata = (struct MMAL_PORT_USERDATA_T *)&state.callback_data;

	status = mmal_port_enable(state.encoder_component->output[0], encoder_buffer_callback);
	if (status) 
		{
		fprintf(stderr, "enable port failed\n");
		}
	
	int num = mmal_queue_length(state.encoder_pool->queue);    
	int q;
	for (q=0; q<num; q++)
		{
		MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get(state.encoder_pool->queue);
		if (!buffer) vcos_log_error("Unable to get a required buffer %d from pool queue", q);
		if (mmal_port_send_buffer(state.encoder_component->output[0], buffer)!= MMAL_SUCCESS)
		vcos_log_error("Unable to send a buffer to encoder output port (%d)", q);
		}
		
	toggle_stream(&state, START);

	while (state.recording > 0 ) 
		{
		read_pcm(&state);
		adjust_q(&state, NULL);
		if (gps_enabled) 
			{
			send_text(gps_data.speed, &state);
			}
		if (timelimit)
			{
			if (stop_pts < ((get_microseconds64()/1000) - state.encodectx.start_time))
				{
				state.recording = 0;
				}
			}
		else
			{
			// 0 closed and 1 open 
			if (bcm2835_gpio_lev(GPIO_SWT)) state.recording = 0;
			}
		}

	toggle_stream(&state, STOP);
	if (state.encoder_component) check_disable_port(state.encoder_component->output[0]);
	if (state.encoder_connection) mmal_connection_destroy(state.encoder_connection);
err_audio:
	flush_audio(&state);
err_encoder:
	destroy_encoder_component(&state);
err_vstream:
	destroy_video_stream(&state);
err_alsa:
	free_alsa(&state.encodectx);
err_aencode:
	free_audio_encode(&state.encodectx);
err_url:  
	if (state.urlctx.fmtctx) free_fmtctx(&state.urlctx);
err_file:
	if (state.filectx.fmtctx) free_fmtctx(&state.filectx);

	free(state.callback_data.vbuf);
	sem_destroy(&def_mutex);
err_gps:

	if (gps_enabled) 
		{
		gps_data.active=0;
		pthread_join(gps_tid, NULL);
		}
	if (gpio_init) {
		bcm2835_gpio_write(GPIO_LED, LOW);
		bcm2835_gpio_write(GPIO_MODEM_LED, LOW);
		bcm2835_gpio_write(GPIO_PWR_LED, LOW);
		bcm2835_close();}
		
	av_packet_unref(&video_packet);
	if (not_killable == -1) return EXIT_FAILURE;
		
	return EXIT_SUCCESS;  
}
