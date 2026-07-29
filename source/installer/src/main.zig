const std = @import("std");
const Io = std.Io;

const installer = @import("installer");

//? real quick: im aware that std.fs.cwd() use is deprecated since 0.16 zig. IDC tho!
//? im not gonna make this more complicated for myself

//* if i need a temp allocator:
// var arena = std.heap.ArenaAllocator.init(std.heap.page_allocator);
// defer arena.deinit();
// const allocator: std.mem.Allocator = arena.allocator();

fn getInstallDir(arena: std.mem.Allocator) ![]u8 {
    // current directory is plausible
    var installDir: []u8 = try std.process.getEnvVarOwned(arena, "cd");
    defer arena.free(installDir);
    // see if executable is here
    if (
        //? this check is only performed twice so im not bothering to make it its own fn
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

pub fn main(init: std.process.Init) !void {
    const arena: std.mem.Allocator = init.arena.allocator();
    const io: Io = init.io;
    var stdout_buffer: [1024]u8 = undefined;
    var stdout_file_writer: Io.File.Writer = .init(.stdout(), io, &stdout_buffer);
    const stdout_writer = &stdout_file_writer.interface;

    const lnkStatus: bool = try addShortcut(arena, );

    try stdout_writer.flush(); // Don't forget to flush!
}
