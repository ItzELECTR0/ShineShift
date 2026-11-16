const std = @import("std");
const logic = @import("logic.zig");

extern "c" fn getenv(name: [*:0]const u8) ?[*:0]u8;
extern "c" fn free(pointer: ?*anyopaque) void;
extern "c" fn usleep(microseconds: c_uint) c_int;

const Handle = ?*anyopaque;
const Ref = ?*anyopaque;

const IoPath = extern struct {
    io_mode: c_int,
    number: c_int,
};

const McssVersion = extern struct {
    major: u8,
    minor: u8,
};

// DDCA_Display_Info2 from ddcutil_types.h (libddcutil 2.1 and later).
const DisplayInfo = extern struct {
    marker: [4]u8,
    dispno: c_int,
    path: IoPath,
    usb_bus: c_int,
    usb_device: c_int,
    mfg_id: [4]u8,
    model_name: [14]u8,
    sn: [14]u8,
    product_code: u16,
    edid_bytes: [128]u8,
    vcp_version: McssVersion,
    dref: Ref,
    drm_card_connector: [32]u8,
    drm_card_connector_found_by: c_int,
    drm_connector_id: i16,
    unused: [8]?*anyopaque,
};

const NonTableValue = extern struct {
    mh: u8,
    ml: u8,
    sh: u8,
    sl: u8,
};

extern "ddcutil" fn ddca_init2(options: ?[*:0]const u8, syslog_level: c_int, init_options: c_int, messages: ?*?[*]?[*:0]u8) c_int;
extern "ddcutil" fn ddca_enable_verify(on: bool) bool;
extern "ddcutil" fn ddca_redetect_displays() c_int;
extern "ddcutil" fn ddca_get_display_refs(include_invalid: bool, refs: *?[*:null]Ref) c_int;
extern "ddcutil" fn ddca_get_display_info2(ref: Ref, info: *?*DisplayInfo) c_int;
extern "ddcutil" fn ddca_open_display2(ref: Ref, wait: bool, handle: *Handle) c_int;
extern "ddcutil" fn ddca_close_display(handle: Handle) c_int;
extern "ddcutil" fn ddca_get_capabilities_string(handle: Handle, capabilities: *?[*:0]u8) c_int;
extern "ddcutil" fn ddca_get_non_table_vcp_value(handle: Handle, code: u8, value: *NonTableValue) c_int;
extern "ddcutil" fn ddca_set_non_table_vcp_value(handle: Handle, code: u8, high: u8, low: u8) c_int;
extern "ddcutil" fn ddca_rc_name(status: c_int) ?[*:0]const u8;

pub const max_displays = 16;

pub const Display = extern struct {
    id: [64]u8,
    mfg: [8]u8,
    model: [32]u8,
    serial: [32]u8,
    connector: [32]u8,
    bus: c_int,
};

const FakeValue = struct { code: u8, value: u16, maximum: u16 };

const Slot = struct {
    handle: Handle = null,
    fake: [5]FakeValue = undefined,
};

var slots: [max_displays]Slot = @splat(.{});
var count: usize = 0;
var fake_count: usize = 0;
var initialised = false;
var last_status: c_int = 0;

pub fn isFake() bool {
    return fake_count > 0;
}

pub fn displayCount() usize {
    return count;
}

pub fn lastError() []const u8 {
    if (isFake()) return "fake display";
    return std.mem.span(ddca_rc_name(last_status) orelse return "unknown");
}

fn setText(destination: []u8, value: []const u8) void {
    @memset(destination, 0);
    const length = @min(destination.len - 1, value.len);
    @memcpy(destination[0..length], value[0..length]);
}

fn field(bytes: []const u8) []const u8 {
    const end = std.mem.indexOfScalar(u8, bytes, 0) orelse bytes.len;
    return std.mem.trim(u8, bytes[0..end], " ");
}

fn makeId(display: *Display) void {
    var buffer: [64]u8 = undefined;
    const serial = field(&display.serial);
    const text = std.fmt.bufPrint(&buffer, "{s}-{s}-{s}", .{ field(&display.mfg), field(&display.model), if (serial.len > 0) serial else "0" }) catch buffer[0..0];
    setText(&display.id, text);
    for (&display.id) |*character| {
        if (character.* == ' ' or character.* == '/' or character.* == '\\') character.* = '_';
    }
}

pub fn init() bool {
    if (initialised) return true;
    if (getenv("SHINESHIFT_FAKE")) |value| {
        fake_count = std.fmt.parseInt(usize, std.mem.span(value), 10) catch 2;
        fake_count = std.math.clamp(fake_count, 1, max_displays);
        initialised = true;
        return true;
    }
    // Option 1 skips ddcutil's own config file so a user's CLI options never change how we behave.
    last_status = ddca_init2(null, 0, 1, null);
    if (last_status != 0) return false;
    _ = ddca_enable_verify(false);
    initialised = true;
    return true;
}

fn closeAll() void {
    for (slots[0..count]) |*slot| {
        if (slot.handle != null) _ = ddca_close_display(slot.handle);
        slot.handle = null;
    }
    count = 0;
}

pub fn shutdown() void {
    if (!isFake()) closeAll();
    count = 0;
}

fn detectFake(displays: []Display) usize {
    const total = @min(fake_count, displays.len);
    for (displays[0..total], 0..) |*display, index| {
        var buffer: [32]u8 = undefined;
        setText(&display.mfg, "FAK");
        setText(&display.model, std.fmt.bufPrint(&buffer, "Fake {d}", .{index + 1}) catch "Fake");
        setText(&display.serial, std.fmt.bufPrint(&buffer, "{d:0>4}", .{index + 1}) catch "0");
        setText(&display.connector, std.fmt.bufPrint(&buffer, "FAKE-{d}", .{index + 1}) catch "FAKE");
        display.bus = @intCast(index);
        makeId(display);
        if (index >= count) {
            slots[index].fake = .{
                .{ .code = 0x10, .value = 50, .maximum = 100 },
                .{ .code = 0x12, .value = 50, .maximum = 100 },
                .{ .code = 0x62, .value = 30, .maximum = 100 },
                .{ .code = 0x60, .value = 0x0f, .maximum = 0x12 },
                .{ .code = 0xd6, .value = 1, .maximum = 5 },
            };
        }
    }
    count = total;
    return total;
}

// Closes every handle, asks libddcutil to look again and opens what answers.
pub fn detect(displays: []Display, rescan: bool) usize {
    if (!initialised) return 0;
    if (isFake()) return detectFake(displays);
    closeAll();
    if (rescan) _ = ddca_redetect_displays();

    var refs: ?[*:null]Ref = null;
    last_status = ddca_get_display_refs(false, &refs);
    if (last_status != 0) return 0;
    const list = refs orelse return 0;

    var index: usize = 0;
    while (list[index] != null and count < @min(displays.len, max_displays)) : (index += 1) {
        var info: ?*DisplayInfo = null;
        if (ddca_get_display_info2(list[index], &info) != 0) continue;
        const record = info orelse continue;
        defer free(record);
        if (!std.mem.eql(u8, &record.marker, "DDIN")) continue;

        var handle: Handle = null;
        last_status = ddca_open_display2(list[index], false, &handle);
        if (last_status != 0) continue;

        const display = &displays[count];
        setText(&display.mfg, field(&record.mfg_id));
        setText(&display.model, field(&record.model_name));
        setText(&display.serial, field(&record.sn));
        // "card1-DP-1" becomes the name compositors use, "DP-1".
        const connector = field(&record.drm_card_connector);
        const dash = std.mem.indexOfScalar(u8, connector, '-');
        setText(&display.connector, if (dash) |position| connector[position + 1 ..] else connector);
        display.bus = if (record.path.io_mode == 0) record.path.number else -1;
        makeId(display);
        slots[count].handle = handle;
        count += 1;
    }
    return count;
}

const fake_capabilities = "(prot(monitor)type(lcd)model(Fake)vcp(10 12 60(0F 11 12) 62 D6(01 02 04))mccs_ver(2.1))";

pub fn capabilities(display: usize, buffer: []u8) ?[]const u8 {
    if (display >= count) return null;
    if (isFake()) return logic.parseCapabilities(fake_capabilities, buffer);
    var raw: ?[*:0]u8 = null;
    last_status = ddca_get_capabilities_string(slots[display].handle, &raw);
    if (last_status != 0) return null;
    const text = raw orelse return null;
    defer free(text);
    return logic.parseCapabilities(std.mem.span(text), buffer);
}

fn fakeValue(display: usize, code: u8) ?*FakeValue {
    for (&slots[display].fake) |*entry| {
        if (entry.code == code) return entry;
    }
    return null;
}

pub const Reading = struct { value: i32, maximum: i32 };

pub fn get(display: usize, code: u8) ?Reading {
    if (display >= count) return null;
    if (isFake()) {
        const entry = fakeValue(display, code) orelse return null;
        return .{ .value = entry.value, .maximum = entry.maximum };
    }
    var value: NonTableValue = undefined;
    last_status = ddca_get_non_table_vcp_value(slots[display].handle, code, &value);
    if (last_status != 0) return null;
    return .{ .value = (@as(i32, value.sh) << 8) | value.sl, .maximum = (@as(i32, value.mh) << 8) | value.ml };
}

pub fn set(display: usize, code: u8, value: i32) bool {
    if (display >= count or value < 0 or value > 0xffff) return false;
    if (isFake()) {
        const entry = fakeValue(display, code) orelse return false;
        // Roughly what one DDC write costs on a real bus, so queueing behaves the same in tests.
        _ = usleep(40_000);
        entry.value = @intCast(value);
        return true;
    }
    last_status = ddca_set_non_table_vcp_value(slots[display].handle, code, @intCast(value >> 8), @intCast(value & 0xff));
    return last_status == 0;
}

// Changes a fake monitor behind the app's back, the way a real one resets itself.
pub fn drift(display: usize, code: u8, value: i32) bool {
    if (!isFake() or display >= count or value < 0 or value > 0xffff) return false;
    const entry = fakeValue(display, code) orelse return false;
    entry.value = @intCast(value);
    return true;
}

test "display info matches the C layout" {
    if (@import("builtin").cpu.arch != .x86_64) return error.SkipZigTest;
    try std.testing.expectEqual(@as(usize, 304), @sizeOf(DisplayInfo));
    try std.testing.expectEqual(@as(usize, 24), @offsetOf(DisplayInfo, "mfg_id"));
    try std.testing.expectEqual(@as(usize, 42), @offsetOf(DisplayInfo, "sn"));
    try std.testing.expectEqual(@as(usize, 186), @offsetOf(DisplayInfo, "vcp_version"));
    try std.testing.expectEqual(@as(usize, 192), @offsetOf(DisplayInfo, "dref"));
    try std.testing.expectEqual(@as(usize, 200), @offsetOf(DisplayInfo, "drm_card_connector"));
    try std.testing.expectEqual(@as(usize, 236), @offsetOf(DisplayInfo, "drm_connector_id"));
}
