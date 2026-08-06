const std = @import("std");
const windows = std.os.windows;

const installer = @import("installer");
var MSI_INSTALL: bool = undefined;

//? Requires Zig 0.15.2
//? std.os.windows.advapi32 registry wrappers were removed in, and 
  //? std.fs.cwd() operations here are deprecated in, versions >=0.16.0

//* if i need a temp allocator:
// var arena = std.heap.ArenaAllocator.init(std.heap.page_allocator);
// defer arena.deinit();
// const allocator: std.mem.Allocator = arena.allocator();

fn _doGetInstallDir_MSI(arena: std.mem.Allocator) ![]u8 {
    const key: windows.HKEY = undefined;
    var ty: windows.DWORD = undefined;
    // get the required size of u16 array
    var size: windows.DWORD = 0;
    var result = windows.advapi32.RegQueryValueExW(
        key,
        std.unicode.utf8ToUtf16LeStringLiteral("InstallPath"),
        null,
        &ty,
        null,
        &size,
    );
    defer _ = windows.advapi32.RegCloseKey(key);
    if (result != 0)
        return error.OpenKeyFailed;
    //// std.debug.assert(ty == windows.REG_SZ); // debug assert type is REG_SZ (the expected type)
    // read in the value
    const utf16: []u16 = try arena.alloc(u16, size / @sizeOf(u16));
    defer arena.free(utf16);
    result = windows.advapi32.RegQueryValueExW(
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
    var installDir = try std.process.getEnvVarOwned(arena, "cd");
    defer arena.free(installDir);
    // see if executable is here
    if (
        //? this check is only performed twice so im not bothering to make it its own fn
        try std.fs.cwd().access(
            try std.fs.path.join(arena, &.{
                installDir,
                "DDCL.exe"
            }), .{})
    ) {
        return installDir;
    } {
        // else try the default install path of the MSI
        installDir = "C:\\Program Files\\DDCL";
        if (
            try std.fs.cwd().access(
                try std.fs.path.join(arena, &.{
                    installDir,
                    "DDCL.exe"
                }), .{}) catch |err| switch (err) {
                error.FileNotFound => {
                    std.debug.print("File does not exist\n", .{});
                    return;
                }, else => return err,
            }
        ) {
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

fn _doEditRegistry(installDir: []const u8) !void {
    const access = windows.KEY_WRITE | windows.KEY_WOW64_64KEY;
    const installDir16 = try std.unicode.utf8ToUtf16LeAlloc(
        std.heap.page_allocator,
        installDir,
    );
    defer std.heap.page_allocator.free(installDir16);
    {
        var key: windows.HKEY = undefined;
        const result = windows.advapi32.RegCreateKeyExW(
            windows.HKEY_LOCAL_MACHINE,
            std.unicode.utf8ToUtf16LeStringLiteral("Software\\DDCL"),
            0,
            null,
            0,
            access,
            null,
            &key,
            null,
        );
        if (result != 0)
            return error.CreateKeyFailed;
        defer _ = windows.advapi32.RegCloseKey(key);
        var set_result = windows.advapi32.RegSetValueExW(
            key,
            std.unicode.utf8ToUtf16LeStringLiteral("InstallPath"),
            0,
            windows.REG_SZ,
            @ptrCast(installDir16.ptr),
            @intCast(installDir16.len * @sizeOf(u16)),
        );
        if (set_result != 0)
            return error.SetValueFailed;
        const value: windows.DWORD = 0;
        set_result = windows.advapi32.RegSetValueExW(
            key,
            std.unicode.utf8ToUtf16LeStringLiteral("InstallStatus"),
            0,
            windows.REG_DWORD,
            @ptrCast(&value),
            @sizeOf(windows.DWORD),
        );
        if (set_result != 0)
            return error.SetValueFailed;
    }
    {
        var key: windows.HKEY = undefined;
        const result = windows.advapi32.RegOpenKeyExW(
            windows.HKEY_LOCAL_MACHINE,
            std.unicode.utf8ToUtf16LeStringLiteral(
                "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
            ),
            0,
            windows.KEY_WRITE | windows.KEY_WOW64_64KEY,
            &key,
        );
        if (result != 0)
            return error.OpenKeyFailed;
        defer _ = windows.advapi32.RegCloseKey(key);
        const set_result = windows.advapi32.RegSetValueExW(
            key,
            std.unicode.utf8ToUtf16LeStringLiteral("DDCL"),
            0,
            windows.REG_SZ,
            @ptrCast(installDir16.ptr),
            @intCast(installDir16.len * @sizeOf(u16)),
        );
        if (set_result != 0)
            return error.SetValueFailed;
    }
}

fn editRegistry(installDir: []u8) bool {
    try _doEditRegistry(installDir) catch {
        return false;
    };
    return true;
}

// fn deleteValue() void {
//     var key: windows.HKEY = undefined;

//     const path = std.unicode.utf8ToUtf16LeStringLiteral(
//         "Software\\MyApp",
//     );

//     if (windows.advapi32.RegOpenKeyExW(
//         windows.HKEY_CURRENT_USER,
//         path,
//         0,
//         windows.KEY_WRITE | windows.KEY_WOW64_64KEY,
//         &key,
//     ) != 0) return;

//     defer _ = windows.advapi32.RegCloseKey(key);

//     const name = std.unicode.utf8ToUtf16LeStringLiteral(
//         "Enabled",
//     );

//     _ = windows.advapi32.RegDeleteValueW(key, name);

//     const path = std.unicode.utf8ToUtf16LeStringLiteral(
//         "Software\\MyApp",
//     );

//     _ = windows.advapi32.RegDeleteKeyW(
//         windows.HKEY_CURRENT_USER,
//         path,
//     );
//     }

fn _doAddShortcut(arena: std.mem.Allocator, installDir: std.fs.path) !void {
    //* let errors propagate out
    // first off create the directory
    try std.fs.cwd().makePath("C:\\ProgramData\\Microsoft\\Windows\\tart Menu\\Programs\\DDCL");
    // VBS script to generate a .lnk
    // multiline strings dont expand escapes after 0.16
    const scriptText: []u8 = try std.fmt.allocPrint(
        arena,
        \\Set WshShell = CreateObject("WScript.Shell")
        \\Set oLink = WshShell.CreateShortcut("C:\ProgramData\Microsoft\Windows\Start Menu\Programs\DDCL\DDCL.lnk")
        \\oLink.TargetPath = "{s}\DDCL.exe"
        \\oLink.WorkingDirectory = "{s}"
        \\oLink.Save
        , .{ installDir, installDir },
    );
    defer arena.free(scriptText);
    // write to a temp file
    const spath = try std.fs.path.join(arena, &.{
        try std.process.getEnvVarOwned(arena, "TEMP"),
        "t_script.vbs" });
    defer arena.free(spath);
    try std.fs.cwd().writeFile(.{
        .sub_path = spath,
        .data = scriptText,
    });
    // then run cscript on it
    var child = std.process.Child.init(
        &[_][]const u8{ "cscript", "//NoLogo", spath, },
        std.heap.page_allocator,
    );
    _ = try child.spawnAndWait();
    // then delete the script file
    try std.fs.cwd().deleteFile(spath);
}

fn addShortcut(arena: std.mem.Allocator, installDir: std.fs.path) bool {
    _doAddShortcut(arena, installDir) catch |err| {
        // still return a bool on failure
        std.debug.print("Error: {}\n", .{err});
        return false;
    };
    // if everything worked:
    return true;
}

fn ensurePath(arena: std.mem.Allocator, wanted: []const u8) !bool {
    const subkey = std.unicode.utf8ToUtf16LeStringLiteral("SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment");
    const value_name = std.unicode.utf8ToUtf16LeStringLiteral("Path");
    // Read HKLM\...\Environment\Path.
    var key: windows.HKEY = undefined;
    var status = windows.advapi32.RegOpenKeyExW(
        windows.HKEY_LOCAL_MACHINE,
        subkey.ptr,
        0,
        windows.KEY_READ | windows.KEY_WOW64_64KEY,
        &key,
    );
    if (status != .SUCCESS) return error.RegOpenKeyFailed;
    defer _ = windows.advapi32.RegCloseKey(key);
    var utf16_buffer: [32768]u16 = undefined;
    var byte_count: u32 = @intCast(utf16_buffer.len * @sizeOf(u16));
    var registry_type: u32 = 0;
    status = windows.advapi32.RegQueryValueExW(
        key,
        value_name.ptr,
        null,
        &registry_type,
        @ptrCast(&utf16_buffer),
        &byte_count,
    );
    if (status != .SUCCESS) return error.RegQueryValueFailed;
    var utf16_len: usize = @intCast(byte_count / @sizeOf(u16));
    if (utf16_len > 0 and utf16_buffer[utf16_len - 1] == 0) { utf16_len -= 1; }
    const current = try std.unicode.utf16LeToUtf8Alloc(arena, utf16_buffer[0..utf16_len]);
    // Check whether the entry already exists.
    var entries = std.mem.splitScalar(u8, current, ';');
    while (entries.next()) |entry_raw| {
        var entry = std.mem.trim(u8, entry_raw, " \t");
        var target = wanted;
        while (entry.len > 3 and (entry[entry.len - 1] == '\\' or entry[entry.len - 1] == '/')) {
            entry = entry[0 .. entry.len - 1];
        }
        while (target.len > 3 and (target[target.len - 1] == '\\' or target[target.len - 1] == '/')) {
            target = target[0 .. target.len - 1];
        }
        if (std.ascii.eqlIgnoreCase(entry, target)) {
            return false;
        }
    }
    const updated = try std.fmt.allocPrint(
        arena,
        "{s}{s}{s}",
        .{
            current,
            if (current.len == 0) "" else ";",
            wanted,
        },
    );
    // Write the modified value back to the same registry key.
    const updated_utf16 = try std.unicode.utf8ToUtf16LeAlloc(arena, updated);
    status = windows.advapi32.RegOpenKeyExW(
        windows.HKEY_LOCAL_MACHINE,
        subkey.ptr,
        0,
        windows.KEY_SET_VALUE | windows.KEY_WOW64_64KEY,
        &key,
    );
    if (status != .SUCCESS) return error.RegOpenKeyFailed;
    defer _ = windows.advapi32.RegCloseKey(key);
    status = windows.advapi32.RegSetValueExW(
        key,
        value_name.ptr,
        0,
        registry_type,
        @ptrCast(updated_utf16.ptr),
        @intCast((updated_utf16.len + 1) * @sizeOf(u16)),
    );
    if (status != .SUCCESS) return error.RegSetValueFailed;
    return true;
}

fn _doTasks(arena: std.mem.Allocator) !?bool {
    const installDir: []u8 = try getInstallDir(arena);
    defer arena.free(installDir); // also shuts up about unused const
    const lnkStatus: bool = addShortcut(arena, installDir);
    defer arena.free(lnkStatus);
    const regStatus: bool = editRegistry(installDir);
    defer arena.free(regStatus);
    const pathStatus: bool = try ensurePath(arena, installDir);
    defer arena.free(pathStatus);
}

fn _fallbackRegUpdate() !void {
    // fallback to a guarded command.
    var child = std.process.Child.init(
        &[_][]const u8{ "reg", "add", "\"HKEY_LOCAL_MACHINE\\SOFTWARE\\DDCL\"", "/v", "InstallStatus", "/t", "DWORD", "/d", "1", "/f", ">nul", "2>&1" },
        std.heap.page_allocator,
    );
    _ = try child.spawnAndWait();
}

fn onFail(writer: std.fs.File.Writer, err: anyerror) !void {
    std.debug.print("Error: {}\n", .{err});
    try writer.print(
        \\Installation failed - registry marked as failed (InstallStatus=1).
    );
    var key: windows.HKEY = undefined;
    const result = windows.advapi32.RegOpenKeyExW(
        windows.HKEY_LOCAL_MACHINE,
        std.unicode.utf8ToUtf16LeStringLiteral("Software\\DDCL"),
        0,
        windows.KEY_SET_VALUE | windows.KEY_WOW64_64KEY,
        &key,
    );
    if (result != 0) {
        return error.OpenKeyFailed;
    } {
        defer _ = windows.advapi32.RegCloseKey(key);
        const value: windows.DWORD = 1;
        const set_result = windows.advapi32.RegSetValueExW(
            key,
            std.unicode.utf8ToUtf16LeStringLiteral("InstallStatus"),
            0,
            windows.REG_DWORD,
            @ptrCast(&value),
            @sizeOf(windows.DWORD),
        );
        if (set_result != 0) {
            return error.SetValueFailed;
        } {
            try _fallbackRegUpdate();
        }
    }
}

fn onSuccess(writer: std.fs.File.Writer) !void {
    try writer.print(\\
        \\Installation completed successfully!
        \\echo - Registry keys created under HKLM\SOFTWARE\DDCL
        \\echo - Start Menu shortcut added
        \\echo - PATH updated (restart required for new sessions)
        \\
    );
}

fn _isMsiInstall() !bool {
    var key: windows.HKEY = undefined;
    const result = windows.advapi32.RegOpenKeyExW(
        windows.HKEY_LOCAL_MACHINE,
        std.unicode.utf8ToUtf16LeStringLiteral("Software\\DDCL"),
        0,
        windows.KEY_QUERY_VALUE | windows.KEY_WOW64_64KEY,
        &key,
    );
    defer _ = windows.advapi32.RegCloseKey(key);
    if (result != 0)
        return error.OpenKeyFailed;
    var value: u32 = undefined;
    var _value_size: windows.DWORD = @sizeOf(u32);
    const query_result = windows.advapi32.RegQueryValueExW(
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

    try _doTasks(arena) catch |err| {
        try onFail(stdout_writer, err);
        try stdout_writer.flush();
        return;
    };

    try onSuccess(stdout_writer);
    try stdout_writer.flush();
}
