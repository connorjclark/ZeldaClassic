// #include "zalleg/zalleg.h"

#include "allegro5/monitor.h"
#include "base/zc_alleg.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_allegro5.h"

static float scale;

void draw(ImGuiIO& io)
{
    // static int c;
    // if (c != 0)
    // {
    //     c -= 1;
    //     return;
    // }

    // c = 10;


    ImGui_ImplAllegro5_NewFrame();
    ImGui::NewFrame();

    // ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    // ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    // ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

    static float f = 0.0f;
    static int counter = 0;
    static bool my_tool_active = true;

    auto pos = ImGui::GetMainViewport()->Pos;
    pos.x = 0;
    pos.y += 10;
    ImGui::SetNextWindowPos(pos);
    ImGui::Begin(" ", &my_tool_active, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);

    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Open..", "Ctrl+O")) { /* Do stuff */ }
            if (ImGui::MenuItem("Save", "Ctrl+S"))   { /* Do stuff */ }
            if (ImGui::MenuItem("Close", "Ctrl+W"))  { my_tool_active = false; }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    // ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 0, 0, 255));
    ImGui::BeginGroup();
    {
        ImGui::GetFont()->Scale *= 2;
        ImGui::PushFont(ImGui::GetFont());
        ImGui::GetFont()->Scale /= 2;

        ImGui::AlignTextToFramePadding();

        ImGui::TextUnformatted("ZQuest Classic");

        bool b;
        if (ImGui::Button("Play"))
            b = true;
        if (ImGui::Button("Create"))
            b = true;
        if (ImGui::Button("Settings"))
            b = true;
        if (ImGui::Button("Help"))
            b = true;

        ImGui::PopFont();
    }
    ImGui::EndGroup();
    // ImGui::PopStyleColor(1);

    ImGui::SameLine();

    ImGui::BeginGroup();
    {
        // ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 0, 0, 255));
        ImGui::TextUnformatted("Foo");
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
        // ImGui::PopStyleColor(1);
    }
    ImGui::EndGroup();


    ImGui::End();
    ImGui::Render();
}

int main(int argc, char* argv[])
{
    // TODO
    // zalleg_setup_allegro(App::launcher, argc, argv);

    if (!al_init())
	{
		// Z_error_fatal("Failed to init allegro: %s\n", "al_init");
	}
	// if (allegro_init() != 0)
	// {
	// 	// Z_error_fatal("Failed to init allegro: %s\n%s\n", "allegro_init", allegro_error);
	// }

	int dpi = al_get_monitor_dpi(0);
	scale = (float)al_get_monitor_dpi(0) / 96;

    al_install_keyboard();
    al_install_mouse();
    al_init_primitives_addon();
    al_set_new_display_option(ALLEGRO_VSYNC, 0, ALLEGRO_SUGGEST);
    al_set_new_display_flags(ALLEGRO_RESIZABLE);
    ALLEGRO_DISPLAY* display = al_create_display(912 * scale, 684 * scale);
    al_set_window_title(display, "Dear ImGui Allegro 5 example");
    ALLEGRO_EVENT_QUEUE* queue = al_create_event_queue();
    al_register_event_source(queue, al_get_display_event_source(display));
    al_register_event_source(queue, al_get_keyboard_event_source());
    al_register_event_source(queue, al_get_mouse_event_source());

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // Enable Docking

    ImGui::StyleColorsDark();

    ImGui_ImplAllegro5_Init(display);

    io.Fonts->AddFontDefault()->Scale = scale;

    bool running = true;
    while (running)
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        ALLEGRO_EVENT ev;
        while (al_get_next_event(queue, &ev))
        {
            ImGui_ImplAllegro5_ProcessEvent(&ev);
            if (ev.type == ALLEGRO_EVENT_DISPLAY_CLOSE)
                running = false;
            if (ev.type == ALLEGRO_EVENT_DISPLAY_RESIZE)
            {
                ImGui_ImplAllegro5_InvalidateDeviceObjects();
                al_acknowledge_resize(display);
                ImGui_ImplAllegro5_CreateDeviceObjects();
            }
        }

        draw(io);

        al_clear_to_color(al_map_rgb(0, 0, 0));
        ImGui_ImplAllegro5_RenderDrawData(ImGui::GetDrawData());
        al_flip_display();
    }

    ImGui_ImplAllegro5_Shutdown();
    ImGui::DestroyContext();
    al_destroy_event_queue(queue);
    al_destroy_display(display);

    return 0;
}

bool handle_close_btn_quit()
{
    return false;
}
