#include <SDL.h>
#include <RmlUi/Core.h>
#include <RmlUi/Core/Input.h>
#include <RmlUi/Debugger/Debugger.h>
#include <RmlUi_Backend.h>

// Window size
#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 480

// Image size
#define IMAGE_WIDTH 256
#define IMAGE_HEIGHT 256

// static SDL_Window* mSdlWindow;
// static SDL_Renderer* mSdlRenderer;
// static Rml::FileInterface* mFileInterface;
// static Rml::SystemInterface* mSystemInterface;
// static Rml::RenderInterface* mRenderInterface;
// static Rml::Context* mRmlContext;
// static Rml::String mFile;
// static Rml::String mTitle;
// static int mX;
// static int mY;
// static int mWidth;
// static int mHeight;
// static bool mActive;

int main(int argc, char* args[])
{
    const int window_width = 1024;
	const int window_height = 768;

	// // Initializes the shell which provides common functionality used by the included samples.
	// if (!Rml::Shell::Initialize())
	// 	return -1;

	// Constructs the system and render interfaces, creates a window, and attaches the renderer.
	if (!Backend::Initialize("Load Document Sample", window_width, window_height, true))
	{
		// Rml::Shell::Shutdown();
		return -1;
	}

	// Install the custom interfaces constructed by the backend before initializing RmlUi.
	Rml::SetSystemInterface(Backend::GetSystemInterface());
	Rml::SetRenderInterface(Backend::GetRenderInterface());

	// RmlUi initialisation.
	Rml::Initialise();

	// Create the main RmlUi context.
	Rml::Context* context = Rml::CreateContext("main", Rml::Vector2i(window_width, window_height));
	if (!context)
	{
		Rml::Shutdown();
		Backend::Shutdown();
		// Rml::Shell::Shutdown();
		return -1;
	}

	// The RmlUi debugger is optional but very useful. Try it by pressing 'F8' after starting this sample.
	Rml::Debugger::Initialise(context);

	// Fonts should be loaded before any documents are loaded.
	{
        const Rml::String directory = "rmlui_assets/";

        struct FontFace {
            const char* filename;
            bool fallback_face;
        };
        FontFace font_faces[] = {
            {"LatoLatin-Regular.ttf", false},
            {"LatoLatin-Italic.ttf", false},
            {"LatoLatin-Bold.ttf", false},
            {"LatoLatin-BoldItalic.ttf", false},
            {"NotoEmoji-Regular.ttf", true},
        };

        for (const FontFace& face : font_faces)
            Rml::LoadFontFace(directory + face.filename, face.fallback_face);
    }


	// Load and show the demo document.
	if (Rml::ElementDocument* document = context->LoadDocument("rmlui_assets/demo.rml"))
		document->Show();

	bool running = true;
	while (running)
	{
		// Handle input and window events.
		running = Backend::ProcessEvents(context, nullptr, true);

		// This is a good place to update your game or application.

		// Always update the context before rendering.
		context->Update();

		// Prepare the backend for taking rendering commands from RmlUi and then render the context.
		Backend::BeginFrame();
		context->Render();
		Backend::PresentFrame();
	}

	// Shutdown RmlUi.
	Rml::Shutdown();

	Backend::Shutdown();
	// Rml::Shell::Shutdown();

	return 0;
}
