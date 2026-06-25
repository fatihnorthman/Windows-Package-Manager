#include "i18n.h"
#include <unordered_map>

namespace pm::gui {

namespace {

Lang g_lang = Lang::En;
const std::unordered_map<std::string, std::string>* g_dict = nullptr;

const std::unordered_map<std::string, std::string> en = {
    {"app_title",          "MyWinApps"},
    {"app_version",        "v1.2.0"},

    {"nav_discover",       "Discover"},
    {"nav_installed",      "Installed Apps"},
    {"nav_updates",        "Updates"},
    {"nav_tasks",          "Active Tasks"},
    {"nav_settings",       "Settings"},

    {"discover_title",     "Discover Packages"},
    {"discover_subtitle",  "Search the unified index of winget, scoop, and chocolatey repositories."},
    {"discover_search_ph", "Search packages (e.g. chrome, vscode)..."},
    {"discover_empty",     "Type a query above to search for packages."},

    {"installed_title",    "Installed Applications"},
    {"installed_subtitle", "All packages currently installed on this system."},
    {"installed_empty",    "No installed packages detected."},

    {"updates_title",      "Available Updates"},
    {"updates_subtitle",   "Update your tools to the latest versions for improved security and features."},
    {"updates_empty",      "Everything is up to date. Nothing to update right now."},
    {"updates_update_all", "Update All Applications"},
    {"updates_col_name",   "Application"},
    {"updates_col_id",     "Package ID"},
    {"updates_col_ver",    "Current"},
    {"updates_col_avail",  "New"},
    {"updates_col_action", "Action"},
    {"updates_btn_update", "Update"},
    {"updates_btn_queued", "Queued"},
    {"updates_btn_done",   "Done"},

    {"discover_btn_install",   "Install"},
    {"discover_btn_installing", "Installing..."},

    {"installed_btn_uninstall",     "Uninstall"},
    {"installed_btn_uninstalling",  "Uninstalling..."},
    {"installed_btn_uninstall_done", "Removed"},

    {"tasks_title",        "Active Tasks & Queue"},
    {"tasks_subtitle",     "Live status of all running, queued, and completed operations."},
    {"tasks_empty",        "The task queue is empty."},
    {"tasks_col_id",       "ID"},
    {"tasks_col_action",   "Action"},
    {"tasks_col_pkg",      "Package"},
    {"tasks_col_state",    "State"},
    {"tasks_col_progress", "Progress"},
    {"tasks_summary",      "Pending: %d   Active: %d   Done: %d"},

    {"settings_title",     "Settings"},
    {"settings_subtitle",  "Configure language and integration options."},
    {"settings_lang_label","Language"},
    {"settings_lang_en",   "English"},
    {"settings_lang_tr",   "Turkce"},
    {"settings_concurrency_label", "Task Concurrency Limit"},
    {"settings_concurrency_desc",  "Sets the maximum number of packages to download and install in parallel."},
    {"settings_store_label",       "Microsoft Store Online Search"},
    {"settings_store_desc",        "Enable querying the online MS Store catalog. Disabling this makes Discover searches instant (<0.5s)."},
    {"settings_cache_label",       "Clean Download & Temp Cache"},
    {"settings_cache_desc",        "Delete temporary winget export files and local package installer cache to free up disk space."},
    {"settings_cache_btn",         "Clear Cache"},
    {"settings_tools_label",       "System Tool Integration Status"},
    {"settings_tools_path",        "Path:"},
    {"settings_tools_ver",         "Version:"},
    {"settings_tools_status",      "Status:"},

    {"common_loading",     "Loading..."},
    {"common_refresh",     "Refresh"},
    {"common_cancel",      "Cancel"},

    {"state_queued",       "Queued"},
    {"state_installing",   "Installing"},
    {"state_updating",     "Updating"},
    {"state_installed",    "Installed"},
    {"state_failed",       "Failed"},
    {"state_up_to_date",   "Up to date"},

    {"footer_summary",     "Pending: %d   Active: %d   Done: %d"},
    {"footer_scan_complete", "Background scan complete: %d updates found"},
    {"footer_winget_ok",    "Winget: OK"},
    {"footer_scoop_ok",     "Scoop: OK"},
    {"footer_choco_ok",     "Choco: OK"},
    {"footer_status_idle", "System Status: Idle"},
    {"top_search_ph",      "Search packages, settings..."},
};

const std::unordered_map<std::string, std::string> tr = {
    {"app_title",          "MyWinApps"},
    {"app_version",        "v1.2.0"},

    {"nav_discover",       "Kesfet"},
    {"nav_installed",      "Kurulu Uygulamalarim"},
    {"nav_updates",        "Guncellemeler"},
    {"nav_tasks",          "Aktif Gorevler"},
    {"nav_settings",       "Ayarlar"},

    {"discover_title",     "Paket Kesfet"},
    {"discover_subtitle",  "Winget, scoop ve chocolatey depolari icin tekil arama."},
    {"discover_search_ph", "Paket ara (ornek: chrome, vscode)..."},
    {"discover_empty",     "Arama yapmak icin yukaridaki kutuya bir sorgu yazin."},

    {"installed_title",    "Kurulu Uygulamalar"},
    {"installed_subtitle", "Sisteminizde su an kurulu olan tum paketler."},
    {"installed_empty",    "Kurulu paket bulunamadi."},

    {"updates_title",      "Guncellemeler"},
    {"updates_subtitle",   "Araclari en guncel ve guvenli surumlerine yukseltin."},
    {"updates_empty",      "Hersey guncel. Su an icin yapilacak guncelleme yok."},
    {"updates_update_all", "Tumunu Guncelle"},
    {"updates_col_name",   "Uygulama"},
    {"updates_col_id",     "Paket ID"},
    {"updates_col_ver",    "Mevcut"},
    {"updates_col_avail",  "Yeni"},
    {"updates_col_action", "Islem"},
    {"updates_btn_update", "Guncelle"},
    {"updates_btn_queued", "Kuyrukta"},
    {"updates_btn_done",   "Tamamlandi"},

    {"discover_btn_install",   "Kur"},
    {"discover_btn_installing", "Kuruluyor..."},

    {"installed_btn_uninstall",     "Kaldir"},
    {"installed_btn_uninstalling",  "Kaldiriliyor..."},
    {"installed_btn_uninstall_done", "Kaldirildi"},

    {"tasks_title",        "Aktif Gorevler ve Kuyruk"},
    {"tasks_subtitle",     "Calisan, kuyrukta ve tamamlanan tum islemlerin canli durumu."},
    {"tasks_empty",        "Gorev kuyrugu bos."},
    {"tasks_col_id",       "ID"},
    {"tasks_col_action",   "Islem"},
    {"tasks_col_pkg",      "Paket"},
    {"tasks_col_state",    "Durum"},
    {"tasks_col_progress", "Ilerleme"},
    {"tasks_summary",      "Bekleyen: %d   Aktif: %d   Tamamlanan: %d"},

    {"settings_title",     "Ayarlar"},
    {"settings_subtitle",  "Dil ve entegrasyon seceneklerini yapilandirin."},
    {"settings_lang_label","Dil"},
    {"settings_lang_en",   "Ingilizce"},
    {"settings_lang_tr",   "Turkce"},
    {"settings_concurrency_label", "Eşzamanlı Görev Sınırı"},
    {"settings_concurrency_desc",  "Paralel olarak indirilip kurulacak maksimum paket sayısını belirler."},
    {"settings_store_label",       "Microsoft Store Çevrimiçi Arama"},
    {"settings_store_desc",        "Çevrimiçi MS Store kataloğunun sorgulanmasını etkinleştirir. Devre dışı bırakılması aramaları anlık (<0.5sn) yapar."},
    {"settings_cache_label",       "İndirme ve Geçici Belleği Temizle"},
    {"settings_cache_desc",        "Disk alanı açmak için geçici winget dışa aktarma dosyalarını ve yerel yükleyici önbelleğini siler."},
    {"settings_cache_btn",         "Önbelleği Temizle"},
    {"settings_tools_label",       "Sistem Araçları Entegrasyon Durumu"},
    {"settings_tools_path",        "Yol:"},
    {"settings_tools_ver",         "Sürüm:"},
    {"settings_tools_status",      "Durum:"},

    {"common_loading",     "Yukleniyor..."},
    {"common_refresh",     "Yenile"},
    {"common_cancel",      "Iptal"},

    {"state_queued",       "Kuyrukta"},
    {"state_installing",   "Kuruluyor"},
    {"state_updating",     "Güncelleniyor"},
    {"state_installed",    "Kuruldu"},
    {"state_failed",       "Hata"},
    {"state_up_to_date",   "Guncel"},

    {"footer_summary",     "Bekleyen: %d   Aktif: %d   Tamamlanan: %d"},
    {"footer_scan_complete", "Arkaplan taramasi tamam: %d guncelleme bulundu"},
    {"footer_winget_ok",    "Winget: OK"},
    {"footer_scoop_ok",     "Scoop: OK"},
    {"footer_choco_ok",     "Choco: OK"},
    {"footer_status_idle", "Sistem Durumu: Bosta"},
    {"top_search_ph",      "Paket, ayar ara..."},
};

} // anonymous

Lang currentLang() { return g_lang; }
void setLang(Lang l) { g_lang = l; g_dict = (l == Lang::En ? &en : &tr); }

std::string t(std::string_view key) {
    if (!g_dict) g_dict = (g_lang == Lang::En ? &en : &tr);
    auto it = g_dict->find(std::string(key));
    return it == g_dict->end() ? std::string(key) : it->second;
}

const char* tc(std::string_view key) {
    if (!g_dict) g_dict = (g_lang == Lang::En ? &en : &tr);
    auto it = g_dict->find(std::string(key));
    return it == g_dict->end() ? "" : it->second.c_str();
}

} // namespace pm::gui
