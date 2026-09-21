Import("env")

from os.path import join

proj = env["PROJECT_DIR"]
env.Append(CPPPATH=[join(proj, "firmware", "include"), join(proj, "platforms", "pio")])
env.Append(ASFLAGS=["-I", proj])
env.BuildSources(
    join("$BUILD_DIR", "koto_fw"),
    join(proj, "firmware", "src"),
    "+<*> -<assets/badapple_blob.S>",
)
