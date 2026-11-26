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
    void show_main(); void show_run(); void show_generate(); void show_import(); void show_benchmark(); void show_settings();
    void refresh_projects(); void refresh_settings();
    void start(AgemirOperation, const std::string& = {}, const std::string& = {}, const std::string& = {}, const std::string& = {}, bool = false);
    void finish_request(); bool refresh_status(); void request_pause();
    void choose_file(Gtk::Entry&, bool); void complete_choose(int);
    void set_status(const Glib::ustring&, bool = false); void append_log(const Glib::ustring&); void alert(const Glib::ustring&, const Glib::ustring&, bool = false); void confirm_close();
    AgemirRuntime* runtime_ = nullptr; std::thread worker_; std::mutex result_mutex_; Glib::Dispatcher completed_; sigc::connection status_poll_;
    Glib::RefPtr<Gtk::FileChooserNative> chooser_; Gtk::Entry* chooser_target_ = nullptr;
    bool busy_ = false, active_run_ = false, pause_requested_ = false, result_ok_ = false;
    std::string active_project_path_, result_message_;
    Gtk::Stack stack_; Gtk::Box main_page_{Gtk::Orientation::VERTICAL, 12}, run_page_{Gtk::Orientation::VERTICAL, 12}, generate_page_{Gtk::Orientation::VERTICAL, 12}, import_page_{Gtk::Orientation::VERTICAL, 12}, benchmark_page_{Gtk::Orientation::VERTICAL, 12}, settings_page_{Gtk::Orientation::VERTICAL, 12};
    Gtk::ListBox projects_; Gtk::Entry generated_name_, generated_path_, import_template_, import_path_, user_name_, custom_model_; Gtk::TextView generated_description_, log_; Gtk::CheckButton generated_graphical_{"Create a graphical project"}, automatic_model_{"Automatic (recommended for this hardware)"}; Gtk::DropDown language_, catalog_model_; Gtk::Label current_model_, status_; Gtk::Button pause_{"Pause after the current safe checkpoint"};
};
