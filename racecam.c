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

#include "stop-sign.xpm"
#include "raspiCamUtilities.h"
#include "racecamUtil.h"
//#include "mmalcomponent.h"
#include "GPSUtil.h"

#define STOP 0
#define START 1
// write target time in micro seconds 250000=.25 second
#define TARGET_TIME 250000

  
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
  
char runtime[9];
char runmessage[80];
pthread_t switch_tid;
GtkWidget *main_win=NULL, *switch_dialog=NULL, *stop_win=NULL, *message=NULL;

char gpio_init=0;
int ignore_signal=0;

unsigned long     kb_xid;
pid_t sh_pid, kbd_pid;

/* Backing pixmap for drawing area */
static GdkPixmap *pixmap = NULL;

RASPIVID_STATE global_state;

static int clean_files(void);

void cleanup_children(int s)
{
  if (ignore_signal) return;
  printf("term signal in handler %d\n", s);
  kill(kbd_pid, SIGTERM);  
  kill(sh_pid, SIGTERM);  
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
  result = atol(buf);

  close(stdout_pipe[0]);
  
  sprintf(buf, "pgrep -P %d\n", sh_pid);
  FILE *child_fd = popen(buf, "r");
  fgets(buf, sizeof(buf), child_fd);
  pclose(child_fd);
  kbd_pid = atol(buf);

  return result;
}

void parms_to_state(RASPIVID_STATE *state)
{
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
  
  state->vflip_o = iparms.fov;
  state->hflip_o = iparms.foh;
  
  state->encodectx.adev = iparms.adev;
  state->achannels = iparms.channels;
  
  state->quantisationParameter=iparms.qcur;
  state->quantisationMin=iparms.qmin;
  state->quantisationMax=iparms.qmax;
  state->framerate=iparms.fps;
  state->intraperiod=iparms.fps*iparms.ifs;
  
  state->gps=iparms.gps;
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

int write_parms(char *mode, size_t size, void *ptr)
  {
  FILE *parm_file;
  parm_file=fopen("/home/pi/racecam.ini", mode);
  if (parm_file == NULL) return 1;
  size_t cnt=0;
  cnt=fwrite(ptr, 1, size, parm_file);
  if (cnt!=size) return 1;
  if (fclose(parm_file)) return 1;
  return 0;
  }
  

void *record_thread(void *argp)
{
  RASPIVID_STATE *state = (RASPIVID_STATE *)argp;
  state->callback_data.pstate = state;
  state->encodectx.start_time = get_microseconds64()/1000;
  state->recording=ignore_signal=1;
  state->lasttime = state->frame = 0;
  	
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
  
  sprintf(runmessage, "Quantization %d", state->quantisationParameter);
  
  int length;
  char str[64];
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
  if (allocate_audio_encode(&state->encodectx)) {state->recording=-1; goto err_aencode;}
  if (allocate_alsa(&state->encodectx)) {state->recording=-1; goto err_alsa;}
  if (create_video_stream(state)) {state->recording=-1; goto err_vstream;}
  if (create_encoder_component(state)) {state->recording=-1; goto err_encoder;}

  MMAL_STATUS_T status; 
  status = connect_ports(state->hvs_component->output[0], state->encoder_component->input[0], &state->encoder_connection);
  if (status != MMAL_SUCCESS)
    {
    vcos_log_error("%s: Failed to connect hvs to encoder input", __func__); 
    state->encoder_connection = NULL;
    state->recording=-1;
    goto err_audio;
    }

  state->encoder_component->output[0]->userdata = (struct MMAL_PORT_USERDATA_T *)&state->callback_data;

  status = mmal_port_enable(state->encoder_component->output[0], encoder_buffer_callback);
  if (status) 
    {
    fprintf(stderr, "enable port failed\n");
    }
	
  int num = mmal_queue_length(state->encoder_pool->queue);    
  int q;
  for (q=0; q<num; q++)
    {
    MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get(state->encoder_pool->queue);
    if (!buffer) vcos_log_error("Unable to get a required buffer %d from pool queue", q);
    if (mmal_port_send_buffer(state->encoder_component->output[0], buffer)!= MMAL_SUCCESS)
      vcos_log_error("Unable to send a buffer to encoder output port (%d)", q);
    }

  toggle_stream(state, START);
    
  while (state->recording > 0) 
    {
    read_pcm(state);
    adjust_q(state, runmessage);
    if (state->gps) 
      {
      send_text(gps_data.speed, state);
      }
    int64_t raw_time=(get_microseconds64()/1000)-state->encodectx.start_time;
    int hours=0, mins=0, secs=0;
    raw_time = raw_time/1000;
    secs = raw_time % 60;
    raw_time = (raw_time-secs)/60;
    mins = raw_time%60;
    hours = (raw_time-mins)/60;       
    sprintf(runtime, "%2d:%02d:%02d", hours, mins, secs);
    }

  toggle_stream(state, STOP);
  if (state->encoder_component) check_disable_port(state->encoder_component->output[0]);
  if (state->encoder_connection) mmal_connection_destroy(state->encoder_connection);
err_audio:
  flush_audio(state);
err_encoder:
  destroy_encoder_component(state);
err_vstream:
  destroy_video_stream(state);
err_alsa:
  free_alsa(&state->encodectx);
err_aencode:
  free_audio_encode(&state->encodectx);
err_url:  
  if (state->urlctx.fmtctx) free_fmtctx(&state->urlctx);
err_file:
  if (state->filectx.fmtctx) 
    {
    char buf[64];
    strcpy(buf, state->filectx.fmtctx->url+5);
    if (write_parms("ab", sizeof(buf), buf)) printf("write failed\n");
    free_fmtctx(&state->filectx);
    }
  free(state->callback_data.vbuf);
  sem_destroy(&def_mutex);
err_gps:
  if (state->gps) 
    {
    gps_data.active=0;
    pthread_join(gps_tid, NULL);
    }
  av_packet_unref(&video_packet);
  clean_files();
  if (state->recording == -1) ignore_signal=0;
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
  if (write_parms("r+b", sizeof(iparms), &iparms)) printf("write failed\n");
  parms_to_state (&global_state);
}

void done_clicked(GtkWidget *widget, gpointer data)
{
  if (write_parms("r+b", sizeof(iparms), &iparms)) printf("write failed\n");
  parms_to_state (&global_state);
  gtk_widget_destroy(data);
  gtk_main_quit ();
}

void stop_clicked(GtkWidget *widget, gpointer data)
{
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
   gtk_main_quit ();
}

/* Create a new backing pixmap of the appropriate size */
static gboolean configure_event(GtkWidget *widget, GdkEventConfigure *event, gpointer data)
{
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

void stop_window(gpointer data, int message_flag)
{
  gtk_widget_hide(data);
  /* stop window */
  stop_win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
//  gtk_window_set_default_size (GTK_WINDOW (stop_win), 800, 480);
  gtk_window_set_decorated (GTK_WINDOW(stop_win), FALSE); 
//  gtk_window_fullscreen (GTK_WINDOW(stop_win));
  /* stop button */
  GtkWidget *wptr = gtk_button_new();
  GdkPixbuf *pixbuf = gdk_pixbuf_new_from_xpm_data (stop_sign);
  GtkWidget *image = gtk_image_new_from_pixbuf (pixbuf);
  gtk_button_set_image(GTK_BUTTON(wptr), image);
  
  GtkWidget *vbox = gtk_vbox_new(FALSE, 5);
  gtk_box_pack_start (GTK_BOX(vbox), wptr, TRUE, TRUE, 0);
  if (message_flag)
    {
    message = gtk_label_new (NULL);
    gtk_label_set_justify (GTK_LABEL (message), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start (GTK_BOX(vbox), message, FALSE, TRUE, 0);
    }

  g_signal_connect(wptr, "clicked", G_CALLBACK(stop_clicked), stop_win);
  gtk_container_add (GTK_CONTAINER (stop_win), vbox);
  gtk_widget_show_all(stop_win);
  
  gtk_main();

  gtk_widget_show_all(data);
}

void preview_clicked(GtkWidget *widget, gpointer data)
{
  MMAL_STATUS_T status = MMAL_SUCCESS;
  global_state.mode=PREVIEW;
  global_state.callback_data.pstate = &global_state;
  
  create_video_stream(&global_state);
  create_preview_component(&global_state);
  if ((status = connect_ports(global_state.hvs_component->output[0], 
    global_state.preview_component->input[0], &global_state.preview_connection)) != MMAL_SUCCESS)
    {
		vcos_log_error("%s: Failed to connect hvs to encoder input", __func__); 
		global_state.preview_connection = NULL;
    goto error;
    } 
  toggle_stream(&global_state, START);
  stop_window(data, FALSE);
  toggle_stream(&global_state, STOP);
  if (global_state.preview_connection)
		mmal_connection_destroy(global_state.preview_connection);

error:  
  destroy_preview_component(&global_state);
  destroy_video_stream(&global_state);
  global_state.mode=NOT_RUNNING;
}

gint record_timeout (gpointer data)
{
  int *rs=(int *)data;
  if (*rs < 0 && (stop_win))
    {
    gtk_widget_destroy(stop_win);
    gtk_main_quit ();
    }
  char buf[90];
  sprintf(buf, "%s %s", runtime, runmessage);
  if (message) gtk_label_set_text (GTK_LABEL(message), buf);
  return 1;
}

void record_clicked(GtkWidget *widget, gpointer data)
{
  global_state.mode=CLICK_RECORD;
  global_state.recording=ignore_signal=0;
  gint timeout_id = g_timeout_add (125, record_timeout, &global_state.recording);
  pthread_t record_tid;
  pthread_create(&record_tid, NULL, record_thread, (void *)&global_state);
  stop_window(data, TRUE);
  g_source_remove (timeout_id);
  global_state.recording=ignore_signal=0;
  pthread_join(record_tid, NULL);
  global_state.mode=NOT_RUNNING;
}

int copy_file(FILE *to, FILE *from, int size)
{
  char buf[201];
  size_t cnt=fread(buf, 1, size, from);
  if (cnt != size)
    {
    printf("read failed in copy_file\n");
    return 1;
    }
  else
    {
    cnt=fwrite(buf, 1, size, to);
    if (cnt!=size) 
      {
      printf("write failed in copy_file\n");
      return 1;
      }
    }
  return 0;
}

int del_file(FILE *file)
{
  char buf[64];
  size_t cnt=fread(buf, 1, 64, file);
  if (cnt != 64)
    printf("delete read failed %d\n", cnt);
  else
    if (remove(buf)) 
      printf("remove file %s failed\n", buf);
  return 0;
}

int clean_files(void)
{
  FILE *init_f1, *init_f2;
  init_f1=fopen("/home/pi/racecam.ini", "rb");
  if (!init_f1) 
    {
    printf("open ini file for cleanup failed\n");
    return 1;
    }
    
  if (fseek(init_f1, 0, SEEK_END))
    {
    printf("seek end failed\n");
    return 1;
    }

  int num_of_files=ftell(init_f1);
  if (num_of_files<0)
    {
    printf("file size check failed\n");
    return 1;
    }
  num_of_files=(num_of_files-sizeof(iparms))/64;
  if (num_of_files <= iparms.file_keep) goto close_f1;
  
  int rc=rename("/home/pi/racecam.ini", "/home/pi/racecam.old");
  if (rc) 
    {
    printf("rename failed\n");
    return 1;
    } 

  init_f2=fopen("/home/pi/racecam.ini", "wb");
  if (!init_f2) goto rename_back;

  if (fseek(init_f1, 0, SEEK_SET))
    {
    printf("seek start failed\n");
    goto rename_back;
    }
           
  if(copy_file(init_f2, init_f1, sizeof(iparms)))
    printf("copy file failed\n");
  else
    {
    int x=num_of_files-iparms.file_keep;
    for (x=num_of_files-iparms.file_keep; x ;x--)
      {
      del_file(init_f1);
      }
    
    for(x=iparms.file_keep; x; x--)  
      {
      copy_file(init_f2, init_f1, 64);
      }
    }
  if (fclose(init_f1)) printf("close failed\n");
  if (fclose(init_f2)) printf("close failed\n");
  if (remove("/home/pi/racecam.old")) 
    printf("remove old init file\n");
  return 0;
  
rename_back:
  printf("open new ini file for cleanup failed\n");
  rc=rename("/home/pi/racecam.old", "/home/pi/racecam.init");
  if (rc) printf("rename back failed\n"); 
  if (fclose(init_f2)) printf("close failed\n");
  goto close_f1;
close_f1:
  if (fclose(init_f1)) printf("close failed\n");
  return 1;
}

gint main_timeout (gpointer data)
{
  RASPIVID_STATE *state = (RASPIVID_STATE *)data;
  static gint timeout_id=0;
  int switch_state = bcm2835_gpio_lev(GPIO_SWT);
  if (switch_state == 0 && state->mode == NOT_RUNNING)
    {
    switch_dialog = gtk_dialog_new();
    gtk_window_set_transient_for (GTK_WINDOW(switch_dialog), GTK_WINDOW(main_win));
    gtk_window_set_destroy_with_parent (GTK_WINDOW(switch_dialog), TRUE);
    gtk_window_set_modal (GTK_WINDOW(switch_dialog), TRUE);
    gtk_window_set_default_size (GTK_WINDOW (switch_dialog), 320, 180);
    gtk_window_set_decorated (GTK_WINDOW(switch_dialog), FALSE); 
    gtk_container_set_border_width (GTK_CONTAINER (switch_dialog), 20);
    message = gtk_label_new (NULL);
    gtk_label_set_justify (GTK_LABEL (message), GTK_JUSTIFY_CENTER);
    GtkWidget *vbox = gtk_dialog_get_content_area (GTK_DIALOG(switch_dialog));
    gtk_box_pack_start (GTK_BOX(vbox), gtk_label_new("\nUse switch to stop\n"), FALSE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX(vbox), message, FALSE, TRUE, 0);
    gtk_widget_show_all(switch_dialog);
    global_state.mode=SWITCH_RECORD;
    global_state.recording=ignore_signal=1;
    timeout_id = g_timeout_add (125, record_timeout, &global_state.recording);
    pthread_create(&switch_tid, NULL, record_thread, (void *)&global_state);
    }
    
  if (switch_state == 1 && state->mode == SWITCH_RECORD)
    {
    global_state.recording=ignore_signal=0;
    global_state.mode=NOT_RUNNING;
    g_source_remove (timeout_id);
    pthread_join(switch_tid, NULL);
    gtk_widget_destroy(switch_dialog);
    }
  return 1;
}

int main(int argc, char **argv)
{
 
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
  if (read_parms())
    {
    strcpy(iparms.url, "a.rtmp.youtube.com/live2/<key>");
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
    if (write_parms("wb", sizeof(iparms), &iparms)) printf("write failed\n");
    } 
  default_status(&global_state);
  parms_to_state(&global_state);
  
  gtk_init (&argc, &argv);
  
  install_signal_handlers();

  kb_xid = launch_keyboard();

  if (!kb_xid)
    {
      perror ("### 'matchbox-keyboard --xid', failed to return valid window ID. ### ");
      exit(-1);
    }   
  /*Main Window */
  main_win = gtk_window_new (GTK_WINDOW_TOPLEVEL);

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
  
  gtk_widget_show_all(main_win);

  gint timeout_id = g_timeout_add (125, main_timeout, &global_state);

  gtk_main();
  
  g_source_remove (timeout_id);
  
  if (gpio_init) {
    bcm2835_gpio_write(GPIO_LED, LOW);
    bcm2835_gpio_write(GPIO_MODEM_LED, LOW);
    bcm2835_gpio_write(GPIO_PWR_LED, LOW);
    bcm2835_close();}
    
  clean_files();

  kill(kbd_pid, 15);
  kill(sh_pid, 15);
  return 0;
}
