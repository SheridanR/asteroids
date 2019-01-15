// Engine.cpp

#include "Main.hpp"
#include "Engine.hpp"
#include "Renderer.hpp"
#include "Directory.hpp"
#include "Game.hpp"

#include <chrono>

std::atomic_bool Engine::paused(false);
std::atomic_bool Engine::timerRunning(true);

// log message code strings
const char* Engine::msgTypeStr[Engine::MSG_TYPE_LENGTH] = {
	"DEBUG",
	"INFO",
	"WARN",
	"ERROR",
	"CRITICAL",
	"FATAL",
	"NOTE",
	"CHAT"
};

Engine::Engine(int argc, char **argv):
	game("base")
{
	for( int c=0; c<256; ++c ) {
		keystatus[c] = false;
	}
	for( int c=0; c<8; ++c ) {
		mousestatus[c] = false;
	}
	for( int c=0; c<32; ++c ) {
		frameval[c] = 0;
	}

	// open log file
	logLock = SDL_CreateMutex();
	if( !logFile ) {
		errno_t err = freopen_s(&logFile, "log.txt", "wb" /*or "wt"*/, stderr);
		if (err || !logFile) {
			mainEngine->fmsg(MSG_CRITICAL, "failed to open log file!");
		}
	}
	fmsg(Engine::MSG_INFO,"hello.");
}

Engine::~Engine() {
	term();
}

void Engine::init() {
	if( isInitialized() )
		return;

	// init sdl
	fmsg(Engine::MSG_INFO,"initializing SDL...");
	Uint32 initFlags = 0;
	initFlags |= SDL_INIT_TIMER;
	initFlags |= SDL_INIT_AUDIO;
	initFlags |= SDL_INIT_VIDEO;
	initFlags |= SDL_INIT_JOYSTICK;
	initFlags |= SDL_INIT_HAPTIC;
	initFlags |= SDL_INIT_GAMECONTROLLER;
	initFlags |= SDL_INIT_EVENTS;
	if( SDL_Init( initFlags ) == -1 ) {
		fmsg(Engine::MSG_CRITICAL,"failed to initialize SDL: %s",SDL_GetError());
		initialized = false;
		return;
	}
	SDL_StopTextInput();

	// init sdl_mixer
	fmsg(Engine::MSG_INFO,"initializing SDL_mixer...");
	if(Mix_OpenAudio(audio_rate, audio_format, audio_channels, audio_buffers)) {
		fmsg(Engine::MSG_CRITICAL,"failed to initialize SDL_mixer: %s",Mix_GetError());
		initialized = false;
		return;
	}

	// init sdl_image
	fmsg(Engine::MSG_INFO,"initializing SDL_image...");
	if( IMG_Init(IMG_INIT_PNG) != (IMG_INIT_PNG) ) {
		fmsg(Engine::MSG_CRITICAL,"failed to initialize SDL_image: %s",IMG_GetError());
		initialized = false;
		return;
	}

	// init sdl_ttf
	fmsg(Engine::MSG_INFO,"initializing SDL_ttf...");
	if( TTF_Init()==-1 ) {
		fmsg(Engine::MSG_CRITICAL,"failed to initialize SDL_ttf: %s",TTF_GetError());
		initialized = false;
		return;
	}

	// open game controllers
	fmsg(Engine::MSG_INFO,"opening game controllers...");
	for( int c=0; c<SDL_NumJoysticks(); ++c ) {
		SDL_GameController* pad = SDL_GameControllerOpen(c);
		if( pad ) {
			controllers.addNodeLast(pad);
		}
	}

	// instantiate a timer
	timerRunning = true;
	timer = std::thread(&Engine::timerCallback, ticksPerSecond);
	SDL_SetThreadPriority(SDL_THREAD_PRIORITY_HIGH);

	// cache resources
	fmsg(Engine::MSG_INFO,"game folder is '%s'",game.path.get());
	loadAllResources();

	// instantiate renderer
	renderer = new Renderer();
	renderer->init();

	// start game
	fmsg(Engine::MSG_INFO,"starting game");
	gamestate = new Game();
	gamestate->init(xres, yres);

	// done
	fmsg(Engine::MSG_INFO,"done");
	initialized = true;
}

void Engine::loadResources(const char* folder) {
	fmsg(Engine::MSG_INFO,"loading resources from '%s'...", folder);
}

void Engine::term() {
	if (gamestate) {
		delete gamestate;
		gamestate = nullptr;
	}
	if (renderer) {
		delete renderer;
		renderer = nullptr;
	}

	// stop engine timer
	fmsg(MSG_INFO,"closing engine...");
	fmsg(MSG_INFO,"removing engine timer...");
	timerRunning = false;
	if (timer.joinable())
		timer.join();

	// free sounds
	Mix_HaltMusic();
	Mix_HaltChannel(-1);

	// close game controllers
	for( Node<SDL_GameController*>* node = controllers.getFirst(); node != nullptr; node = node->getNext() ) {
		SDL_GameController* pad = node->getData();
		SDL_GameControllerClose(pad);
	}
	controllers.removeAll();

	// shutdown SDL subsystems
	fmsg(MSG_INFO,"shutting down SDL and its subsystems...");
	TTF_Quit();
	IMG_Quit();
	Mix_CloseAudio();
	SDL_Quit();

	// dump engine resources
	dumpResources();

	fmsg(MSG_INFO,"successfully shut down game engine.");
	fmsg(MSG_INFO,"goodbye.");

	if( logFile )
		fclose(logFile);
}

void Engine::loadAllResources() {
	fmsg(MSG_INFO,"loading engine resources...");

	// reload the important assets
	loadResources(game.path.get());
	for( mod_t& mod : mods ) {
		loadResources(mod.path.get());
	}
}

int Engine::playSound(const char* path, bool loop) {
	Sound* sound = soundResource.dataForString(path);
	if (sound) {
		return sound->play(loop);
	} else {
		return -1;
	}
}

int Engine::stopSound(int channel) {
	if (channel >= 0) {
		return Mix_HaltChannel(channel);
	}
	return -1;
}

void Engine::dumpResources() {
	fmsg(MSG_INFO,"dumping engine resources...");

	// dump all resources
	materialResource.dumpCache();
	textResource.dumpCache();
	imageResource.dumpCache();
	soundResource.dumpCache();
}

void Engine::timerCallback(double interval) {
	std::chrono::duration<double, std::ratio<1, 1>> msInterval( 1.0 / interval);
	while (timerRunning) {
		auto start = std::chrono::steady_clock::now();
		if (!paused) {
			SDL_Event event;
			SDL_UserEvent userevent;

			userevent.type = SDL_USEREVENT;
			userevent.code = 0;
			userevent.data1 = nullptr;
			userevent.data2 = nullptr;

			event.type = SDL_USEREVENT;
			event.user = userevent;

			SDL_PushEvent(&event);
		}
		while (std::chrono::steady_clock::now() - start < msInterval);
	}
}

void Engine::fmsg(const Uint32 msgType, const char* fmt, ...) {
#ifdef NDEBUG
	if( msgType==Engine::MSG_DEBUG ) {
		return;
	}
#endif

	// wait for current log to finish
	if (!logLock) {
		while( 1 ) {
			SDL_LockMutex(logLock);
			if( logging ) {
				SDL_UnlockMutex(logLock);
			} else {
				logging = true;
				SDL_UnlockMutex(logLock);
				break;
			}
		}
	}

	char str[1024];
	str[1024-1] = '\0';

	// format the string
	va_list argptr;
	va_start( argptr, fmt );
	vsnprintf( str, 1024, fmt, argptr );
	va_end( argptr );
	str[1024-1] = '\0';

	// bust multi-line strings into individual strings...
	for( size_t c=0; c<strlen(str); ++c ) {
		if( str[c]=='\n' ) {
			str[c]=0;

			// disable lock temporarily
			if (logLock) {
				SDL_LockMutex(logLock);
				logging = false;
				SDL_UnlockMutex(logLock);
			}

			// write second line
			fmsg(msgType,(const char *)(str+c+1));

			// reactivate log lock
			if (logLock) {
				while( 1 ) {
					SDL_LockMutex(logLock);
					if( logging ) {
						SDL_UnlockMutex(logLock);
					} else {
						logging = true;
						SDL_UnlockMutex(logLock);
						break;
					}
				}
			}
		}
	}

	// create timestamp string
	time_t timer;
	char buffer[32];
	struct tm tm_info;
	time(&timer);
	localtime_s(&tm_info, &timer);
	strftime( buffer, 32, "%H-%M-%S", &tm_info );

	// print message to stderr and stdout
	fprintf( stderr, "[%s] %s: %s\r\n", buffer, (const char *)msgTypeStr[msgType], str );
	fprintf( stdout, "[%s] %s: %s\r\n", buffer, (const char *)msgTypeStr[msgType], str );
	fflush( stderr );
	fflush( stdout );

	// add message to log list
	logmsg_t logMsg;
	logMsg.text = str;
	logMsg.uid = logUids++;
	logMsg.kind = static_cast<msg_t>(msgType);
	switch( msgType ) {
		case MSG_DEBUG:
			logMsg.color = glm::vec3(0.f,.7f,0.f);
			break;

		case MSG_WARN:
			logMsg.color = glm::vec3(1.f,1.f,0.f);
			break;
		case MSG_ERROR:
			logMsg.color = glm::vec3(1.f,.5f,0.f);
			break;
		case MSG_CRITICAL:
			logMsg.color = glm::vec3(1.f,0.f,1.f);
			break;
		case MSG_FATAL:
			logMsg.color = glm::vec3(1.f,0.f,0.f);
			break;

		case MSG_INFO:
			logMsg.color = glm::vec3(1.f,1.f,1.f);
			break;
		case MSG_NOTE:
			logMsg.color = glm::vec3(0.f,1.f,1.f);
			break;
		case MSG_CHAT:
			logMsg.color = glm::vec3(0.f,1.f,0.f);
			break;

		default:
			logMsg.color = glm::vec3(1.f,1.f,1.f);
			break;
	}

	logList.addNodeLast(logMsg);

	// unlock log stuff
	if (logLock) {
		SDL_LockMutex(logLock);
		logging = false;
		SDL_UnlockMutex(logLock);
	}
}

void Engine::smsg(const Uint32 msgType, const String& str) {
	fmsg(msgType,str.get());
}

void Engine::msg(const Uint32 msgType, const char* str) {
	fmsg(msgType,str);
}

void Engine::freadl( void* ptr, size_t size, size_t count, FILE* stream, const char* filename, const char* funcName ) {
	if( fread(ptr, size, count, stream) != count ) {
		if( filename != nullptr ) {
			char buf[128];
			strerror_s(buf, 128, errno);
			buf[127] = '\0';
			if( funcName != nullptr ) {
				mainEngine->fmsg(Engine::MSG_WARN,"%s: file read error in '%s': %s",funcName,filename,buf);
			} else {
				mainEngine->fmsg(Engine::MSG_WARN,"file read error in '%s': %s",filename,buf);
			}
		}
	}
}

int Engine::readInt( const char* str, int* arr, int numToRead ) {
	int numRead = 0;
	char* curr = nullptr;
	while( numRead < numToRead ) {
		if( str[0]=='\0' ) {
			if( numRead != numToRead ) {
				mainEngine->fmsg(Engine::MSG_DEBUG,"readInt(): could only read %d numbers of %d", numRead, numToRead);
			}
			break;
		}
		if( numRead != 0 ) {
			++str;
			if( str[0]=='\0' ) {
				if( numRead != numToRead ) {
					mainEngine->fmsg(Engine::MSG_DEBUG,"readInt(): could only read %d numbers of %d", numRead, numToRead);
				}
				break;
			}
		}
		if( curr ) {
			arr[numRead] = strtol( (const char*)curr, &curr, 10 );
		} else {
			arr[numRead] = strtol( str, &curr, 10 );
		}
		++numRead;
	}
	return numRead;
}

int Engine::readFloat( const char* str, float* arr, int numToRead ) {
	int numRead = 0;
	char* curr = nullptr;
	while( numRead < numToRead ) {
		if( str[0]=='\0' ) {
			if( numRead != numToRead ) {
				mainEngine->fmsg(Engine::MSG_DEBUG,"readFloat(): could only read %d numbers of %d", numRead, numToRead);
			}
			break;
		}
		if( numRead != 0 ) {
			++str;
			if( str[0]=='\0' ) {
				if( numRead != numToRead ) {
					mainEngine->fmsg(Engine::MSG_DEBUG,"readFloat(): could only read %d numbers of %d", numRead, numToRead);
				}
				break;
			}
		}
		if( curr ) {
			arr[numRead] = strtof( (const char*)(curr), &curr );
		} else {
			arr[numRead] = strtof( str, &curr );
		}
		++numRead;
	}
	return numRead;
}

bool Engine::lineIntersectPlane( const Vector& lineStart, const Vector& lineEnd, const Vector& planeOrigin, const Vector& planeNormal, Vector& outIntersection ) {
	Vector u = lineEnd - lineStart;
	Vector w = lineStart - planeOrigin;

	float d = u.dot(planeNormal);
	float n = -w.dot(planeNormal);

	// check that the line isn't parallel to the plane
	if( fabs(d) <= 0.f ) {
		if( n == 0.f ) {
			// the line is in the plane
			outIntersection = lineStart;
			return true;
		} else {
			// no intersection
			return false;
		}
	}

	// compute point of intersection
	float intersect = n / d;
	if( intersect < 0.f || intersect > 1.f )
		return false; // out of range, no intersection

	// compute intersection
	outIntersection = lineStart + Vector(intersect) * u;
	return true;
}

Vector Engine::triangleCoords(const Vector& a, const Vector& b, const Vector& c, const Vector& p) {
	Vector v0 = c - a;
	Vector v1 = b - a;
	Vector v2 = p - a;

	float dot00 = v0.dot(v0);
	float dot01 = v0.dot(v1);
	float dot02 = v0.dot(v2);
	float dot11 = v1.dot(v1);
	float dot12 = v1.dot(v2);

	// barycentric coords
	float invDenom = 1.f / (dot00 * dot11 - dot01 * dot01);
	float u = (dot11 * dot02 - dot01 * dot12) * invDenom;
	float v = (dot00 * dot12 - dot01 * dot02) * invDenom;

	return Vector(u, v, 0.f);
}

bool Engine::pointInTriangle( const Vector& a, const Vector& b, const Vector& c, const Vector& p ) {
	Vector result = triangleCoords(a, b, c, p);
	float u = result.x;
	float v = result.y;
	return (u >= 0) && (v >= 0) && (u + v < 1.f);
}

bool Engine::triangleOverlapsTriangle( const Vector& a0, const Vector& b0, const Vector& c0, const Vector& a1, const Vector& b1, const Vector& c1 ) {
	if( pointInTriangle(a0, b0, c0, a1) ) {
		return true;
	}
	if( pointInTriangle(a0, b0, c0, b1) ) {
		return true;
	}
	if( pointInTriangle(a0, b0, c0, c1) ) {
		return true;
	}
	return false;
}

void Engine::shutdown() {
	running = false;
}

bool Engine::charsHaveLetters(const char *arr, size_t len) {
	for( int c=0; c<len; ++c ) {
		if( arr[c] == 0 ) {
			return false;
		}

		else if( (arr[c] < '0' || arr[c] > '9' ) && arr[c] != '.' && arr[c] != '-' ) {
			return true;
		}
	}

	return false;
}

void Engine::preProcess() {
	anykeystatus = false;

	// lock mouse to window?
	SDL_SetRelativeMouseMode((SDL_bool)mainEngine->isMouseRelative());

	// restart timer
	unsigned int newTicksPerSecond = 60;
	if( newTicksPerSecond != ticksPerSecond ) {
		ticksPerSecond = newTicksPerSecond;
		timerRunning = false;
		if (timer.joinable())
			timer.join();
		timerRunning = true;
		timer = std::thread(&Engine::timerCallback, ticksPerSecond);
	}

	SDL_GameController* pad = nullptr;
	while( SDL_PollEvent(&event) ) {
		switch( event.type ) {
			case SDL_QUIT: // if SDL receives the shutdown signal
				shutdown();
				break;
			case SDL_KEYDOWN: // if a key is pressed...
				if( SDL_IsTextInputActive() ) {
					if( event.key.keysym.sym == SDLK_BACKSPACE && strlen(inputstr) > 0 ) {
						inputstr[strlen(inputstr)-1]=0;
						cursorflash=ticks;
					} else if( event.key.keysym.sym == SDLK_c && SDL_GetModState()&KMOD_CTRL ) {
						if( inputstr )
							SDL_SetClipboardText(inputstr);
						cursorflash=ticks;
					} else if( event.key.keysym.sym == SDLK_v && SDL_GetModState()&KMOD_CTRL ) {
						if( inputstr )
							strcpy_s(inputstr, inputlen, SDL_GetClipboardText());
						cursorflash=ticks;
					}
				}
				lastkeypressed = SDL_GetKeyName(SDL_GetKeyFromScancode(event.key.keysym.scancode));
				keystatus[event.key.keysym.scancode] = true;
				anykeystatus = true;
				break;
			case SDL_KEYUP: // if a key is unpressed...
				keystatus[event.key.keysym.scancode] = false;
				break;
			case SDL_TEXTINPUT:
				if( !inputnum || !charsHaveLetters(event.text.text,SDL_TEXTINPUTEVENT_TEXT_SIZE) ) {
					if( (event.text.text[0] != 'c' && event.text.text[0] != 'C') || !(SDL_GetModState()&KMOD_CTRL) ) {
						if( (event.text.text[0] != 'v' && event.text.text[0] != 'V') || !(SDL_GetModState()&KMOD_CTRL) ) {
							if( inputstr ) {
								int i = inputlen-(int)strlen(inputstr)-1;
								strncat_s(inputstr, inputlen, event.text.text, max(0,i));
								strcpy_s(lastInput, SDL_TEXTINPUTEVENT_TEXT_SIZE, event.text.text);
							}
							cursorflash=ticks;
						}
					}
				}
				break;
			case SDL_MOUSEBUTTONDOWN: // if a mouse button is pressed...
				if( !mousestatus[event.button.button] ) {
					mousestatus[event.button.button] = true;
					if( ticks - mouseClickTime <= doubleClickTime ) {
						dbc_mousestatus[event.button.button] = true;
					}
					mouseClickTime = ticks;
				}
				break;
			case SDL_MOUSEBUTTONUP: // if a mouse button is released...
				mousestatus[event.button.button] = false;
				dbc_mousestatus[event.button.button] = false;
				break;
			case SDL_MOUSEWHEEL:
				mousewheelx = event.wheel.x;
				mousewheely = event.wheel.y;
				break;
			case SDL_MOUSEMOTION: // if the mouse is moved...
				if( ticks==0 ) {
					// fixes a bug with unpredictable starting mouse movement
					break;
				}
				mousex = event.motion.x;
				mousey = event.motion.y;
				mousexrel += event.motion.xrel;
				mouseyrel += event.motion.yrel;

				if( std::abs(mousexrel) > 2 || std::abs(mouseyrel) > 2 ) {
					mouseClickTime = 0;
				}
				break;
			case SDL_CONTROLLERDEVICEADDED:
				pad = SDL_GameControllerOpen(event.cdevice.which);
				if( pad == nullptr ) {
					fmsg(MSG_WARN, "A controller was plugged in, but no handle is available!");
				} else {
					controllers.addNode(event.cdevice.which, pad);
					fmsg(MSG_INFO, "Added controller with device index (%d)", event.cdevice.which);
					break;
				}
				break;
			case SDL_CONTROLLERDEVICEREMOVED:
				pad = SDL_GameControllerFromInstanceID(event.cdevice.which);
				if( pad == nullptr ) {
					fmsg(MSG_WARN, "A controller was removed, but I don't know which one!");
				} else {
					Uint32 index = 0;
					Node<SDL_GameController*>* node = controllers.getFirst();
					for( ; node != nullptr; node = node->getNext(), ++index ) {
						SDL_GameController* curr = node->getData();
						if( pad == curr ) {
							SDL_GameControllerClose(curr);
							controllers.removeNode(node);
							fmsg(MSG_INFO, "Removed controller with device index (%d), instance id (%d)", index, event.cdevice.which);
							break;
						}
					}
				}
				break;
			case SDL_USEREVENT: // if the game timer has elapsed
				++ticks;
				++framesToRun;
				ranFrames = true;
				break;
		}
	}
	if( !mousestatus[SDL_BUTTON_LEFT] ) {
		omousex=mousex;
		omousey=mousey;
	}

	if( framesToRun ) {
		// calculate engine rate
		t = SDL_GetTicks();
		timesync = t-ot;
		ot = t;

		// calculate fps
		if( timesync != 0 )
			frameval[cycles&(fpsAverage-1)] = 1.0/timesync;
		else
			frameval[cycles&(fpsAverage-1)] = 1.0;
		double d = frameval[0];
		for( Uint32 i=1; i<fpsAverage; ++i )
			d += frameval[i];
		if( SDL_GetTicks()-lastfpscount > 500 ) {
			lastfpscount = SDL_GetTicks();
			fps = (d/fpsAverage)*1000;
		}
	}
}

void Engine::process() {
	// game logic here
	while (framesToRun > 0) {
		gamestate->process();
		--framesToRun;
	}
}

void Engine::postProcess() {
	if( ranFrames ) {
		renderer->clearBuffers();
		gamestate->draw(renderer->getCamera());
		renderer->swapWindow();

		mousexrel = 0;
		mouseyrel = 0;
		mousewheelx = 0;
		mousewheely = 0;
		ranFrames = false;
	}

	++cycles;
}

String Engine::buildPath(const char* path) {
	StringBuf<256> result(game.path.get());
	result.appendf("/%s",path);

	// if a mod has the same path, use the mod's path instead...
	for( mod_t& mod : mods ) {
		StringBuf<256> modResult(mod.path.get());
		modResult.appendf("/%s",path);

		errno_t err;
		FILE* fp = nullptr;
		if( !(err=fopen_s(&fp, modResult.get(), "rb" )) ) {
			result = modResult;
		}
		if (fp) {
			fclose(fp);
		}
	}

	return result;
}

bool Engine::addMod(const char* name) {
	if( name == nullptr || name[0] == '\0' || game.path.get() == name )
		return false;

	// check that we have not already added the mod
	bool foundMod = false;
	for( mod_t& mod : mods ) {
		if( mod.path == name ) {
			foundMod = true;
			break;
		}
	}

	if( !foundMod ) {
		mod_t mod(name);
		if (mod.loaded == false) {
			Engine::fmsg(MSG_ERROR,"failed to install '%s' mod.",name);
			return false;
		}
		mods.addNodeLast(mod);

		Engine::fmsg(MSG_INFO,"installed '%s' mod",name);

		return true;
	} else {
		Engine::fmsg(MSG_ERROR,"'%s' mod is already installed.",name);

		return false;
	}
}

bool Engine::removeMod(const char* name) {
	if( name == nullptr || name[0] == '\0' || game.path == name )
		return false;

	// check that we have not already added the mod
	size_t index = 0;
	for( mod_t& mod : mods ) {
		if( mod.path == name ) {
			mods.removeNode(index);
			Engine::fmsg(MSG_INFO,"uninstalled '%s' mod",name);
			return true;
		}
		++index;
	}

	Engine::fmsg(MSG_ERROR,"'%s' mod is not installed.",name);
	return false;
}

bool Engine::copyLog(LinkedList<Engine::logmsg_t>& dest) {
	bool result = false;

	// wait for current logging action to finish
	while( 1 ) {
		SDL_LockMutex(logLock);
		if( logging ) {
			SDL_UnlockMutex(logLock);
		} else {
			logging = true;
			SDL_UnlockMutex(logLock);
			break;
		}
	}

	if( dest.getSize() == logList.getSize() ) {
		SDL_LockMutex(logLock);
		logging = false;
		SDL_UnlockMutex(logLock);
		return false;
	} else if( dest.getSize() > logList.getSize() ) {
		dest.removeAll();
		result = true;
	}

	for( Node<Engine::logmsg_t>* node = logList.nodeForIndex(dest.getSize()); node != nullptr; node = node->getNext() ) {
		Engine::logmsg_t newMsg;
		newMsg = node->getData();
		dest.addNodeLast(newMsg);
	}

	SDL_LockMutex(logLock);
	logging = false;
	SDL_UnlockMutex(logLock);

	return result;
}

void Engine::clearLog() {

	// wait for current logging action to finish
	while( 1 ) {
		SDL_LockMutex(logLock);
		if( logging ) {
			SDL_UnlockMutex(logLock);
		} else {
			logging = true;
			SDL_UnlockMutex(logLock);
			break;
		}
	}

	logList.removeAll();

	SDL_LockMutex(logLock);
	logging = false;
	SDL_UnlockMutex(logLock);
}

Uint32 Engine::random() {
	return rand.getUint32();
}

Engine::mod_t::mod_t(const char* _path):
	path(_path),
	name("Untitled"),
	author("Unknown")
{
	if( !path ) {
		return;
	}
	StringBuf<128> fullPath("%s/game.json", _path);
	if( !FileHelper::readObject(fullPath, *this) ) {
		mainEngine->fmsg(Engine::MSG_ERROR, "Failed to read mod manifest: '%s'", fullPath.get());
		return;
	}
	loaded = true;
}

void Engine::mod_t::serialize(FileInterface* file) {
	file->property("name", name);
	file->property("author", author);
}