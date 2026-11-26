#include "main_window.hpp"

#include "../agemir_api.h"

#include <adwaita.h>
#include <gtkmm/cssprovider.h>
#include <gtkmm/stylecontext.h>

#include <array>
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
    language_.set_placeholder_text("Language: 1 English, 2 Spanish, 3 French, 4 German, 5 Japanese");
    add_field(*settings, "Name", user_name_);
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
    save_settings_.signal_clicked().connect(sigc::mem_fun(*this, &AgemirWindow::save_settings));
    completed_.connect(sigc::mem_fun(*this, &AgemirWindow::finish_request));
    load_settings();
    set_message(runtime_ ? "Ready. Requests run outside the GTK UI thread." :
                "Could not initialize the Agemir C runtime.", !runtime_);
}

AgemirWindow::~AgemirWindow() {
    if (worker_.joinable()) worker_.join();
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
    int projects = agemir_runtime_project_count(runtime_);
    runtime_check_.set_text("C runtime initialized; registered projects: " + std::to_string(projects) +
                            ". Model availability is checked when work begins.");
}

void AgemirWindow::save_settings() {
    if (!runtime_ || busy_) return;
    bool ok = agemir_runtime_set_setting(runtime_, "username", user_name_.get_text().c_str()) &&
              agemir_runtime_set_setting(runtime_, "model", model_.get_text().c_str()) &&
              agemir_runtime_set_setting(runtime_, "language", language_.get_text().c_str());
    set_message(ok ? "Settings saved." : "Settings need a name, a model, and a language from 1 to 5.", !ok);
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
    generate_.set_sensitive(false);
    run_.set_sensitive(false);
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
}

void AgemirWindow::finish_request() {
    std::lock_guard<std::mutex> lock(result_mutex_);
    busy_ = false;
    generate_.set_sensitive(true);
    run_.set_sensitive(true);
    save_settings_.set_sensitive(true);
    set_message(result_message_, !result_ok_);
}
