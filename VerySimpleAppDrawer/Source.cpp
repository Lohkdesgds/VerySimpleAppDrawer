#include <nlohmann/json.hpp>

#include <allegro5/allegro5.h>
#include <allegro5/allegro_image.h>

#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <Windows.h>

const std::string json_app_conf = "verysimpleappdrawer.json";

void get_max_useful(int&, int&, int&, int&);
BOOL do_run(const std::string&);

struct app_info {
	std::string app_path;
	std::string app_icon;

	ALLEGRO_BITMAP* texture = nullptr; // handled later

	nlohmann::json to_json();
	bool from_json(const nlohmann::json&);
};

int main()
{
	std::cout << "VerySimpleAppDrawer V1.1.0 by Lohk launching..." << std::endl;

	al_init();
	al_init_image_addon();
	al_install_mouse();
	al_install_keyboard();

	ALLEGRO_EVENT_QUEUE* ev_queue = al_create_event_queue();
	if (!ev_queue) {
		std::cout << "Can't create event queue!" << std::endl;
		return 0;
	}

	al_register_event_source(ev_queue, al_get_keyboard_event_source());
	al_register_event_source(ev_queue, al_get_mouse_event_source());

	int mouse_px = 0, mouse_py = 0;
	if (!al_get_mouse_cursor_position(&mouse_px, &mouse_py)) {
		std::cout << "Can't find mouse" << std::endl;
		mouse_py = mouse_px = -1; // skip
	}

	int min_x = 0, min_y = 0, max_x = 0, max_y = 0;
	get_max_useful(min_x, min_y, max_x, max_y);

	unsigned app_icon_size = 0;
	std::vector<app_info> apps;

	std::fstream file;
	file.open(json_app_conf, std::ios_base::binary | std::ios_base::in);
	if (!file.is_open()) {
		std::cout << "Failed to open config!" << std::endl;
		return 0;
	}

	try {
		nlohmann::json json;
		json = nlohmann::json::parse(file);

		for (const auto& i : json["apps"])
		{
			app_info app;
			if (!app.from_json(i)) {
				std::cout << "Configuration is invalid! You need \"app\" array of a object of \"app_path\" and \"app_icon\" (string)." << std::endl;
				return 0;
			}
			apps.push_back(app);
		}

		app_icon_size = json["icon_size"];
	}
	catch (const std::exception& e)
	{
		std::cout << "Failed to read config: " << e.what() << "." << std::endl;
		return 0;
	}
	catch (...)
	{
		std::cout << "Failed to read config: unknown error." << std::endl;
		return 0;
	}

	// has apps

	if (apps.size() == 0)
	{
		std::cout << "No apps?" << std::endl;
		return 0;
	}
	else if (apps.size() == 1)
	{
		std::cout << "One app, direct launch..." << std::endl;
		//WinExec(apps[0].app_path.c_str(), SW_HIDE);
		//system(apps[0].app_path.c_str());

		if (do_run(apps[0].app_path))	std::cout << "Started '" << apps[0].app_path << "' successfully. Ending itself..." << std::endl;
		else							std::cout << "Didn't start '" << apps[0].app_path << "' correctly. Please check args." << std::endl;

		return 0;
	}

	// more apps

	if (app_icon_size > (1 << 16))
	{
		std::cout << "Resolution is invalid! More than 64k px is too big!" << std::endl;
		return 0;
	}


	al_set_new_bitmap_flags(ALLEGRO_VIDEO_BITMAP | ALLEGRO_MIN_LINEAR | ALLEGRO_MAG_LINEAR);
	al_set_new_display_flags(ALLEGRO_OPENGL | ALLEGRO_NOFRAME);

	al_set_new_display_option(ALLEGRO_SAMPLE_BUFFERS, 1, ALLEGRO_SUGGEST);
	al_set_new_display_option(ALLEGRO_SAMPLES, 8, ALLEGRO_SUGGEST);

	const int width = app_icon_size * static_cast<int>(apps.size());
	const int height = app_icon_size;

	if (mouse_px != -1 && mouse_py != -1) {
		int exp_x = mouse_px - (width / 2);
		int exp_y = mouse_py - height;

		if (exp_x < min_x)				exp_x = min_x;
		if ((exp_x + width) > max_x)	exp_x = max_x - width;

		if (exp_y < min_y)				exp_y = min_y;
		if ((exp_y + height) > max_y)	exp_y = max_y - height;

		al_set_new_window_position(exp_x, exp_y);
	}

	ALLEGRO_DISPLAY* display = al_create_display(width, height);

	al_register_event_source(ev_queue, al_get_display_event_source(display));

	if (al_get_display_option(display, ALLEGRO_SAMPLE_BUFFERS)) {
		std::cout << "With multisampling, level " << al_get_display_option(display, ALLEGRO_SAMPLES) << "" << std::endl;
	}
	else {
		std::cout << "Multisampling disabled on this system (configuration)." << std::endl;
	}

	if (!display) {
		std::cout << "Can't create screen!" << std::endl;
		al_destroy_event_queue(ev_queue);
		return 0;
	}

	for (auto& i : apps)
	{
		if (!(i.texture = al_load_bitmap(i.app_icon.c_str()))) {
			std::cout << "Could not open path of app " << i.app_path << std::endl;
			al_destroy_display(display);
			al_destroy_event_queue(ev_queue);
			return 0;
		}
		else std::cout << "Loaded texture " << i.app_icon << std::endl;
	}

	al_clear_to_color(al_map_rgb(50, 50, 50));	

	for (size_t p = 0; p < apps.size(); p++)
	{
		const auto& i = apps[p];
		al_draw_scaled_bitmap(
			i.texture,
			0,
			0,
			al_get_bitmap_width(i.texture),
			al_get_bitmap_height(i.texture),
			app_icon_size * p + 1,
			1,
			app_icon_size - 2,
			app_icon_size - 2,
			0);			
	}

	al_flip_display();

	const auto run_opt = [&](const size_t opt)
	{
		std::cout << "Called to run #" << opt << std::endl;

		for (auto& i : apps) {
			al_destroy_bitmap(i.texture);
			i.texture = nullptr;
		}
		al_destroy_display(display);
		al_destroy_event_queue(ev_queue);

		if (opt == static_cast<size_t>(-1)) std::exit(0); // just dies

		if (opt < apps.size()) {
			if (do_run(apps[opt].app_path))	std::cout << "Started '" << apps[opt].app_path << "' successfully. Ending itself..." << std::endl;
			else							std::cout << "Didn't start '" << apps[opt].app_path << "' correctly. Please check args." << std::endl;
		}
		else{
			std::cout << "This option was invalid!" << std::endl;
		}

		std::exit(0);
	};

	while (1) {
		ALLEGRO_EVENT ev;
		al_wait_for_event(ev_queue, &ev);

		switch (ev.type) {
		case ALLEGRO_EVENT_MOUSE_BUTTON_DOWN:
		{
			const size_t conv = (ev.mouse.x + 1) / app_icon_size;
			run_opt(conv);
		}
		break;
		case ALLEGRO_EVENT_KEY_DOWN:
		{
			//if (ev.keyboard.keycode == ALLEGRO_KEY_ESCAPE) {

			switch (ev.keyboard.keycode) {
			case ALLEGRO_KEY_1:
			case ALLEGRO_KEY_PAD_0:
				run_opt(0);
				break;
			case ALLEGRO_KEY_2:
			case ALLEGRO_KEY_PAD_1:
				run_opt(1);
				break;
			case ALLEGRO_KEY_3:
			case ALLEGRO_KEY_PAD_2:
				run_opt(2);
				break;
			case ALLEGRO_KEY_4:
			case ALLEGRO_KEY_PAD_3:
				run_opt(3);
				break;
			case ALLEGRO_KEY_5:
			case ALLEGRO_KEY_PAD_4:
				run_opt(4);
				break;
			case ALLEGRO_KEY_6:
			case ALLEGRO_KEY_PAD_5:
				run_opt(5);
				break;
			case ALLEGRO_KEY_7:
			case ALLEGRO_KEY_PAD_6:
				run_opt(6);
				break;
			case ALLEGRO_KEY_8:
			case ALLEGRO_KEY_PAD_7:
				run_opt(7);
				break;
			case ALLEGRO_KEY_9:
			case ALLEGRO_KEY_PAD_8:
				run_opt(8);
				break;
			case ALLEGRO_KEY_0:
			case ALLEGRO_KEY_PAD_9:
				run_opt(9);
				break;
			default:
				run_opt(static_cast<size_t>(-1));
				break;
			}

			return 0;
			//}
		}
		break;
		case ALLEGRO_EVENT_DISPLAY_CLOSE:
		case ALLEGRO_EVENT_DISPLAY_SWITCH_OUT:
		{
			std::cout << "Close/switch display called, exiting..." << std::endl;

			for (auto& i : apps) {
				al_destroy_bitmap(i.texture);
				i.texture = nullptr;
			}
			al_destroy_display(display);
			al_destroy_event_queue(ev_queue);

			return 0;
		}
		break;
		}
	}
}

void get_max_useful(int& min_x, int& min_y, int& max_x, int& max_y)
{
	RECT rect;
	SystemParametersInfoA(SPI_GETWORKAREA, 0, &rect, 0);
	min_x = rect.left;
	min_y = rect.top;
	max_x = rect.right;
	max_y = rect.bottom;
	/*int res = rect.bottom - rect.top;
	return GetSystemMetrics(SM_CYSCREEN) - res;*/ // this is if you want the 40 px
}

BOOL do_run(const std::string& command)
{
	BOOL Result = FALSE;
	DWORD retSize;
	LPTSTR pTemp = NULL;

	std::string cmd = "start /min cmd /c \"" + command + "\"";

	Result = (BOOL)ShellExecute(GetActiveWindow(), "OPEN", "cmd", cmd.c_str(), NULL, 0L);
	if (!Result)
	{
		retSize = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_ARGUMENT_ARRAY,
			NULL,
			GetLastError(),
			LANG_NEUTRAL,
			(LPTSTR)&pTemp,
			0,
			NULL);
		MessageBox(NULL, pTemp, "Error", MB_OK);
	}
	return Result;
}

nlohmann::json app_info::to_json()
{
	nlohmann::json json;
	json["app_path"] = app_path;
	json["app_icon"] = app_icon;
	return json;
}

bool app_info::from_json(const nlohmann::json& json)
{
	app_path = json["app_path"];
	app_icon = json["app_icon"];

	return !app_path.empty() && !app_icon.empty();
}