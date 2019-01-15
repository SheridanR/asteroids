// Main.cpp

#include "Main.hpp"
#include "Engine.hpp"
#include "LinkedList.hpp"

Engine* mainEngine = nullptr;

const float PI = 3.14159265358979323f;
const float SQRT2 = 1.41421356237309504f;
const char* versionStr = "1.0.0.0";

int main(int argc, char **argv) {
	mainEngine = new Engine(argc,argv);

	// initialize mainEngine
	mainEngine->init();
	if( !mainEngine->isInitialized() ) {
		mainEngine->fmsg(Engine::MSG_CRITICAL,"failed to start engine.");
		delete mainEngine;
		return 1;
	}
	
	// main loop
	while(mainEngine->isRunning()) {
		mainEngine->preProcess();
		mainEngine->process();
		mainEngine->postProcess();
	}
	
	delete mainEngine;

	return 0;
}