Purpose: To build and front and rear facing live stream race camera with GUI

Description racecam: Creates a 2 camera components and tunnels them to the 
Hardware Video Scaler (HVS) component to overlay the second camera 
frame on the frame of the main camera. The HVS output is tunneled to 
the encoder to create a video H.264 stream.  The video stream is 
created using MMAL api and run on the GPU.  The audio stream is 
created from ALSA api using Adafruit I2S MEMS Microphone 
(SPH0645LM4H).  The audio stream is encoded to ACC using FFPMEG api 
and both streams are added to flash video container by FFMPEG api.


Install: cmake .

Custom keyboard layout (keyboard-rc.xml) must be copied to /usr/share/matchbox-keyboard as root after matchbox-keyboard install of change racecam.c to use a standard layout (fi). 

Software required:
* GTK+2.0 sudo apt-get install libgtk2.0-dev 
* matchbox keyboard sudo apt-get install matchbox-keyboard
* FFMPEG library sudo apt-get install ffmpeg
* bcm2835 library http://www.airspayce.com/mikem/bcm2835/index.html
* Cairo library sudo apt-get install libcairo2-dev

Hardware required:
* 2 Camera Raspberry Pi Compute Module 
	(with carrier board - Raspberry Pi CM3/CM4 I/O board or WaveShare POE board)
* CSI touch display 
* Adafruit I2S MEMS Microphone (SPH0645LM4H)
* SixFab Raspberry Pi 3G/4G & LTE Base HAT (for Cellular modem and GPS)

