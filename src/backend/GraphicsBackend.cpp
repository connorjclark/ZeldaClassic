#include "GraphicsBackend.h"
#include "MouseBackend.h"
#include "PaletteBackend.h"
#include "Backend.h"
#include "../zc_alleg.h"
#include <cassert>

void Z_message(const char *format, ...);

void (*GraphicsBackend::switch_in_func_)() = NULL;
void (*GraphicsBackend::switch_out_func_)() = NULL;
int GraphicsBackend::fps_ = 60;

volatile int GraphicsBackend::frame_counter = 0;
volatile int GraphicsBackend::frames_this_second = 0;
volatile int GraphicsBackend::prev_frames_this_second = 0;

bool GraphicsBackend::windowsFullscreenFix_ = false;

void update_frame_counter()
{
	static int cnt;
	GraphicsBackend::frame_counter++;
	cnt++;
	if (cnt == GraphicsBackend::fps_)
	{
		GraphicsBackend::prev_frames_this_second = GraphicsBackend::frames_this_second;
		GraphicsBackend::frames_this_second = 0;
		cnt = 0;
	}
}
END_OF_FUNCTION(update_frame_counter)

void onSwitchOut()
{
	if (GraphicsBackend::switch_out_func_)
		GraphicsBackend::switch_out_func_();
}
END_OF_FUNCTION(onSwitchOut);

void onSwitchIn()
{
#ifdef _WINDOWS
	GraphicsBackend::windowsFullscreenFix_ = true;
#endif
	if (GraphicsBackend::switch_in_func_)
		GraphicsBackend::switch_in_func_();
}
END_OF_FUNCTION(onSwitchIn);


GraphicsBackend::GraphicsBackend() :
	hw_screen_(NULL),
	backbuffer_(NULL),
	nativebuffer_(NULL),
	convertbuffer_(NULL),
	initialized_(false),
	screenw_(320),
	screenh_(240),
	fullscreen_(false),
	native_(true),
	curmode_(-1),
	virtualw_(1),
	virtualh_(1),
	switchdelay_(1),
	screenTexture_(NULL),
	screenTextureH_(0),
	screenTextureW_(0),
	extString(NULL)
{	
}

GraphicsBackend::~GraphicsBackend()
{
	if (initialized_)
	{
		remove_int(update_frame_counter);
		screen = hw_screen_;
		destroy_bitmap(backbuffer_);
		destroy_bitmap(convertbuffer_);
		allegro_gl_set_allegro_mode();
		clear_to_color(screen, 0);
		allegro_gl_unset_allegro_mode();
	}
	if(nativebuffer_)
		destroy_bitmap(nativebuffer_);
}

void GraphicsBackend::readConfigurationOptions(const std::string &prefix)
{
	std::string section = prefix + "-graphics";
	const char *secname = section.c_str();

	screenw_ = get_config_int(secname, "resx", 640);
	screenh_ = get_config_int(secname, "resy", 480);
	fullscreen_ = get_config_int(secname, "fullscreen", 0) != 0;
	native_ = get_config_int(secname, "native", 1) != 0;
	fps_ = get_config_int(secname, "fps", 60);
}

void GraphicsBackend::writeConfigurationOptions(const std::string &prefix)
{
	std::string section = prefix + "-graphics";
	const char *secname = section.c_str();

	set_config_int(secname, "resx", screenw_);
	set_config_int(secname, "resy", screenh_);
	set_config_int(secname,"fullscreen", isFullscreen() ? 1 : 0 );
	set_config_int(secname, "native", native_ ? 1 : 0);
	set_config_int(secname, "fps", fps_);
}

bool GraphicsBackend::initialize()
{
	if (initialized_)
		return true;

	LOCK_VARIABLE(frame_counter);
	LOCK_FUNCTION(update_frame_counter);
	install_int_ex(update_frame_counter, BPS_TO_TIMER(fps_));
	
	backbuffer_ = create_bitmap_ex(8, virtualScreenW(), virtualScreenH());
	initialized_ = true;

	bool ok = trySettingVideoMode();

	return ok;
}

void GraphicsBackend::waitTick()
{
	if (!initialized_)
		return;

	while (frame_counter == 0)
		rest(0);
	frame_counter = 0;
}

bool GraphicsBackend::showBackBuffer()
{
#ifdef _WINDOWS
	if (windowsFullscreenFix_)
	{
		windowsFullscreenFix_ = false;
		if (fullscreen_)
			if (!trySettingVideoMode())
				return false;
	}
#endif
	Backend::mouse->renderCursor(backbuffer_);

	// Allegro crashes if you call set_palette and screen does not point to the hardware buffer
	if (get_color_depth() == 8)
	{
		screen = hw_screen_;
		Backend::palette->applyPaletteToScreen();
		screen = backbuffer_;
	}

	Backend::palette->selectPalette();

	screen = hw_screen_;
	BITMAP *src = NULL;
	if (native_)
	{
		set_color_conversion(COLORCONV_TOTAL);
		blit(backbuffer_, convertbuffer_, 0, 0, 0, 0, virtualScreenW(), virtualScreenH());
		stretch_blit(convertbuffer_, nativebuffer_, 0, 0, virtualScreenW(), virtualScreenH(), 0, 0, SCREEN_W, SCREEN_H);
		src = nativebuffer_;
	}
	else
	{
		set_color_conversion(COLORCONV_TOTAL);
		blit(backbuffer_, convertbuffer_, 0, 0, 0, 0, virtualScreenW(), virtualScreenH());
		src = convertbuffer_;
	}
	//std::vector<GLubyte> srcData;// (virtualw_ * virtualh_ * 4, 0);
	GLubyte *srcData = new GLubyte[virtualw_ * virtualh_ * 4];
	for (int i = 0; i < src->h; i++)
	{
		//srcData.insert(srcData.end(), (GLubyte*)src->line[i], (GLubyte*)src->line[i]+(src->w*4));
		memcpy(srcData + (i*virtualw_ * 4), src->line[i], virtualw_ * 4);
	}

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, screenTexture_);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, virtualw_, virtualh_, GL_RGBA, GL_UNSIGNED_BYTE, src->line[0]);

	delete srcData;

	glDisable(GL_BLEND);

	glBegin(GL_QUADS);
	glColor4ub(255, 128, 255, 255);
	float temp = 1.0f * virtualw_ / screenTextureW_;
	float temp2 = 1.0f * virtualh_ / screenTextureH_;
	glTexCoord2f(0.0f, 0.0f);									glVertex3f(0.0f, 0.0f, 0.0f);
	glTexCoord2f(temp, 0.0f);									glVertex3f(virtualw_ * 1.0f, 0.0f, 0.0f);
	glTexCoord2f(temp, temp2);									glVertex3f(virtualw_ * 1.0f, virtualh_ * 1.0f, 0.0f);
	glTexCoord2f(0.0f, temp2);									glVertex3f(0.0f, virtualh_ * 1.0f, 0.0f);
	glEnd();
	allegro_gl_flip();
	screen = backbuffer_;
	Backend::mouse->unrenderCursor(backbuffer_);
	frames_this_second++;
	return true;
}

bool GraphicsBackend::setScreenResolution(int w, int h)
{
	if (w != screenw_ || h != screenh_)
	{
		screenw_ = w;
		screenh_ = h;
		if (initialized_ && !fullscreen_)
		{
			return trySettingVideoMode();
		}
	}
	return true;
}

bool GraphicsBackend::setFullscreen(bool fullscreen)
{
	if (fullscreen != fullscreen_)
	{
		fullscreen_ = fullscreen;
		if (initialized_)
		{
			return trySettingVideoMode();
		}
	}
	return true;
}

bool GraphicsBackend::setUseNativeColorDepth(bool native)
{
	if (native_ != native)
	{
		native_ = native;
		if (initialized_)
		{
			return trySettingVideoMode();
		}
	}
	return true;
}

int GraphicsBackend::getLastFPS()
{
	return prev_frames_this_second;
}

void GraphicsBackend::setVideoModeSwitchDelay(int msec)
{
	switchdelay_ = msec;
}

void GraphicsBackend::registerVirtualModes(const std::vector<std::pair<int, int> > &desiredModes)
{
	virtualmodes_ = desiredModes;
	curmode_ = -1;
	if(initialized_)
		trySettingVideoMode();
}

bool GraphicsBackend::trySettingVideoMode()
{
	if (!initialized_)
		return false;

	Backend::mouse->setCursorVisibility(false);

	screen = hw_screen_;

	//int depth = native_ ? desktop_color_depth() : 8;
	int depth = desktop_color_depth();

	bool ok = false;

	if (fullscreen_)
	{
		// Try each registered mode, in decreasing succession
		int nmodes = (int)virtualmodes_.size();
		
		for (int i = 0; i < nmodes; i++)
		{
			int w = virtualmodes_[i].first;
			int h = virtualmodes_[i].second;

			allegro_gl_set(AGL_Z_DEPTH, 8);
			allegro_gl_set(AGL_COLOR_DEPTH, depth);
			allegro_gl_set(AGL_SUGGEST, AGL_Z_DEPTH | AGL_COLOR_DEPTH);
			allegro_gl_set(AGL_REQUIRE, AGL_DOUBLEBUFFER);

			if (set_gfx_mode(GFX_OPENGL_FULLSCREEN, w, h, 0, 0) != 0)
			{
				Z_message("Unable to set gfx mode at -%d %dbpp %d x %d \n", GFX_OPENGL_FULLSCREEN, depth, w, h);
				rest(switchdelay_);
			}
			else
			{
				ok = true;
				break;
			}
		}

		if (!ok)
		{
			// Can't do fullscreen
			Z_message("Fullscreen mode not supported by your driver. Using windowed mode.");
			fullscreen_ = false;
		}
	}

	if(!ok)
	{
		// Try the prescribed resolution
		allegro_gl_set(AGL_Z_DEPTH, 8);
		allegro_gl_set(AGL_COLOR_DEPTH, depth);
		allegro_gl_set(AGL_SUGGEST, AGL_Z_DEPTH | AGL_COLOR_DEPTH);
		allegro_gl_set(AGL_REQUIRE, AGL_DOUBLEBUFFER);

		if (set_gfx_mode(GFX_OPENGL_WINDOWED, screenw_, screenh_, 0, 0) != 0)
		{
			Z_message("Unable to set gfx mode at -%d %dbpp %d x %d \n", GFX_OPENGL_WINDOWED, depth, screenw_, screenh_);
			rest(switchdelay_);
		}
		else
		{
			ok = true;
		}
	}

	// Try each of the registered mode, in succession
	if (!ok)
	{
		int nmodes = (int)virtualmodes_.size();

		for (int i = 0; i < nmodes; i++)
		{
			int w = virtualmodes_[i].first;
			int h = virtualmodes_[i].second;
			if (w == screenw_ && h == screenh_)
			{
				// we already tried this combo, don't waste the user's time
				continue;
			}

			allegro_gl_set(AGL_Z_DEPTH, 8);
			allegro_gl_set(AGL_COLOR_DEPTH, depth);
			allegro_gl_set(AGL_SUGGEST, AGL_Z_DEPTH | AGL_COLOR_DEPTH);
			allegro_gl_set(AGL_REQUIRE, AGL_DOUBLEBUFFER);

			if (set_gfx_mode(GFX_OPENGL_WINDOWED, w, h, 0, 0) != 0)
			{
				Z_message("Unable to set gfx mode at -%d %dbpp %d x %d \n", GFX_OPENGL_WINDOWED, depth, w, h);
				rest(switchdelay_);
			}
			else
			{
				Z_message("Desired resolution not supported; using %d x %d instead\n", w, h);
				ok = true;
				break;
			}
		}
	}

	if (!ok)
	{
		// give up
		return false;
	}

	//Get OpenGL extensions list
	extString = glGetString(GL_EXTENSIONS);

	screenw_ = SCREEN_W;
	screenh_ = SCREEN_H;

	if(get_color_depth() == 8) Backend::palette->applyPaletteToScreen();

	hw_screen_ = screen;
		
	set_display_switch_mode(fullscreen_ ? SWITCH_BACKAMNESIA : SWITCH_BACKGROUND);
	set_display_switch_callback(SWITCH_IN, &onSwitchIn);
	set_display_switch_callback(SWITCH_OUT, &onSwitchOut);

	int nmodes = (int)virtualmodes_.size();
	int newmode = -1;

	// first check for an exact integer match
	for (int i = 0; i < nmodes; i++)
	{
		if (screenw_ % virtualmodes_[i].first == 0)
		{
			int quot = screenw_ / virtualmodes_[i].first;
			if (screenh_ == quot * virtualmodes_[i].second)
			{
				newmode = i;
				break;
			}
		}
	}

	// fall back to the closest resolution that fits
	if (newmode == -1)
	{
		for (int i = 0; i < nmodes; i++)
		{
			if (virtualmodes_[i].first <= screenw_ && virtualmodes_[i].second <= screenh_)
			{
				newmode = i;
				break;
			}
		}
	}

	if (newmode == -1)
		return false;

	curmode_ = newmode;
	if(virtualw_ != virtualmodes_[newmode].first || virtualh_ != virtualmodes_[newmode].second)
	{
		virtualw_ = virtualmodes_[newmode].first;
		virtualh_ = virtualmodes_[newmode].second;
		destroy_bitmap(backbuffer_);
		backbuffer_ = create_bitmap_ex(8, virtualw_, virtualh_);
		convertbuffer_ = create_bitmap_ex(32, virtualw_, virtualh_);
		clear_to_color(backbuffer_, 0);
		clear_to_color(convertbuffer_, 0);
	}
	if (native_)
	{
		if (nativebuffer_)
			destroy_bitmap(nativebuffer_);

		nativebuffer_ = create_bitmap_ex(32, SCREEN_W, SCREEN_H);
	}

	//Initialize the OpenGL state to support 2D drawing
	glMatrixMode(GL_PROJECTION); //Projection matrix
	glLoadIdentity(); //Clear it
	glOrtho(0.0f, virtualw_ * 1.0f, virtualh_ * 1.0f, 0.0f, -100.0f, 100.0f); //Translate the projection viewport to match the virtual screen size.
	glMatrixMode(GL_MODELVIEW); //Model-view matrix
	glLoadIdentity(); //Clear it

	//Disable a bunch of stuff we don't need so we can display the screen properly.
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	//Create and initialize texture to represent the screen. AllegroGL seems to use glDrawPixelsfor screen drawing which is not really satisfactory
	screenTextureW_ = 2048;
	screenTextureH_ = 2048;
	while (screenTextureW_ > virtualw_ && screenTextureW_ > 1) screenTextureW_ /= 2;
	while (screenTextureH_ > virtualh_ && screenTextureH_ > 1) screenTextureH_ /= 2;
	screenTextureW_ *= 2;
	screenTextureH_ *= 2;

	glGenTextures(1, &screenTexture_);
	glBindTexture(GL_TEXTURE_2D, screenTexture_);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	//Clear texture to 0 color
	std::vector<GLubyte> empty(screenTextureW_ * screenTextureH_ * 4, 0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, screenTextureW_, screenTextureH_, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

	screen = backbuffer_;
	showBackBuffer();

	return true;
}

int GraphicsBackend::screenW()
{
	return SCREEN_W;
}

int GraphicsBackend::screenH()
{
	return SCREEN_H;
}

void GraphicsBackend::physicalToVirtual(int &x, int &y)
{
	if (initialized_)
	{
		x = x*virtualScreenW() / screenW();
		y = y*virtualScreenH() / screenH();
	}
}

void GraphicsBackend::virtualToPhysical(int &x, int &y)
{
	if (initialized_)
	{
		x = x*screenW() / virtualScreenW();
		y = y*screenH() / virtualScreenH();
	}
}

void GraphicsBackend::registerSwitchCallbacks(void(*switchin)(), void(*switchout)())
{
	switch_in_func_ = switchin;
	switch_out_func_ = switchout;
	if (initialized_)
	{
		set_display_switch_callback(SWITCH_IN, &onSwitchIn);
		set_display_switch_callback(SWITCH_OUT, &onSwitchOut);
	}
}

const char *GraphicsBackend::videoModeString()
{
	if (!initialized_)
		return "Uninitialized";

	int VidMode = gfx_driver->id;
#ifdef ALLEGRO_DOS

	switch (VidMode)
	{
	case GFX_MODEX:
		return "VGA Mode X";
		break;

	case GFX_VESA1:
		return "VESA 1.x";
		break;

	case GFX_VESA2B:
		return "VESA2 Banked";
		break;

	case GFX_VESA2L:
		return "VESA2 Linear";
		break;

	case GFX_VESA3:
		return "VESA3";
		break;

	default:
		return "Unknown... ?";
		break;
	}

#elif defined(ALLEGRO_WINDOWS)

	switch (VidMode)
	{
	case GFX_DIRECTX:
		return "DirectX Hardware Accelerated";
		break;

	case GFX_DIRECTX_SOFT:
		return "DirectX Software Accelerated";
		break;

	case GFX_DIRECTX_SAFE:
		return "DirectX Safe";
		break;

	case GFX_DIRECTX_WIN:
		return "DirectX Windowed";
		break;

	case GFX_GDI:
		return "GDI";
		break;

	default:
		return "Unknown... ?";
		break;
	}

#elif defined(ALLEGRO_MACOSX)

	switch (VidMode)
	{
	case GFX_SAFE:
		return "MacOS X Safe";
		break;

	case GFX_QUARTZ_FULLSCREEN:
		return "MacOS X Fullscreen Quartz";
		break;

	case GFX_QUARTZ_WINDOW:
		return "MacOS X Windowed Quartz";
		break;

	default:
		return "Unknown... ?";
		break;
	}

#elif defined(ALLEGRO_UNIX)

	switch (VidMode)
	{
	case GFX_AUTODETECT_WINDOWED:
		return "Autodetect Windowed";
		break;

	default:
		return "Unknown... ?";
		break;
	}
#endif
	return "Unknown... ?";
}

int GraphicsBackend::desktopW()
{
	int w, h;
	get_desktop_resolution(&w, &h);
	return w;
}

int GraphicsBackend::desktopH()
{
	int w, h;
	get_desktop_resolution(&w, &h);
	return h;
}