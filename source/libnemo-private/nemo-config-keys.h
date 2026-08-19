/* Generated from the old gschema - the settings table.
   Edit this file directly; the gschema it came from is gone. */

#ifndef NEMO_CONFIG_KEYS_H
#define NEMO_CONFIG_KEYS_H

#include "nemo-config.h"

static const NemoConfigEnumValue enum_ActivationChoice[] = {
	{ "launch", 0 },
	{ "display", 1 },
	{ "ask", 2 },
	{ NULL, 0 }
};

static const NemoConfigEnumValue enum_AppearanceMode[] = {
	{ "system", 0 },
	{ "light", 1 },
	{ "dark", 2 },
	{ NULL, 0 }
};

static const NemoConfigEnumValue enum_ClickPolicy[] = {
	{ "single", 0 },
	{ "double", 1 },
	{ NULL, 0 }
};

static const NemoConfigEnumValue enum_DateFormat[] = {
	{ "locale", 0 },
	{ "iso", 1 },
	{ "informal", 2 },
	{ NULL, 0 }
};

static const NemoConfigEnumValue enum_FolderView[] = {
	{ "icon-view", 0 },
	{ "compact-view", 1 },
	{ "list-view", 2 },
	{ NULL, 0 }
};

static const NemoConfigEnumValue enum_SizePrefixes[] = {
	{ "base-10", 0 },
	{ "base-10-full", 1 },
	{ "base-2", 2 },
	{ "base-2-full", 3 },
	{ NULL, 0 }
};

static const NemoConfigEnumValue enum_SortOrder[] = {
	{ "manually", 0 },
	{ "name", 1 },
	{ "size", 2 },
	{ "type", 3 },
	{ "detailed_type", 4 },
	{ "mtime", 5 },
	{ "atime", 6 },
	{ "trash-time", 7 },
	{ NULL, 0 }
};

static const NemoConfigEnumValue enum_SpeedTradeoff[] = {
	{ "always", 0 },
	{ "local-only", 1 },
	{ "never", 2 },
	{ NULL, 0 }
};

static const NemoConfigEnumValue enum_TabPosition[] = {
	{ "after-current-tab", 0 },
	{ "end", 1 },
	{ NULL, 0 }
};

static const NemoConfigEnumValue enum_ZoomLevel[] = {
	{ "smallest", 0 },
	{ "smaller", 1 },
	{ "small", 2 },
	{ "standard", 3 },
	{ "large", 4 },
	{ "larger", 5 },
	{ "largest", 6 },
	{ NULL, 0 }
};

static const char *const deflist__favorites[] = { NULL };
static const char *const deflist__favorites_root_metadata[] = { NULL };
static const char *const deflist_desktop_ignored_desktop_handlers[] = { "conky", "csd-background", NULL };
static const char *const deflist_icon_view_captions[] = { "none", "size", "date_modified", NULL };
static const char *const deflist_icon_view_text_ellipsis_limit[] = { "3", NULL };
static const char *const deflist_list_view_default_column_order[] = { "name", "size", "type", "date_modified", "owner", "group", "permissions", NULL };
static const char *const deflist_list_view_default_visible_columns[] = { "name", "size", "type", "date_modified", "owner", "group", "permissions", NULL };
static const char *const deflist_plugins_disabled_actions[] = { NULL };
static const char *const deflist_plugins_disabled_extensions[] = { NULL };
static const char *const deflist_plugins_disabled_scripts[] = { NULL };
static const char *const deflist_preferences_image_viewers_with_external_sort[] = { "xviewer", "feh", "sxiv", NULL };
/* Windows "Open in Terminal": first one found on PATH wins. */
static const char *const deflist_terminal_win32_candidates[] = { "wt.exe", "pwsh.exe", "powershell.exe", "cmd.exe", NULL };
static const char *const deflist_search_disabled_search_helpers[] = { NULL };
static const char *const deflist_search_search_skip_folders[] = { "/dev", "/proc", "/sys", "dosdevices", ".git", NULL };
static const char *const deflist_search_search_visible_columns[] = { NULL };
static const char *const deflist_thumbnailers_disable[] = { NULL };

static const NemoConfigKey nemo_config_keys[] = {
	{ "", "favorites", NEMO_CONFIG_STRING_LIST, NULL, deflist__favorites, NULL, "Favorite files and folders" },
	{ "", "favorites-root-metadata", NEMO_CONFIG_STRING_LIST, NULL, deflist__favorites_root_metadata, NULL, "Metadata for the favorites root" },
	{ "appearance", "gtk-theme", NEMO_CONFIG_STRING, "", NULL, NULL, "Widget theme to use, or empty to leave it to the platform" },
	{ "appearance", "icon-theme", NEMO_CONFIG_STRING, "", NULL, NULL, "Icon theme to use, or empty to leave it to the platform" },
	{ "appearance", "mode", NEMO_CONFIG_ENUM, "system", NULL, enum_AppearanceMode, "Light or dark appearance, or follow the system" },
	{ "compact-view", "all-columns-have-same-width", NEMO_CONFIG_BOOL, "false", NULL, NULL, "All columns have same width" },
	{ "compact-view", "default-zoom-level", NEMO_CONFIG_ENUM, "standard", NULL, enum_ZoomLevel, "Default compact view zoom level" },
	{ "interface", "clock-use-24h", NEMO_CONFIG_BOOL, "true", NULL, NULL, NULL },
	{ "media-handling", "automount", NEMO_CONFIG_BOOL, "true", NULL, NULL, NULL },
	{ "media-handling", "automount-open", NEMO_CONFIG_BOOL, "true", NULL, NULL, NULL },
	{ "privacy", "remember-recent-files", NEMO_CONFIG_BOOL, "true", NULL, NULL, NULL },
	{ "terminal", "exec", NEMO_CONFIG_STRING, "", NULL, NULL, NULL },
	{ "terminal", "exec-arg", NEMO_CONFIG_STRING, "-e", NULL, NULL, NULL },
	{ "terminal", "win32-candidates", NEMO_CONFIG_STRING_LIST, NULL, deflist_terminal_win32_candidates, NULL, "Terminals to try for \"Open in Terminal\" on Windows, in order" },
	{ "desktop", "background-fade", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Fade the background on change" },
	{ "desktop", "computer-icon-visible", NEMO_CONFIG_BOOL, "false", NULL, NULL, "Computer icon visible on desktop" },
	{ "desktop", "desktop-layout", NEMO_CONFIG_STRING, "true::false", NULL, NULL, "Desktop layout" },
	{ "desktop", "font", NEMO_CONFIG_STRING, "Noto Sans 10", NULL, NULL, "Desktop font" },
	{ "desktop", "home-icon-visible", NEMO_CONFIG_BOOL, "false", NULL, NULL, "Home icon visible on desktop" },
	{ "desktop", "horizontal-grid-adjust", NEMO_CONFIG_FLOAT, "1.0", NULL, NULL, "Horizontal desktop grid adjustment" },
	{ "desktop", "ignored-desktop-handlers", NEMO_CONFIG_STRING_LIST, NULL, deflist_desktop_ignored_desktop_handlers, NULL, "List of desktop-handling to ignore when determining whether or not to manager the desktop." },
	{ "desktop", "network-icon-visible", NEMO_CONFIG_BOOL, "false", NULL, NULL, "Network Servers icon visible on the desktop" },
	{ "desktop", "show-desktop-icons", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Deprecated: Allow Nemo to manage the desktop" },
	{ "desktop", "show-orphaned-desktop-icons", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Whether to show icons from inactive monitors on another monitor" },
	{ "desktop", "text-ellipsis-limit", NEMO_CONFIG_INT, "2", NULL, NULL, "Text Ellipsis Limit" },
	{ "desktop", "trash-icon-visible", NEMO_CONFIG_BOOL, "false", NULL, NULL, "Trash icon visible on desktop" },
	{ "desktop", "use-desktop-grid", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Which desktop view type to use" },
	{ "desktop", "vertical-grid-adjust", NEMO_CONFIG_FLOAT, "1.0", NULL, NULL, "Vertical desktop grid adjustment" },
	{ "desktop", "volumes-visible", NEMO_CONFIG_BOOL, "false", NULL, NULL, "Show mounted volumes on the desktop" },
	{ "icon-view", "captions", NEMO_CONFIG_STRING_LIST, NULL, deflist_icon_view_captions, NULL, "List of possible captions on icons" },
	{ "icon-view", "default-use-tighter-layout", NEMO_CONFIG_BOOL, "false", NULL, NULL, "deprecated - not used" },
	{ "icon-view", "default-zoom-level", NEMO_CONFIG_ENUM, "standard", NULL, enum_ZoomLevel, "Default icon zoom level" },
	{ "icon-view", "labels-beside-icons", NEMO_CONFIG_BOOL, "false", NULL, NULL, "Put labels beside icons" },
	{ "icon-view", "text-ellipsis-limit", NEMO_CONFIG_STRING_LIST, NULL, deflist_icon_view_text_ellipsis_limit, NULL, "Text Ellipsis Limit" },
	{ "icon-view", "thumbnail-size", NEMO_CONFIG_INT, "64", NULL, NULL, "Default Thumbnail Icon Size" },
	{ "list-view", "default-column-order", NEMO_CONFIG_STRING_LIST, NULL, deflist_list_view_default_column_order, NULL, "Default column order in the list view" },
	{ "list-view", "default-visible-columns", NEMO_CONFIG_STRING_LIST, NULL, deflist_list_view_default_visible_columns, NULL, "Default list of columns visible in the list view" },
	{ "list-view", "default-zoom-level", NEMO_CONFIG_ENUM, "small", NULL, enum_ZoomLevel, "Default list zoom level" },
	{ "list-view", "enable-folder-expansion", NEMO_CONFIG_BOOL, "true", NULL, NULL, "If true, allow folders with content to be expanded in the current view." },
	{ "plugins", "disabled-actions", NEMO_CONFIG_STRING_LIST, NULL, deflist_plugins_disabled_actions, NULL, "List of NemoActions -not- to load." },
	{ "plugins", "disabled-extensions", NEMO_CONFIG_STRING_LIST, NULL, deflist_plugins_disabled_extensions, NULL, "List of extensions -not- to load." },
	{ "plugins", "disabled-scripts", NEMO_CONFIG_STRING_LIST, NULL, deflist_plugins_disabled_scripts, NULL, "List of scripts -not- to load." },
	{ "preferences", "always-use-browser", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Enables the classic Nemo behavior, where all windows are browsers" },
	{ "preferences", "bulk-rename-tool", NEMO_CONFIG_STRING, "", NULL, NULL, "Bulk rename utility" },
	{ "preferences", "click-double-parent-folder", NEMO_CONFIG_BOOL, "false", NULL, NULL, "If true, double click left on blank area will go to parent folder" },
	{ "preferences", "click-policy", NEMO_CONFIG_ENUM, "double", NULL, enum_ClickPolicy, "Type of click used to launch/open files" },
	{ "preferences", "close-device-view-on-device-eject", NEMO_CONFIG_BOOL, "false", NULL, NULL, "Whether to close a view of a removeable device instead of navigating Home" },
	{ "preferences", "confirm-move-to-trash", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Whether to ask for confirmation when moving files to Trash" },
	{ "preferences", "confirm-trash", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Whether to ask for confirmation when deleting files, or emptying Trash" },
	{ "preferences", "context-menus-show-all-actions", NEMO_CONFIG_BOOL, "false", NULL, NULL, "deprecated - no longer used" },
	{ "preferences", "date-format", NEMO_CONFIG_ENUM, "iso", NULL, enum_DateFormat, "Date Format" },
	{ "preferences", "default-folder-viewer", NEMO_CONFIG_ENUM, "list-view", NULL, enum_FolderView, "Default folder viewer" },
	{ "preferences", "default-sort-in-reverse-order", NEMO_CONFIG_BOOL, "false", NULL, NULL, "Reverse sort order in new windows" },
	{ "preferences", "default-sort-order", NEMO_CONFIG_ENUM, "name", NULL, enum_SortOrder, "Default sort order" },
	{ "preferences", "deferred-attribute-preload-limit", NEMO_CONFIG_INT, "150", NULL, NULL, "Maximum number of files to preload deferred attributes for when opening a directory" },
	{ "preferences", "desktop-is-home-dir", NEMO_CONFIG_BOOL, "false", NULL, NULL, "Nemo uses the users home folder as the desktop" },
	{ "preferences", "detect-content", NEMO_CONFIG_BOOL, "true", NULL, NULL, "If true, enable detection of the type of content of a mounted media and display a suggested application to open the media." },
	{ "preferences", "disable-menu-warning", NEMO_CONFIG_BOOL, "false", NULL, NULL, "Don't show the explainer message when turning off the main menu" },
	{ "preferences", "enable-delete", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Whether to enable immediate deletion" },
	{ "preferences", "enable-mime-actions-make-executable", NEMO_CONFIG_BOOL, "false", NULL, NULL, "Display the 'Make executable and run' button in the mime-action dialog (open an unknown filetype)" },
	{ "preferences", "executable-text-activation", NEMO_CONFIG_ENUM, "ask", NULL, enum_ActivationChoice, "What to do with executable text files when activated" },
	{ "preferences", "expand-row-on-dnd-dwell", NEMO_CONFIG_BOOL, "true", NULL, NULL, "During drag-and-drop operations, automatically expand rows when hovering them briefly" },
	{ "preferences", "ignore-view-metadata", NEMO_CONFIG_BOOL, "false", NULL, NULL, "Whether to ignore folder metadata for view zoom levels and layouts" },
	{ "preferences", "image-viewers-with-external-sort", NEMO_CONFIG_STRING_LIST, NULL, deflist_preferences_image_viewers_with_external_sort, NULL, "Image viewer executables to pass sort order to" },
	{ "preferences", "inherit-folder-viewer", NEMO_CONFIG_BOOL, "false", NULL, NULL, "Inherit the view type (icon, compact, list) from parent to children" },
	{ "preferences", "inherit-show-thumbnails", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Inherit thumbnail visibility from parent" },
	{ "preferences", "last-server-connect-method", NEMO_CONFIG_INT, "2", NULL, NULL, "Last server connect method used" },
	{ "preferences", "mouse-back-button", NEMO_CONFIG_INT, "8", NULL, NULL, "Mouse button to activate the \"Back\" command in browser window" },
	{ "preferences", "mouse-forward-button", NEMO_CONFIG_INT, "9", NULL, NULL, "Mouse button to activate the \"Forward\" command in browser window" },
	{ "preferences", "mouse-use-extra-buttons", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Use extra mouse button events in Nemo' browser window" },
	{ "preferences", "never-queue-file-ops", NEMO_CONFIG_BOOL, "false", NULL, NULL, "If true, all file operations will start immediately" },
	{ "preferences", "quick-renames-with-pause-in-between", NEMO_CONFIG_BOOL, "false", NULL, NULL, "Enables renaming of icons by two times clicking with pause between clicks" },
	{ "preferences", "show-advanced-permissions", NEMO_CONFIG_BOOL, "false", NULL, NULL, "Show advanced permissions in the file property dialog" },
	{ "preferences", "show-bookmarks-in-to-menus", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Whether to list bookmarks in the Move To/Copy To menus" },
	{ "preferences", "show-compact-view-icon-toolbar", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Show Compact View button in nemo toolbar" },
	{ "preferences", "show-computer-icon-toolbar", NEMO_CONFIG_BOOL, "false", NULL, NULL, "Show Computer button in nemo toolbar" },
	{ "preferences", "show-directory-item-counts", NEMO_CONFIG_ENUM, "local-only", NULL, enum_SpeedTradeoff, "When to show number of items in a folder" },
	{ "preferences", "show-edit-icon-toolbar", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Show toggle button location entry/pathbar" },
	{ "preferences", "show-full-path-titles", NEMO_CONFIG_BOOL, "false", NULL, NULL, "Whether to show the full path of the current view in the title bar and tab bars" },
	{ "preferences", "show-hidden-files", NEMO_CONFIG_BOOL, "false", NULL, NULL, "Whether to show hidden files" },
	{ "preferences", "show-home-icon-toolbar", NEMO_CONFIG_BOOL, "false", NULL, NULL, "Show Home button in nemo toolbar" },
	{ "preferences", "show-icon-view-icon-toolbar", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Show Icon View button in nemo toolbar" },
	{ "preferences", "show-image-thumbnails", NEMO_CONFIG_ENUM, "local-only", NULL, enum_SpeedTradeoff, "When to show thumbnails of image files" },
	{ "preferences", "show-list-view-icon-toolbar", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Show List View button in nemo toolbar" },
	{ "preferences", "show-location-entry", NEMO_CONFIG_BOOL, "false", NULL, NULL, "Show the location entry by default" },
	{ "preferences", "show-new-folder-icon-toolbar", NEMO_CONFIG_BOOL, "false", NULL, NULL, "Show new folder button in nemo toolbar" },
	{ "preferences", "show-next-icon-toolbar", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Show Next button in nemo toolbar" },
	{ "preferences", "show-open-in-terminal-toolbar", NEMO_CONFIG_BOOL, "false", NULL, NULL, "Show open in terminal in the nemo toolbar" },
	{ "preferences", "show-places-in-to-menus", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Whether to list places in the Move To/Copy To menus" },
	{ "preferences", "show-previous-icon-toolbar", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Show Previous button in nemo toolbar" },
	{ "preferences", "show-reload-icon-toolbar", NEMO_CONFIG_BOOL, "false", NULL, NULL, "Show refresh button in nemo toolbar" },
	{ "preferences", "show-root-warning", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Show warning when opening as root" },
	{ "preferences", "show-search-icon-toolbar", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Show Search button in nemo toolbar" },
	{ "preferences", "show-show-thumbnails-toolbar", NEMO_CONFIG_BOOL, "false", NULL, NULL, "Show Thumbnails button in nemo toolbar" },
	{ "preferences", "show-toggle-extra-pane-toolbar", NEMO_CONFIG_BOOL, "false", NULL, NULL, "Show Extra Pane button in nemo toolbar" },
	{ "preferences", "show-up-icon-toolbar", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Show Up button in nemo toolbar" },
	{ "preferences", "size-prefixes", NEMO_CONFIG_ENUM, "base-2", NULL, enum_SizePrefixes, "Prefixes used for file sizes" },
	{ "preferences", "sort-directories-first", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Show folders first in windows" },
	{ "preferences", "sort-favorites-first", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Show favorites first in windows" },
	{ "preferences", "start-with-dual-pane", NEMO_CONFIG_BOOL, "false", NULL, NULL, "Whether to default to showing dual-pane view when a new window is opened" },
	{ "preferences", "swap-trash-delete", NEMO_CONFIG_BOOL, "false", NULL, NULL, "Whether to swap the hotkeys for Trash and Delete" },
	{ "preferences", "tabs-open-position", NEMO_CONFIG_ENUM, "after-current-tab", NULL, enum_TabPosition, "Where to position newly open tabs in browser windows." },
	{ "preferences", "thumbnail-limit", NEMO_CONFIG_INT, "1048576", NULL, NULL, "Maximum image size for thumbnailing" },
	{ "preferences", "thumbnail-threads", NEMO_CONFIG_INT, "-1", NULL, NULL, "Number of threads to dedicate to thumbnailing. -1 to let the program decide. The maximum allowed threads is half the number of logical processors, regardless of what is set here. If you change this setting you must restart Nemo for it to take effect." },
	{ "preferences", "tooltips-in-icon-view", NEMO_CONFIG_BOOL, "false", NULL, NULL, "Show tooltips when hovering on items in an icon or compact view" },
	{ "preferences", "tooltips-in-list-view", NEMO_CONFIG_BOOL, "false", NULL, NULL, "Show tooltips when hovering on items in a list view" },
	{ "preferences", "tooltips-on-desktop", NEMO_CONFIG_BOOL, "false", NULL, NULL, "Show tooltips for desktop items" },
	{ "preferences", "tooltips-show-access-date", NEMO_CONFIG_BOOL, "false", NULL, NULL, "Show file accessed date in tooltip" },
	{ "preferences", "tooltips-show-birth-date", NEMO_CONFIG_BOOL, "false", NULL, NULL, "Show file creation (birth) date in tooltip" },
	{ "preferences", "tooltips-show-file-type", NEMO_CONFIG_BOOL, "false", NULL, NULL, "Show detailed file type in tooltip" },
	{ "preferences", "tooltips-show-mod-date", NEMO_CONFIG_BOOL, "false", NULL, NULL, "Show file modified date in tooltip" },
	{ "preferences", "tooltips-show-path", NEMO_CONFIG_BOOL, "false", NULL, NULL, "Show full path in tooltip" },
	{ "preferences", "treat-root-as-normal", NEMO_CONFIG_BOOL, "false", NULL, NULL, "Suppress any safeguards when running nemo/nemo-desktop as the root user. For some systems there is only a root user." },
	{ "preferences.menu-config", "background-menu-create-new-folder", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Show the background context menu's Create New Folder item." },
	{ "preferences.menu-config", "background-menu-open-as-root", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Show the background context menu's Open as Root item." },
	{ "preferences.menu-config", "background-menu-open-in-terminal", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Show the background context menu's Open in Terminal item." },
	{ "preferences.menu-config", "background-menu-paste", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Show the background context menu's Paste item." },
	{ "preferences.menu-config", "background-menu-properties", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Show the background context menu's Properties item." },
	{ "preferences.menu-config", "background-menu-scripts", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Show the background context menu's Scripts submenu." },
	{ "preferences.menu-config", "background-menu-show-hidden-files", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Show the background context menu's Show Hidden Files item." },
	{ "preferences.menu-config", "desktop-menu-customize", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Show the background context menu's Customize item (new-style desktop only)." },
	{ "preferences.menu-config", "iconview-menu-arrange-items", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Show the background context menu's Arrange Items submenu (icon view only)." },
	{ "preferences.menu-config", "iconview-menu-organize-by-name", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Show the background context menu's Organize by Name item (icon view only)." },
	{ "preferences.menu-config", "selection-menu-copy", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Show the selection context menu's Copy item." },
	{ "preferences.menu-config", "selection-menu-copy-to", NEMO_CONFIG_BOOL, "false", NULL, NULL, "Show the selection context menu's Copy To submenu." },
	{ "preferences.menu-config", "selection-menu-cut", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Show the selection context menu's Cut item." },
	{ "preferences.menu-config", "selection-menu-duplicate", NEMO_CONFIG_BOOL, "false", NULL, NULL, "Show the selection context menu's Duplicate item." },
	{ "preferences.menu-config", "selection-menu-favorite", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Show the selection context menu's Favorite/Unfavorite item." },
	{ "preferences.menu-config", "selection-menu-make-link", NEMO_CONFIG_BOOL, "false", NULL, NULL, "Show the selection context menu's Create Link item." },
	{ "preferences.menu-config", "selection-menu-move-to", NEMO_CONFIG_BOOL, "false", NULL, NULL, "Show the selection context menu's Move To submenu." },
	{ "preferences.menu-config", "selection-menu-move-to-trash", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Show the selection context menu's Move to Trash item." },
	{ "preferences.menu-config", "selection-menu-open", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Show the selection context menu's Open item." },
	{ "preferences.menu-config", "selection-menu-open-as-root", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Show the selection context menu's Open As Root item." },
	{ "preferences.menu-config", "selection-menu-open-in-new-tab", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Show the selection context menu's Open in New Tab item." },
	{ "preferences.menu-config", "selection-menu-open-in-new-window", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Show the selection context menu's Open in New Window item." },
	{ "preferences.menu-config", "selection-menu-open-in-terminal", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Show the selection context menu's Open in Terminal item." },
	{ "preferences.menu-config", "selection-menu-paste", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Show the selection context menu's Paste item." },
	{ "preferences.menu-config", "selection-menu-pin", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Show the selection context menu's Pin/Unpin item." },
	{ "preferences.menu-config", "selection-menu-properties", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Show the selection context menu's Properties item." },
	{ "preferences.menu-config", "selection-menu-rename", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Show the selection context menu's Rename item." },
	{ "preferences.menu-config", "selection-menu-scripts", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Show the selection context menu's Scripts submenu." },
	{ "search", "disabled-search-helpers", NEMO_CONFIG_STRING_LIST, NULL, deflist_search_disabled_search_helpers, NULL, "List of search helper filenames to skip when using content search." },
	{ "search", "search-content-case-sensitive", NEMO_CONFIG_BOOL, "false", NULL, NULL, "Stores the most recent state of the content search case toggle" },
	{ "search", "search-content-use-raw", NEMO_CONFIG_BOOL, "false", NULL, NULL, "Treat patterns as raw bytes, not utf-8" },
	{ "search", "search-content-use-regex", NEMO_CONFIG_BOOL, "false", NULL, NULL, "Stores the most recent state of the content search regex toggle" },
	{ "search", "search-file-case-sensitive", NEMO_CONFIG_BOOL, "false", NULL, NULL, "Stores the most recent state of the file search case toggle" },
	{ "search", "search-files-recursively", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Recurse into subfolders when performing a search" },
	{ "search", "search-files-use-regex", NEMO_CONFIG_BOOL, "false", NULL, NULL, "Stores the most recent state of the file search regex toggle" },
	{ "search", "search-regex-format", NEMO_CONFIG_STRING, "pcre", NULL, NULL, "valid formats: pcre, javascript" },
	{ "search", "search-reverse-sort", NEMO_CONFIG_BOOL, "false", NULL, NULL, "Reverse the direction of the sort when viewing search results" },
	{ "search", "search-skip-folders", NEMO_CONFIG_STRING_LIST, NULL, deflist_search_search_skip_folders, NULL, "Paths or folder names to never recurse into when searching" },
	{ "search", "search-sort-column", NEMO_CONFIG_STRING, "", NULL, NULL, "Column to sort on when viewing search results" },
	{ "search", "search-visible-columns", NEMO_CONFIG_STRING_LIST, NULL, deflist_search_search_visible_columns, NULL, "Saved list of columns visible in the search view." },
	{ "sidebar-panels.tree", "show-only-directories", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Only show folders in the tree side pane" },
	{ "thumbnailers", "disable", NEMO_CONFIG_STRING_LIST, NULL, deflist_thumbnailers_disable, NULL, "Disable external thumbnailers for these mime types" },
	{ "thumbnailers", "disable-all", NEMO_CONFIG_BOOL, "false", NULL, NULL, "Disable all external thumbnailers" },
	{ "window-state", "bookmarks-expanded", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Expand Bookmark section in places sidebar" },
	{ "window-state", "devices-expanded", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Expand Devices section in places sidebar" },
	{ "window-state", "geometry", NEMO_CONFIG_STRING, "", NULL, NULL, "The geometry string for a navigation window." },
	{ "window-state", "maximized", NEMO_CONFIG_BOOL, "false", NULL, NULL, "Whether the navigation window should be maximized." },
	{ "window-state", "my-computer-expanded", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Expand My Computer section in places sidebar" },
	{ "window-state", "network-expanded", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Expand Network section in places sidebar" },
	{ "window-state", "side-pane-view", NEMO_CONFIG_STRING, "places", NULL, NULL, "Side pane view" },
	{ "window-state", "sidebar-bookmark-breakpoint", NEMO_CONFIG_INT, "-1", NULL, NULL, "Index of the bookmark list to jump to the dedicated sidebar bookmark section" },
	{ "window-state", "sidebar-width", NEMO_CONFIG_INT, "240", NULL, NULL, "Width of the side pane" },
	{ "window-state", "start-with-location-bar", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Show location bar in new windows" },
	{ "window-state", "start-with-menu-bar", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Show menu bar in new windows" },
	{ "window-state", "start-with-sidebar", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Show side pane in new windows" },
	{ "window-state", "start-with-status-bar", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Show status bar in new windows" },
	{ "window-state", "start-with-toolbar", NEMO_CONFIG_BOOL, "true", NULL, NULL, "Show toolbar in new windows" },
	{ NULL, NULL, 0, NULL, NULL, NULL, NULL }
};

#endif /* NEMO_CONFIG_KEYS_H */
