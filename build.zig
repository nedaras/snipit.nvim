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

    const lib = b.addSharedLibrary(.{
        .name = "snipit",
        .target = target,
        .optimize = optimize,
    });

    // todo: add "-DFT_CONFIG_OPTION_ERROR_STRINGS=1", inside freetype

    lib.linkLibC();
    lib.linkLibrary(libpng.artifact("png"));
    lib.linkLibrary(freetype.artifact("freetype"));

    lib.addCSourceFile(.{ .file = b.path("src/main.c") });

    b.installArtifact(lib);

    const run_cmd = b.addRunArtifact(lib);
    run_cmd.step.dependOn(b.getInstallStep());

    if (b.args) |args| {
        run_cmd.addArgs(args);
    }

    const run_step = b.step("run", "Run the app");
    run_step.dependOn(&run_cmd.step);
}
