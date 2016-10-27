#include <stdlib.h>
#include <ctime>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <fstream>
#include <wiringPi.h>
#include <SFML/Audio.hpp>
#include <stdio.h>
#include <unistd.h>
#include "camera.h"
#include "graphics.h"
#include "png.h"

// for camera
#define MAIN_TEXTURE_WIDTH 512
#define MAIN_TEXTURE_HEIGHT 512

// raspicam v2 capable of 3280 x 2464?
#define CAMERA_WIDTH 1280
#define CAMERA_HEIGHT 720

#define CAMERA_OUTPUT_FOLDER "./output/"

// for camera
//char tmpbuff[MAIN_TEXTURE_WIDTH*MAIN_TEXTURE_HEIGHT*4];
//should the camera convert frame data from yuv to argb automatically?
bool showimage = false;
bool do_argb_conversion = true;
GfxTexture texture;

//proto functions
bool isValidPin(int tpin);

enum GPIO_P {PIN0, PIN1, PIN2, PIN3, PIN4, PIN5, PIN6, PIN7, PINCOUNT};

enum _EVENT_TYPES{EVENT_RELAY, EVENT_DELAY, EVENT_MOTION, EVENT_SOUND_PLAY, EVENT_SOUND_WAIT, EVENT_SOUND_STOP,
	 EVENT_PRINT, EVENT_LABEL, EVENT_GOTO, EVENT_TAKEPICTURE, EVENT_SHOWFEED};

//sound files
sf::SoundBuffer mysoundbuf;
sf::Sound mysound;

std::string pfilename;

// camera
CCamera *cam = NULL;

//struct for relay timing and condition
struct rstat
{
	unsigned int time;
	bool on;
};


struct soundObject
{
	sf::SoundBuffer soundbuf;
	sf::Sound sound;
	std::string fname;
};

class sevent
{
	public:

	sevent(int etype);
	~sevent();

	int eventType;
	int pin;
	int value;
	std::string estring;
	soundObject *soundobj;
};

sevent::sevent(int etype)
{
	eventType = etype;
	soundobj = NULL;
}

sevent::~sevent()
{
	if(soundobj != NULL) delete soundobj;
}


//create banks for each relay
std::vector< std::vector<rstat> > relays;

void threadtest()
{
	std::cout << "I'm running a thread!\n";
}

void initCamera()
{
	//cam = StartCamera(MAIN_TEXTURE_WIDTH, MAIN_TEXTURE_HEIGHT,30,1 /*numlevels*/,do_argb_conversion);
	cam = StartCamera(CAMERA_WIDTH, CAMERA_HEIGHT,30,1,do_argb_conversion);
}

std::string getTimeAndDate()
{
    std::stringstream timess;
    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);

    timess << (timeinfo->tm_year -100 + 2000) << timeinfo->tm_mon + 1 << timeinfo->tm_mday <<
        "_" << timeinfo->tm_hour << "_" << timeinfo->tm_min << "_" << timeinfo->tm_sec;

    return timess.str();	
	
}

bool save_png_libpng(const char *filename, char *pixels, int w, int h)
{
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png)
        return false;

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_write_struct(&png, &info);
        return false;
    }

    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        png_destroy_write_struct(&png, &info);
        return false;
    }

    png_init_io(png, fp);
    png_set_IHDR(png, info, w, h, 8 /* depth */, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
    png_colorp palette = (png_colorp)png_malloc(png, PNG_MAX_PALETTE_LENGTH * sizeof(png_color));
    if (!palette) {
        fclose(fp);
        png_destroy_write_struct(&png, &info);
        return false;
    }
    png_set_PLTE(png, info, palette, PNG_MAX_PALETTE_LENGTH);
    png_write_info(png, info);
    png_set_packing(png);

    png_bytepp rows = (png_bytepp)png_malloc(png, h * sizeof(png_bytep));
    for (int i = 0; i < h; ++i)
        rows[i] = (png_bytep)(pixels + (h - i) * w * 4);

    png_write_image(png, rows);
    png_write_end(png, info);
    png_free(png, palette);
    png_destroy_write_struct(&png, &info);

    fclose(fp);
    delete[] rows;
    return true;
}


void snapPicture()
{
	//has camera been initialized?
	if(cam == NULL) return;
	
	char raw_cam[CAMERA_WIDTH * CAMERA_HEIGHT * 4];
	char raw_for_texture[MAIN_TEXTURE_WIDTH * MAIN_TEXTURE_HEIGHT * 4];
	int frame_sz;
	
	//capture camera frame
	cam->ReadFrame(0, raw_cam, sizeof(raw_cam));
	cam->ReadFrame(0, raw_for_texture, sizeof(raw_for_texture));
	
	// save picture to png format
	std::string savefile( CAMERA_OUTPUT_FOLDER + getTimeAndDate() + ".png");
	save_png_libpng(savefile.c_str(), raw_cam, CAMERA_WIDTH, CAMERA_HEIGHT);
	

	// able to toggle whether or not to show image
	if(showimage)
	{
		// create gl texture from camera data
		texture.SetPixels(raw_for_texture);
		
		//begin frame, draw the texture then end frame (the bit of maths just fits the image to the screen while maintaining aspect ratio)
		BeginFrame();
		float aspect_ratio = float(MAIN_TEXTURE_WIDTH)/float(MAIN_TEXTURE_HEIGHT);
		float screen_aspect_ratio = 1280.f/720.f;
		DrawTextureRect(&texture,-aspect_ratio/screen_aspect_ratio,-1.f,aspect_ratio/screen_aspect_ratio,1.f);
		EndFrame();	
		
		ReleaseGraphics();			
	}

	
	/*
	//has camera been initialized?
	if(cam == NULL) return;
	
	const void* frame_data;
	int frame_sz;
	if(cam->BeginReadFrame(0, frame_data, frame_sz) )
	{
		if(do_argb_conversion)
		{
			//if doing argb conversion the frame data will be exactly the right size so just set directly
			texture.SetPixels(frame_data);
			save_png_libpng("test.png", frame_data, MAIN_TEXTURE_WIDTH, MAIN_TEXTURE_HEIGHT);
		}
		else
		{
			//if not converting argb the data will be the wrong size and look weird, put copy it in
			//via a temporary buffer just so we can observe something happening!
			memcpy(tmpbuff,frame_data,frame_sz);
			texture.SetPixels(tmpbuff);
		}
		cam->EndReadFrame(0);
	}
	
	//begin frame, draw the texture then end frame (the bit of maths just fits the image to the screen while maintaining aspect ratio)
	BeginFrame();
	float aspect_ratio = float(MAIN_TEXTURE_WIDTH)/float(MAIN_TEXTURE_HEIGHT);
	float screen_aspect_ratio = 1280.f/720.f;
	DrawTextureRect(&texture,-aspect_ratio/screen_aspect_ratio,-1.f,aspect_ratio/screen_aspect_ratio,1.f);
	EndFrame();	
	
	ReleaseGraphics();
	*/
}

void showFeed(int feedtime)
{
	//has camera been initialized?
	if(cam!= NULL)
	{
		// stop active camera
		StopCamera();
		cam = NULL;
	}
	
	CCamera *camfeed = StartCamera(MAIN_TEXTURE_WIDTH, MAIN_TEXTURE_HEIGHT,30,1,do_argb_conversion);
	
	time_t starttime = time(NULL);
	
	while(time(NULL) <= starttime + feedtime)
	{
		char raw_for_texture[MAIN_TEXTURE_WIDTH * MAIN_TEXTURE_HEIGHT * 4];
		
		//capture camera frame
		camfeed->ReadFrame(0, raw_for_texture, sizeof(raw_for_texture));

			// create gl texture from camera data
			texture.SetPixels(raw_for_texture);
			
			//begin frame, draw the texture then end frame (the bit of maths just fits the image to the screen while maintaining aspect ratio)
			BeginFrame();
			float aspect_ratio = float(MAIN_TEXTURE_WIDTH)/float(MAIN_TEXTURE_HEIGHT);
			float screen_aspect_ratio = 1280.f/720.f;
			DrawTextureRect(&texture,-aspect_ratio/screen_aspect_ratio,-1.f,aspect_ratio/screen_aspect_ratio,1.f);
			EndFrame();	
			
			ReleaseGraphics();			

	}
	
	StopCamera();
	camfeed = NULL;
	
	// reinit main camera
	initCamera();
	
}


void hitEnter()
{
	std::string buf;

	std::cout << "<Hit Enter to Continue>\n";
	std::getline(std::cin, buf);
	//std::cin.clear();
	//std::cin.ignore();

}

void pinOn(int tpin)
{

	//if pin is out of bounds, ignore and print message
	if(!isValidPin(tpin) )
	{
		std::cout << "Invalid GPIO pin (#0-7)\n";
		hitEnter();
		return;
	}

	digitalWrite(tpin, LOW);

	return;

}

void pinOff(int tpin)
{


	//if pin is out of bounds, ignore and print message
	if( !isValidPin(tpin) )
	{
		std::cout << "Invalid GPIO pin (#0-7)\n";
		hitEnter();
		return;
	}

	digitalWrite(tpin, HIGH);

	return;


}

void configAllPins()
{
	//walk through each pin and set all 8 as outputs
	for(int i = 0; i < PINCOUNT; i++)
	{
		pinMode(i, OUTPUT);
	}

	std::cout << "All pins (0-" << PINCOUNT-1 << ") configured for OUTPUT\n";
}

void allOn()
{
	for(int i = 0; i < PINCOUNT; i++) pinOn(i);
}

void allOff()
{
	for(int i = 0; i < PINCOUNT; i++) pinOff(i);
}

bool isValidPin(int tpin)
{
	//check if pin is valid, if not , return false
	if( tpin < 0 || tpin >= PINCOUNT) return false;

	else return true;

}

void resetAll()
{
	//wiring pi init
	wiringPiSetup();
	//used to reset clock
	//wiringPiSetupSys();

	//config all pins
	//configAllPins();

	//turn all pins off
	//allOff();

	//reset system clock
	//wiringPiSetupSys();
}

void testPattern()
{

	//get desired speed in ms
	std::cout << "Enter test pattern speed (ms):";
	int speed = 0;
	std::cin >> speed;

	//demo delay
	std::string ddelay;
	std::cout << "Enter time delay (ms) before test pattern runs <enter for no delay>:";
	std::cin >> ddelay;
	std::cin.clear();
	std::cin.ignore(1000,'\n');

	if(ddelay[0] != '\n') delay( atoi(ddelay.c_str()) );

	//all pins off
	allOff();

	//walk lights back and forth then flash several times
	for(int i = 0; i <  PINCOUNT; i++)
	{
		pinOn(i);
		std::cout << i << "=ON\n";
		delay(speed);
		pinOff(i);
		std::cout << i << "=OFF\n";
	}

	for(int i = PINCOUNT-2; i >= 0; i--)
	{
		pinOn(i);
		std::cout << i << "=ON\n";
		delay(speed);
		pinOff(i);
		std::cout << i << "=OFF\n";
		delay(speed);
	}

	//walk lights in pair
	for(int i = 0; i < PINCOUNT; i++)
	{
		int offset = i + 2;
		if(offset >= PINCOUNT)
		{
			offset = offset - PINCOUNT;
		}

		pinOn(i);
		pinOn(offset);

		std::cout << i << " & " << offset << " are on\n";
		delay(speed);

		pinOff(i);
		pinOff(offset);

	}

	//flash on and off a couple times
	for(int i = 0; i < 3; i++)
	{
		allOn();
		std::cout << "All ON\n";
		delay(speed);
		allOff();
		std::cout << "All OFF\n";
		delay(speed);
	}

	std::cout << "Test pattern finished..\n";
	hitEnter();
}

void motionTest()
{

	//reset clock
	resetAll();
	

	digitalWrite(PIN7, 0);
	pinMode(PIN7, INPUT);
	bool motiondetected = false;



	//pinOn(PIN7);
	//pinMode(PIN7, INPUT);
	//pullUpDnControl(int pin, int pud) pud = PUD_UP (pull to 3.3v,  PUD_DOWN - ground, or PUD_OFF)
	//pullUpDnControl(PIN7, PUD_DOWN);
	//pinOff(PIN7);

	std::cout << "MOTION SENSOR TEST....\n\n";

	//run test for 30 seconds
	while(millis()/1000 < 30)
	{
		bool pinhigh = digitalRead(PIN7);

		if(pinhigh && !motiondetected)
		{
			std::cout << "motion detected\n";
			motiondetected = true;
		}
		else if(!pinhigh && motiondetected)
		{
			std::cout << "....................motion stopped\n";
			motiondetected = false;
		}
	}
}

void clearRelays()
{
	for(int i = 0; i < int(relays.size()); i++) relays[i].clear();
}

//this function will find the latest relay timing and use that as the terminator
unsigned int getLastRelay()
{
	//timeFound will store the latest timing found
	unsigned int timeFound = 0;

	for(int i = 0; i < int(relays.size()); i++)
	{
		for(int n = 0; n < int(relays[i].size()); n++)
		{
			if(relays[i][n].time > timeFound) timeFound = relays[i][n].time;
		}
	}

	return timeFound;
}

void soundTest()
{
	mysound.Play();
}

void runScript(std::string fname)
{
	//reset all pins
	resetAll();

	//list of events
	std::vector<sevent*> events;

	//parse in file to events
	std::ifstream ifile;
	ifile.open(fname.c_str());

	if(!ifile.is_open())
	{
		std::cout << "Error opening script " << fname << std::endl;
		hitEnter();
		return;
	}

	//parse file
	while(!ifile.eof())
	{
		//get line from file
		std::string buf;
		std::getline(ifile, buf);

		//ignore lines that might be used for commenting
		if(buf[0] == '#' || buf[0] == '\n' || buf[0] == ' ' || buf[0] == '\0') continue;

		//separate cmd and values
		std::string cmd( buf.substr(0, buf.find_first_of(':')) );
		std::string param( buf.substr(buf.find_first_of(':')+1, buf.size()-1) );

		//create wait for motion event on pin
		if(cmd == "WAITONMOTION")
		{
			sevent *newevent = new sevent(EVENT_MOTION);

			newevent->pin = atoi(param.c_str());
			events.push_back(newevent);

			
		}
		//load an play sound file
		else if(cmd == "PLAYSOUND")
		{
			sevent *newevent = new sevent(EVENT_SOUND_PLAY);
			
			newevent->soundobj = new soundObject;
			newevent->soundobj->soundbuf.LoadFromFile(param);
			newevent->soundobj->sound = sf::Sound(newevent->soundobj->soundbuf);
			events.push_back(newevent);
		}
		else if(cmd == "WAITONSOUND")
		{
			sevent *newevent = new sevent(EVENT_SOUND_WAIT);
			
			events.push_back(newevent);
		}
		else if(cmd == "STOPALLSOUND")
		{
			sevent *newevent = new sevent(EVENT_SOUND_STOP);

			events.push_back(newevent);
		}
		else if(cmd == "PRINT")
		{
			sevent *newevent = new sevent(EVENT_PRINT);

			newevent->estring = param;
			events.push_back(newevent);
		}
		else if(cmd == "DELAY")
		{
			sevent *newevent = new sevent(EVENT_DELAY);

			newevent->value = atoi(param.c_str());
			events.push_back(newevent);
		}
		else if(cmd == "RELAY")
		{
			sevent *newevent = new sevent(EVENT_RELAY);

			int rpin = atoi(param.substr(0, param.find(',')).c_str());
			param.erase(0, param.find(',')+1);
			int rstate = atoi(param.substr(0, param.find(',')).c_str());
			newevent->pin = rpin;
			newevent->value = rstate;
			events.push_back(newevent);
		}
		else if(cmd == "LABEL")
		{
			sevent *newevent = new sevent(EVENT_LABEL);

			newevent->estring = param;
			events.push_back(newevent);

		}
		else if(cmd == "GOTO")
		{
			sevent *newevent = new sevent(EVENT_GOTO);
			
			newevent->estring = param;
			events.push_back(newevent);
		}
		else if(cmd == "TAKEPICTURE")
		{
			sevent *newevent = new sevent(EVENT_TAKEPICTURE);
			events.push_back(newevent);
		}
		else if(cmd == "SHOWFEED")
		{
			sevent *newevent = new sevent(EVENT_SHOWFEED);
			newevent->value = atoi(param.c_str());
			events.push_back(newevent);			
		}
		//found an unregonized command
		else
		{
			std::cout << "Couldn't parse : " << buf << std::endl;
		}

	}

	//process events
	for(int i = 0; i < int(events.size()); i++)
	{
		//wait on motion detection event
		if(events[i]->eventType == EVENT_MOTION)
		{
			digitalWrite(events[i]->pin, 0);
			pinMode(events[i]->pin, INPUT);

			bool motiondetected = false;


			delay(10);

			while(!motiondetected)
			{
				if(digitalRead(events[i]->pin) == 1) motiondetected = true;
			}

			digitalWrite(events[i]->pin, 0);
		}
		else if(events[i]->eventType == EVENT_DELAY)
		{
			delay(events[i]->value);
		}
		else if(events[i]->eventType == EVENT_PRINT)
		{
			std::cout << events[i]->estring << std::endl;
		}
		else if(events[i]->eventType == EVENT_SOUND_PLAY)
		{
			if(events[i]->soundobj != NULL) events[i]->soundobj->sound.Play();
		}
		else if(events[i]->eventType == EVENT_SOUND_WAIT)
		{
			//look through all events and look for sounds
			for(int n = int(events.size()-1); n >= 0; n--)
			{
				//if sound is found, wait on sound status to = stopped
				if(events[n]->eventType == EVENT_SOUND_PLAY)
				{
					if(events[n]->soundobj != NULL)
					{
						if(events[n]->soundobj->sound.GetStatus() == sf::Sound::Paused)
							events[n]->soundobj->sound.Stop();

						while(events[n]->soundobj->sound.GetStatus() != sf::Sound::Stopped)
						{

						}
					}
				}
			}
		}
		else if(events[i]->eventType == EVENT_SOUND_STOP)
		{
			for(int n = int(events.size()-1); n >= 0; n--)
			{
				if(events[n]->eventType == EVENT_SOUND_PLAY)
				{
					if(events[n]->soundobj != NULL) events[n]->soundobj->sound.Stop();
				}
			}
		}
		else if(events[i]->eventType == EVENT_LABEL)
		{
			//do nothing, this is just a label
		}
		else if(events[i]->eventType == EVENT_GOTO)
		{
			//set the iterator to where a label is found
			for(int n = 0; n < int(events.size()); n++)
			{
				//if a label was found
				if(events[n]->eventType == EVENT_LABEL)
				{
					//if label matches goto, set iterator i to position
					if(events[n]->estring == events[i]->estring) i = n-1;
				}
			}
		}
		else if(events[i]->eventType == EVENT_RELAY)
		{
			bool relayon = events[i]->value;
			if(relayon) pinOn(events[i]->pin);
			else pinOff(events[i]->pin);
		}
		else if(events[i]->eventType == EVENT_TAKEPICTURE)
		{
			snapPicture();
		}
		else if(events[i]->eventType == EVENT_SHOWFEED)
		{
			showFeed(events[i]->value);
		}
		//found an unimplemented command?
		else
		{
			std::cout << "Found an unimplemented event type : " << events[i]->eventType << std::endl;
		}
	}

}

void mainMenu()
{

	bool quit = false;

	while(!quit)
	{
		//clear screen
		system("clear");

		//input buffer
		std::string buf;

		//print main menu
		std::cout << "RASPI PROJECT 3\n";
		std::cout << "---------------\n";
	        std::cout << "1) Set Relay # ON\n";
       		std::cout << "2) Set Relay # OFF\n";
	        std::cout << "3) Set all Relays ON\n";
       		std::cout << "4) Set all Relays OFF\n\n";

                std::cout << "5) Get clock time (sec)\n";
		std::cout << "6) Get clock time (ms)\n";
		std::cout << "7) Get clock time (us)\n";
		std::cout << "8) RESET CLOCK\n\n";

		std::cout << "\nT) Test pattern\n";
		std::cout << "M) Motion Sensor Test v1\n";
		std::cout << "P) Play sound file\n";
		
		std::cout << "\nS) Snap picture";
		std::cout << "\nF) Show Feed 10sec\n";

		std::cout << "\nR) Run Script\n";
		//resetAll to reset all io to off and clock to 0

	        std::cout << "Q) Exit\n";

		std::cout << ">";

		std::getline(std::cin,buf);
		//std::cin.clear();
		//std::cin.ignore(1000, '\n');

		//handle input
		if(buf == "Q" || buf == "q") quit = true;
		else if(buf == "1")
		{
			std::string ron;
			std::cout << "\n\nEnter relay # to turn on :";
			std::getline(std::cin, ron);

			pinOn( atoi(ron.c_str()) );
		}
		else if(buf == "2")
		{
			std::string roff;
			std::cout << "\n\nEnter relay # to turn off :";
			std::getline(std::cin, roff);

			pinOff( atoi(roff.c_str()) );
		}
		else if(buf == "3") allOn();
		else if(buf == "4") allOff();
		else if(buf == "5")
		{
			std::cout << "\n\nClock in sec : " << millis()/1000 << std::endl;
			hitEnter();
		}
		else if(buf == "6")
		{
			std::cout << "\n\nClock in ms : " << millis() << std::endl;
			hitEnter();
		}
		else if(buf == "7")
		{
			std::cout << "\n\nClock in us : " << micros() << std::endl;
			hitEnter();
		}
		else if(buf == "8")
		{
			resetAll();
		}
		else if(buf == "T" || buf == "t") testPattern();
		else if(buf == "M" || buf == "m") motionTest();
		else if(buf == "P" || buf == "p") soundTest();
		else if(buf == "S" || buf == "s") snapPicture();
		else if(buf == "F" || buf == "f") showFeed(10);
		else if(buf == "R" || buf == "r")
		{
			runScript("script.txt");
			hitEnter();
		}
		//else input is not a valid entry!
		else
		{
			std::cout << "\n\nNot a valid entry.\n";
			hitEnter();
		}


	}
}

int main(int argc, char *argv[])
{
	bool doscript = false;
	std::string scriptfile;
	if(argc >= 2)
	{
		scriptfile = std::string(argv[1]);
		doscript = true;
	}
	
	//initialize relay vectors
	relays.resize(PINCOUNT);

	//initialize / reset all
	resetAll();
	
	// initialize graphics
	InitGraphics();
	texture.Create(MAIN_TEXTURE_WIDTH, MAIN_TEXTURE_HEIGHT);
	
	//initialize camera
	initCamera();
	
	//load sound file
	//mysoundbuf.LoadFromFile("testwav.wav");
	//mysound = sf::Sound(mysoundbuf);

	resetAll();
	
	if(!doscript) mainMenu();
	else runScript(scriptfile);

	// shut down camera
	if(cam != NULL) StopCamera();
	
	//by default, turn off all gpios on exit
	allOff();
	
	return 0;
}


