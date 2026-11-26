#include "main_window.hpp"

#include "../agemir_api.h"

#include <adwaita.h>
#include <gtkmm/application.h>

#include <cstring>

int main(int argc, char *argv[]) {
    if (argc == 2 && std::strcmp(argv[1], "--cli") == 0)
        return agemir_cli_main();

    adw_init();
    auto app = Gtk::Application::create("org.agemir.Agemir");
    app->signal_activate().connect([&app] {
        auto *window = new AgemirWindow();
        window->set_application(app);
        window->present();
    });
    return app->run(argc, argv);
}
