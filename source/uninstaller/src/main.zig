const std = @import("std");
const c = @cImport({
    @cDefine("WIN32_LEAN_AND_MEAN", "1");
    @cInclude("windows.h");
});

//? Requires Zig 0.15.2
//? std.fs.cwd() operations here are deprecated in versions >=0.16.0
//? and the entirety of the file is written for 0.15.2 jank which will NOT be updated bc i hate working on this

// and reg consts for compat
const REG_DWORD: c.DWORD = 4;
const REG_SZ: c.DWORD = 1;
const HKLM: usize = 0x80000002;
const HKCU: usize = 0x80000001;

//* HKEY_LOCAL_MACHINE and HKEY_CURRENT_USER are Win32 predefined pseudo-handles (0x80000002 and 0x80000001 respectively).
//* Zig 0.15.2's @cImport represents HKEY as an aligned C pointer ([*c]struct_HKEY__) and therefore cannot represent this value directly.
//* Keep the root handle as usize and use the ABI-compatible function signature below.

const RegCreateKeyExW = @as(*const fn (
    usize,
    [*:0]const u16,
    c.DWORD,
    ?[*:0]u16,
    c.DWORD,
    c.REGSAM,
    ?*c.SECURITY_ATTRIBUTES,
    *c.HKEY,
    ?*c.DWORD,
) callconv(.winapi) c.LSTATUS, @ptrCast(&c.RegCreateKeyExW));

const RegOpenKeyExW = @as(*const fn (
    usize,
    [*:0]const u16,
    c.DWORD,
    c.REGSAM,
    *c.HKEY,
) callconv(.winapi) c.LSTATUS, @ptrCast(&c.RegOpenKeyExW));

const installer = @import("installer");
var MSI_INSTALL: bool = undefined;

//* if i need a temp allocator:
// var arena = std.heap.ArenaAllocator.init(std.heap.page_allocator);
// defer arena.deinit();
// const allocator: std.mem.Allocator = arena.allocator();

fn _doGetInstallDir_MSI(arena: std.mem.Allocator) ![]u8 {
    const key: c.HKEY = undefined;
    var ty: c.DWORD = undefined;
    // get the required size of u16 array
    var size: c.DWORD = 0;
    var result = c.RegQueryValueExW(
        key,
        std.unicode.utf8ToUtf16LeStringLiteral("InstallPath"),
        null,
        &ty,
        null,
        &size,
    );
    defer _ = c.RegCloseKey(key);
    if (result != 0)
        return error.OpenKeyFailed;
    //// std.debug.assert(ty == windows.REG_SZ); // debug assert type is REG_SZ (the expected type)
    // read in the value
    const utf16: []u16 = try arena.alloc(u16, size / @sizeOf(u16));
    defer arena.free(utf16);
    result = c.RegQueryValueExW(
        key,
        std.unicode.utf8ToUtf16LeStringLiteral("InstallPath"),
        null,
        null,
        @ptrCast(utf16.ptr),
        &size,
    );
    if (result != 0)
        return error.OpenKeyFailed;
    // normalize and convert
    const len = std.mem.indexOfScalar(u16, utf16, 0) orelse utf16.len;
    const install_path: []u8 = try std.unicode.utf16LeToUtf8Alloc(arena, utf16[0..len]);
    defer arena.free(install_path);
    return install_path;
}

fn _doGetInstallDir_CWD(arena: std.mem.Allocator) ![]u8 {
    // current directory is plausible
    var installDir: []u8 = try std.process.getEnvVarOwned(arena, "cd");
    defer arena.free(installDir);
    // see if executable is here
    if (blk: {
        //? this check is only performed twice so im not bothering to make it its own fn
        std.fs.cwd().access(
            try std.fs.path.join(arena, &.{
                installDir,
                "DDCL.exe",
            }), .{}, )
        catch |err| {
            if (err != error.FileNotFound) {
                return err;
            }
            break :blk false; // FileNotFound
        };
        break :blk true; // access succeeded
    }) {
        return installDir;
    } {
        // else try the default install path of the MSI
        installDir = try arena.dupe(u8, "C:\\Program Files\\DDCL");
        if (blk: {
            std.fs.cwd().access(
                try std.fs.path.join(arena, &.{
                    installDir,
                    "DDCL.exe"
                }), .{})
            catch |err| {
                if (err != error.FileNotFound) {
                    return err;
                }
                std.debug.print("File does not exist\n", .{});
                break :blk false; // FileNotFound
            };
            break :blk true; // access succeeded
        }) {
            return installDir;
        }
    }
    // couldnt verify dir
    return error.FileNotFound;
}

fn getInstallDir(arena: std.mem.Allocator) ![]u8 {
    var installDir: []u8 = undefined;
    if (MSI_INSTALL) {
        // if msi installed, check the registry key for location and assume it to be correct.
        installDir = try _doGetInstallDir_MSI(arena);
    } {
        // else use cwd
        installDir = try _doGetInstallDir_CWD(arena);
    }
    return installDir;
}

fn removeShortcut() bool {
    _ = std.fs.deleteTreeAbsolute("C:\\ProgramData\\Microsoft\\Windows\\tart Menu\\Programs\\DDCL") catch |err| {
        // still return a bool on failure
        std.debug.print("Error: {}\n", .{err});
        return false;
    };
    // if everything worked:
    return true;
}

fn cleanPath(arena: std.mem.Allocator, wanted: []const u8) bool {
    const subkey = std.unicode.utf8ToUtf16LeStringLiteral(
        "SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment",
    );
    const value_name = std.unicode.utf8ToUtf16LeStringLiteral("Path");
    var key: c.HKEY = undefined;
    var status = RegOpenKeyExW(
        HKLM,
        subkey.ptr,
        0,
        c.KEY_READ | c.KEY_WOW64_64KEY,
        &key,
    );
    if (status != 0) return false;
    defer _ = c.RegCloseKey(key);
    var utf16_buffer: [32768]u16 = undefined;
    var byte_count: u32 = @intCast(utf16_buffer.len * @sizeOf(u16));
    var registry_type: u32 = 0;
    status = c.RegQueryValueExW(
        key,
        value_name.ptr,
        null,
        &registry_type,
        @ptrCast(&utf16_buffer),
        &byte_count,
    );
    if (status != 0) return false;
    var utf16_len: usize = @intCast(byte_count / @sizeOf(u16));
    if (utf16_len > 0 and utf16_buffer[utf16_len - 1] == 0) {
        utf16_len -= 1;
    }
    const current = std.unicode.utf16LeToUtf8Alloc(
        arena,
        utf16_buffer[0..utf16_len],
    ) catch return false;
    var updated = std.ArrayList(u8).init(arena);
    var removed = false;
    var entries = std.mem.splitScalar(u8, current, ';');
    while (entries.next()) |entry_raw| {
        var entry = std.mem.trim(u8, entry_raw, " \t");
        var target = wanted;
        while (entry.len > 3 and
            (entry[entry.len - 1] == '\\' or entry[entry.len - 1] == '/'))
        {
            entry = entry[0 .. entry.len - 1];
        }
        while (target.len > 3 and
            (target[target.len - 1] == '\\' or target[target.len - 1] == '/'))
        {
            target = target[0 .. target.len - 1];
        }
        // Remove every matching occurrence.
        if (std.ascii.eqlIgnoreCase(entry, target)) {
            removed = true;
            continue;
        }
        if (updated.items.len != 0) {
            try updated.append(';');
        }
        try updated.appendSlice(entry_raw);
    }
    // Nothing was removed, but the operation itself succeeded.
    if (!removed) {
        return true;
    }
    const updated_utf16 = std.unicode.utf8ToUtf16LeAlloc(
        arena,
        updated.items,
    ) catch return false;
    status = RegOpenKeyExW(
        HKLM,
        subkey.ptr,
        0,
        c.KEY_SET_VALUE | c.KEY_WOW64_64KEY,
        &key,
    );
    if (status != 0) return false;
    defer _ = c.RegCloseKey(key);
    status = c.RegSetValueExW(
        key,
        value_name.ptr,
        0,
        registry_type,
        @ptrCast(updated_utf16.ptr),
        @intCast((updated_utf16.len + 1) * @sizeOf(u16)),
    );
    if (status != 0) return false;
    return true;
}

fn _doTasks(arena: std.mem.Allocator) !bool {
    const installDir: []u8 = try getInstallDir(arena);
        defer arena.free(installDir);
    const lnkStatus = removeShortcut();
    const pathStatus = cleanPath(arena, installDir);
    return (lnkStatus & pathStatus & (installDir.len != 0));
}

fn onFail(writer: *std.Io.Writer, err: anyerror) !void {
    std.debug.print("Error: {}\n", .{err});
    try writer.print(
        "Uninstallation failed!",
        .{},
    );
}

fn onSuccess(writer: *std.Io.Writer) !void {
    try writer.print(\\
        \\Uninstallation completed successfully!
        \\ - Registry keys under HKLM\SOFTWARE\DDCL deleted
        \\ - Start Menu shortcut removed
        \\ - PATH updated
        \\
        ,
        .{},
    );
}

fn _isMsiInstall() !bool {
    var key: c.HKEY = undefined;
    const result = RegOpenKeyExW(
        HKLM,
        std.unicode.utf8ToUtf16LeStringLiteral("Software\\DDCL"),
        0,
        c.KEY_QUERY_VALUE | c.KEY_WOW64_64KEY,
        &key,
    );
    defer _ = c.RegCloseKey(key);
    if (result != 0)
        return error.OpenKeyFailed;
    var value: u32 = undefined;
    var _value_size: c.DWORD = @sizeOf(u32);
    const query_result = c.RegQueryValueExW(
        key,
        std.unicode.utf8ToUtf16LeStringLiteral("InstallType"),
        null,
        null,
        @ptrCast(&value),
        &_value_size,
    );
    if (query_result != 0)
        return error.QueryValueFailed;
    if (value == 1)
        return true;
    return false;
}

pub fn main() !void {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const gpa_allocator = gpa.allocator();
    var arena_alloc = std.heap.ArenaAllocator.init(gpa_allocator);
    defer arena_alloc.deinit();
    const arena: std.mem.Allocator = arena_alloc.allocator();

    var stdout_buffer: [1024]u8 = undefined;
    var stdout_file_writer = std.fs.File.stdout().writer(&stdout_buffer);
    const stdout_writer = &stdout_file_writer.interface;
    MSI_INSTALL = try _isMsiInstall();

    const status = _doTasks(arena) catch |err| {
        try onFail(stdout_writer, err);
        try stdout_writer.flush();
        return;
    };

    if (!status) {
        try onFail(stdout_writer, error.null);
        try stdout_writer.flush();
        return;
    }

    try onSuccess(stdout_writer);
    try stdout_writer.flush();
}
