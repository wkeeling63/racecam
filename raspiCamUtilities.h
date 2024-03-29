#ifndef RASPICAMCONTROL_H_
#define RASPICAMCONTROL_H_

#define MAX_NUMBER_OF_CAMERAS 2
#define MAIN_CAMERA 0
#define OVERLAY_CAMERA 1

// There isn't actually a MMAL structure for the following, so make one
typedef struct mmal_param_colourfx_s
{
   int enable;       /// Turn colourFX on or off
   int u,v;          /// U and V to use
} MMAL_PARAM_COLOURFX_T; 

typedef struct mmal_param_thumbnail_config_s
{
   int enable;
   int width,height;
   int quality;
} MMAL_PARAM_THUMBNAIL_CONFIG_T; 

typedef struct param_float_rect_s
{
   double x;
   double y;
   double w;
   double h;
} PARAM_FLOAT_RECT_T; 

/// struct contain camera settings
typedef struct raspicam_camera_parameters_s
{
   int sharpness;             /// -100 to 100
   int contrast;              /// -100 to 100
   int brightness;            ///  0 to 100
   int saturation;            ///  -100 to 100
   int ISO;                   ///  TODO : what range?
   int videoStabilisation;    /// 0 or 1 (false or true)
   int exposureCompensation;  /// -10 to +10 ?
   MMAL_PARAM_EXPOSUREMODE_T exposureMode;
   MMAL_PARAM_EXPOSUREMETERINGMODE_T exposureMeterMode;
   MMAL_PARAM_AWBMODE_T awbMode;
   MMAL_PARAM_IMAGEFX_T imageEffect;
   MMAL_PARAMETER_IMAGEFX_PARAMETERS_T imageEffectsParameters;
   MMAL_PARAM_COLOURFX_T colourEffects;
   MMAL_PARAM_FLICKERAVOID_T flickerAvoidMode;
   int rotation;              /// 0-359
   int hflip[MAX_NUMBER_OF_CAMERAS];                 /// 0 or 1
   int vflip[MAX_NUMBER_OF_CAMERAS];                 /// 0 or 1
   PARAM_FLOAT_RECT_T  roi;   /// region of interest to use on the sensor. Normalised [0,1] values in the rect
   int shutter_speed;         /// 0 = auto, otherwise the shutter speed in ms
   float awb_gains_r;         /// AWB red gain
   float awb_gains_b;         /// AWB blue gain
   MMAL_PARAMETER_DRC_STRENGTH_T drc_level;  // Strength of Dynamic Range compression to apply
   MMAL_BOOL_T stats_pass;    /// Stills capture statistics pass on/off
   int enable_annotate;       /// Flag to enable the annotate, 0 = disabled, otherwise a bitmask of what needs to be displayed
   char annotate_string[MMAL_CAMERA_ANNOTATE_MAX_TEXT_LEN_V2]; /// String to use for annotate - overrides certain bitmask settings
   int annotate_text_size;    // Text size for annotation
   int annotate_text_colour;  // Text colour for annotation
   int annotate_bg_colour;    // Background colour for annotation
 
   MMAL_PARAMETER_STEREOSCOPIC_MODE_T stereo_mode;
   float analog_gain;         // Analog gain
   float digital_gain;        // Digital gain

   int settings;
} RASPICAM_CAMERA_PARAMETERS;

typedef enum
{
   ZOOM_IN, ZOOM_OUT, ZOOM_RESET
} ZOOM_COMMAND_T;

/// Cross reference structure, mode string against mode id
typedef struct xref_t
{
   char *mode;
   int mmal_mode;
} XREF_T; 

int raspicli_map_xref(const char *str, const XREF_T *map, int num_refs);
const char *raspicli_unmap_xref(const int en, XREF_T *map, int num_refs);

int mmal_status_to_int(MMAL_STATUS_T status);
uint64_t get_microseconds64();

int raspicamcontrol_set_all_parameters(MMAL_COMPONENT_T *camera, const RASPICAM_CAMERA_PARAMETERS *params, int camera_type);

// Individual setting functions
int raspicamcontrol_set_saturation(MMAL_COMPONENT_T *camera, int saturation);
int raspicamcontrol_set_sharpness(MMAL_COMPONENT_T *camera, int sharpness);
int raspicamcontrol_set_contrast(MMAL_COMPONENT_T *camera, int contrast);
int raspicamcontrol_set_brightness(MMAL_COMPONENT_T *camera, int brightness);
int raspicamcontrol_set_ISO(MMAL_COMPONENT_T *camera, int ISO);
int raspicamcontrol_set_metering_mode(MMAL_COMPONENT_T *camera, MMAL_PARAM_EXPOSUREMETERINGMODE_T mode);
int raspicamcontrol_set_video_stabilisation(MMAL_COMPONENT_T *camera, int vstabilisation);
int raspicamcontrol_set_exposure_compensation(MMAL_COMPONENT_T *camera, int exp_comp);
int raspicamcontrol_set_exposure_mode(MMAL_COMPONENT_T *camera, MMAL_PARAM_EXPOSUREMODE_T mode);
int raspicamcontrol_set_flicker_avoid_mode(MMAL_COMPONENT_T *camera, MMAL_PARAM_FLICKERAVOID_T mode);
int raspicamcontrol_set_awb_mode(MMAL_COMPONENT_T *camera, MMAL_PARAM_AWBMODE_T awb_mode);
int raspicamcontrol_set_awb_gains(MMAL_COMPONENT_T *camera, float r_gain, float b_gain);
int raspicamcontrol_set_imageFX(MMAL_COMPONENT_T *camera, MMAL_PARAM_IMAGEFX_T imageFX);
int raspicamcontrol_set_colourFX(MMAL_COMPONENT_T *camera, const MMAL_PARAM_COLOURFX_T *colourFX);
int raspicamcontrol_set_rotation(MMAL_COMPONENT_T *camera, int rotation);
int raspicamcontrol_set_flips(MMAL_COMPONENT_T *camera, int hflip, int vflip);
int raspicamcontrol_set_ROI(MMAL_COMPONENT_T *camera, PARAM_FLOAT_RECT_T rect);
int raspicamcontrol_zoom_in_zoom_out(MMAL_COMPONENT_T *camera, ZOOM_COMMAND_T zoom_command, PARAM_FLOAT_RECT_T *roi);
int raspicamcontrol_set_shutter_speed(MMAL_COMPONENT_T *camera, int speed_ms);
int raspicamcontrol_set_DRC(MMAL_COMPONENT_T *camera, MMAL_PARAMETER_DRC_STRENGTH_T strength);
int raspicamcontrol_set_stats_pass(MMAL_COMPONENT_T *camera, int stats_pass);
int raspicamcontrol_set_stereo_mode(MMAL_PORT_T *port, MMAL_PARAMETER_STEREOSCOPIC_MODE_T *stereo_mode);
int raspicamcontrol_set_gains(MMAL_COMPONENT_T *camera, float analog, float digital);

//Individual getting functions
int raspicamcontrol_get_saturation(MMAL_COMPONENT_T *camera);
int raspicamcontrol_get_sharpness(MMAL_COMPONENT_T *camera);
int raspicamcontrol_get_contrast(MMAL_COMPONENT_T *camera);
int raspicamcontrol_get_brightness(MMAL_COMPONENT_T *camera);
int raspicamcontrol_get_ISO(MMAL_COMPONENT_T *camera);
MMAL_PARAM_EXPOSUREMETERINGMODE_T raspicamcontrol_get_metering_mode(MMAL_COMPONENT_T *camera);
int raspicamcontrol_get_video_stabilisation(MMAL_COMPONENT_T *camera);
int raspicamcontrol_get_exposure_compensation(MMAL_COMPONENT_T *camera);
MMAL_PARAM_THUMBNAIL_CONFIG_T raspicamcontrol_get_thumbnail_parameters(MMAL_COMPONENT_T *camera);
MMAL_PARAM_EXPOSUREMODE_T raspicamcontrol_get_exposure_mode(MMAL_COMPONENT_T *camera);
MMAL_PARAM_FLICKERAVOID_T raspicamcontrol_get_flicker_avoid_mode(MMAL_COMPONENT_T *camera);
MMAL_PARAM_AWBMODE_T raspicamcontrol_get_awb_mode(MMAL_COMPONENT_T *camera);
MMAL_PARAM_IMAGEFX_T raspicamcontrol_get_imageFX(MMAL_COMPONENT_T *camera);
MMAL_PARAM_COLOURFX_T raspicamcontrol_get_colourFX(MMAL_COMPONENT_T *camera);
MMAL_STEREOSCOPIC_MODE_T stereo_mode_from_string(const char *str);

/** Default camera callback function
  */
void default_camera_control_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);

#endif /* RASPICAMCONTROL_H_ */
