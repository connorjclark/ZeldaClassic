#ifndef BASE_VERSION_H
#define BASE_VERSION_H

#include <string>

struct ZCVersion {
	const char* version_string;
	int major, minor, patch;
};

std::string getVersionString();
ZCVersion getVersion();
int getAlphaState();
bool isStableRelease();

#endif
