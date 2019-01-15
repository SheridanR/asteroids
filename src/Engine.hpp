// Engine.hpp

#pragma once

#include "Asset.hpp"
#include "Resource.hpp"
#include "Random.hpp"
#include "Shader.hpp"
#include "ShaderProgram.hpp"
#include "Image.hpp"
#include "Material.hpp"
#include "Text.hpp"
#include "Vector.hpp"
#include "LinkedList.hpp"
#include "Sound.hpp"

#include <atomic>
#include <thread>

class Game;
class Renderer;
class FileInterface;

class Engine {
public:
	Engine(int argc, char **argv);
	~Engine();

	// log message codes
	enum msg_t {
		MSG_DEBUG = 0,		// only printed when NDEBUG is undefined

		MSG_INFO,			// for when you have an unimportant message to report
		MSG_WARN,			// for when the game has encountered a small problem
		MSG_ERROR,			// for when the game has encountered a definite problem
		MSG_CRITICAL,		// for when the game likely will not continue past this point
		MSG_FATAL,			// for when you have to exit() immediately

		MSG_NOTE,			// like info, but more important (not for errors!)
		MSG_CHAT,			// for when a chat message was received over the net

		MSG_TYPE_LENGTH
	};

	// a console message
	struct logmsg_t {
		unsigned int uid = 0;
		String text;
		glm::vec3 color = glm::vec3(1.f);
		msg_t kind = MSG_INFO;
	};

	// a mod
	struct mod_t {
		mod_t(const char* _path);

		String path;
		String name;
		String author;
		bool loaded = false;

		void serialize(FileInterface * file);
	};

	// default ticks per second
	static const unsigned int defaultTickRate = 60;

	// amount of ticks within which two clicks register as a "double-click"
	static const unsigned int doubleClickTime = 30;

	// getters & setters
	const bool							isInitialized() const							{ return initialized; }
	const bool							isRunning() const								{ return running; }
	const bool							isPaused() const								{ return paused; }
	const bool							isFullscreen() const							{ return fullscreen; }
	const Renderer*						getRenderer() const								{ return renderer; }
	Renderer*							getRenderer()									{ return renderer; }
	const char*							getGameTitle() const							{ return game.name.get(); }
	const Sint32						getXres() const									{ return xres; }
	const Sint32						getYres() const									{ return yres; }
	const bool							getKeyStatus(const int index) const				{ return keystatus[index]; }
	const bool							getAnyKeyStatus() const							{ return anykeystatus; }
	const char*							getLastKeyPressed() const						{ return lastkeypressed; }
	const bool							getMouseStatus(const int index) const			{ return mousestatus[index]; }
	const bool							getDBCMouseStatus(const int index) const		{ return dbc_mousestatus[index]; }
	const Sint32						getMouseX() const								{ return mousex; }
	const Sint32						getMouseY() const								{ return mousey; }
	const Sint32						getOldMouseX() const							{ return omousex; }
	const Sint32						getOldMouseY() const							{ return omousey; }
	const Sint32						getMouseWheelX() const							{ return mousewheelx; }
	const Sint32						getMouseWheelY() const							{ return mousewheely; }
	const Sint32						getMouseMoveX() const							{ return mousexrel; }
	const Sint32						getMouseMoveY() const							{ return mouseyrel; }
	const double						getFPS() const									{ return fps; }
	const double						getTimeSync() const								{ return timesync; }
	const Uint32						getTicks() const								{ return ticks; }
	const unsigned int					getTicksPerSecond() const						{ return ticksPerSecond; }
	Resource<Sound>&					getSoundResource()								{ return soundResource; }
	Resource<Image>&					getImageResource()								{ return imageResource; }
	Resource<Material>&					getMaterialResource()							{ return materialResource; }
	Resource<Text>&						getTextResource()								{ return textResource; }
	LinkedList<String>&					getCommandHistory()								{ return commandHistory; }
	const char*							getInputStr()									{ return inputstr; }
	const bool							isCursorVisible() const							{ return (ticks-cursorflash)%ticksPerSecond<ticksPerSecond/2; }
	const bool							isMouseRelative() const							{ return mouseRelative; }
	const bool							isKillSignal() const							{ return killSignal; }
	Random&								getRandom()										{ return rand; }
	const char*							getLastInput() const							{ return lastInput; }
	LinkedList<SDL_GameController*>&	getControllers()								{ return controllers; }
	const bool							isPlayTest() const								{ return playTest; }
		
	void								setPaused(const bool _paused)					{ paused = _paused; }
	void								setInputStr(char* const _inputstr)				{ inputstr = _inputstr; inputnum = false; }
	void								setInputLen(const int _inputlen)				{ inputlen = _inputlen; }
	void								setInputNumbersOnly(const bool _inputnum)		{ inputnum = _inputnum; }
	void								setMouseRelative(const bool _mouseRelative)		{ mouseRelative = _mouseRelative; }
	void								setKillSignal(const bool _killSignal)			{ killSignal = _killSignal; }
	void								setFullscreen(const bool _b)					{ fullscreen = _b; }
	void								setXres(const Sint32 i)							{ xres = i; }
	void								setYres(const Sint32 i)							{ yres = i; }
	void								setPlayTest(const bool b)						{ playTest = b; }
	void								setConsoleSleep(Uint32 i)						{ consoleSleep = i; }

	// initialize the engine
	void init();

	// loads all game resources
	void loadAllResources();

	// loads resources from a particular mod / game folder
	void loadResources(const char* folder);

	// clears all resource caches, effectively starting the engine "fresh"
	// this does NOT unmount mods! It simply causes the engine to recache any loaded resources
	void dumpResources();

	// copy the contents of the engine log to another one
	// @param dest the destination log to copy to
	// @return true if the dest was cleared in the process
	bool copyLog(LinkedList<Engine::logmsg_t>& dest);

	// play the given sound
	// @param path Path to the sound file
	// @param loop True if the sound should loop
	// @return the channel used by the sound
	int playSound(const char* path, bool loop);

	// stop the given sound channel
	// @param channel the channel of the sound we wish to stop
	// @return error code
	int stopSound(int channel);

	// clears the log
	void clearLog();

	// shutdown the engine safely (perhaps from another class)
	void shutdown();

	// timer thread function (not actually a callback, name is a holdover)
	// @param interval The exact time in ms between each heartbeat
	static void timerCallback(double interval);

	// logs a formatted char string to the console
	// @param msgType the type of message to send to the console
	// @param fmt a formatted string to print to the console
	void fmsg(const Uint32 msgType, const char* fmt, ...);

	// logs a String to the console
	// @param msgType the type of message to send to the console
	// @param str a String to print to the console
	void smsg(const Uint32 msgType, const String& str);

	// logs a char string to the console
	// @param msgType the type of message to send to the console
	// @param str a char string to print to the console
	void msg(const Uint32 msgType, const char* str);

	// reads data from a file stream and outputs errors to the log
	// @param ptr a pointer to the variable in which to store the data read
	// @param size the size of each data element to read
	// @param count the number of data elements to read
	// @param stream the file stream to read the data from
	// @param filename the filename of the file that is being read from
	// @param funcName the name of the function wherein the file is being read
	static void freadl( void* ptr, size_t size, size_t count, FILE* stream, const char* filename, const char* funcName );

	// reads multiple ints from a character string
	// @param str the string to read the space-separated character-encoded ints from
	// @param arr an array to store each of the ints in
	// @param numToRead the number of ints to read from str
	// @return the number of ints that were successfully read from str
	static int readInt( const char* str, int* arr, int numToRead );

	// reads multiple floats from a character string
	// @param str the string to read the space-separated character-encoded floats from
	// @param arr an array to store each of the floats in
	// @param numToRead the number of floats to read from str
	// @return the number of floats that were successfully read from str
	static int readFloat( const char* str, float* arr, int numToRead );

	// determines if some text is anything but a number
	// @param arr the array of characters to check for letters
	// @param len the length of the array
	// @return true if the character array has non-numeral characters, false otherwise
	static bool charsHaveLetters( const char* arr, size_t len );

	// finds the intersection between a point and a plane in 3D
	// @param lineStart the start of the line segment
	// @param lineEnd the end of the line segment
	// @param planeOrigin where the plane is "centered" in 3D
	// @param planeNormal the surface normal of the plane
	// @param outIntersection the point of intersection between the line and the plane
	// @return true if the line intersects with the plane, false otherwise
	static bool lineIntersectPlane( const Vector& lineStart, const Vector& lineEnd, const Vector& planeOrigin, const Vector& planeNormal, Vector& outIntersection );

	// determines where in the triangle defined by a, b, c, that the point p lies (does not use Z coord)
	// @param a point A on the triangle
	// @param b point B on the triangle
	// @param c point C on the triangle
	// @param p the point to test
	// @return a pair of coordinates that define where the point is in the triangle
	static Vector triangleCoords( const Vector& a, const Vector& b, const Vector& c, const Vector& p );

	// determines if the given point p lies in the triangle defined by a, b, c (does not use Z coord)
	// @param a point A on the triangle
	// @param b point B on the triangle
	// @param c point C on the triangle
	// @param p the point to test
	// @return true if the point is in the triangle, otherwise false
	static bool pointInTriangle( const Vector& a, const Vector& b, const Vector& c, const Vector& p );

	// determines if one triangle lies inside of another (two-dimensional)
	// @param a0: point A on first triangle (container)
	// @param b0: point B on first triangle (container)
	// @param c0: point C on first triangle (container)
	// @param a1: point A on second triangle
	// @param b1: point B on second triangle
	// @param c1: point C on second triangle
	// @return true if the second triangle is in the first one, otherwise false
	static bool triangleOverlapsTriangle( const Vector& a0, const Vector& b0, const Vector& c0, const Vector& a1, const Vector& b1, const Vector& c1 );

	// perform pre-processing on the current frame
	void preProcess();

	// process the current frame
	void process();

	// perform post-processing on the current frame
	void postProcess();

	// return the value of a key and reset it if it's been pressed
	// !IMPORTANT! should generally NOT be used, as it "locks" the key for the rest of the frame after being pressed
	// use a local variable to record the press for your object instead!
	// @param index the key scancode
	// @return true if the key has been pressed, false otherwise
	const bool pressKey(const int index)	{ if( keystatus[index] ) { keystatus[index]=false; return true; } else { return false; } }

	// return the value of a mouse button and reset it if it's been clicked
	// !IMPORTANT! should generally NOT be used, as it "locks" the button for the rest of the frame after being pressed
	// use a local variable to record the press for your object instead!
	// @param index the button number
	// @return true if the button has been clicked, false otherwise
	const bool pressMouse(const int index)	{ if( mousestatus[index] ) { mousestatus[index]=false; return true; } else { return false; } }

	// gets the path to an asset and returns its full path as a string
	// @param path the path without including mod or base folders
	// @return the full path string
	String buildPath(const char* path);

	// add a mod to the game
	// @param name the name of the mod folder to add
	// @return true if the mod was added, false otherwise
	bool addMod(const char* name);

	// remove a mod from the game
	// @param name the name of the mod folder to remove
	// @return true if the mod was removed, false otherwise
	bool removeMod(const char* name);

	// generate a random number
	// @return a random 32-bit number
	Uint32 random();

private:
	static const char* msgTypeStr[MSG_TYPE_LENGTH];

	// shuts down the engine
	void term();

	// general data
	bool playTest = false;
	String combinedVersion;
	bool initialized = false;
	bool running = true;
	SDL_Event event;
	static std::atomic_bool paused;
	unsigned int ticksPerSecond = defaultTickRate;

	// mod data
	mod_t game;
	LinkedList<mod_t> mods;

	// log data
	FILE *logFile = nullptr;
	LinkedList<logmsg_t> logList;
	LinkedList<String> commandHistory;
	unsigned int logUids = 0;
	SDL_mutex* logLock = nullptr;
	bool logging = false;

	// renderer
	Renderer* renderer = nullptr;

	// resource caches
	Resource<Sound> soundResource;
	Resource<Image> imageResource;
	Resource<Material> materialResource;
	Resource<Text> textResource;

	// random number generator
	Random rand;

	// video data (startup settings)
	bool fullscreen = false;
	Sint32 xres = 1280;
	Sint32 yres = 720;

	// timing data
	double fps=0, timesync=0, t=0, ot=0;
	static const int fpsAverage = 32;
	double frameval[fpsAverage];
	Uint32 ticks=0, cycles=0, lastfpscount=0;
	std::thread timer;
	static std::atomic_bool timerRunning;
	int framesToRun = 0;
	bool ranFrames = false;

	// console data
	Uint32 consoleSleep = 0;

	// input data
	bool inputAllowed = true;
	const char* lastkeypressed = nullptr;
	char lastInput[SDL_TEXTINPUTEVENT_TEXT_SIZE] = { 0 };
	bool keystatus[SDL_NUM_SCANCODES];
	bool anykeystatus = false;
	char keypressed = 0;
	bool mousestatus[8];
	bool dbc_mousestatus[8];
	Uint32 mouseClickTime = 0;
	bool mouseRelative = false;
	Sint32 mousex=0, mousey=0;
	Sint32 omousex=0, omousey=0;
	Sint32 mousexrel=0, mouseyrel=0;
	Sint32 mousewheelx=0, mousewheely=0;
	char *inputstr = nullptr; // text input buffer
	int inputlen = 0; // length of the text input buffer
	bool inputnum = false; // if true, the text input is for numbers only
	Uint32 cursorflash = 0;
	bool killSignal = false; // if true, engine received a quit signal from the OS
	LinkedList<SDL_GameController*> controllers;

	// sound data
	int audio_rate = 44100;
	Uint16 audio_format = AUDIO_S16;
	int audio_channels = 1;
	int audio_buffers = 512;

	// game state
	Game* gamestate = nullptr;
};