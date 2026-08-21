-- ~/.config/wezterm/wezterm.lua  (Linux/macOS)
-- ~/.wezterm.lua                 (Windows)
--
-- Cross-platform WezTerm config. Designed to be curled down to any machine
-- (rig, laptop, homelab box) and just work, with a few OS-specific tweaks
-- handled automatically.

local wezterm = require("wezterm")
local config = wezterm.config_builder()

-- ---------------------------------------------------------------------
-- Detect platform / host so we can branch config below
-- ---------------------------------------------------------------------
local target_triple = wezterm.target_triple
local is_windows = target_triple:find("windows") ~= nil
local is_mac = target_triple:find("apple") ~= nil
local is_linux = target_triple:find("linux") ~= nil
local hostname = wezterm.hostname()

-- ---------------------------------------------------------------------
-- Appearance
-- ---------------------------------------------------------------------
config.color_scheme = "Dracula"
config.font = wezterm.font_with_fallback({
	"JetBrains Mono",
	"Cascadia Code",
	"Fira Code",
})

-- Slightly bigger font on high-DPI laptop screens, standard elsewhere.
-- Adjust the hostname check to match your actual laptop's hostname.
if hostname:lower():find("laptop") then
	config.font_size = 13.0
else
	config.font_size = 12.0
end

config.window_background_opacity = 0.95
config.macos_window_background_blur = 20 -- ignored on non-mac
config.window_decorations = "TITLE | RESIZE"
config.window_padding = {
	left = 8,
	right = 8,
	top = 8,
	bottom = 8,
}

config.enable_tab_bar = true
config.hide_tab_bar_if_only_one_tab = true
config.use_fancy_tab_bar = false
config.tab_bar_at_bottom = false

-- ---------------------------------------------------------------------
-- Scrollback / performance
-- ---------------------------------------------------------------------
config.scrollback_lines = 10000
config.animation_fps = 60
config.max_fps = 120
config.front_end = "WebGpu" -- falls back automatically if unsupported

-- ---------------------------------------------------------------------
-- Shell / platform-specific defaults
-- ---------------------------------------------------------------------
if is_windows then
	-- Default to PowerShell 7 if installed, else Windows PowerShell
	config.default_prog = { "pwsh.exe" }

	-- Handy launch menu for switching shells on Windows
	config.launch_menu = {
		{ label = "PowerShell 7", args = { "pwsh.exe" } },
		{ label = "Windows PowerShell", args = { "powershell.exe" } },
		{ label = "Command Prompt", args = { "cmd.exe" } },
		{ label = "WSL", args = { "wsl.exe" } },
	}
elseif is_mac then
	-- macOS ships zsh by default, so this is safe as-is
	config.default_prog = { "/bin/zsh", "-l" }
elseif is_linux then
	-- Don't hardcode a shell path here — it varies by distro/install and
	-- breaks this config on any box that doesn't have zsh. Let WezTerm
	-- fall back to $SHELL (the user's actual login shell) instead.
	-- If you want zsh specifically, install it first: sudo apt install zsh
	-- then uncomment the line below.
	-- config.default_prog = { "/usr/bin/zsh", "-l" }
end

-- ---------------------------------------------------------------------
-- Keybindings
-- ---------------------------------------------------------------------
config.leader = { key = "a", mods = "CTRL", timeout_milliseconds = 1000 }

-- Shared: copy if text is selected, otherwise send the normal Ctrl+C
-- interrupt (SIGINT) so stuck commands can still be killed.
local function copy_or_interrupt(window, pane)
	local has_selection = window:get_selection_text_for_pane(pane) ~= ""
	if has_selection then
		window:perform_action(wezterm.action.CopyTo("ClipboardAndPrimarySelection"), pane)
		window:perform_action(wezterm.action.ClearSelection, pane)
	else
		window:perform_action(wezterm.action.SendKey({ key = "c", mods = "CTRL" }), pane)
	end
end

config.keys = {
	-- Splits (tmux-style leader key)
	{ key = "|", mods = "LEADER|SHIFT", action = wezterm.action.SplitHorizontal({ domain = "CurrentPaneDomain" }) },
	{ key = "-", mods = "LEADER", action = wezterm.action.SplitVertical({ domain = "CurrentPaneDomain" }) },

	-- Pane navigation
	{ key = "h", mods = "LEADER", action = wezterm.action.ActivatePaneDirection("Left") },
	{ key = "l", mods = "LEADER", action = wezterm.action.ActivatePaneDirection("Right") },
	{ key = "k", mods = "LEADER", action = wezterm.action.ActivatePaneDirection("Up") },
	{ key = "j", mods = "LEADER", action = wezterm.action.ActivatePaneDirection("Down") },

	-- Close pane
	{ key = "x", mods = "LEADER", action = wezterm.action.CloseCurrentPane({ confirm = true }) },

	-- Tabs
	-- Browser-style new tab. This overrides WezTerm's default Ctrl+Shift+T
	-- binding for new tab — Ctrl+T now does it without needing Shift.
	{ key = "t", mods = "CTRL", action = wezterm.action.SpawnTab("CurrentPaneDomain") },
	{ key = "t", mods = "LEADER", action = wezterm.action.SpawnTab("CurrentPaneDomain") },
	{ key = "n", mods = "LEADER", action = wezterm.action.ActivateTabRelative(1) },
	{ key = "p", mods = "LEADER", action = wezterm.action.ActivateTabRelative(-1) },

	-- Browser-style tab close. NOTE: Ctrl+W is also the readline shortcut
	-- for "delete word backward" in bash/zsh/PowerShell — binding it here
	-- means that shortcut is gone everywhere in WezTerm. If you rely on
	-- word-delete a lot, consider Cmd+W (mac) / Ctrl+Shift+W instead.
	{ key = "w", mods = "CTRL", action = wezterm.action.CloseCurrentTab({ confirm = true }) },

	-- Browser-style copy/paste, made "smart" so they don't break terminal
	-- conventions:
	--   Ctrl+C: copy if you have a selection, otherwise send the normal
	--           SIGINT interrupt (so Ctrl+C still kills stuck commands).
	{ key = "c", mods = "CTRL", action = wezterm.action_callback(copy_or_interrupt) },

	--   Shift+3: alias for the same copy-or-interrupt behavior as Ctrl+C.
	-- { key = "#", mods = "SHIFT", action = wezterm.action_callback(copy_or_interrupt) },

	--   Ctrl+V: straightforward paste. (Overrides readline's rarely-used
	--           "insert next char literally" binding.)
	{ key = "v", mods = "CTRL", action = wezterm.action.PasteFrom("Clipboard") },

	--   Ctrl+X: terminal scrollback can't actually be edited/deleted, so
	--           this copies a selection if you have one (closest thing to
	--           "cut"), otherwise clears whatever you're currently typing
	--           at the prompt (readline's kill-whole-line).
	{
		key = "x",
		mods = "CTRL",
		action = wezterm.action_callback(function(window, pane)
			local has_selection = window:get_selection_text_for_pane(pane) ~= ""
			if has_selection then
				window:perform_action(wezterm.action.CopyTo("ClipboardAndPrimarySelection"), pane)
				window:perform_action(wezterm.action.ClearSelection, pane)
			else
				window:perform_action(wezterm.action.SendKey({ key = "u", mods = "CTRL" }), pane)
			end
		end),
	},

	-- Font size
	{ key = "=", mods = "CTRL", action = wezterm.action.IncreaseFontSize },
	{ key = "-", mods = "CTRL", action = wezterm.action.DecreaseFontSize },
	{ key = "0", mods = "CTRL", action = wezterm.action.ResetFontSize },

	-- Launcher (shell picker on Windows, domain picker elsewhere)
	{ key = "p", mods = "CTRL|SHIFT", action = wezterm.action.ShowLauncher },
}

-- ---------------------------------------------------------------------
-- Misc quality-of-life
-- ---------------------------------------------------------------------
config.audible_bell = "Disabled"
config.check_for_updates = true
config.automatically_reload_config = true


return config
