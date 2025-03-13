const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const libpng = b.dependency("libpng", .{
        .target = target,
        .optimize = optimize,
    });

    const freetype = b.dependency("freetype", .{
        .target = target,
        .optimize = optimize,
        .@"enable-libpng" = true,
    });

    const exe = b.addExecutable(.{
        .name = "snipit",
        .target = target,
        .optimize = optimize,
    });

    exe.linkLibC();

    exe.linkLibrary(libpng.artifact("png"));

    exe.linkLibrary(freetype.artifact("freetype"));
    exe.addIncludePath(b.path("src"));
    exe.addCSourceFile(.{ .file = b.path("src/main.c") });

    b.installArtifact(exe);

    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());

    if (b.args) |args| {
        run_cmd.addArgs(args);
    }

    const run_step = b.step("run", "Run the app");
    run_step.dependOn(&run_cmd.step);
}
