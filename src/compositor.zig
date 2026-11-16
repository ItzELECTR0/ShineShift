const std = @import("std");
const logic = @import("logic.zig");
const ddc = @import("ddc.zig");

extern "c" fn getenv(name: [*:0]const u8) ?[*:0]u8;
extern "c" fn socket(domain: c_int, kind: c_int, protocol: c_int) c_int;
extern "c" fn connect(fd: c_int, address: *const anyopaque, length: c_uint) c_int;
extern "c" fn bind(fd: c_int, address: *const anyopaque, length: c_uint) c_int;
extern "c" fn setsockopt(fd: c_int, level: c_int, name: c_int, value: *const anyopaque, length: c_uint) c_int;
extern "c" fn read(fd: c_int, buffer: [*]u8, count: usize) isize;
extern "c" fn write(fd: c_int, buffer: [*]const u8, count: usize) isize;
extern "c" fn close(fd: c_int) c_int;
extern "c" fn open(path: [*:0]const u8, flags: c_int, ...) c_int;
extern "c" fn rename(old: [*:0]const u8, new: [*:0]const u8) c_int;
extern "c" fn access(path: [*:0]const u8, mode: c_int) c_int;
extern "c" fn realpath(path: [*:0]const u8, resolved: ?[*]u8) ?[*:0]u8;
extern "c" fn free(pointer: ?*anyopaque) void;
extern "c" fn fork() c_int;
extern "c" fn execvp(file: [*:0]const u8, argv: [*:null]const ?[*:0]const u8) c_int;
extern "c" fn _exit(status: c_int) noreturn;
extern "c" fn waitpid(pid: c_int, status: *c_int, options: c_int) c_int;

const af_unix = 1;
const af_netlink = 16;
const sock_stream = 1;
const sock_dgram = 2;
const sock_cloexec = 0o2000000;
const netlink_kobject_uevent = 15;
const sol_socket = 1;
const so_rcvtimeo = 20;
const o_rdonly = 0;
const o_wronly = 1;
const o_create_truncate = 0o1101;
const o_append = 0o2000;
const o_cloexec = 0o2000000;

const SockaddrUn = extern struct {
    family: u16 = af_unix,
    path: [108]u8 = @splat(0),
};

const SockaddrNl = extern struct {
    family: u16 = af_netlink,
    pad: u16 = 0,
    pid: u32 = 0,
    groups: u32 = 1,
};

const Timeval = extern struct {
    seconds: i64,
    microseconds: i64,
};

pub const Kind = enum { none, hyprland, fake };

const module = "shineshift";
const header = "-- Written by ShineShift. Changes made here are overwritten.\n";

var fake_sdr: [ddc.max_displays]f64 = @splat(1.0);
var known_config: ?[]u8 = null;
var carry: [512]u8 = undefined;
var carry_len: usize = 0;

fn env(name: [*:0]const u8) ?[]const u8 {
    const value = std.mem.span(getenv(name) orelse return null);
    return if (value.len > 0) value else null;
}

pub fn kind() Kind {
    if (ddc.isFake()) return .fake;
    if (env("HYPRLAND_INSTANCE_SIGNATURE") != null) return .hyprland;
    return .none;
}

fn socketPath(buffer: []u8, name: []const u8) ?[]const u8 {
    const signature = env("HYPRLAND_INSTANCE_SIGNATURE") orelse return null;
    const runtime = env("XDG_RUNTIME_DIR") orelse return null;
    return std.fmt.bufPrint(buffer, "{s}/hypr/{s}/{s}", .{ runtime, signature, name }) catch null;
}

fn connectTo(name: []const u8) ?c_int {
    var address: SockaddrUn = .{};
    const path = socketPath(address.path[0 .. address.path.len - 1], name) orelse return null;
    _ = path;
    const fd = socket(af_unix, sock_stream | sock_cloexec, 0);
    if (fd < 0) return null;
    if (connect(fd, &address, @sizeOf(SockaddrUn)) != 0) {
        _ = close(fd);
        return null;
    }
    return fd;
}

fn writeAll(fd: c_int, bytes: []const u8) bool {
    var remaining = bytes;
    while (remaining.len > 0) {
        const written = write(fd, remaining.ptr, remaining.len);
        if (written <= 0) return false;
        remaining = remaining[@intCast(written)..];
    }
    return true;
}

// One request on Hyprland's command socket; the reply runs until the compositor closes it.
fn request(allocator: std.mem.Allocator, command: []const u8) ?[]u8 {
    const fd = connectTo(".socket.sock") orelse return null;
    defer _ = close(fd);
    const timeout: Timeval = .{ .seconds = 3, .microseconds = 0 };
    _ = setsockopt(fd, sol_socket, so_rcvtimeo, &timeout, @sizeOf(Timeval));
    if (!writeAll(fd, command)) return null;
    var reply: std.ArrayList(u8) = .empty;
    var buffer: [8192]u8 = undefined;
    while (true) {
        const size = read(fd, &buffer, buffer.len);
        if (size <= 0) break;
        reply.appendSlice(allocator, buffer[0..@intCast(size)]) catch return null;
    }
    return reply.toOwnedSlice(allocator) catch null;
}

fn readFile(allocator: std.mem.Allocator, path: [:0]const u8) ?[]u8 {
    const fd = open(path, o_rdonly | o_cloexec);
    if (fd < 0) return null;
    defer _ = close(fd);
    var content: std.ArrayList(u8) = .empty;
    var buffer: [8192]u8 = undefined;
    while (content.items.len < 4 * 1024 * 1024) {
        const size = read(fd, &buffer, buffer.len);
        if (size <= 0) break;
        content.appendSlice(allocator, buffer[0..@intCast(size)]) catch return null;
    }
    return content.toOwnedSlice(allocator) catch null;
}

fn mainConfigPath(allocator: std.mem.Allocator) ?[:0]u8 {
    if (env("SHINESHIFT_HYPR_CONFIG")) |path| return std.fmt.allocPrintSentinel(allocator, "{s}", .{path}, 0) catch null;
    if (env("XDG_CONFIG_HOME")) |config| return std.fmt.allocPrintSentinel(allocator, "{s}/hypr/hyprland.lua", .{config}, 0) catch null;
    const home = env("HOME") orelse return null;
    return std.fmt.allocPrintSentinel(allocator, "{s}/.config/hypr/hyprland.lua", .{home}, 0) catch null;
}

fn resolveModule(allocator: std.mem.Allocator, directory: []const u8, name: []const u8) ?[:0]u8 {
    if (name.len == 0 or std.mem.indexOf(u8, name, "..") != null) return null;
    if (name[0] == '/') return std.fmt.allocPrintSentinel(allocator, "{s}", .{name}, 0) catch null;
    if (std.mem.startsWith(u8, name, "~/")) {
        const home = env("HOME") orelse return null;
        return std.fmt.allocPrintSentinel(allocator, "{s}/{s}", .{ home, name[2..] }, 0) catch null;
    }
    const relative = allocator.dupe(u8, if (std.mem.startsWith(u8, name, "./")) name[2..] else name) catch return null;
    if (!std.mem.endsWith(u8, relative, ".lua")) std.mem.replaceScalar(u8, relative, '.', '/');
    const suffixes = [_][]const u8{ "", ".lua", "/init.lua" };
    for (suffixes) |suffix| {
        if (suffix.len == 0 and !std.mem.endsWith(u8, relative, ".lua")) continue;
        const path = std.fmt.allocPrintSentinel(allocator, "{s}/{s}{s}", .{ directory, relative, suffix }, 0) catch return null;
        if (access(path, 0) == 0) return path;
    }
    return null;
}

// The main config and everything it requires, comment-stripped, as one text.
fn loadConfig(allocator: std.mem.Allocator) ?[]u8 {
    const main_path = mainConfigPath(allocator) orelse return null;
    const directory = std.fs.path.dirname(main_path) orelse return null;
    var paths: std.ArrayList([:0]u8) = .empty;
    paths.append(allocator, main_path) catch return null;
    var combined: std.ArrayList(u8) = .empty;
    var index: usize = 0;
    while (index < paths.items.len and index < 128) : (index += 1) {
        const text = readFile(allocator, paths.items[index]) orelse {
            if (index == 0) return null;
            continue;
        };
        logic.stripLuaComments(text);
        combined.appendSlice(allocator, text) catch return null;
        combined.append(allocator, '\n') catch return null;
        var requires = logic.RequireIterator.init(text);
        next: while (requires.next()) |name| {
            const path = resolveModule(allocator, directory, name) orelse continue;
            for (paths.items) |known| {
                if (std.mem.eql(u8, known, path)) continue :next;
            }
            paths.append(allocator, path) catch return null;
        }
    }
    return combined.items;
}

fn jsonString(object: std.json.ObjectMap, key: []const u8) []const u8 {
    const value = object.get(key) orelse return "";
    return if (value == .string) value.string else "";
}

fn jsonNumber(object: std.json.ObjectMap, key: []const u8) f64 {
    const value = object.get(key) orelse return 1.0;
    return switch (value) {
        .float => |number| number,
        .integer => |number| @floatFromInt(number),
        else => 1.0,
    };
}

fn appendLine(allocator: std.mem.Allocator, lines: *std.ArrayList(u8), name: []const u8, description: []const u8, hdr: bool, sdr: f64, selector: []const u8) void {
    const line = std.fmt.allocPrint(allocator, "{s}\t{s}\t{d}\t{d:.3}\t{s}\n", .{ name, description, @intFromBool(hdr), sdr, selector }) catch return;
    lines.appendSlice(allocator, line) catch {};
}

pub fn formatOutputs(allocator: std.mem.Allocator, monitors_json: []const u8, config: ?[]const u8) ?[]u8 {
    const parsed = std.json.parseFromSlice(std.json.Value, allocator, monitors_json, .{}) catch return null;
    if (parsed.value != .array) return null;
    var lines: std.ArrayList(u8) = .empty;
    for (parsed.value.array.items) |entry| {
        if (entry != .object) continue;
        const name = jsonString(entry.object, "name");
        const description = jsonString(entry.object, "description");
        var selector: []const u8 = "";
        if (config) |text| {
            if (logic.findSelector(text, name, description)) |found| {
                if (logic.safeForLua(found)) selector = found;
            }
        }
        appendLine(allocator, &lines, name, description, logic.isHdrPreset(jsonString(entry.object, "colorManagementPreset")), jsonNumber(entry.object, "sdrBrightness"), selector);
    }
    return lines.items;
}

// One line per output: name, description, hdr, sdr brightness, rule selector (empty when unsafe to override).
pub fn outputs(allocator: std.mem.Allocator, rescan_config: bool) ?[]u8 {
    switch (kind()) {
        .none => return null,
        .fake => {
            var lines: std.ArrayList(u8) = .empty;
            for (0..ddc.displayCount()) |index| {
                var name: [16]u8 = undefined;
                const text = std.fmt.bufPrint(&name, "FAKE-{d}", .{index + 1}) catch continue;
                appendLine(allocator, &lines, text, "Fake display", index == 0, fake_sdr[index], text);
            }
            return lines.items;
        },
        .hyprland => {
            const reply = request(allocator, "j/monitors") orelse return null;
            if (rescan_config or known_config == null) {
                if (known_config) |old| std.heap.page_allocator.free(old);
                known_config = if (loadConfig(allocator)) |text| std.heap.page_allocator.dupe(u8, text) catch null else null;
            }
            return formatOutputs(allocator, reply, known_config);
        },
    }
}

pub fn setSdr(allocator: std.mem.Allocator, selector: []const u8, value: f64) bool {
    if (!(value >= 0.05 and value <= 10.0)) return false;
    switch (kind()) {
        .none => return false,
        .fake => {
            if (!std.mem.startsWith(u8, selector, "FAKE-")) return false;
            const number = std.fmt.parseInt(usize, selector[5..], 10) catch return false;
            if (number == 0 or number > ddc.max_displays) return false;
            fake_sdr[number - 1] = value;
            return true;
        },
        .hyprland => {
            if (!logic.safeForLua(selector)) return false;
            const command = std.fmt.allocPrint(allocator, "/eval hl.monitor({{ output = \"{s}\", sdrbrightness = {d:.3} }})", .{ selector, value }) catch return false;
            const reply = request(allocator, command) orelse return false;
            return std.mem.eql(u8, std.mem.trim(u8, reply, " \n"), "ok");
        },
    }
}

pub fn eventsOpen() c_int {
    if (kind() != .hyprland) return -1;
    carry_len = 0;
    return connectTo(".socket2.sock") orelse -1;
}

// Call when the events descriptor is readable. Returns a mask of logic.event_* bits, or -1 once closed.
pub fn eventsRead(fd: c_int) c_int {
    var buffer: [4096]u8 = undefined;
    const size = read(fd, &buffer, buffer.len);
    if (size <= 0) return -1;
    var mask: c_int = 0;
    var lines = std.mem.splitScalar(u8, buffer[0..@intCast(size)], '\n');
    var first = true;
    while (lines.next()) |piece| {
        const complete = lines.peek() != null;
        if (first and carry_len > 0) {
            const room = @min(piece.len, carry.len - carry_len);
            @memcpy(carry[carry_len..][0..room], piece[0..room]);
            carry_len += room;
            if (complete) {
                mask |= logic.eventMask(carry[0..carry_len]);
                carry_len = 0;
            }
        } else if (complete) {
            mask |= logic.eventMask(piece);
        } else {
            carry_len = @min(piece.len, carry.len);
            @memcpy(carry[0..carry_len], piece[0..carry_len]);
        }
        first = false;
    }
    return mask;
}

pub fn hotplugOpen() c_int {
    const fd = socket(af_netlink, sock_dgram | sock_cloexec, netlink_kobject_uevent);
    if (fd < 0) return -1;
    const address: SockaddrNl = .{};
    if (bind(fd, &address, @sizeOf(SockaddrNl)) != 0) {
        _ = close(fd);
        return -1;
    }
    return fd;
}

// Call when the hotplug descriptor is readable. 1 for a display change, 0 for anything else, -1 once closed.
pub fn hotplugRead(fd: c_int) c_int {
    var buffer: [8192]u8 = undefined;
    const size = read(fd, &buffer, buffer.len);
    if (size <= 0) return -1;
    var fields = std.mem.splitScalar(u8, buffer[0..@intCast(size)], 0);
    while (fields.next()) |entry| {
        if (std.mem.eql(u8, entry, "SUBSYSTEM=drm")) return 1;
    }
    return 0;
}

fn run(argv: [*:null]const ?[*:0]const u8) bool {
    const child = fork();
    if (child < 0) return false;
    if (child == 0) {
        _ = execvp(argv[0].?, argv);
        _exit(127);
    }
    var status: c_int = 0;
    if (waitpid(child, &status, 0) < 0) return false;
    return status == 0;
}

pub fn screensOff(allocator: std.mem.Allocator) bool {
    if (kind() == .hyprland) {
        const reply = request(allocator, "/dispatch hl.dsp.dpms({ action = \"off\" })") orelse return false;
        return std.mem.startsWith(u8, std.mem.trim(u8, reply, " \n"), "ok");
    }
    if (env("SWAYSOCK") != null) return run(&.{ "swaymsg", "output * power off", null });
    if (env("WAYLAND_DISPLAY") == null and env("DISPLAY") != null) return run(&.{ "xset", "dpms", "force", "off", null });
    return false;
}

pub fn floatWindow(allocator: std.mem.Allocator, title: []const u8, width: c_int, height: c_int) bool {
    if (!logic.safeForLua(title)) return false;
    if (env("HYPRLAND_INSTANCE_SIGNATURE") != null) {
        const command = std.fmt.allocPrint(allocator, "/eval local title = '{s}'; for _, w in ipairs(hl.get_windows({{ title = title }})) do " ++
            "if w.title == title then hl.dispatch(hl.dsp.window.float({{ action = 'set', window = w }})); " ++
            "hl.dispatch(hl.dsp.window.resize({{ window = w, x = {d}, y = {d}, relative = false }})); " ++
            "hl.dispatch(hl.dsp.window.center({{ window = w }})); end end", .{ title, width, height }) catch return false;
        return request(allocator, command) != null;
    }
    if (env("SWAYSOCK") != null) {
        const command = std.fmt.allocPrintSentinel(allocator, "[title=\"^{s}$\"] floating enable, resize set {d} {d}, move position center", .{ title, width, height }, 0) catch return false;
        return run(&.{ "swaymsg", command.ptr, null });
    }
    return false;
}

fn modulePath(allocator: std.mem.Allocator) ?[:0]u8 {
    const main_path = mainConfigPath(allocator) orelse return null;
    const directory = std.fs.path.dirname(main_path) orelse return null;
    return std.fmt.allocPrintSentinel(allocator, "{s}/{s}.lua", .{ directory, module }, 0) catch null;
}

fn writeFile(path: [:0]const u8, flags: c_int, bytes: []const u8) bool {
    const fd = open(path, flags | o_cloexec, @as(c_uint, 0o644));
    if (fd < 0) return false;
    defer _ = close(fd);
    return writeAll(fd, bytes);
}

// -1 when there is no Lua config to hook into, 0 when the require line is missing, 1 when it is there.
pub fn configStatus(allocator: std.mem.Allocator) c_int {
    if (kind() != .hyprland) return -1;
    const main_path = mainConfigPath(allocator) orelse return -1;
    const text = readFile(allocator, main_path) orelse return -1;
    logic.stripLuaComments(text);
    return @intFromBool(logic.hasRequire(text, module));
}

// Replaces the module through a rename, so Hyprland never reads a half-written file.
pub fn configWrite(allocator: std.mem.Allocator, body: []const u8) bool {
    if (kind() != .hyprland) return false;
    const path = modulePath(allocator) orelse return false;
    const resolved = realpath(path, null);
    defer free(resolved);
    const target: [:0]const u8 = if (resolved) |real| std.mem.span(real) else path;
    const temporary = std.fmt.allocPrintSentinel(allocator, "{s}.tmp", .{target}, 0) catch return false;
    const content = std.fmt.allocPrint(allocator, "{s}{s}", .{ header, body }) catch return false;
    if (readFile(allocator, target)) |existing| {
        if (std.mem.eql(u8, existing, content)) return true;
    }
    if (!writeFile(temporary, o_create_truncate, content)) return false;
    return rename(temporary, target) == 0;
}

// Makes sure the module exists, then appends one require line to the main config with a single write.
pub fn configInstall(allocator: std.mem.Allocator) bool {
    const status = configStatus(allocator);
    if (status != 0) return status == 1;
    const path = modulePath(allocator) orelse return false;
    if (access(path, 0) != 0 and !configWrite(allocator, "")) return false;
    const main_path = mainConfigPath(allocator) orelse return false;
    const resolved = realpath(main_path, null) orelse return false;
    defer free(resolved);
    const text = readFile(allocator, main_path) orelse return false;
    const lead: []const u8 = if (text.len == 0 or text[text.len - 1] == '\n') "" else "\n";
    const line = std.fmt.allocPrint(allocator, "{s}require(\"{s}\")\n", .{ lead, module }) catch return false;
    return writeFile(std.mem.span(resolved), o_wronly | o_append, line);
}

pub fn configLine(allocator: std.mem.Allocator, selector: []const u8, value: f64) ?[]u8 {
    if (!logic.safeForLua(selector)) return null;
    return std.fmt.allocPrint(allocator, "hl.monitor({{ output = \"{s}\", sdrbrightness = {d:.3} }})\n", .{ selector, value }) catch null;
}

test "outputs carry the hdr state and a selector only for an existing rule" {
    var arena = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena.deinit();
    const json =
        \\[{"name":"DP-1","description":"GIGA-BYTE GS27QC 24035B","colorManagementPreset":"hdr","sdrBrightness":1.5},
        \\ {"name":"HDMI-A-1","description":"Acer EK240Y","colorManagementPreset":"adobe","sdrBrightness":1}]
    ;
    const config = "hl.monitor({ output = \"DP-1\", sdrbrightness = 1.5 })\nhl.monitor({ output = \"\" })\n";
    const lines = formatOutputs(arena.allocator(), json, config).?;
    try std.testing.expectEqualStrings("DP-1\tGIGA-BYTE GS27QC 24035B\t1\t1.500\tDP-1\nHDMI-A-1\tAcer EK240Y\t0\t1.000\t\n", lines);
    const line = configLine(arena.allocator(), "DP-1", 1.25).?;
    try std.testing.expectEqualStrings("hl.monitor({ output = \"DP-1\", sdrbrightness = 1.250 })\n", line);
}
