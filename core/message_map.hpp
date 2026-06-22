typedef struct {
	std::string prompt;
	std::string help;
	} msg;
	
static const std::map<std::string, msg> msgs_map =
{
	{ "ControlSelect", {"Select control", "Select control number to change"}},
	{ "CameraSelect", {"Use Camera ","Select camera to use"
		" in configuration with one of the following\nYes/Y/True/T/No/N/False/F"}},
	{ "ModeSelect", {"Select native sensor mode or Enter for none","Select native sensor mode to be used. Use largest format"
		" and bit depth in any stream."}},
	{ "OutputSize", {"Select camera output size","Select camera output resolution."}},
	{ "StreamSize", {"Select camera output size for stream 1","Select camera output resolution for stream 1."}},
	{ "StreamSize2", {"Select camera output size for stream 2","Select camera output resolution for stream 2."}},
	{ "SaveConfig", {"Save Configuration ","Save or over write configuration with your new selections"
		"\nYes/Y/True/T/No/N/False/F"}},
	{ "ClientID", {"Enter YouTube ClientID ","Enter ClientID associated with YouTube API"}},
	{ "ClientSecret", {"Enter YouTube Client Secret code ","Enter Client Secret code associated with YouTube API"}},
	{ "AudioString", {"Enter audio device name ", "The ALSA device name use \"add command here\" to list"}},
	{ "RawString", {"Enter raw file path and filename prefix ", "Raw file path and filename prefix to save raw stream. The date time stamp will be added to prefix."}},
	{ "CompositeString", {"Enter composite file path and filename prefix ", " composite file path and filename prefix to save composite stream. The date time stamp will be added to prefix."}},
	{ "YouTubeTitle", {"Enter YouTube title prefix ", "YouTube title prefix. The date time stamp will be added to prefix."}},
	{ "YouTubePublish", {"Enter YouTube Publish parameter ", "???."}},
	{ "UpdateCamera", {"Update camera ", "Yes or True to update camera."}},
	{ "AddCrop", {"Add crop value ", "Yes or True to add crop value."}},
	{ "UpdateCrop", {"Update crop value ", "Yes or True to update or delete crop value."}},
	{ "AddScale", {"Add scale value ", "Yes or True to add scale value."}},
	{ "UpdateScale", {"Update scale value ", "Yes or True to update or delete scale value."}},
	{ "AddOverlay", {"Add overlay value ", "Yes or True to add overlay value."}},
	{ "UpdateOverlay", {"Update overlay value ", "Yes or True to update or delete overlay value."}},
	{ "CropValue", {"Update crop value ", "???."}},
	{ "ScaleValue", {"Update scale value ", "???."}},
	{ "OverlayValue", {"Update overlay value ", "???."}},
	{ "DropStream", {"Drop stream from raw output ", "Yes or True to drop stream from raw output."}},
	{ "DropLayer", {"Drop layer from composite output ", "Yes or True to drop layer from composite output."}},
//	{ "AcceptFPS", {"Accept low FPS ","yes accept low fps or no reconfigure sensor"}},
	{ "GetBool", {"Enter boolean value for control  ","Enter boolean value for control\nYes/Y/True/T/No/N/False/F"}},
	{ "GetInt", {"Enter integer value for control ","Enter integer value for control."}},	
	{ "GetFloat", {"Enter decimal value for control ","Enter decimal value for control."}},
	{ "GetRect", {"Enter rectangle value for control ","Enter rectangle value for control."}},
	{ "SecondStream", {"Keep second stream ","Save or update second stream Yes/Y/True/T/No/N/False/F"}},
	{ "Compress", {"Compress stream ", "Compress stream Yes/Y/True/T/No/N/False/F"}},
	{ "AddBitRate", {"Add Bit Rate ","Yes or True to add Bit Rate"}},
	{ "UpdateBitRate", {"Update Bit Rate ","Save or update Bit Rate Yes/Y/True/T/No/N/False/F"}},
	{ "BitRate", {"Enter Bit Rate in Kbps ","Select Bit Rate ind Kbps. \n720p 30fps: 1,500 – 4,000 Kbps\n1080p 30fps: 3,000 – 6,000 Kbps\n4K (2160p) 30fps: 13,000 – 30,000 Kbps"}},
	{ "AddCRF", {"Add Constant Rate Factor ","Yes or True to add Constant Rate Factor"}},
	{ "UpdateCRF", {"Update Constant Rate Factor ","Save or update Constant Rate Factor Yes/Y/True/T/No/N/False/F"}},
	{ "CRF", {"Enter Constant Rate Factor ","Select Constant Rate Factor (default 23).\n0=lowest compression/highest quality \n51=highest compression/lowest quality"}},
	{ "BiteRate", {"Enter biterate in kbps ","Select biterate in kbps YouTube recommends 1500kbps to 6000kbps"}},
	{ "TestString", {"Enter test string ","help for test string"}}
	
};

