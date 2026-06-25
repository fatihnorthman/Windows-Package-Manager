#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

namespace pm::gui {

enum class Lang { En, Tr };

// Get the current language (TR or EN).
Lang currentLang();
void setLang(Lang l);

// Look up a translation by key in the current language.
std::string t(std::string_view key);
const char* tc(std::string_view key);  // const char* for ImGui (no copy)

// All available translation keys.
namespace keys {
    constexpr std::string_view app_title          = "app_title";
    constexpr std::string_view app_version        = "app_version";
    constexpr std::string_view nav_discover       = "nav_discover";
    constexpr std::string_view nav_installed      = "nav_installed";
    constexpr std::string_view nav_updates        = "nav_updates";
    constexpr std::string_view nav_tasks          = "nav_tasks";
    constexpr std::string_view nav_settings       = "nav_settings";

    constexpr std::string_view discover_title     = "discover_title";
    constexpr std::string_view discover_subtitle  = "discover_subtitle";
    constexpr std::string_view discover_search_ph = "discover_search_ph";
    constexpr std::string_view discover_empty     = "discover_empty";

    constexpr std::string_view installed_title    = "installed_title";
    constexpr std::string_view installed_subtitle = "installed_subtitle";
    constexpr std::string_view installed_empty    = "installed_empty";

    constexpr std::string_view updates_title      = "updates_title";
    constexpr std::string_view updates_subtitle   = "updates_subtitle";
    constexpr std::string_view updates_empty      = "updates_empty";
    constexpr std::string_view updates_update_all = "updates_update_all";
    constexpr std::string_view updates_col_name   = "updates_col_name";
    constexpr std::string_view updates_col_id     = "updates_col_id";
    constexpr std::string_view updates_col_ver    = "updates_col_ver";
    constexpr std::string_view updates_col_avail  = "updates_col_avail";
    constexpr std::string_view updates_col_action = "updates_col_action";
    constexpr std::string_view updates_btn_update = "updates_btn_update";
    constexpr std::string_view updates_btn_queued = "updates_btn_queued";
    constexpr std::string_view updates_btn_done   = "updates_btn_done";

    constexpr std::string_view discover_btn_install   = "discover_btn_install";
    constexpr std::string_view discover_btn_installing = "discover_btn_installing";

    constexpr std::string_view installed_btn_uninstall     = "installed_btn_uninstall";
    constexpr std::string_view installed_btn_uninstalling  = "installed_btn_uninstalling";
    constexpr std::string_view installed_btn_uninstall_done = "installed_btn_uninstall_done";

    constexpr std::string_view tasks_title        = "tasks_title";
    constexpr std::string_view tasks_subtitle     = "tasks_subtitle";
    constexpr std::string_view tasks_empty        = "tasks_empty";
    constexpr std::string_view tasks_col_id       = "tasks_col_id";
    constexpr std::string_view tasks_col_action   = "tasks_col_action";
    constexpr std::string_view tasks_col_pkg      = "tasks_col_pkg";
    constexpr std::string_view tasks_col_state    = "tasks_col_state";
    constexpr std::string_view tasks_col_progress = "tasks_col_progress";
    constexpr std::string_view tasks_summary      = "tasks_summary";

    constexpr std::string_view settings_title     = "settings_title";
    constexpr std::string_view settings_subtitle  = "settings_subtitle";
    constexpr std::string_view settings_lang_label= "settings_lang_label";
    constexpr std::string_view settings_lang_en   = "settings_lang_en";
    constexpr std::string_view settings_lang_tr   = "settings_lang_tr";
    constexpr std::string_view settings_concurrency_label = "settings_concurrency_label";
    constexpr std::string_view settings_concurrency_desc  = "settings_concurrency_desc";
    constexpr std::string_view settings_store_label       = "settings_store_label";
    constexpr std::string_view settings_store_desc        = "settings_store_desc";
    constexpr std::string_view settings_cache_label        = "settings_cache_label";
    constexpr std::string_view settings_cache_desc         = "settings_cache_desc";
    constexpr std::string_view settings_cache_btn          = "settings_cache_btn";
    constexpr std::string_view settings_tools_label        = "settings_tools_label";
    constexpr std::string_view settings_tools_path         = "settings_tools_path";
    constexpr std::string_view settings_tools_ver          = "settings_tools_ver";
    constexpr std::string_view settings_tools_status       = "settings_tools_status";

    constexpr std::string_view common_loading     = "common_loading";
    constexpr std::string_view common_refresh     = "common_refresh";
    constexpr std::string_view common_cancel      = "common_cancel";

    constexpr std::string_view state_queued       = "state_queued";
    constexpr std::string_view state_installing   = "state_installing";
    constexpr std::string_view state_updating     = "state_updating";
    constexpr std::string_view state_installed    = "state_installed";
    constexpr std::string_view state_failed       = "state_failed";
    constexpr std::string_view state_up_to_date   = "state_up_to_date";

    constexpr std::string_view footer_summary     = "footer_summary";
    constexpr std::string_view footer_scan_complete = "footer_scan_complete";
    constexpr std::string_view footer_winget_ok   = "footer_winget_ok";
    constexpr std::string_view footer_scoop_ok    = "footer_scoop_ok";
    constexpr std::string_view footer_choco_ok    = "footer_choco_ok";
    constexpr std::string_view footer_status_idle = "footer_status_idle";
    constexpr std::string_view top_search_ph      = "top_search_ph";
} // namespace keys

} // namespace pm::gui
