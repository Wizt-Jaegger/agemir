#include "main_window.hpp"

#include "../agemir_api.h"

#include <adwaita.h>
#include <gtkmm/cssprovider.h>
#include <gtkmm/stylecontext.h>

#include <array>
#include <cstdlib>
#include <utility>

namespace {
Gtk::Box *page_box() {
    auto *box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 14);
    box->add_css_class("page");
    return box;
}

void add_field(Gtk::Box &box, const Glib::ustring &label, Gtk::Widget &field) {
    auto *caption = Gtk::make_managed<Gtk::Label>(label);
    caption->set_halign(Gtk::Align::START);
    box.append(*caption);
    box.append(field);
}
} // namespace

AgemirWindow::AgemirWindow() {
    runtime_ = agemir_runtime_create();
    set_title("Agemir");
    set_default_size(880, 660);

    auto provider = Gtk::CssProvider::create();
    provider->load_from_resource("/org/agemir/Agemir/agemir.css");
    Gtk::StyleContext::add_provider_for_display(get_display(), provider,
                                                GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    auto *header = Gtk::make_managed<Gtk::HeaderBar>();
    auto *switcher = Gtk::make_managed<Gtk::StackSwitcher>();
    switcher->set_stack(stack_);
    header->set_title_widget(*switcher);
    set_titlebar(*header);

    auto *workspace = page_box();
    auto *heading = Gtk::make_managed<Gtk::Label>("Build a project with Agemir");
    heading->set_halign(Gtk::Align::START);
    heading->add_css_class("heading");
    workspace->append(*heading);
    auto *hint = Gtk::make_managed<Gtk::Label>(
        "Describe the work, choose a destination, and keep the window open while the core runs.");
    hint->set_halign(Gtk::Align::START);
    hint->set_wrap();
    hint->add_css_class("muted");
    workspace->append(*hint);

    project_path_.set_placeholder_text("Project folder, for example /home/me/projects/weather");
    project_name_.set_placeholder_text("Project name, for example weather");
    model_.set_placeholder_text("Model (leave blank for the configured model)");
    description_.set_wrap_mode(Gtk::WrapMode::WORD_CHAR);
    description_.set_size_request(-1, 170);
    add_field(*workspace, "Project location", project_path_);
    add_field(*workspace, "Project name", project_name_);
    add_field(*workspace, "Task or project request", description_);
    add_field(*workspace, "Model", model_);
    workspace->append(graphical_);
    auto *actions = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
    generate_.add_css_class("suggested-action");
    actions->append(generate_);
    actions->append(run_);
    actions->append(pause_);
    pause_.set_sensitive(false);
    workspace->append(*actions);
    status_.set_halign(Gtk::Align::START);
    status_.set_wrap();
    status_.add_css_class("muted");
    workspace->append(status_);
    stack_.add(*workspace, "workspace", "Workspace");

    auto *settings = page_box();
    auto *settings_heading = Gtk::make_managed<Gtk::Label>("Settings");
    settings_heading->set_halign(Gtk::Align::START);
    settings_heading->add_css_class("heading");
    settings->append(*settings_heading);
    user_name_.set_placeholder_text("Name shown by the CLI and native app");
    default_project_path_.set_placeholder_text("Folder used when starting a new request");
    language_.set_placeholder_text("Language: 1 English, 2 Spanish, 3 French, 4 German, 5 Japanese");
    add_field(*settings, "Name", user_name_);
    add_field(*settings, "Default project folder", default_project_path_);
    add_field(*settings, "Language", language_);
    save_settings_.add_css_class("suggested-action");
    settings->append(save_settings_);
    runtime_check_.set_halign(Gtk::Align::START);
    runtime_check_.set_wrap();
    runtime_check_.add_css_class("muted");
    settings->append(runtime_check_);
    stack_.add(*settings, "settings", "Settings");
    set_child(stack_);

    generate_.signal_clicked().connect(sigc::mem_fun(*this, &AgemirWindow::submit_generate));
    run_.signal_clicked().connect(sigc::mem_fun(*this, &AgemirWindow::submit_run));
    pause_.signal_clicked().connect(sigc::mem_fun(*this, &AgemirWindow::request_pause));
    save_settings_.signal_clicked().connect(sigc::mem_fun(*this, &AgemirWindow::save_settings));
    completed_.connect(sigc::mem_fun(*this, &AgemirWindow::finish_request));
    runtime_check_completed_.connect(
        sigc::mem_fun(*this, &AgemirWindow::finish_runtime_check));
    load_settings();
    check_runtime();
    set_message(runtime_ ? "Ready. Requests run outside the GTK UI thread." :
                "Could not initialize the Agemir C runtime.", !runtime_);
}

AgemirWindow::~AgemirWindow() {
    status_poll_.disconnect();
    if (worker_.joinable()) worker_.join();
    if (runtime_check_worker_.joinable()) runtime_check_worker_.join();
    agemir_runtime_destroy(runtime_);
}

void AgemirWindow::set_message(const Glib::ustring &text, bool is_error) {
    status_.set_text(text);
    status_.remove_css_class("status-ok");
    status_.remove_css_class("status-error");
    status_.add_css_class(is_error ? "status-error" : "status-ok");
}

void AgemirWindow::load_settings() {
    if (!runtime_) return;
    std::array<char, 64> value{};
    if (agemir_runtime_get_setting(runtime_, "username", value.data(), value.size()))
        user_name_.set_text(value.data());
    if (agemir_runtime_get_setting(runtime_, "model", value.data(), value.size()))
        model_.set_text(value.data());
    if (agemir_runtime_get_setting(runtime_, "language", value.data(), value.size()))
        language_.set_text(value.data());
    std::array<char, 512> project_path{};
    if (agemir_runtime_get_setting(runtime_, "project_path", project_path.data(), project_path.size())) {
        default_project_path_.set_text(project_path.data());
        if (project_path_.get_text().empty()) project_path_.set_text(project_path.data());
    }
    int projects = agemir_runtime_project_count(runtime_);
    runtime_check_.set_text("C runtime initialized; registered projects: " + std::to_string(projects) + ".");
}

void AgemirWindow::check_runtime() {
    if (!runtime_) {
        runtime_check_.set_text("Could not initialize the Agemir C runtime.");
        runtime_check_.remove_css_class("status-ok");
        runtime_check_.add_css_class("status-error");
        return;
    }
    if (runtime_check_worker_.joinable()) runtime_check_worker_.join();
    {
        std::lock_guard<std::mutex> lock(result_mutex_);
        ollama_available_ = -1;
    }
    runtime_check_.set_text("C runtime initialized. Checking Ollama availability…");
    runtime_check_worker_ = std::thread([this] {
        const int available = agemir_runtime_ollama_available();
        {
            std::lock_guard<std::mutex> lock(result_mutex_);
            ollama_available_ = available;
        }
        runtime_check_completed_.emit();
    });
}

void AgemirWindow::finish_runtime_check() {
    if (runtime_check_worker_.joinable()) runtime_check_worker_.join();
    int available;
    {
        std::lock_guard<std::mutex> lock(result_mutex_);
        available = ollama_available_;
    }
    const bool found = available > 0;
    runtime_check_.set_text(found ?
        "Ollama executable found. Server and model readiness are checked when work begins." :
        "Ollama executable not found. Install Ollama before starting a request.");
    runtime_check_.remove_css_class("status-ok");
    runtime_check_.remove_css_class("status-error");
    runtime_check_.add_css_class(found ? "status-ok" : "status-error");
}

void AgemirWindow::save_settings() {
    if (!runtime_ || busy_) return;
    const std::string name = user_name_.get_text();
    const std::string model = model_.get_text();
    const std::string project_path = default_project_path_.get_text();
    const std::string language = language_.get_text();
    char *end = nullptr;
    const long language_number = std::strtol(language.c_str(), &end, 10);
    const bool valid = !name.empty() && name.size() < 64 && name.find_first_of("\r\n") == std::string::npos &&
                       !model.empty() && model.size() < 64 && model.find_first_of("\r\n") == std::string::npos &&
                       project_path.size() < 512 && project_path.find_first_of("\r\n") == std::string::npos &&
                       !language.empty() && end && *end == '\0' && language_number >= 1 && language_number <= 5;
    const bool ok = valid &&
                    agemir_runtime_set_setting(runtime_, "username", name.c_str()) &&
                    agemir_runtime_set_setting(runtime_, "model", model.c_str()) &&
                    agemir_runtime_set_setting(runtime_, "project_path", project_path.c_str()) &&
                    agemir_runtime_set_setting(runtime_, "language", language.c_str());
    set_message(ok ? "Settings saved." :
                "Check the name, model, project folder, and language (1 to 5).", !ok);
    if (ok) load_settings();
}

void AgemirWindow::submit_generate() { begin_request(AGEMIR_GENERATE_PROJECT); }
void AgemirWindow::submit_run() { begin_request(AGEMIR_RUN_PROJECT); }

void AgemirWindow::begin_request(AgemirOperation operation) {
    if (!runtime_ || busy_) return;
    const std::string path = project_path_.get_text();
    const std::string name = project_name_.get_text();
    const std::string model = model_.get_text();
    const std::string description = description_.get_buffer()->get_text();
    const bool graphical = graphical_.get_active();
    if (path.empty() || (operation == AGEMIR_GENERATE_PROJECT && (name.empty() || description.empty()))) {
        set_message("Enter a project location and, for a new project, a name and request.", true);
        return;
    }
    if (worker_.joinable()) worker_.join();
    busy_ = true;
    active_run_ = operation == AGEMIR_RUN_PROJECT;
    pause_requested_ = false;
    active_project_path_ = path;
    generate_.set_sensitive(false);
    run_.set_sensitive(false);
    pause_.set_sensitive(false);
    save_settings_.set_sensitive(false);
    set_message("Working… Agemir is running in the background.");
    worker_ = std::thread([this, operation, path, name, model, description, graphical] {
        AgemirRequest request{};
        request.operation = operation;
        request.project_path = path.c_str();
        request.project_name = name.c_str();
        request.description = description.c_str();
        request.model = model.empty() ? nullptr : model.c_str();
        request.graphical = graphical ? 1 : 0;
        std::array<char, 512> error{};
        bool ok = agemir_runtime_execute(runtime_, &request, error.data(), error.size()) != 0;
        AgemirStatus snapshot{};
        const bool has_status = agemir_runtime_get_status(runtime_, path.c_str(), &snapshot) != 0;
        std::string message = ok ? "Completed." : (error[0] ? error.data() : "The request did not complete.");
        if (has_status) message += " Status: " + std::string(snapshot.state) + ".";
        {
            std::lock_guard<std::mutex> lock(result_mutex_);
            result_ok_ = ok;
            result_message_ = std::move(message);
        }
        completed_.emit();
    });
    status_poll_ = Glib::signal_timeout().connect(
        sigc::mem_fun(*this, &AgemirWindow::refresh_status), 400);
}

void AgemirWindow::finish_request() {
    std::lock_guard<std::mutex> lock(result_mutex_);
    status_poll_.disconnect();
    busy_ = false;
    pause_.set_sensitive(false);
    generate_.set_sensitive(true);
    run_.set_sensitive(true);
    save_settings_.set_sensitive(true);
    set_message(result_message_, !result_ok_);
}

bool AgemirWindow::refresh_status() {
    if (!busy_ || !runtime_ || !active_run_) return busy_;

    AgemirStatus snapshot{};
    if (!agemir_runtime_get_status(runtime_, active_project_path_.c_str(), &snapshot))
        return true;

    const std::string state(snapshot.state);
    const bool failed = state == "failed" || state == "error";
    std::string text = state.empty() ? "Working" : state;
    if (snapshot.attempts > 0)
        text += " · attempt " + std::to_string(snapshot.attempts);
    if (snapshot.task[0]) text += " · " + std::string(snapshot.task);
    if (snapshot.reason[0]) text += " · " + std::string(snapshot.reason);
    set_message(text, failed);

    const bool can_pause = state == "running" && !pause_requested_;
    pause_.set_sensitive(can_pause);
    return true;
}

void AgemirWindow::request_pause() {
    if (!runtime_ || !busy_ || !active_run_ || pause_requested_) return;

    std::array<char, 256> error{};
    if (agemir_runtime_pause(runtime_, active_project_path_.c_str(), error.data(), error.size())) {
        pause_requested_ = true;
        pause_.set_sensitive(false);
        set_message("Pause requested. Agemir will stop at the next safe checkpoint.");
    } else {
        set_message(error[0] ? error.data() : "Could not request a pause.", true);
    }
}
