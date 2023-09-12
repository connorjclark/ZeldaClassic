#include "base/version.h"
#include <string_view>

consteval bool is_digit(char c)
{
    return c <= '9' && c >= '0';
}

consteval int stoi_impl(const char* str, int value = 0)
{
    return *str ?
            is_digit(*str) ?
                stoi_impl(str + 1, (*str - '0') + value * 10)
                : value
            : value;
}

consteval int stoi(const char* str, int start)
{
    return stoi_impl(str + start);
}

#define RELEASE_TAG "3.0.1-nightly+2023-01-02"

consteval ZCVersion parseVersion()
{
	int components[] = {-1, -1, -1};
	int last = 0;
	int index = 0;
	int i = 0;
    for (const auto& ch : RELEASE_TAG)
	{
		if (ch == '.')
		{
			components[i++] = stoi(RELEASE_TAG, last);
			last = index + 1;
		}
		else if (!is_digit(ch))
		{
			break;
		}

		if (i == 3)
			break;
		index++;
    }

	if (i == 2)
		components[2] = stoi(RELEASE_TAG, last);

	return {RELEASE_TAG, components[0], components[1], components[2]};
}

static constexpr auto components = parseVersion();
static_assert(components.major == 3, "version not set correctly");
static_assert(components.minor != -1, "version not set correctly");
static_assert(components.patch != -1, "version not set correctly");
// Matches zquestheader::zelda_version_string
// static_assert(strlen(components.version_string) < 35, "version not set correctly");


std::string getVersion()
{
	return components.version_string;
}

ZCVersion getVersionComponents()
{
	return components;
}

int getAlphaState()
{
	if (components.patch == 0) return 3;
	return 0;
}

bool isStableRelease()
{
	return components.patch == 0;
}
