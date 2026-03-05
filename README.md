# WPC RaceCam 
**Overview of purpose:**  This project is to capture multiple cameras and an audio stream and output to up to 2 containers. One with composite video (selected cameras are overlayed into one stream) and/or one raw video (each selected camera to its own video stream). The composite container is envisioned to be a live stream (although it can be written to local file), and the raw container is envisioned to be used to record the streams to be used in post-race production. This is designed to run only on Pi5 (as only the Pi5 and CM4 support multiple cameras, and the CM4 with a carrier board would be more expensive than Pi5 that was ruled out).

**Flow of capture:** Two APIs are used in this program to process video and audio streams; libcamera open-source camera stack to manage the camera and deliver video frames to the program and FFmpeg API to manipulate (pixel conversion, crop, scale, encode and mux) video frames. All audio processing (capture from ALSA, encoding and muxing) uses FFmpeg API. Cameras can be connected to the Pi in two ways; directly thru CSI-2 interface/connectors and thru UVC (USB Video Class). For the directly attached Raspberry Pi CSI-2 cameras libcamera use the Pi hardware ISP (Image Signal Processor) and UVC cameras uses software image processing. It is more efficient for CSI camera to perform image processing (pixel format change, cropping and scaling) in the libcamera pipeline before frames are sent to FFmpeg. If two output containers are selected the video frames are duplicated, can be crop, and scaled differently.  For CSI cameras this cropping and scaling should be done by libcamera so that hardware ISP is used (setup when in camera configuration). This must be done by FFmpeg in software UVC cameras (setup in the configuration of layers (in composite) and streams (in raw) outputs. The racecam-config program has no intelligence built into it this gives you the freedom to configure the capture, processing, and containerization of the streams anyway you like (and overload the finite compute resources of the Pi). So test any configuration and any process like cropping and scaling that can be done 2 places for CSI camera should be prioritized to the camera/libcamera not FFmpeg.  The composite output has the ability to upload to YouTube.  If you do not need to develop code I would recommend Pi OS Lite as all the programs a designed with text based UI.

**Three programs:**
racecamd – daemon capture process that catches all exception. Starts and stop capture thru GPIO switch. And can be run with systemctl (see setup).  
racecam – command line capture process that will end on any unhandled exception and runs one capture for the number of second in the command line parameters.  
racecam-config – builds the JSON file that defines the way the capture programs (racecam and racecamd) run. The program has little validation built in (if it does not crash the program is it allowed however little since it makes). It also allows you to select a configuration that is beyond the Pi5 processing powers (lots of cameras running at high frame rates, resolutions and all being encoded in H264 compression).   

**Build instructions(draft):**

**Update package repo and update any packages:**
sudo apt update  
sudo apt full-upgrade  
sudo reboot now  

**Install needed packages:**
sudo apt install -y libyaml-cpp-dev libboost-json-dev libcurl4-openssl-dev libcurlpp-dev  
sudo apt install -y libcamera-dev libepoxy-dev libjpeg-dev libtiff5-dev libpng-dev libopencv-dev (remove those not needed  epoxy, jpeg, tiff png and opencv)  
sudo apt install -y libavcodec-dev libavdevice-dev libavformat-dev libswresample-dev  

**For Pi OS Lite also:**
sudo apt install -y git  
sudo apt install -y meson cmake   

**For cell phone tethering:**
sudo apt install usbmuxd libimobiledevice-utils ipheth-utils -y  

**Download, configure and build:**
git clone https://github.com/wkeeling63/racecam.git  

**reconfigure meson for new version:**
meson setup --reconfigure build  

**Build instructions:**
meson setup build  
sudo meson install -C build  
sudo ldconfig //if lib not found  

**Setup udev rule:**
sudo groupadd racecam 
sudo usermod -aG racecam <USERNAME>
create /etc/udev/rules.d/99-leds.rules with the following
ACTION=="add", KERNEL=="LED1", SUBSYSTEM=="leds", RUN+="/bin/chown -R root:racecam /sys%p", RUN+="/bin/chmod -R g+rw /sys%p"  

**Recommended hardware:**
Pi5 2gb (with optional  RTC battery and active cooler)  
NVMe hat+ and SSD 512GB or 1TB  
WPC RaceCam board (I am making these on request for $20 labor plus parts currently $20) I will be posting the KiCad file, BOM and everything you need to build your own in another repository shortly.  
CSI cameras and cables (I have tested with Pi V2 cameras but should work with any Pi cameras.  The [UVC camera tested](https://www.digikey.com/en/products/detail/dfrobot/FIT0701/13166487) was but most UVC cameras should work.  

