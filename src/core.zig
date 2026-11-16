const std = @import("std");
const logic = @import("logic.zig");
const ddc = @import("ddc.zig");
const compositor = @import("compositor.zig");

const ok = 0;
const failed = 1;
const unsupported = 2;

fn copyOut(destination: [*]u8, capacity: usize, value: []const u8) void {
    if (capacity == 0) return;
    const count = @min(capacity - 1, value.len);
    @memcpy(destination[0..count], value[0..count]);
    destination[count] = 0;
}

fn index(display: c_int) ?usize {
    return if (display >= 0) @intCast(display) else null;
}

fn code(value: c_int) ?u8 {
    return if (value >= 0 and value <= 0xff) @intCast(value) else null;
}

export fn shine_init(errbuf: [*]u8, error_capacity: usize) c_int {
    if (ddc.init()) return ok;
    copyOut(errbuf, error_capacity, ddc.lastError());
    return failed;
}

export fn shine_shutdown() void {
    ddc.shutdown();
}

export fn shine_is_fake() c_int {
    return @intFromBool(ddc.isFake());
}

export fn shine_detect(displays: [*]ddc.Display, capacity: c_int, rescan: c_int) c_int {
    if (capacity <= 0) return 0;
    return @intCast(ddc.detect(displays[0..@intCast(capacity)], rescan != 0));
}

export fn shine_capabilities(display: c_int, out: [*]u8, capacity: usize) c_int {
    var buffer: [2048]u8 = undefined;
    const text = ddc.capabilities(index(display) orelse return failed, &buffer) orelse return failed;
    copyOut(out, capacity, text);
    return ok;
}

export fn shine_get(display: c_int, feature: c_int, value: *c_int, maximum: *c_int) c_int {
    const reading = ddc.get(index(display) orelse return failed, code(feature) orelse return failed) orelse return failed;
    value.* = reading.value;
    maximum.* = reading.maximum;
    return ok;
}

export fn shine_set(display: c_int, feature: c_int, value: c_int) c_int {
    return if (ddc.set(index(display) orelse return failed, code(feature) orelse return failed, value)) ok else failed;
}

export fn shine_drift(display: c_int, feature: c_int, value: c_int) c_int {
    return if (ddc.drift(index(display) orelse return failed, code(feature) orelse return failed, value)) ok else failed;
}

export fn shine_to_raw(percent: c_int, minimum: c_int, maximum: c_int) c_int {
    return logic.toRaw(percent, minimum, maximum);
}

export fn shine_to_percent(raw: c_int, minimum: c_int, maximum: c_int) c_int {
    return logic.toPercent(raw, minimum, maximum);
}

export fn shine_compositor() c_int {
    return switch (compositor.kind()) {
        .none => 0,
        .hyprland => 1,
        .fake => 2,
    };
}

// The config is only read again when `rescan_config` is set, which callers do after a reload.
export fn shine_outputs(out: [*]u8, capacity: usize, rescan_config: c_int) c_int {
    var arena = std.heap.ArenaAllocator.init(std.heap.page_allocator);
    defer arena.deinit();
    const lines = compositor.outputs(arena.allocator(), rescan_config != 0) orelse return unsupported;
    copyOut(out, capacity, lines);
    return ok;
}

export fn shine_set_sdr(selector: [*:0]const u8, value: f64) c_int {
    var arena = std.heap.ArenaAllocator.init(std.heap.page_allocator);
    defer arena.deinit();
    return if (compositor.setSdr(arena.allocator(), std.mem.span(selector), value)) ok else failed;
}

export fn shine_events_open() c_int {
    return compositor.eventsOpen();
}

export fn shine_events_read(fd: c_int) c_int {
    return compositor.eventsRead(fd);
}

export fn shine_hotplug_open() c_int {
    return compositor.hotplugOpen();
}

export fn shine_hotplug_read(fd: c_int) c_int {
    return compositor.hotplugRead(fd);
}

export fn shine_screens_off() c_int {
    var arena = std.heap.ArenaAllocator.init(std.heap.page_allocator);
    defer arena.deinit();
    return if (compositor.screensOff(arena.allocator())) ok else unsupported;
}

export fn shine_float_window(title: [*:0]const u8, width: c_int, height: c_int) c_int {
    var arena = std.heap.ArenaAllocator.init(std.heap.page_allocator);
    defer arena.deinit();
    return if (compositor.floatWindow(arena.allocator(), std.mem.span(title), width, height)) ok else unsupported;
}

export fn shine_config_status() c_int {
    var arena = std.heap.ArenaAllocator.init(std.heap.page_allocator);
    defer arena.deinit();
    return compositor.configStatus(arena.allocator());
}

export fn shine_config_install() c_int {
    var arena = std.heap.ArenaAllocator.init(std.heap.page_allocator);
    defer arena.deinit();
    return if (compositor.configInstall(arena.allocator())) ok else failed;
}

// Entries are "selector<TAB>value" lines; an empty string leaves only the header.
export fn shine_config_write(entries: [*:0]const u8) c_int {
    var arena = std.heap.ArenaAllocator.init(std.heap.page_allocator);
    defer arena.deinit();
    const allocator = arena.allocator();
    var body: std.ArrayList(u8) = .empty;
    var lines = std.mem.tokenizeScalar(u8, std.mem.span(entries), '\n');
    while (lines.next()) |line| {
        const tab = std.mem.indexOfScalar(u8, line, '\t') orelse continue;
        const value = std.fmt.parseFloat(f64, line[tab + 1 ..]) catch continue;
        const text = compositor.configLine(allocator, line[0..tab], value) orelse continue;
        body.appendSlice(allocator, text) catch return failed;
    }
    return if (compositor.configWrite(allocator, body.items)) ok else failed;
}

test {
    _ = logic;
    _ = ddc;
    _ = compositor;
}
