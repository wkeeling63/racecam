#include "interface/mmal/util/mmal_util_params.h"

#include "raspiCamUtilities.h"
#include "racecamLogger.h"  

int mmal_status_to_int(MMAL_STATUS_T status)
{
   if (status == MMAL_SUCCESS)
      return 0;
   else
   {
      switch (status)
      {
      case MMAL_ENOMEM :
         log_error("Out of memory");
         break;
      case MMAL_ENOSPC :
         log_error("Out of resources (other than memory)");
         break;
      case MMAL_EINVAL:
         log_error("Argument is invalid");
         break;
      case MMAL_ENOSYS :
         log_error("Function not implemented");
         break;
      case MMAL_ENOENT :
         log_error("No such file or directory");
         break;
      case MMAL_ENXIO :
         log_error("No such device or address");
         break;
      case MMAL_EIO :
         log_error("I/O error");
         break;
      case MMAL_ESPIPE :
         log_error("Illegal seek");
         break;
      case MMAL_ECORRUPT :
         log_error("Data is corrupt \attention FIXME: not POSIX");
         break;
      case MMAL_ENOTREADY :
         log_error("Component is not ready \attention FIXME: not POSIX");
         break;
      case MMAL_ECONFIG :
         log_error("Component is not configured \attention FIXME: not POSIX");
         break;
      case MMAL_EISCONN :
         log_error("Port is already connected ");
         break;
      case MMAL_ENOTCONN :
         log_error("Port is disconnected");
         break;
      case MMAL_EAGAIN :
         log_error("Resource temporarily unavailable. Try again later");
         break;
      case MMAL_EFAULT :
         log_error("Bad address");
         break;
      default :
         log_error("Unknown status error");
         break;
      }

      return 1;
   }
} 

uint64_t get_microseconds64()
{
   struct timespec spec;
   uint64_t us;

   clock_gettime(CLOCK_MONOTONIC_RAW, &spec);

   us = spec.tv_sec * 1000000ULL;
   us += spec.tv_nsec / 1000;

   return us;
}

/**
 * Set the specified camera to all the specified settings
 * @param camera Pointer to camera component
 * @param params Pointer to parameter block containing parameters
 * @return 0 if successful, none-zero if unsuccessful.
 */
int raspicamcontrol_set_all_parameters(MMAL_COMPONENT_T *camera, const RASPICAM_CAMERA_PARAMETERS *params, int camera_type)
{
   int result;

   result  = raspicamcontrol_set_saturation(camera, params->saturation);
   result += raspicamcontrol_set_sharpness(camera, params->sharpness);
   result += raspicamcontrol_set_contrast(camera, params->contrast);
   result += raspicamcontrol_set_brightness(camera, params->brightness);
   result += raspicamcontrol_set_ISO(camera, params->ISO);
   result += raspicamcontrol_set_video_stabilisation(camera, params->videoStabilisation);
   result += raspicamcontrol_set_exposure_compensation(camera, params->exposureCompensation);
   result += raspicamcontrol_set_exposure_mode(camera, params->exposureMode);
   result += raspicamcontrol_set_flicker_avoid_mode(camera, params->flickerAvoidMode);
   result += raspicamcontrol_set_metering_mode(camera, params->exposureMeterMode);
   result += raspicamcontrol_set_awb_mode(camera, params->awbMode);
   result += raspicamcontrol_set_awb_gains(camera, params->awb_gains_r, params->awb_gains_b);
   result += raspicamcontrol_set_imageFX(camera, params->imageEffect);
   result += raspicamcontrol_set_colourFX(camera, &params->colourEffects);
   result += raspicamcontrol_set_rotation(camera, params->rotation);
   result += raspicamcontrol_set_flips(camera, params->hflip[camera_type], params->vflip[camera_type]);
   result += raspicamcontrol_set_ROI(camera, params->roi);
   result += raspicamcontrol_set_shutter_speed(camera, params->shutter_speed);
   result += raspicamcontrol_set_DRC(camera, params->drc_level);
   result += raspicamcontrol_set_stats_pass(camera, params->stats_pass);
   result += raspicamcontrol_set_gains(camera, params->analog_gain, params->digital_gain);

   if (params->settings)
   {
      MMAL_PARAMETER_CHANGE_EVENT_REQUEST_T change_event_request =
      {
         {MMAL_PARAMETER_CHANGE_EVENT_REQUEST, sizeof(MMAL_PARAMETER_CHANGE_EVENT_REQUEST_T)},
         MMAL_PARAMETER_CAMERA_SETTINGS, 1
      };

      MMAL_STATUS_T status = mmal_port_parameter_set(camera->control, &change_event_request.hdr);
      if ( status != MMAL_SUCCESS )
      {
         log_error("No camera settings events");
      }

      result += status;
   }

   return result;
}

/**
 * Adjust the saturation level for images
 * @param camera Pointer to camera component
 * @param saturation Value to adjust, -100 to 100
 * @return 0 if successful, non-zero if any parameters out of range
 */
int raspicamcontrol_set_saturation(MMAL_COMPONENT_T *camera, int saturation)
{
   int ret = 0;

   if (!camera)
      return 1;

   if (saturation >= -100 && saturation <= 100)
   {
      MMAL_RATIONAL_T value = {saturation, 100};
      ret = mmal_status_to_int(mmal_port_parameter_set_rational(camera->control, MMAL_PARAMETER_SATURATION, value));
   }
   else
   {
      log_error("Invalid saturation value");
      ret = 1;
   }

   return ret;
}

/**
 * Set the sharpness of the image
 * @param camera Pointer to camera component
 * @param sharpness Sharpness adjustment -100 to 100
 */
int raspicamcontrol_set_sharpness(MMAL_COMPONENT_T *camera, int sharpness)
{
   int ret = 0;

   if (!camera)
      return 1;

   if (sharpness >= -100 && sharpness <= 100)
   {
      MMAL_RATIONAL_T value = {sharpness, 100};
      ret = mmal_status_to_int(mmal_port_parameter_set_rational(camera->control, MMAL_PARAMETER_SHARPNESS, value));
   }
   else
   {
      log_error("Invalid sharpness value");
      ret = 1;
   }

   return ret;
}

/**
 * Set the contrast adjustment for the image
 * @param camera Pointer to camera component
 * @param contrast Contrast adjustment -100 to  100
 * @return
 */
int raspicamcontrol_set_contrast(MMAL_COMPONENT_T *camera, int contrast)
{
   int ret = 0;

   if (!camera)
      return 1;

   if (contrast >= -100 && contrast <= 100)
   {
      MMAL_RATIONAL_T value = {contrast, 100};
      ret = mmal_status_to_int(mmal_port_parameter_set_rational(camera->control, MMAL_PARAMETER_CONTRAST, value));
   }
   else
   {
      log_error("Invalid contrast value");
      ret = 1;
   }

   return ret;
}

/**
 * Adjust the brightness level for images
 * @param camera Pointer to camera component
 * @param brightness Value to adjust, 0 to 100
 * @return 0 if successful, non-zero if any parameters out of range
 */
int raspicamcontrol_set_brightness(MMAL_COMPONENT_T *camera, int brightness)
{
   int ret = 0;

   if (!camera)
      return 1;

   if (brightness >= 0 && brightness <= 100)
   {
      MMAL_RATIONAL_T value = {brightness, 100};
      ret = mmal_status_to_int(mmal_port_parameter_set_rational(camera->control, MMAL_PARAMETER_BRIGHTNESS, value));
   }
   else
   {
      log_error("Invalid brightness value");
      ret = 1;
   }

   return ret;
}

/**
 * Adjust the ISO used for images
 * @param camera Pointer to camera component
 * @param ISO Value to set TODO :
 * @return 0 if successful, non-zero if any parameters out of range
 */
int raspicamcontrol_set_ISO(MMAL_COMPONENT_T *camera, int ISO)
{
   if (!camera)
      return 1;

   return mmal_status_to_int(mmal_port_parameter_set_uint32(camera->control, MMAL_PARAMETER_ISO, ISO));
}

/**
 * Adjust the metering mode for images
 * @param camera Pointer to camera component
 * @param saturation Value from following
 *   - MMAL_PARAM_EXPOSUREMETERINGMODE_AVERAGE,
 *   - MMAL_PARAM_EXPOSUREMETERINGMODE_SPOT,
 *   - MMAL_PARAM_EXPOSUREMETERINGMODE_BACKLIT,
 *   - MMAL_PARAM_EXPOSUREMETERINGMODE_MATRIX
 * @return 0 if successful, non-zero if any parameters out of range
 */
int raspicamcontrol_set_metering_mode(MMAL_COMPONENT_T *camera, MMAL_PARAM_EXPOSUREMETERINGMODE_T m_mode )
{
   MMAL_PARAMETER_EXPOSUREMETERINGMODE_T meter_mode = {{MMAL_PARAMETER_EXP_METERING_MODE,sizeof(meter_mode)},
      m_mode
   };
   if (!camera)
      return 1;

   return mmal_status_to_int(mmal_port_parameter_set(camera->control, &meter_mode.hdr));
}


/**
 * Set the video stabilisation flag. Only used in video mode
 * @param camera Pointer to camera component
 * @param saturation Flag 0 off 1 on
 * @return 0 if successful, non-zero if any parameters out of range
 */
int raspicamcontrol_set_video_stabilisation(MMAL_COMPONENT_T *camera, int vstabilisation)
{
   if (!camera)
      return 1;

   return mmal_status_to_int(mmal_port_parameter_set_boolean(camera->control, MMAL_PARAMETER_VIDEO_STABILISATION, vstabilisation));
}

/**
 * Adjust the exposure compensation for images (EV)
 * @param camera Pointer to camera component
 * @param exp_comp Value to adjust, -10 to +10
 * @return 0 if successful, non-zero if any parameters out of range
 */
int raspicamcontrol_set_exposure_compensation(MMAL_COMPONENT_T *camera, int exp_comp)
{
   if (!camera)
      return 1;

   return mmal_status_to_int(mmal_port_parameter_set_int32(camera->control, MMAL_PARAMETER_EXPOSURE_COMP, exp_comp));
}


/**
 * Set exposure mode for images
 * @param camera Pointer to camera component
 * @param mode Exposure mode to set from
 *   - MMAL_PARAM_EXPOSUREMODE_OFF,
 *   - MMAL_PARAM_EXPOSUREMODE_AUTO,
 *   - MMAL_PARAM_EXPOSUREMODE_NIGHT,
 *   - MMAL_PARAM_EXPOSUREMODE_NIGHTPREVIEW,
 *   - MMAL_PARAM_EXPOSUREMODE_BACKLIGHT,
 *   - MMAL_PARAM_EXPOSUREMODE_SPOTLIGHT,
 *   - MMAL_PARAM_EXPOSUREMODE_SPORTS,
 *   - MMAL_PARAM_EXPOSUREMODE_SNOW,
 *   - MMAL_PARAM_EXPOSUREMODE_BEACH,
 *   - MMAL_PARAM_EXPOSUREMODE_VERYLONG,
 *   - MMAL_PARAM_EXPOSUREMODE_FIXEDFPS,
 *   - MMAL_PARAM_EXPOSUREMODE_ANTISHAKE,
 *   - MMAL_PARAM_EXPOSUREMODE_FIREWORKS,
 *
 * @return 0 if successful, non-zero if any parameters out of range
 */
int raspicamcontrol_set_exposure_mode(MMAL_COMPONENT_T *camera, MMAL_PARAM_EXPOSUREMODE_T mode)
{
   MMAL_PARAMETER_EXPOSUREMODE_T exp_mode = {{MMAL_PARAMETER_EXPOSURE_MODE,sizeof(exp_mode)}, mode};

   if (!camera)
      return 1;

   return mmal_status_to_int(mmal_port_parameter_set(camera->control, &exp_mode.hdr));
}


/**
 * Set flicker avoid mode for images
 * @param camera Pointer to camera component
 * @param mode Exposure mode to set from
 *   - MMAL_PARAM_FLICKERAVOID_OFF,
 *   - MMAL_PARAM_FLICKERAVOID_AUTO,
 *   - MMAL_PARAM_FLICKERAVOID_50HZ,
 *   - MMAL_PARAM_FLICKERAVOID_60HZ,
 *
 * @return 0 if successful, non-zero if any parameters out of range
 */
int raspicamcontrol_set_flicker_avoid_mode(MMAL_COMPONENT_T *camera, MMAL_PARAM_FLICKERAVOID_T mode)
{
   MMAL_PARAMETER_FLICKERAVOID_T fl_mode = {{MMAL_PARAMETER_FLICKER_AVOID,sizeof(fl_mode)}, mode};

   if (!camera)
      return 1;

   return mmal_status_to_int(mmal_port_parameter_set(camera->control, &fl_mode.hdr));
}


/**
 * Set the aWB (auto white balance) mode for images
 * @param camera Pointer to camera component
 * @param awb_mode Value to set from
 *   - MMAL_PARAM_AWBMODE_OFF,
 *   - MMAL_PARAM_AWBMODE_AUTO,
 *   - MMAL_PARAM_AWBMODE_SUNLIGHT,
 *   - MMAL_PARAM_AWBMODE_CLOUDY,
 *   - MMAL_PARAM_AWBMODE_SHADE,
 *   - MMAL_PARAM_AWBMODE_TUNGSTEN,
 *   - MMAL_PARAM_AWBMODE_FLUORESCENT,
 *   - MMAL_PARAM_AWBMODE_INCANDESCENT,
 *   - MMAL_PARAM_AWBMODE_FLASH,
 *   - MMAL_PARAM_AWBMODE_HORIZON,
 * @return 0 if successful, non-zero if any parameters out of range
 */
int raspicamcontrol_set_awb_mode(MMAL_COMPONENT_T *camera, MMAL_PARAM_AWBMODE_T awb_mode)
{
   MMAL_PARAMETER_AWBMODE_T param = {{MMAL_PARAMETER_AWB_MODE,sizeof(param)}, awb_mode};

   if (!camera)
      return 1;

   return mmal_status_to_int(mmal_port_parameter_set(camera->control, &param.hdr));
}

int raspicamcontrol_set_awb_gains(MMAL_COMPONENT_T *camera, float r_gain, float b_gain)
{
   MMAL_PARAMETER_AWB_GAINS_T param = {{MMAL_PARAMETER_CUSTOM_AWB_GAINS,sizeof(param)}, {0,0}, {0,0}};

   if (!camera)
      return 1;

   if (!r_gain || !b_gain)
      return 0;

   param.r_gain.num = (unsigned int)(r_gain * 65536);
   param.b_gain.num = (unsigned int)(b_gain * 65536);
   param.r_gain.den = param.b_gain.den = 65536;
   return mmal_status_to_int(mmal_port_parameter_set(camera->control, &param.hdr));
}

/**
 * Set the image effect for the images
 * @param camera Pointer to camera component
 * @param imageFX Value from
 *   - MMAL_PARAM_IMAGEFX_NONE,
 *   - MMAL_PARAM_IMAGEFX_NEGATIVE,
 *   - MMAL_PARAM_IMAGEFX_SOLARIZE,
 *   - MMAL_PARAM_IMAGEFX_POSTERIZE,
 *   - MMAL_PARAM_IMAGEFX_WHITEBOARD,
 *   - MMAL_PARAM_IMAGEFX_BLACKBOARD,
 *   - MMAL_PARAM_IMAGEFX_SKETCH,
 *   - MMAL_PARAM_IMAGEFX_DENOISE,
 *   - MMAL_PARAM_IMAGEFX_EMBOSS,
 *   - MMAL_PARAM_IMAGEFX_OILPAINT,
 *   - MMAL_PARAM_IMAGEFX_HATCH,
 *   - MMAL_PARAM_IMAGEFX_GPEN,
 *   - MMAL_PARAM_IMAGEFX_PASTEL,
 *   - MMAL_PARAM_IMAGEFX_WATERCOLOUR,
 *   - MMAL_PARAM_IMAGEFX_FILM,
 *   - MMAL_PARAM_IMAGEFX_BLUR,
 *   - MMAL_PARAM_IMAGEFX_SATURATION,
 *   - MMAL_PARAM_IMAGEFX_COLOURSWAP,
 *   - MMAL_PARAM_IMAGEFX_WASHEDOUT,
 *   - MMAL_PARAM_IMAGEFX_POSTERISE,
 *   - MMAL_PARAM_IMAGEFX_COLOURPOINT,
 *   - MMAL_PARAM_IMAGEFX_COLOURBALANCE,
 *   - MMAL_PARAM_IMAGEFX_CARTOON,
 * @return 0 if successful, non-zero if any parameters out of range
 */
int raspicamcontrol_set_imageFX(MMAL_COMPONENT_T *camera, MMAL_PARAM_IMAGEFX_T imageFX)
{
   MMAL_PARAMETER_IMAGEFX_T imgFX = {{MMAL_PARAMETER_IMAGE_EFFECT,sizeof(imgFX)}, imageFX};

   if (!camera)
      return 1;

   return mmal_status_to_int(mmal_port_parameter_set(camera->control, &imgFX.hdr));
}


/**
 * Set the colour effect  for images (Set UV component)
 * @param camera Pointer to camera component
 * @param colourFX  Contains enable state and U and V numbers to set (e.g. 128,128 = Black and white)
 * @return 0 if successful, non-zero if any parameters out of range
 */
int raspicamcontrol_set_colourFX(MMAL_COMPONENT_T *camera, const MMAL_PARAM_COLOURFX_T *colourFX)
{
   MMAL_PARAMETER_COLOURFX_T colfx = {{MMAL_PARAMETER_COLOUR_EFFECT,sizeof(colfx)}, 0, 0, 0};

   if (!camera)
      return 1;

   colfx.enable = colourFX->enable;
   colfx.u = colourFX->u;
   colfx.v = colourFX->v;

   return mmal_status_to_int(mmal_port_parameter_set(camera->control, &colfx.hdr));

}


/**
 * Set the rotation of the image
 * @param camera Pointer to camera component
 * @param rotation Degree of rotation (any number, but will be converted to 0,90,180 or 270 only)
 * @return 0 if successful, non-zero if any parameters out of range
 */
int raspicamcontrol_set_rotation(MMAL_COMPONENT_T *camera, int rotation)
{
   int ret;
   int my_rotation = ((rotation % 360 ) / 90) * 90;

   ret = mmal_port_parameter_set_int32(camera->output[0], MMAL_PARAMETER_ROTATION, my_rotation);
   mmal_port_parameter_set_int32(camera->output[1], MMAL_PARAMETER_ROTATION, my_rotation);
   mmal_port_parameter_set_int32(camera->output[2], MMAL_PARAMETER_ROTATION, my_rotation);

   return mmal_status_to_int(ret);
}

/**
 * Set the flips state of the image
 * @param camera Pointer to camera component
 * @param hflip If true, horizontally flip the image
 * @param vflip If true, vertically flip the image
 *
 * @return 0 if successful, non-zero if any parameters out of range
 */
int raspicamcontrol_set_flips(MMAL_COMPONENT_T *camera, int hflip, int vflip)
{
   MMAL_PARAMETER_MIRROR_T mirror = {{MMAL_PARAMETER_MIRROR, sizeof(MMAL_PARAMETER_MIRROR_T)}, MMAL_PARAM_MIRROR_NONE};

   if (hflip && vflip)
      mirror.value = MMAL_PARAM_MIRROR_BOTH;
   else if (hflip)
      mirror.value = MMAL_PARAM_MIRROR_HORIZONTAL;
   else if (vflip)
      mirror.value = MMAL_PARAM_MIRROR_VERTICAL;

   mmal_port_parameter_set(camera->output[0], &mirror.hdr);
   mmal_port_parameter_set(camera->output[1], &mirror.hdr);
   return mmal_status_to_int(mmal_port_parameter_set(camera->output[2], &mirror.hdr));
}

/**
 * Set the ROI of the sensor to use for captures/preview
 * @param camera Pointer to camera component
 * @param rect   Normalised coordinates of ROI rectangle
 *
 * @return 0 if successful, non-zero if any parameters out of range
 */
int raspicamcontrol_set_ROI(MMAL_COMPONENT_T *camera, PARAM_FLOAT_RECT_T rect)
{
   MMAL_PARAMETER_INPUT_CROP_T crop = {{MMAL_PARAMETER_INPUT_CROP, sizeof(MMAL_PARAMETER_INPUT_CROP_T)}};

   crop.rect.x = (65536 * rect.x);
   crop.rect.y = (65536 * rect.y);
   crop.rect.width = (65536 * rect.w);
   crop.rect.height = (65536 * rect.h);

   return mmal_status_to_int(mmal_port_parameter_set(camera->control, &crop.hdr));
}

/**
 * Adjust the exposure time used for images
 * @param camera Pointer to camera component
 * @param shutter speed in microseconds
 * @return 0 if successful, non-zero if any parameters out of range
 */
int raspicamcontrol_set_shutter_speed(MMAL_COMPONENT_T *camera, int speed)
{
   if (!camera)
      return 1;

   return mmal_status_to_int(mmal_port_parameter_set_uint32(camera->control, MMAL_PARAMETER_SHUTTER_SPEED, speed));
}

/**
 * Adjust the Dynamic range compression level
 * @param camera Pointer to camera component
 * @param strength Strength of DRC to apply
 *        MMAL_PARAMETER_DRC_STRENGTH_OFF
 *        MMAL_PARAMETER_DRC_STRENGTH_LOW
 *        MMAL_PARAMETER_DRC_STRENGTH_MEDIUM
 *        MMAL_PARAMETER_DRC_STRENGTH_HIGH
 *
 * @return 0 if successful, non-zero if any parameters out of range
 */
int raspicamcontrol_set_DRC(MMAL_COMPONENT_T *camera, MMAL_PARAMETER_DRC_STRENGTH_T strength)
{
   MMAL_PARAMETER_DRC_T drc = {{MMAL_PARAMETER_DYNAMIC_RANGE_COMPRESSION, sizeof(MMAL_PARAMETER_DRC_T)}, strength};

   if (!camera)
      return 1;

   return mmal_status_to_int(mmal_port_parameter_set(camera->control, &drc.hdr));
}

int raspicamcontrol_set_stats_pass(MMAL_COMPONENT_T *camera, int stats_pass)
{
   if (!camera)
      return 1;

   return mmal_status_to_int(mmal_port_parameter_set_boolean(camera->control, MMAL_PARAMETER_CAPTURE_STATS_PASS, stats_pass));
} 

int raspicamcontrol_set_stereo_mode(MMAL_PORT_T *port, MMAL_PARAMETER_STEREOSCOPIC_MODE_T *stereo_mode)
{
   MMAL_PARAMETER_STEREOSCOPIC_MODE_T stereo = { {MMAL_PARAMETER_STEREOSCOPIC_MODE, sizeof(stereo)},
      MMAL_STEREOSCOPIC_MODE_NONE, MMAL_FALSE, MMAL_FALSE
   };
   if (stereo_mode->mode != MMAL_STEREOSCOPIC_MODE_NONE)
   {
      stereo.mode = stereo_mode->mode;
      stereo.decimate = stereo_mode->decimate;
      stereo.swap_eyes = stereo_mode->swap_eyes;
   }
   return mmal_status_to_int(mmal_port_parameter_set(port, &stereo.hdr));
}

int raspicamcontrol_set_gains(MMAL_COMPONENT_T *camera, float analog, float digital)
{
   MMAL_RATIONAL_T rational = {0,65536};
   MMAL_STATUS_T status;

   if (!camera)
      return 1;

   rational.num = (unsigned int)(analog * 65536);
   status = mmal_port_parameter_set_rational(camera->control, MMAL_PARAMETER_ANALOG_GAIN, rational);
   if (status != MMAL_SUCCESS)
      return mmal_status_to_int(status);

   rational.num = (unsigned int)(digital * 65536);
   status = mmal_port_parameter_set_rational(camera->control, MMAL_PARAMETER_DIGITAL_GAIN, rational);
   return mmal_status_to_int(status);
}

/** Default camera callback function
 * Handles the --settings
 * @param port
 * @param Callback data
 */
void default_camera_control_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
   log_error("Camera control callback  cmd=0x%08x", buffer->cmd);

   if (buffer->cmd == MMAL_EVENT_PARAMETER_CHANGED)
   {
      MMAL_EVENT_PARAMETER_CHANGED_T *param = (MMAL_EVENT_PARAMETER_CHANGED_T *)buffer->data;
      switch (param->hdr.id)
      {
      case MMAL_PARAMETER_CAMERA_SETTINGS:
      {
         MMAL_PARAMETER_CAMERA_SETTINGS_T *settings = (MMAL_PARAMETER_CAMERA_SETTINGS_T*)param;
         log_error("Exposure now %u, analog gain %u/%u, digital gain %u/%u",
                        settings->exposure,
                        settings->analog_gain.num, settings->analog_gain.den,
                        settings->digital_gain.num, settings->digital_gain.den);
         log_error("AWB R=%u/%u, B=%u/%u",
                        settings->awb_red_gain.num, settings->awb_red_gain.den,
                        settings->awb_blue_gain.num, settings->awb_blue_gain.den);
      }
      break;
      }
   }
   else if (buffer->cmd == MMAL_EVENT_ERROR)
   {
      log_error("No data received from sensor. Check all connections, including the Sunny one on the camera board");
   }
   else
   {
      log_error("Received unexpected camera control callback event, 0x%08x", buffer->cmd);
   }

   mmal_buffer_header_release(buffer);
}
