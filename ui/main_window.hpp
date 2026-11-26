#pragma once

#include "../agemir_api.h"

#include <gtkmm.h>

#include <mutex>
#include <string>
#include <thread>

class AgemirWindow final : public Gtk::ApplicationWindow {
public:
    AgemirWindow();
    ~AgemirWindow() override;

private:
    void submit_generate();
    void submit_run();
    void begin_request(AgemirOperation operation);
    void finish_request();
    bool refresh_status();
    void request_pause();
    void save_settings();
    void load_settings();
    void set_message(const Glib::ustring& text, bool is_error = false);

    AgemirRuntime *runtime_ = nullptr;
    std::thread worker_;
    std::mutex result_mutex_;
    Glib::Dispatcher completed_;
    sigc::connection status_poll_;
    bool busy_ = false;
    bool active_run_ = false;
    bool pause_requested_ = false;
    bool result_ok_ = false;
    std::string active_project_path_;
    std::string result_message_;

    Gtk::Stack stack_;
    Gtk::Entry project_path_;
    Gtk::Entry project_name_;
    Gtk::Entry model_;
    Gtk::TextView description_;
    Gtk::CheckButton graphical_{"Graphical project"};
    Gtk::Button generate_{"Create project from request"};
    Gtk::Button run_{"Run existing project"};
    Gtk::Button pause_{"Pause run"};
    Gtk::Label status_;

    Gtk::Entry user_name_;
    Gtk::Entry default_project_path_;
    Gtk::Entry language_;
    Gtk::Label runtime_check_;
    Gtk::Button save_settings_{"Save settings"};
};
