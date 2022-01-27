#include <gtk/gtk.h>

#include "racecamCommon.h"

#include <bcm2835.h>

#include "GPSUtil.h"

#include "stop-sign.xpm"

enum {STOP, START};

//#define GPIO_MODEM_LED	RPI_BPLUS_GPIO_J8_18 
//#define GPIO_LED	RPI_BPLUS_GPIO_J8_13 
//#define GPIO_SWT	RPI_BPLUS_GPIO_J8_15
//#define GPIO_PWR_LED	RPI_BPLUS_GPIO_J8_16

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 480

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
  float keep_free;      
  char adev[18];  // dmic_sv
  short int main_size;  // 2: 854x480 1: 1280x720 0: 1920x1080
  float ovrl_size;
  float ovrl_x;
  float ovrl_y;
  char fmh;
  char fmv;
  char foh;
  char fov;
  float qmin;
  float qcur;
  float fps;
  float ifs;
  char cam;
  char gps;
  char preview;
  } iparms;

GtkWidget *main_win=NULL, *stop_win=NULL;

// char gpio_init=0;

unsigned long     kb_xid;
pid_t sh_pid, kbd_pid;

/* Backing pixmap for drawing area */
static GdkPixmap *pixmap = NULL;

RACECAM_STATE global_state;

int gps_enabled = 0; 
char alsa_dev[17] = "dmic_sv";
char file[64];
char url[64];

GPS_T gps_data;
	
static int clean_files(void);

void cleanup_children(int s)
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
  static int64_t kill_time = -1;
  if (global_state.current_mode) return;
  log_error("term signal in handler %d", s);
  kill(kbd_pid, SIGTERM);  
  kill(sh_pid, SIGTERM);  
  exit(0);
}

void install_signal_handlers(void)
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
// removed so system() would return correct RC
//  signal (SIGCHLD, SIG_IGN);  /* kernel can deal with zombies  */
  signal(SIGINT, cleanup_children);
  signal(SIGQUIT, cleanup_children);
  signal(SIGTERM, cleanup_children);
}

unsigned long launch_keyboard(void)
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
  int    i = 0, fd[2];
  int    stdout_pipe[2];
  int    stdin_pipe[2];
  char   buf[256], c;
  size_t n;

  unsigned long result;

  pipe (stdout_pipe);
  pipe (stdin_pipe);

  sh_pid = fork();
  switch (sh_pid)
    {
    case 0:
      {
	/* Close the Child process' STDOUT */
      close(1);
      dup(stdout_pipe[1]);
      close(stdout_pipe[0]);
      close(stdout_pipe[1]);
  
  /* use the following for standard keyboard layout 
   * 	execlp ("/bin/sh", "sh", "-c", "matchbox-keyboard --xid fi", NULL); */ 
      execlp ("/bin/sh", "sh", "-c", "matchbox-keyboard --xid rc", NULL);
      }
    case -1:
      log_error("### Failed to launch 'matchbox-keyboard --xid', is it installed? ### ");
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
  result = atol(buf);

  close(stdout_pipe[0]);
  
  sprintf(buf, "pgrep -P %d\n", sh_pid);
  FILE *child_fd = popen(buf, "r");
  fgets(buf, sizeof(buf), child_fd);
  pclose(child_fd);
  kbd_pid = atol(buf);

  return result;
}

void parms_to_state(RACECAM_STATE *state)
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
  switch (iparms.main_size)    // 2: 854x480 1: 1280x720 0: 1920x1080
    {
    case 0:
      state->common_settings[MAIN_CAMERA].cam.width = 1920;
      state->common_settings[MAIN_CAMERA].cam.height = 1080;
      break;
    case 1:
      state->common_settings[MAIN_CAMERA].cam.width = 1280;
      state->common_settings[MAIN_CAMERA].cam.height = 720;
      break;
    default:
      state->common_settings[MAIN_CAMERA].cam.width = 854;
      state->common_settings[MAIN_CAMERA].cam.height = 480;
  }
    
  state->common_settings[OVERLAY_CAMERA].cam.width = state->common_settings[MAIN_CAMERA].cam.width*iparms.ovrl_size;
  state->common_settings[OVERLAY_CAMERA].cam.height = state->common_settings[MAIN_CAMERA].cam.height*iparms.ovrl_size;
  state->common_settings[OVERLAY_CAMERA].cam.x = state->common_settings[MAIN_CAMERA].cam.width*iparms.ovrl_x;
  state->common_settings[OVERLAY_CAMERA].cam.y = state->common_settings[MAIN_CAMERA].cam.height*iparms.ovrl_y;

  state->common_settings[0].cameraNum = state->common_settings[1].cameraNum = 1;
  state->common_settings[iparms.cam].cameraNum = 0;

  state->camera_parameters.vflip[MAIN_CAMERA] = iparms.fmv;
  state->camera_parameters.hflip[MAIN_CAMERA] = iparms.fmh;
  
  state->camera_parameters.vflip[OVERLAY_CAMERA] = iparms.fov;
  state->camera_parameters.hflip[OVERLAY_CAMERA] = iparms.foh;
  
  state->quantisationParameter=iparms.qcur;
  state->quantisationMin=iparms.qmin;
  state->framerate=iparms.fps;
  state->intraperiod=iparms.fps*iparms.ifs;
  
  int i;
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
  
  gps_enabled=iparms.gps;
  
  state->output_state[FILE_STRM].run_state = iparms.write_file;
  state->output_state[URL_STRM].run_state = iparms.write_url;

  state->preview_mode=iparms.preview;
  state->adev = iparms.adev;
}

void dest_parms(void)
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
    
  if (iparms.write_url)
		{
    strcpy(url, "rtmp://");
    strcpy(url+7, iparms.url);
    }
  else
    url[0]='\0';
    
  if (iparms.write_file)
		{
		int length = 0;	
		time_t time_uf;
		struct tm *time_fmt;
		time(&time_uf);
		time_fmt = localtime(&time_uf);
		strcpy(file, "file:");
		length=strlen(file);
		strcpy(file+length, iparms.file);
		length=strlen(file);
		strftime(file+length, 20,"%Y-%m-%d_%H_%M_%S", time_fmt);
		length=strlen(file);
		strcpy(file+length, ".flv"); 
    }
  else
    file[0]='\0';

}

int read_parms(void)
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
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
  dest_parms();
  return 0;
}

int write_parms(char *mode, size_t size, void *ptr)
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
  FILE *parm_file;
  parm_file=fopen("/home/pi/racecam.ini", mode);
  if (parm_file == NULL) return 1;
  size_t cnt=0;
  cnt=fwrite(ptr, 1, size, parm_file);
  if (cnt!=size) return 1;
  if (fclose(parm_file)) return 1;
  dest_parms();
  return 0;
}
  

void *record_thread(void *argp)
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
  
  int file_selected = 0, url_selected = 0, length = 0;

  if (global_state.output_state[FILE_STRM].run_state == SELECTED)
		{
		// setup states
//    int length = 0;	
		time_t time_uf;
		struct tm *time_fmt;
		time(&time_uf);
		time_fmt = localtime(&time_uf);
		strcpy(file, "file:");
		length=strlen(file);
		strcpy(file+length, iparms.file);
		length=strlen(file);
		strftime(file+length, 20,"%Y-%m-%d_%H_%M_%S", time_fmt);
		length=strlen(file);
		strcpy(file+length, ".flv"); 
    global_state.output_state[FILE_STRM].dest = file;
    global_state.output_state[FILE_STRM].queue = global_state.userdata[FILE_STRM].queue = alloc_queue();
		}
		
  if (global_state.output_state[URL_STRM].run_state == SELECTED)
		{
		// setup states
    strcpy(url, "rtmp://");
		length=strlen(url);
    strcpy(url+length, iparms.url);
    global_state.output_state[URL_STRM].dest = url;
    global_state.output_state[URL_STRM].queue = global_state.userdata[URL_STRM].queue = alloc_queue();
		}

  if (allocate_audio_encode(&global_state)) 
		{
		goto err_aencode;
		}

  if (allocate_alsa(&global_state)) 
		{ 
		goto err_alsa;
		}

  if (create_video_stream(&global_state)) 
		{
		goto err_vstream;
		}
    
  if (gps_enabled) 
		{
    gps_data.t_queue = global_state.hvs_textin_pool->queue;  
    gps_data.t_port = global_state.hvs_component->input[2];
    gps_data.active = SENDING;
    } 
    
	pthread_t file_tid, url_tid, adjq_tid;
  if (global_state.output_state[FILE_STRM].run_state == SELECTED)
		{
    file_selected = global_state.output_state[FILE_STRM].run_state = WRITING;
    pthread_create(&file_tid, NULL, write_stream, (void *)&global_state.output_state[FILE_STRM]);
		}  

  if (global_state.output_state[URL_STRM].run_state == SELECTED)
		{
    url_selected = global_state.output_state[URL_STRM].run_state = WRITING;
	  pthread_create(&url_tid, NULL, write_stream, (void *)&global_state.output_state[URL_STRM]);
		global_state.adjust_q_state.queue = global_state.userdata[URL_STRM].queue;
    global_state.adjust_q_state.running = &global_state.output_state[URL_STRM].run_state;
		global_state.adjust_q_state.port = global_state.encoder_component[URL_STRM]->output[0];
		global_state.adjust_q_state.min_q = global_state.quantisationMin;
		pthread_create(&adjq_tid, NULL, adjust_q, (void *)&global_state.adjust_q_state);
		}  
    	
	global_state.current_mode=RECORDING;

  snd_pcm_drop(global_state.pcmhnd);
  snd_pcm_prepare(global_state.pcmhnd);
  snd_pcm_start(global_state.pcmhnd);
  global_state.sample_cnt = 0;
	mmal_port_parameter_set_boolean(global_state.camera_component[MAIN_CAMERA]->output[MMAL_CAMERA_VIDEO_PORT], MMAL_PARAMETER_CAPTURE, START);
	mmal_port_parameter_set_boolean(global_state.camera_component[OVERLAY_CAMERA]->output[MMAL_CAMERA_VIDEO_PORT], MMAL_PARAMETER_CAPTURE, START);
	
	while (global_state.current_mode > 0 ) 
		{
    read_pcm(&global_state);
    check_output_status(&global_state);
		}
    
  mmal_port_parameter_set_boolean(global_state.camera_component[MAIN_CAMERA]->output[MMAL_CAMERA_VIDEO_PORT], MMAL_PARAMETER_CAPTURE, STOP);
	mmal_port_parameter_set_boolean(global_state.camera_component[OVERLAY_CAMERA]->output[MMAL_CAMERA_VIDEO_PORT], MMAL_PARAMETER_CAPTURE, STOP);
	
  encode_queue_audio(&global_state, TRUE); // flush audio encoder
	
			
err_vstream:
  destroy_video_stream(&global_state);
	
  if (file_selected)
		{
    if (queue_end(global_state.output_state[FILE_STRM].queue)) {log_error("End queue file stream failed");}
    if (global_state.output_state[FILE_STRM].run_state == WRITING) global_state.output_state[FILE_STRM].run_state = STOPPING_WRITE;
    char buf[64];
    strcpy(buf, global_state.output_state[FILE_STRM].dest+5);
    if (write_parms("ab", sizeof(buf), buf)) printf("write failed\n");
		}

  if (url_selected)
		{
	  if (queue_end(global_state.output_state[URL_STRM].queue)) {log_error("End queue url stream failed");}
    if (global_state.output_state[URL_STRM].run_state == WRITING) global_state.output_state[URL_STRM].run_state = STOPPING_WRITE;
		} 
		
err_alsa:
  free_alsa(&global_state);
	
err_aencode:
  free_audio_encode(&global_state);

  if (file_selected) 
		{
		pthread_join(file_tid, NULL);
		}  
		 
  if (url_selected)
		{
		pthread_join(url_tid, NULL);
		pthread_join(adjq_tid, NULL);
		}  	
	
  clean_files();
  
}

void inc_val_lbl(GtkWidget *widget, gpointer data)
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
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
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
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
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
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
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
  check *chk=data;
  *chk->status = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(chk->button));  
  log_status("Check value %d", *chk->status);
}

void check_res0(GtkWidget *widget, gpointer data)
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
  if(gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(data))) iparms.main_size=0;  
}

void check_res1(GtkWidget *widget, gpointer data)
{
 log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
  if(gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(data))) iparms.main_size=1;  
}

void check_res2(GtkWidget *widget, gpointer data)
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
  if(gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(data))) iparms.main_size=2;  
}

void check_main(GtkWidget *widget, gpointer data)
{
 log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
  if(gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(data))) iparms.preview=1;
}

void check_overlay(GtkWidget *widget, gpointer data)
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
  if(gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(data))) iparms.preview=2; 
}

void check_composite(GtkWidget *widget, gpointer data)
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
  if(gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(data))) iparms.preview=3; 
}

void draw_it(ovrl *ol)
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
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
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
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
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
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
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
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
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
  draw *dptr=data;
  if (*dptr->val > *dptr->min) 
    {
    *dptr->val -= dptr->incv;
    if(*dptr->val < *dptr->min) *dptr->val += *dptr->min-*dptr->val;
    draw_it(dptr->ol);
    }
}

int test_dest(const char *dest)
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
  AVIOContext *ioctx;

  if (avio_open(&ioctx,	dest, AVIO_FLAG_WRITE) < 0) 
    {
    return 1;
    }
  else 
    {
    avio_close(ioctx);
    if (!(strncmp(dest, "file:", 5)))
      {
      char *only_file=strstr(dest, ":");
      only_file++;    
      if (remove(only_file)) log_error("remove %s failed %d", only_file, errno);
      }
    }
  return 0;
}
int warn_dest(GtkWindow *parent, int flag)
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
  char msg[256]={'\0'};
  int error=FALSE;
  
  if (iparms.write_url) 
    {
    if (system("ping -c 1 8.8.8.8 > /dev/null 2>&1")) 
      {
      strcpy(msg, "URL selected but unable to validate as network unavailable!\n\n");
      error=TRUE;
      }
    else
      {
      if (test_dest(url))
        {
        sprintf(msg, "URL:%s\n selected but unable to validate!\n\n", url);
        error=TRUE;
        }
      }
    }
    
  int length=strlen(msg);
  if (iparms.write_file)
    {
    if (test_dest(file))
      {
      sprintf(msg+length, "File:%s\n selected but unable to validate!\n\n", file);
      error=TRUE;
      } 
    }
  int response=0;
  if (error)
    {
    length=strlen(msg);
    GtkWidget *dlg=NULL;
   if (flag)
      {
      strcpy(msg+length, "Select OK to proceed to setup");
      dlg = gtk_dialog_new_with_buttons(NULL, GTK_WINDOW(parent), 
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, "OK", 1, NULL);
      }
    else
      { 
      strcpy(msg+length, "Select Ignore error or Cancel to return");
      dlg = gtk_dialog_new_with_buttons(NULL, GTK_WINDOW(parent), 
             GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, "Ignore", 0, "Cancel", 1, NULL);
      }
    GtkWidget *lbl = gtk_label_new(msg);
    gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dlg))), lbl);
 /*   GValue val = G_VALUE_INIT;
    g_value_init (&val, G_TYPE_ENUM);
    g_value_set_enum (&val, GTK_ALIGN_CENTER);
    g_object_set_property(GTK_OBJECT(gtk_dialog_get_action_area(GTK_DIALOG(dlg))), "halign", &val); */
//    gtk_widget_set_halign(gtk_dialog_get_action_area(GTK_DIALOG(dlg)), GTK_ALIGN_CENTER);
//    gtk_widget_set_halign(gtk_dialog_get_action_area(GTK_DIALOG(dlg)), 3);
    gtk_widget_show(lbl);
    error=gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);
    }
  return error;
}

void cancel_clicked(GtkWidget *widget, gpointer data)
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
  if (read_parms()) log_error("read failed");
  gtk_widget_destroy(data);
  gtk_main_quit ();
}

void save_clicked(GtkWidget *widget, gpointer data)
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
  if (write_parms("r+b", sizeof(iparms), &iparms)) log_error("write failed");
  parms_to_state (&global_state);
  warn_dest(GTK_WINDOW(data), TRUE);
}

void done_clicked(GtkWidget *widget, gpointer data)
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
  if (write_parms("r+b", sizeof(iparms), &iparms)) log_error("write failed");
  parms_to_state (&global_state);
  if (warn_dest(GTK_WINDOW(data), FALSE)) return;
  gtk_widget_destroy(data);
  gtk_main_quit();
}

void stop_clicked(GtkWidget *widget, gpointer data)
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
  gtk_widget_destroy(data);
  data=NULL;
  gtk_main_quit();
 }

 void text_cb(GtkWidget *widget, gpointer data)
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
  char *cptr=data;
  const gchar *text;
  text = gtk_entry_get_text (GTK_ENTRY (widget));
  strcpy(cptr, text);
}

void widget_destroy(GtkWidget *widget, gpointer data)
{
   log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
   gtk_widget_destroy(widget);
   gtk_main_quit ();
}

/* Create a new backing pixmap of the appropriate size */
static gboolean configure_event(GtkWidget *widget, GdkEventConfigure *event, gpointer data)
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
  ovrl *ol=data;
  
  if (pixmap)
    g_object_unref (pixmap);
    
  pixmap = gdk_pixmap_new (widget->window, *ol->x, *ol->y, -1);
  draw_it(data);

  return TRUE; 
}

/* Redraw the screen from the backing pixmap */
static gboolean expose_event( GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
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
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);  
  char num_buf[3];
  char intf[6]="%2.0f"; 
  char fltf[6]="%1.1f";
  float qmax = 39;
  float fps_min=20, fps_max=60, q_min=15, q_max=40, ifs_min=.1, ifs_max=2, fk_min=1, fk_max=25;
  limit fk_lmt = {0, &iparms.keep_free, intf, &fk_min, &fk_max, 1};
  limit fps_lmt = {0, &iparms.fps, intf, &fps_min, &fps_max, 1};
  limit ifs_lmt = {0, &iparms.ifs, fltf, &ifs_min, &ifs_max, .1};
  limit qmin_lmt = {0, &iparms.qmin, intf, &q_min, &iparms.qcur, 1};
  limit qcur_lmt = {0, &iparms.qcur, intf, &iparms.qmin, &qmax, 1};
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
   
  if (data) gtk_widget_hide(data);
  /* setup window */
  GtkWidget *setup_win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (G_OBJECT (setup_win), "destroy", G_CALLBACK (cancel_clicked), setup_win);
//  gtk_window_set_default_size (GTK_WINDOW (setup_win), WINDOW_WIDTH, WINDOW_HEIGHT);
  gtk_window_set_default_size (GTK_WINDOW (setup_win), 800, 400);
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
  g_signal_connect(button, "clicked", G_CALLBACK(save_clicked), setup_win);
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
  wptr = gtk_label_new ("Keep free (GB)");
  gtk_box_pack_start (GTK_BOX(hbox), wptr, FALSE, FALSE, 5);
  
  wptr = gtk_button_new();
  g_signal_connect(wptr, "clicked", G_CALLBACK(dec_val_lbl), &fk_lmt);
  gtk_button_set_image(GTK_BUTTON(wptr), gtk_image_new_from_stock(GTK_STOCK_REMOVE, GTK_ICON_SIZE_BUTTON));
  gtk_box_pack_start (GTK_BOX(hbox), wptr, FALSE, FALSE, 0);
  
  sprintf(num_buf, "%2.0f", iparms.keep_free);
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

  /* setup window keyboard socket stuff */ 
  GtkWidget *socket_box = gtk_event_box_new ();
  gtk_widget_show (socket_box);
  
  wptr = gtk_socket_new ();

  gtk_container_add (GTK_CONTAINER (socket_box), wptr);
  
  gtk_box_pack_start (GTK_BOX (vbox1), GTK_WIDGET(socket_box), TRUE, TRUE, 40);

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
  
  gps_check.button=gtk_check_button_new_with_label ("GPS spd  ");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gps_check.button), iparms.gps);
  g_signal_connect(gps_check.button, "toggled", G_CALLBACK(check_status), &gps_check);
  gtk_box_pack_start (GTK_BOX(hbox), gps_check.button, FALSE, TRUE, 2);
   
   /* setup window preview select composite/main/overlay */ 
  wptr=gtk_radio_button_new_with_label (NULL, "Composite");
  g_signal_connect(wptr, "clicked", G_CALLBACK(check_composite), wptr);
  if (iparms.preview == 3) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (wptr), TRUE);
  gtk_box_pack_start (GTK_BOX(hbox), wptr, FALSE, TRUE, 2);

  wptr=gtk_radio_button_new_with_label (gtk_radio_button_get_group (GTK_RADIO_BUTTON(wptr)), "Main");
  g_signal_connect(wptr, "clicked", G_CALLBACK(check_main), wptr);
  if (iparms.preview == 1) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (wptr), TRUE);
  gtk_box_pack_start (GTK_BOX(hbox), wptr, FALSE, TRUE, 2);
  
  wptr=gtk_radio_button_new_with_label (gtk_radio_button_get_group (GTK_RADIO_BUTTON(wptr)), "Overlay");
  g_signal_connect(wptr, "clicked", G_CALLBACK(check_overlay), wptr);
  if (iparms.preview == 2) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (wptr), TRUE);
    gtk_box_pack_start (GTK_BOX(hbox), wptr, FALSE, TRUE, 2); 
    
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
  if (data) gtk_widget_show_all(data);

}

void stop_window(gpointer data)
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
  gtk_widget_hide(data);
  /* stop window */
  stop_win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (stop_win), WINDOW_WIDTH, WINDOW_HEIGHT);
  gtk_window_set_decorated (GTK_WINDOW(stop_win), FALSE); 
//  gtk_window_fullscreen (GTK_WINDOW(stop_win));
  /* stop button */
  GtkWidget *wptr = gtk_button_new();
  GdkPixbuf *pixbuf = gdk_pixbuf_new_from_xpm_data (stop_sign);
  GtkWidget *image = gtk_image_new_from_pixbuf (pixbuf);
  gtk_button_set_image(GTK_BUTTON(wptr), image);
  
  GtkWidget *vbox = gtk_vbox_new(FALSE, 5);
  gtk_box_pack_start (GTK_BOX(vbox), wptr, TRUE, TRUE, 0);

  g_signal_connect(wptr, "clicked", G_CALLBACK(stop_clicked), stop_win);
  gtk_container_add (GTK_CONTAINER (stop_win), vbox);
  gtk_widget_show_all(stop_win);
  
  gtk_main();

  gtk_widget_show_all(data);
}

void preview_clicked(GtkWidget *widget, gpointer data)
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);

  create_video_preview(&global_state);

  stop_window(data);

  destroy_video_preview(&global_state);
}

void record_clicked(GtkWidget *widget, gpointer data)
{
 log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
  
  if (warn_dest(GTK_WINDOW(data), FALSE)) return;
  
  if (iparms.write_file) global_state.output_state[FILE_STRM].run_state = SELECTED;
  if (iparms.write_url) global_state.output_state[URL_STRM].run_state = SELECTED;
  global_state.current_mode = RECORDING;
  
  pthread_t record_tid;
  pthread_create(&record_tid, NULL, record_thread, (void *)&global_state);

  stop_window(data);

  global_state.current_mode=STOPPED;
  pthread_join(record_tid, NULL);

}

int get_free(void)
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
  char buf[80];
  FILE *free_fd = popen("df --output=avail / | tail -1", "r");
  fgets(buf, sizeof(buf), free_fd);
  pclose(free_fd);
  return (atoi(buf))/1048576;
} 
int copy_file(FILE *to, FILE *from, int size)
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
  char buf[201];
  size_t cnt=fread(buf, 1, size, from);
  if (cnt != size)
    {
    log_error("read failed in copy_file");
    return 1;
    }
  else
    {
    cnt=fwrite(buf, 1, size, to);
    if (cnt!=size) 
      {
      log_error("write failed in copy_file");
      return 1;
      }
    }
  return 0;
}

int del_file(FILE *file)
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
  char buf[64];
  size_t cnt=fread(buf, 1, 64, file);
  if (cnt != 64)
    log_error("delete read failed %d", cnt);
  else
    if (remove(buf)) 
      log_error("remove file %s failed", buf);
  return 0;
}

int clean_files(void)
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__); 

  if (get_free() >= iparms.keep_free) return 0;

  FILE *init_f1, *init_f2;
  init_f1=fopen("/home/pi/racecam.ini", "rb");
  if (!init_f1) 
    {
    log_error("open ini file for cleanup failed");
    return 1;
    }
    
  if (fseek(init_f1, 0, SEEK_END))
    {
    log_error("seek end failed");
    return 1;
    }

  int num_of_files=ftell(init_f1);
  if (num_of_files<0)
    {
    log_error("file size check failed");
    return 1;
    }
  num_of_files=(num_of_files-sizeof(iparms))/64;
  int files_left=num_of_files;
  
  int rc=rename("/home/pi/racecam.ini", "/home/pi/racecam.old");
  if (rc) 
    {
    log_error("rename failed");
    return 1;
    } 

  init_f2=fopen("/home/pi/racecam.ini", "wb");
  if (!init_f2) goto rename_back;

  if (fseek(init_f1, 0, SEEK_SET))
    {
    log_error("seek start failed");
    goto rename_back;
    }
           
  if(copy_file(init_f2, init_f1, sizeof(iparms)))
    log_error("copy file failed");
  else
    {
    int x=num_of_files;
    for (x=num_of_files; x>1 ;x--)
      {
      del_file(init_f1);
      if (get_free() >= iparms.keep_free) x=0;
      files_left--; 
      }

    for(x=files_left; x; x--)  
      {
      copy_file(init_f2, init_f1, 64);
      }
    }
  if (fclose(init_f1)) log_error("close failed");
  if (fclose(init_f2)) log_error("close failed");
  if (remove("/home/pi/racecam.old")) 
    log_error("remove old init file");
  return 0;
  
rename_back:
  log_error("open new ini file for cleanup failed"); 
  rc=rename("/home/pi/racecam.old", "/home/pi/racecam.init");
  if (rc) printf("rename back failed\n"); 
  if (fclose(init_f2)) log_error("close failed");
  goto close_f1;
close_f1:
  if (fclose(init_f1)) log_error("close failed");
  return 1;
}

int main(int argc, char **argv)
{
 // set message levels as needed 
//  logger_set_log_level(LOG_MAX_LEVEL_ERROR_WARNING_STATUS_DEBUG);	
	logger_set_log_level(LOG_MAX_LEVEL_ERROR_WARNING_STATUS);	
//  logger_set_out_stdout();
  logger_set_log_file("/home/pi/racecam.log");
  
// AV_LOG_ QUIET, PANIC, FATAL, ERROR, WARNING, INFO, VERBOSE, DEBUG and TRACE
  av_log_set_level(AV_LOG_PANIC);
//	av_log_set_level(AV_LOG_ERROR);
//  av_log_set_level(AV_LOG_TRACE);
    
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
  
  gps_data.text_size = global_state.common_settings[MAIN_CAMERA].cam.height/20;
	gps_data.text.width = global_state.common_settings[MAIN_CAMERA].cam.width;
	gps_data.text.height =  global_state.common_settings[MAIN_CAMERA].cam.height;
	gps_data.text.x = 2000;
	gps_data.text.y = 2000;

	pthread_t gps_tid;	
  
  FILE *url_file;
  size_t url_size=0;
  char url_new[64];
  url_file=fopen("/home/pi/racecam.url", "rb");
  if (url_file)
    {
    url_size=fread(&url_new, 1, sizeof(url_new), url_file);
    int i;
    for (i=0; i<=url_size;i++)
      {
      if (url_new[i] < '!')
        {
          url_new[i] = '\0';
          url_size = i;
        }
        log_error(">%c< %d", url_new[i], url_size);
      }
    if (remove("/home/pi/racecam.url")) log_error("rename of racecam.url failed");
    }
  
  if (read_parms())
    {
    if (url_size)
      {
      strcpy(iparms.url, url_new);
      }
    else
      {
      strcpy(iparms.url, "a.rtmp.youtube.com/live2/<key>");
      }
    strcpy(iparms.file, "/RaceCam/video/racecam");
    strcpy(iparms.adev, "dmic_sv");
    iparms.fmh=iparms.fmv=iparms.foh=iparms.fov=iparms.main_size=0;
    iparms.ovrl_size=iparms.ovrl_x=iparms.ovrl_y=.5;
    iparms.fps=30;
    iparms.ifs=2;
    iparms.qmin=25;
    iparms.qcur=28;
    iparms.preview=3;
    iparms.write_url=iparms.write_file=1;
    iparms.keep_free=3;
    if (write_parms("wb", sizeof(iparms), &iparms)) log_error("write failed");
    } 
   else
    {
    if (url_size)
      {
      strcpy(iparms.url, url_new);
      if (write_parms("r+b", sizeof(iparms), &iparms)) log_error("write failed");
      }
    }
    
  memset(&global_state, 0, sizeof(RACECAM_STATE));
  default_status(&global_state);
  global_state.output_state[FILE_STRM].r_state = global_state.output_state[URL_STRM].r_state =&global_state;
  parms_to_state(&global_state);
  
  install_signal_handlers();
  gtk_init (&argc, &argv);
  
  log_status("gps_flag %d %d", gps_enabled, iparms.gps);

	if (gps_enabled) 
		{
    log_status("gps_starting");
    gps_data.active = WAITING;
    pthread_create(&gps_tid, NULL, gps_thread, (void *)&gps_data);
		}   
  
  kb_xid = launch_keyboard();

  if (!kb_xid)
    {
      perror ("### 'matchbox-keyboard --xid', failed to return valid window ID. ### ");
      exit(-1);
    }   
  
  /*Main Window */
  main_win = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  gtk_window_set_default_size (GTK_WINDOW (main_win), WINDOW_WIDTH, WINDOW_HEIGHT);
  gtk_container_set_border_width (GTK_CONTAINER (main_win), 20);

  g_signal_connect (G_OBJECT (main_win), "destroy", G_CALLBACK (widget_destroy), NULL);

  GtkWidget *vbox = gtk_vbox_new(FALSE, 20);
  gtk_container_add (GTK_CONTAINER (main_win), vbox);
  
  PangoFontDescription *df = pango_font_description_from_string("Monospace");
  pango_font_description_set_size(df,40*PANGO_SCALE);
  pango_font_description_set_weight (df, PANGO_WEIGHT_HEAVY);
  
  GtkWidget *button, *label; 
  
  button = gtk_button_new ();
  label = gtk_label_new("RECORD");
  gtk_widget_modify_font(label, df);
  gtk_container_add(GTK_CONTAINER(button), label);
  g_signal_connect(button, "clicked", G_CALLBACK(record_clicked), main_win);
  gtk_box_pack_start (GTK_BOX(vbox), button, TRUE, TRUE, 2);
  
  button = gtk_button_new();
  label = gtk_label_new("PREVIEW");
  gtk_widget_modify_font(label, df);
  gtk_container_add(GTK_CONTAINER(button), label);
  g_signal_connect(button, "clicked", G_CALLBACK(preview_clicked), main_win);
  gtk_box_pack_start (GTK_BOX(vbox), button, TRUE, TRUE, 2);

  button= gtk_button_new ();
  label = gtk_label_new("SETUP");
  gtk_widget_modify_font(label, df);
  gtk_container_add(GTK_CONTAINER(button), label);
  g_signal_connect(button, "clicked", G_CALLBACK(setup_clicked), main_win);
  gtk_box_pack_start (GTK_BOX(vbox), button, TRUE, TRUE, 2);
  
  gtk_widget_show_all(main_win);

  gtk_main();
    
  if (gps_enabled) 
		{
		gps_data.active=0;
		pthread_join(gps_tid, NULL);
		} 

  clean_files();

  kill(kbd_pid, 15);
  kill(sh_pid, 15);
  return 0;
}
