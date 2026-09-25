//! The menu tree, mirroring the Qt menu bar in `src/nekobox/ui/mainwindow.ui`.
//!
//! The GUI exposes its functionality through five tool buttons, each opening
//! one menu (`MainWindow::setup_menus`):
//!
//! | tool button          | menu              |
//! |----------------------|-------------------|
//! | `toolButton_program` | `menu_program`    |
//! | `toolButton_server`  | `menu_profiles`   |
//! | `toolButton_preferences` | `menu_preferences` |
//! | `toolButton_routing` | `menuRouting_Menu` |
//! | `url_test_button`    | `menuTest`        |
//! | `toolButton_update`  | `fetch_tool`      |
//!
//! The same tree is reproduced here so the two front-ends stay discoverable in
//! the same way. Entries the terminal cannot provide (QR scanning, global
//! hotkeys, Windows elevated task, tray) are omitted rather than shown
//! disabled.

/// Everything the menus can trigger.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Action {
    // --- Program ---
    SpModeSystemProxy,
    SpModeTun,
    SpModeDisabled,
    ToggleSystemDns,
    ToggleRememberLastProfile,
    ToggleAllowLan,
    RestartProxy,
    OpenConfigFolder,
    Exit,

    // --- Preferences ---
    ShowGroups,
    ShowBasicSettings,
    ShowRoutingSettings,

    // --- Profiles > Current Group ---
    SwitchGroup(usize),

    // --- Profiles > Current Selected > Share ---
    CopyLink,
    CopyLinkNekoray,
    ExportConfig,

    // --- Profiles > Current Selected > Update profile ---
    UpdateProfileFromClipboard,
    UpdateProfileFromFile,

    // --- Profiles > Current Selected > Test ---
    UrlTestSelected,
    ClearTestResultSelected,
    SpeedTestSelected,
    DownloadTestSelected,
    UploadTestSelected,
    CountryTestSelected,
    SimpleDlSelected,

    // --- Profiles > Current Selected ---
    Start,
    Stop,
    SelectAll,
    UnselectAll,
    CloneProfile,
    MoveProfile,
    DeleteProfile,
    ResetTrafficSelected,

    // --- Profiles ---
    AddFromClipboard,
    AddNewGroup,
    AddProfileFromFile,
    DeleteGroup,
    ToggleSearchBox,

    // --- Group operations (the GUI's "Hidden menu", reachable from the
    //     group tab context menu) ---
    UpdateSubscription,
    RemoveInvalid,
    RemoveUnavailable,
    ClearTestResultGroup,
    RemoveDuplicates,
    SpeedTestGroup,
    ResetTrafficGroup,

    // --- Routing ---
    SetRoute(i32),

    // --- Test ---
    UrlTestGroup,
    SpeedTestCurrent,
    StopTesting,

    // --- Information ---
    ShowStats,
    ShowAbout,
}

/// One row in a menu: a command, a submenu, or a separator.
#[derive(Debug, Clone)]
pub struct MenuItem {
    pub label: String,
    pub action: Option<Action>,
    pub submenu: Vec<MenuItem>,
    /// Rendered as a checkbox when set (mirrors Qt's `setCheckable`).
    pub checked: Option<bool>,
    pub enabled: bool,
    pub separator: bool,
}

impl MenuItem {
    pub fn new(label: impl Into<String>, action: Action) -> Self {
        Self {
            label: label.into(),
            action: Some(action),
            submenu: Vec::new(),
            checked: None,
            enabled: true,
            separator: false,
        }
    }

    pub fn sub(label: impl Into<String>, items: Vec<MenuItem>) -> Self {
        Self {
            label: label.into(),
            action: None,
            submenu: items,
            checked: None,
            enabled: true,
            separator: false,
        }
    }

    pub fn sep() -> Self {
        Self {
            label: String::new(),
            action: None,
            submenu: Vec::new(),
            checked: None,
            enabled: true,
            separator: true,
        }
    }

    pub fn check(mut self, checked: bool) -> Self {
        self.checked = Some(checked);
        self
    }

    pub fn enabled(mut self, enabled: bool) -> Self {
        self.enabled = enabled;
        self
    }

    /// Whether the cursor may land on this row.
    pub fn selectable(&self) -> bool {
        !self.separator && self.enabled && (self.action.is_some() || !self.submenu.is_empty())
    }

    /// The width this row needs, including the checkbox and submenu marker.
    pub fn width(&self) -> u16 {
        if self.separator {
            return 0;
        }
        let marker = if self.submenu.is_empty() { 0 } else { 2 };
        let check = if self.checked.is_some() { 4 } else { 0 };
        // Display width, not chars: group names may be CJK.
        ratatui::text::Span::raw(self.label.as_str()).width() as u16 + marker + check
    }
}

/// A top-level menu in the bar.
#[derive(Debug, Clone)]
pub struct Menu {
    pub title: String,
    pub items: Vec<MenuItem>,
}

/// State of the currently open menu: a stack of (items, cursor) so submenus
/// can be entered and left.
#[derive(Debug, Clone)]
pub struct MenuState {
    /// Index into the menu bar.
    pub root: usize,
    /// One level per open (sub)menu; the last is the focused one.
    pub levels: Vec<MenuLevel>,
}

#[derive(Debug, Clone)]
pub struct MenuLevel {
    pub items: Vec<MenuItem>,
    pub cursor: usize,
}

impl MenuLevel {
    fn new(items: Vec<MenuItem>) -> Self {
        let cursor = items.iter().position(MenuItem::selectable).unwrap_or(0);
        Self { items, cursor }
    }

    pub fn width(&self) -> u16 {
        self.items.iter().map(MenuItem::width).max().unwrap_or(8) + 4
    }
}

impl MenuState {
    pub fn open(root: usize, items: Vec<MenuItem>) -> Self {
        Self {
            root,
            levels: vec![MenuLevel::new(items)],
        }
    }

    fn level(&mut self) -> &mut MenuLevel {
        self.levels.last_mut().expect("menu always has one level")
    }

    /// Move the cursor, skipping separators and disabled rows. Wraps around.
    pub fn move_cursor(&mut self, delta: isize) {
        let level = self.level();
        let len = level.items.len();
        if len == 0 {
            return;
        }
        let mut idx = level.cursor;
        for _ in 0..len {
            idx = (idx as isize + delta).rem_euclid(len as isize) as usize;
            if level.items[idx].selectable() {
                level.cursor = idx;
                return;
            }
        }
    }

    pub fn current(&self) -> Option<&MenuItem> {
        self.levels.last()?.items.get(self.levels.last()?.cursor)
    }

    /// Enter the highlighted submenu. Returns false if it is not a submenu.
    pub fn enter_submenu(&mut self) -> bool {
        let Some(item) = self.current() else {
            return false;
        };
        if item.submenu.is_empty() {
            return false;
        }
        let items = item.submenu.clone();
        self.levels.push(MenuLevel::new(items));
        true
    }

    /// Leave the current submenu. Returns false at the top level (the caller
    /// should close the menu instead).
    pub fn leave_submenu(&mut self) -> bool {
        if self.levels.len() <= 1 {
            return false;
        }
        self.levels.pop();
        true
    }
}

/// Build the menu bar. Everything that depends on app state (group list,
/// routing chains, checkbox states) is passed in, so the tree is rebuilt each
/// time a menu is opened — the same thing the GUI does in `refresh_status`.
pub struct MenuContext<'a> {
    pub groups: &'a [(String, bool)],
    pub routes: &'a [(i32, String, bool)],
    pub spmode_system_proxy: bool,
    pub spmode_tun: bool,
    pub system_dns: bool,
    pub remember_last_profile: bool,
    pub allow_lan: bool,
    pub search_visible: bool,
    pub has_selection: bool,
    pub single_selection: bool,
    pub is_subscription_group: bool,
    pub running: bool,
    pub testing: bool,
}

pub fn build_menu_bar(ctx: &MenuContext<'_>) -> Vec<Menu> {
    vec![
        Menu {
            title: "Program".into(),
            items: program_menu(ctx),
        },
        Menu {
            title: "Profiles".into(),
            items: profiles_menu(ctx),
        },
        Menu {
            title: "Preferences".into(),
            items: preferences_menu(),
        },
        Menu {
            title: "Routing".into(),
            items: routing_menu(ctx),
        },
        Menu {
            title: "Test".into(),
            items: test_menu(ctx),
        },
        Menu {
            title: "Information".into(),
            items: information_menu(),
        },
    ]
}

fn program_menu(ctx: &MenuContext<'_>) -> Vec<MenuItem> {
    vec![
        MenuItem::sub(
            "System Proxy",
            vec![
                MenuItem::new("Enable System Proxy", Action::SpModeSystemProxy)
                    .check(ctx.spmode_system_proxy),
                MenuItem::new("Enable Tun", Action::SpModeTun).check(ctx.spmode_tun),
                MenuItem::new("Disable", Action::SpModeDisabled)
                    .check(!ctx.spmode_system_proxy && !ctx.spmode_tun),
            ],
        ),
        MenuItem::new("Set system DNS", Action::ToggleSystemDns).check(ctx.system_dns),
        MenuItem::sep(),
        MenuItem::new("Remember last profile", Action::ToggleRememberLastProfile)
            .check(ctx.remember_last_profile),
        MenuItem::new("Allow other devices to connect", Action::ToggleAllowLan)
            .check(ctx.allow_lan),
        MenuItem::sep(),
        MenuItem::new("Restart Proxy", Action::RestartProxy).enabled(ctx.running),
        MenuItem::new("Exit", Action::Exit),
    ]
}

fn profiles_menu(ctx: &MenuContext<'_>) -> Vec<MenuItem> {
    let groups = ctx
        .groups
        .iter()
        .enumerate()
        .map(|(i, (name, current))| MenuItem::new(name.clone(), Action::SwitchGroup(i)).check(*current))
        .collect();

    vec![
        MenuItem::sub("Current Group", groups),
        MenuItem::sub("Current Selected", current_selected_menu(ctx)),
        MenuItem::sep(),
        MenuItem::new("Add profile from clipboard", Action::AddFromClipboard),
        MenuItem::new("Add profile from File", Action::AddProfileFromFile),
        MenuItem::new("Add new Group", Action::AddNewGroup),
        MenuItem::new("Delete current Group", Action::DeleteGroup),
        MenuItem::sep(),
        MenuItem::new("Toggle searchbox", Action::ToggleSearchBox).check(ctx.search_visible),
        MenuItem::sep(),
        MenuItem::sub("Group operations", group_ops_menu(ctx)),
    ]
}

fn current_selected_menu(ctx: &MenuContext<'_>) -> Vec<MenuItem> {
    let sel = ctx.has_selection;
    let one = ctx.single_selection;
    vec![
        MenuItem::sub(
            "Share",
            vec![
                MenuItem::new("Copy links", Action::CopyLink).enabled(sel),
                MenuItem::new("Copy Nekoray links", Action::CopyLinkNekoray).enabled(sel),
                MenuItem::sep(),
                MenuItem::new("Export sing-box config", Action::ExportConfig).enabled(one),
            ],
        ),
        MenuItem::sub(
            "Update profile",
            vec![
                MenuItem::new("From clipboard", Action::UpdateProfileFromClipboard).enabled(one),
                MenuItem::new("From file", Action::UpdateProfileFromFile).enabled(one),
            ],
        ),
        MenuItem::sub(
            "Test",
            vec![
                MenuItem::new("Url Test", Action::UrlTestSelected).enabled(sel),
                MenuItem::new("Clear Test Result", Action::ClearTestResultSelected).enabled(sel),
                MenuItem::sep(),
                // Like the GUI, speed tests run over the whole selection.
                MenuItem::new("Full test", Action::SpeedTestSelected).enabled(sel),
                MenuItem::new("Download test", Action::DownloadTestSelected).enabled(sel),
                MenuItem::new("Upload test", Action::UploadTestSelected).enabled(sel),
                MenuItem::new("Country test", Action::CountryTestSelected).enabled(sel),
                MenuItem::new("Simple download test", Action::SimpleDlSelected).enabled(sel),
            ],
        ),
        MenuItem::sep(),
        MenuItem::new("Start", Action::Start).enabled(one),
        MenuItem::new("Stop", Action::Stop).enabled(ctx.running),
        MenuItem::sep(),
        MenuItem::new("Select All", Action::SelectAll),
        MenuItem::new("Unselect", Action::UnselectAll).enabled(sel),
        MenuItem::sep(),
        MenuItem::new("Clone", Action::CloneProfile).enabled(sel),
        MenuItem::new("Move", Action::MoveProfile).enabled(sel),
        MenuItem::new("Delete", Action::DeleteProfile).enabled(sel),
        MenuItem::sep(),
        MenuItem::new("Reset Traffic", Action::ResetTrafficSelected).enabled(sel),
    ]
}

fn group_ops_menu(ctx: &MenuContext<'_>) -> Vec<MenuItem> {
    vec![
        MenuItem::new("Update subscription", Action::UpdateSubscription)
            .enabled(ctx.is_subscription_group),
        MenuItem::sep(),
        MenuItem::new("Clear Test Result", Action::ClearTestResultGroup),
        MenuItem::new("Remove Unavailable", Action::RemoveUnavailable),
        MenuItem::new("Remove Invalid", Action::RemoveInvalid),
        MenuItem::new("Remove Duplicates", Action::RemoveDuplicates),
        MenuItem::sep(),
        MenuItem::new("Speedtest Group", Action::SpeedTestGroup),
        MenuItem::new("Reset Traffic", Action::ResetTrafficGroup),
    ]
}

fn preferences_menu() -> Vec<MenuItem> {
    vec![
        MenuItem::new("Groups", Action::ShowGroups),
        MenuItem::new("Basic Settings", Action::ShowBasicSettings),
        MenuItem::new("Routing Settings", Action::ShowRoutingSettings),
        MenuItem::sep(),
        MenuItem::new("Open Config Folder", Action::OpenConfigFolder),
    ]
}

fn routing_menu(ctx: &MenuContext<'_>) -> Vec<MenuItem> {
    if ctx.routes.is_empty() {
        return vec![MenuItem::new("(no routing profiles)", Action::ShowRoutingSettings)];
    }
    ctx.routes
        .iter()
        .map(|(id, name, active)| MenuItem::new(name.clone(), Action::SetRoute(*id)).check(*active))
        .collect()
}

fn test_menu(ctx: &MenuContext<'_>) -> Vec<MenuItem> {
    vec![
        MenuItem::new("Url Test Group", Action::UrlTestGroup),
        MenuItem::new("Speedtest Current", Action::SpeedTestCurrent).enabled(ctx.running),
        MenuItem::sep(),
        MenuItem::new("Stop testing", Action::StopTesting).enabled(ctx.testing),
    ]
}

fn information_menu() -> Vec<MenuItem> {
    vec![
        MenuItem::new("Statistics", Action::ShowStats),
        MenuItem::new("About", Action::ShowAbout),
    ]
}

#[cfg(test)]
mod tests {
    use super::*;

    fn ctx() -> MenuContext<'static> {
        MenuContext {
            groups: &[],
            routes: &[],
            spmode_system_proxy: false,
            spmode_tun: false,
            system_dns: false,
            remember_last_profile: false,
            allow_lan: false,
            search_visible: false,
            has_selection: true,
            single_selection: true,
            is_subscription_group: true,
            running: true,
            testing: false,
        }
    }

    #[test]
    fn cursor_skips_separators_and_disabled_rows() {
        let items = vec![
            MenuItem::new("a", Action::Exit),
            MenuItem::sep(),
            MenuItem::new("b", Action::Start).enabled(false),
            MenuItem::new("c", Action::Stop),
        ];
        let mut state = MenuState::open(0, items);
        assert_eq!(state.levels[0].cursor, 0);
        state.move_cursor(1);
        assert_eq!(state.levels[0].cursor, 3, "separator and disabled row skipped");
        state.move_cursor(1);
        assert_eq!(state.levels[0].cursor, 0, "wraps around");
    }

    #[test]
    fn submenus_push_and_pop() {
        let mut state = MenuState::open(0, profiles_menu(&ctx()));
        assert!(state.enter_submenu());
        assert_eq!(state.levels.len(), 2);
        assert!(state.leave_submenu());
        assert!(!state.leave_submenu(), "cannot leave the top level");
    }

    #[test]
    fn spmode_is_tri_state() {
        let mut c = ctx();
        c.spmode_tun = true;
        let program = program_menu(&c);
        let spmode = &program[0].submenu;
        assert_eq!(spmode[1].checked, Some(true), "Tun checked");
        assert_eq!(spmode[0].checked, Some(false), "System proxy unchecked");
        assert_eq!(spmode[2].checked, Some(false), "Disable unchecked");
    }
}
