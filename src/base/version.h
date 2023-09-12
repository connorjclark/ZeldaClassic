#ifndef BASE_VERSION_H
#define BASE_VERSION_H

#include <string>

#ifndef RELEASE_TAG
#define RELEASE_TAG "3.0.0"
#endif

struct ZCVersion {
	const char* version_string;
	int major, minor, patch;
};

std::string getVersion();
ZCVersion getVersionComponents();
int getAlphaState();
bool isStableRelease();

#endif
