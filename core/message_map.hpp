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
	{ "SaveConfig", {"Save Configuration ","Save or over write configuration with your new selections"
		"\nYes/Y/True/T/No/N/False/F"}},
	{ "ClientID", {"Enter YouTube ClientID ","Enter ClientID associated with YouTube API"}},
	{ "ClientSecret", {"Enter YouTube Client Secret code ","Enter Client Secret code associated with YouTube API"}},
	{ "AudioString", {"Enter audio device name ", "The ALSA device name use \"add command here\" to list"}},
	{ "RawString", {"Enter raw file path and filename prefix ", "Raw file path and filename prefix to save raw stream. The date time stamp will be added to prefix."}},
	{ "CompositeString", {"Enter composite file path and filename prefix ", " composite file path and filename prefix to save composite stream. The date time stamp will be added to prefix."}},
	{ "YouTubeTitle", {"Enter YouTube title prefix ", "YouTube title prefix. The date time stamp will be added to prefix."}},
	{ "YouTubePublish", {"Enter YouTube Publish parameter ", "???."}},
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
	{ "DropLayer", {"Drop layer from raw output ", "Yes or True to drop layer from raw output."}},
	{ "AcceptFPS", {"Accept low FPS ","yes accept low fps or no reconfigure sensor"}},
	{ "TestString", {"Enter test string ","help for test string"}}
	
};

