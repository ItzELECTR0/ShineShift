const std = @import("std");

pub fn toRaw(percent: i32, minimum: i32, maximum: i32) i32 {
    const p = std.math.clamp(percent, 0, 100);
    if (maximum <= minimum) return minimum;
    return minimum + @divTrunc((maximum - minimum) * p + 50, 100);
}

pub fn toPercent(raw: i32, minimum: i32, maximum: i32) i32 {
    if (maximum <= minimum) return 0;
    const r = std.math.clamp(raw, minimum, maximum);
    return @divTrunc((r - minimum) * 100 + @divTrunc(maximum - minimum, 2), maximum - minimum);
}

const Out = struct {
    buffer: []u8,
    len: usize = 0,

    fn put(self: *Out, bytes: []const u8) void {
        const count = @min(bytes.len, self.buffer.len - self.len);
        @memcpy(self.buffer[self.len..][0..count], bytes[0..count]);
        self.len += count;
    }
};

fn isHex(character: u8) bool {
    return std.ascii.isHex(character);
}

// Turns the vcp(...) part of a capabilities string into "10 12 60=0F,11,12".
pub fn parseCapabilities(capabilities: []const u8, buffer: []u8) []const u8 {
    var out: Out = .{ .buffer = buffer };
    var start: usize = 0;
    while (std.mem.indexOfPos(u8, capabilities, start, "vcp(")) |found| {
        if (found == 0 or !std.ascii.isAlphanumeric(capabilities[found - 1])) {
            start = found + 4;
            break;
        }
        start = found + 4;
    } else return out.buffer[0..0];

    var index = start;
    var depth: usize = 0;
    var first_value = true;
    while (index < capabilities.len) {
        const character = capabilities[index];
        if (character == '(') {
            depth += 1;
            if (depth == 1) {
                out.put("=");
                first_value = true;
            }
            index += 1;
        } else if (character == ')') {
            if (depth == 0) break;
            depth -= 1;
            index += 1;
        } else if (isHex(character) and index + 1 < capabilities.len and isHex(capabilities[index + 1])) {
            var upper: [2]u8 = .{ std.ascii.toUpper(character), std.ascii.toUpper(capabilities[index + 1]) };
            if (depth == 0) {
                if (out.len > 0) out.put(" ");
                out.put(&upper);
            } else if (depth == 1) {
                if (!first_value) out.put(",");
                out.put(&upper);
                first_value = false;
            }
            index += 2;
        } else {
            index += 1;
        }
    }
    return out.buffer[0..out.len];
}

// Blanks Lua comments in place so a commented-out rule or require never counts.
pub fn stripLuaComments(text: []u8) void {
    var index: usize = 0;
    var quote: u8 = 0;
    while (index < text.len) {
        const character = text[index];
        if (quote != 0) {
            if (character == '\\') {
                index += 2;
                continue;
            }
            if (character == quote) quote = 0;
            index += 1;
        } else if (character == '"' or character == '\'') {
            quote = character;
            index += 1;
        } else if (character == '-' and index + 1 < text.len and text[index + 1] == '-') {
            var end: usize = undefined;
            if (std.mem.startsWith(u8, text[index..], "--[[")) {
                end = if (std.mem.indexOfPos(u8, text, index, "]]")) |close| close + 2 else text.len;
            } else {
                end = std.mem.indexOfScalarPos(u8, text, index, '\n') orelse text.len;
            }
            for (text[index..end]) |*byte| {
                if (byte.* != '\n') byte.* = ' ';
            }
            index = end;
        } else {
            index += 1;
        }
    }
}

fn skipSpace(text: []const u8, from: usize) usize {
    var index = from;
    while (index < text.len and std.ascii.isWhitespace(text[index])) index += 1;
    return index;
}

fn stringLiteral(text: []const u8, from: usize) ?[]const u8 {
    if (from >= text.len) return null;
    const quote = text[from];
    if (quote != '"' and quote != '\'') return null;
    const end = std.mem.indexOfScalarPos(u8, text, from + 1, quote) orelse return null;
    return text[from + 1 .. end];
}

const KeyIterator = struct {
    text: []const u8,
    key: []const u8,
    index: usize = 0,

    // Yields the string literal after each `key =` or `key(`.
    fn next(self: *KeyIterator, opener: u8) ?[]const u8 {
        while (std.mem.indexOfPos(u8, self.text, self.index, self.key)) |found| {
            self.index = found + self.key.len;
            if (found > 0 and (std.ascii.isAlphanumeric(self.text[found - 1]) or self.text[found - 1] == '_')) continue;
            var cursor = skipSpace(self.text, self.index);
            if (opener != 0) {
                if (cursor < self.text.len and self.text[cursor] == opener) cursor = skipSpace(self.text, cursor + 1);
            }
            if (stringLiteral(self.text, cursor)) |literal| return literal;
        }
        return null;
    }
};

// The selector an existing hl.monitor rule uses for this output, in comment-stripped config text.
pub fn findSelector(config: []const u8, name: []const u8, description: []const u8) ?[]const u8 {
    var by_description: ?[]const u8 = null;
    var iterator: KeyIterator = .{ .text = config, .key = "output" };
    while (iterator.next('=')) |literal| {
        if (std.mem.eql(u8, literal, name)) return literal;
        if (std.mem.startsWith(u8, literal, "desc:")) {
            const wanted = std.mem.trim(u8, literal[5..], " \t");
            if (wanted.len > 0 and std.mem.startsWith(u8, description, wanted)) by_description = literal;
        }
    }
    return by_description;
}

pub const RequireIterator = struct {
    inner: KeyIterator,

    pub fn init(config: []const u8) RequireIterator {
        return .{ .inner = .{ .text = config, .key = "require" } };
    }

    pub fn next(self: *RequireIterator) ?[]const u8 {
        return self.inner.next('(');
    }
};

pub fn hasRequire(config: []const u8, module: []const u8) bool {
    var iterator = RequireIterator.init(config);
    while (iterator.next()) |literal| {
        if (std.mem.eql(u8, literal, module)) return true;
    }
    return false;
}

pub const event_reloaded = 1;
pub const event_outputs = 2;

pub fn eventMask(line: []const u8) c_int {
    const name = line[0 .. std.mem.indexOf(u8, line, ">>") orelse return 0];
    if (std.mem.eql(u8, name, "configreloaded")) return event_reloaded;
    if (std.mem.eql(u8, name, "monitoradded") or std.mem.eql(u8, name, "monitorremoved")) return event_outputs;
    return 0;
}

pub fn isHdrPreset(preset: []const u8) bool {
    return std.mem.eql(u8, preset, "hdr") or std.mem.eql(u8, preset, "hdredid");
}

// A selector may only hold characters that are safe inside a Lua string literal.
pub fn safeForLua(value: []const u8) bool {
    for (value) |character| {
        if (character < 0x20 or character == '"' or character == '\'' or character == '\\' or character == ']') return false;
    }
    return value.len > 0;
}

test "normalisation maps the slider onto the chosen range" {
    try std.testing.expectEqual(@as(i32, 20), toRaw(0, 20, 80));
    try std.testing.expectEqual(@as(i32, 80), toRaw(100, 20, 80));
    try std.testing.expectEqual(@as(i32, 50), toRaw(50, 20, 80));
    try std.testing.expectEqual(@as(i32, 50), toPercent(50, 20, 80));
    try std.testing.expectEqual(@as(i32, 100), toPercent(100, 20, 80));
    try std.testing.expectEqual(@as(i32, 0), toPercent(5, 20, 80));
    var percent: i32 = 0;
    while (percent <= 100) : (percent += 1) try std.testing.expectEqual(percent, toPercent(toRaw(percent, 0, 100), 0, 100));
}

test "capabilities keep feature codes and their allowed values" {
    var buffer: [256]u8 = undefined;
    const caps = "(prot(monitor)type(lcd)model(GS27QC)cmds(01 02 03 07 0C 4E F3 E3)vcp(02 04 10 12 14(04 05 08 0b) 60(11 12 0F) D6(01 02 04) DF)mccs_ver(2.0))";
    try std.testing.expectEqualStrings("02 04 10 12 14=04,05,08,0B 60=11,12,0F D6=01,02,04 DF", parseCapabilities(caps, &buffer));
    try std.testing.expectEqualStrings("", parseCapabilities("(prot(monitor))", &buffer));
    try std.testing.expectEqualStrings("10 12", parseCapabilities("vcp(10 12)", &buffer));
}

test "selectors come from live rules only" {
    const source =
        \\-- hl.monitor({ output = "DP-9", mode = "preferred" })
        \\--[[ hl.monitor({ output = "DP-8" }) ]]
        \\hl.monitor({
        \\    output = "DP-1", -- output = "DP-7"
        \\    mode = "2560x1440@170.00",
        \\})
        \\hl.monitor({ output = "desc:Acer Technologies EK240Y", mode = "preferred" })
        \\hl.monitor({ output = "", mode = "preferred" })
        \\local note = "-- output = \"DP-6\""
    ;
    var text: [source.len]u8 = undefined;
    @memcpy(&text, source);
    stripLuaComments(&text);
    try std.testing.expectEqualStrings("DP-1", findSelector(&text, "DP-1", "GIGA-BYTE GS27QC").?);
    try std.testing.expectEqualStrings("desc:Acer Technologies EK240Y", findSelector(&text, "HDMI-A-1", "Acer Technologies EK240Y TJEEE0053W01").?);
    try std.testing.expect(findSelector(&text, "DP-9", "x") == null);
    try std.testing.expect(findSelector(&text, "DP-8", "x") == null);
    try std.testing.expect(findSelector(&text, "DP-7", "x") == null);
    try std.testing.expect(findSelector(&text, "DP-2", "Some Other Monitor") == null);
}

test "requires are found with and without parentheses" {
    const source =
        \\require("display/monitors")
        \\require 'theme/theme'
        \\--require("plugins/dynamic-cursors")
        \\require("shineshift")
    ;
    var text: [source.len]u8 = undefined;
    @memcpy(&text, source);
    stripLuaComments(&text);
    try std.testing.expect(hasRequire(&text, "shineshift"));
    try std.testing.expect(hasRequire(&text, "theme/theme"));
    try std.testing.expect(!hasRequire(&text, "plugins/dynamic-cursors"));
    var iterator = RequireIterator.init(&text);
    try std.testing.expectEqualStrings("display/monitors", iterator.next().?);
}

test "events" {
    try std.testing.expectEqual(@as(c_int, event_reloaded), eventMask("configreloaded>>"));
    try std.testing.expectEqual(@as(c_int, event_outputs), eventMask("monitoradded>>DP-1"));
    try std.testing.expectEqual(@as(c_int, 0), eventMask("monitoraddedv2>>1,DP-1,x"));
    try std.testing.expectEqual(@as(c_int, 0), eventMask("workspace>>2"));
    try std.testing.expect(isHdrPreset("hdr") and isHdrPreset("hdredid") and !isHdrPreset("srgb"));
    try std.testing.expect(safeForLua("desc:Acer Technologies EK240Y") and !safeForLua("a\"b") and !safeForLua(""));
}
