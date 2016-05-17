#include <cstdlib>

#include <adasworks/sx/log.h>
#include <nowide/args.hpp>

#include "cmakexengine.h"
#include "filesystem.h"
#include "misc_util.h"
#include "using-decls.h"

#define STRINGIZE_CORE(x) #x
#define STRINGIZE(x) STRINGIZE_CORE(x)

namespace cmakex {

namespace fs = filesystem;

const char* cmakex_version_string = STRINGIZE(CMAKEX_VERSION_STRING);

const char* usage_text =
    "Execute multiple `cmake` commands with a single `cmakex` command.\n"
    "For detailed help, see README.md\n"
    "\n"
    "Usage: cmakex <subcommand>] [options...]\n"
    "\n"
    "The first (compulsory) parameter is the subcommand word which can be:\n"
    "\n"
    "- mix of the characters c, b, i, t, d, r, w to execute multiple `cmake` commands\n"
    "- 'help' to display this message\n"
    "\n"
    "Execute Multiple `cmake` Commands\n"
    "=================================\n"
    "\n"
    "    cmakex c|b|i|t|d|r|w [cmake-options]\n"
    "\n"
    "Specify one or more of the characters c, b, i, t to execute one or more of\n"
    "these steps:\n"
    "\n"
    "- `c`: CMake configure step (`cmake ...`)\n"
    "- `b`: CMake build step (`cmake --build ...`)\n"
    "- `i`: CMake install step (`cmake --build ... --target install`)\n"
    "- `t`: CMake test step (`ctest ...`)\n"
    "\n"
    "The remaining characters control the configurations. You can specify zero, one,\n"
    "or more of d, r, w for the configurations: Debug, Release, RelWithDebInfo.\n"
    "\n"
    "CMake Options\n"
    "-------------\n"
    "\n"
    "After the command word you can specify\n"
    "\n"
    "- any options the cmake command accepts (the normal, non-build mode)\n"
    "- `-H` and `-B` to specify source and build directories. Note that unlike cmake,\n"
    "  cmakex accepts the `-H <path>` and `-B <path>` forms, too.\n"
    "- `--target <tgt>` (also multiple times)\n"
    "- `--config <cfg>` (also multiple times)\n"
    "- `--clean-first`\n"
    "- double-dash \"--\" followed by options to the native build tool\n"
    "\n"
    "Examples:\n"
    "=========\n"
    "\n"
    "Configure, install and test a project from scrach, for `Debug` and `Release`\n"
    "configurations, clean build:\n"
    "\n"
    "    cd project_source_dir\n"
    "    cmakex citdr -H. -Bb -DCMAKE_INSTALL_PREFIX=$PWD/out --clean-first\n"
    "\n"
    "Install the 'Debug' and 'Release' configs:\n"
    "\n"
    "    cmakex cidr -Hsource_dir -Bbuild_dir -DMY_OPTION=something\n"
    "\n"
    "Test the 'Release' config:\n"
    "\n"
    "    cd build_dir\n"
    "    cmakex tr .\n"
    "\n"
    "Note, that the configure/build steps are omitted in that case.\n"
    "To execute the build step, use:\n"
    "\n"
    "    cd build_dir\n"
    "    cmakex btr .\n"
    "\n"
    "To test a project which has not been configured yet:\n"
    "\n"
    "    cd build_dir\n"
    "    cmakex cbtr .\n"
    "\n"
    "or\n"
    "\n"
    "    cmakex cbtr -Hsource_dir -Bbuild_dir\n"
    "\n";

void display_usage_and_exit(int exit_code)
{
    fprintf(stderr, "cmakex v%s\n\n", cmakex_version_string);
    fprintf(stderr, "%s", usage_text);
    exit(exit_code);
}

void badpars_exit(string_par msg)
{
    fprintf(stderr, "Error, bad parameters: %s\n", msg.c_str());
    exit(EXIT_FAILURE);
}

cmakex_pars_t process_command_line(int argc, char* argv[])
{
    cmakex_pars_t pars;
    if (argc <= 1)
        display_usage_and_exit(EXIT_SUCCESS);
    string command = argv[1];
    if (command == "help")
        display_usage_and_exit(argc == 2 ? EXIT_SUCCESS : EXIT_FAILURE);

    pars.subcommand = cmakex_pars_t::subcommand_cmake_steps;
    for (auto c : command) {
        switch (c) {
            case 'c':
                pars.c = true;
                break;
            case 'b':
                pars.b = true;
                break;
            case 'i':
                pars.build_targets.emplace_back("install");
                break;
            case 't':
                pars.t = true;
                break;
            case 'd':
                pars.configs.emplace_back("Debug");
                break;
            case 'r':
                pars.configs.emplace_back("Release");
                break;
            case 'w':
                pars.configs.emplace_back("RelWithDebInfo");
                break;
            default:
                badpars_exit(stringf("Invalid character in subcommand: %c", c));
        }
    }

    for (int argix = 2; argix < argc; ++argix) {
        string arg = argv[argix];
        if (!pars.native_tool_args.empty() || arg == "--") {
            pars.native_tool_args.emplace_back(arg);
            continue;
        }

        if (arg == "--target") {
            if (++argix >= argc)
                badpars_exit("Missing target name after '--target'");
            pars.build_targets.emplace_back(argv[argix]);
        } else if (arg == "--config") {
            if (++argix >= argc)
                badpars_exit("Missing config name after '--config'");
            pars.configs.emplace_back(argv[argix]);
        } else if (is_one_of(arg, {"--clean-first", "--use-stderr"}))
            pars.build_args.emplace_back(arg);
        else {
            pars.config_args.emplace_back(arg);
            if (starts_with(arg, "-H")) {
                if (arg == "-H") {
                    if (++argix >= argc)
                        badpars_exit("Missing path after '-H'");
                    pars.source_dir = argv[argix];
                    // cmake doesn't support separated -H <path>
                    pars.config_args.pop_back();
                    pars.config_args.emplace_back(string("-H") + pars.source_dir);
                } else
                    pars.source_dir = make_string(butleft(arg, 2));
                pars.config_args_besides_binary_dir = true;
            } else if (starts_with(arg, "-B")) {
                if (arg == "-B") {
                    if (++argix >= argc)
                        badpars_exit("Missing path after '-B'");
                    pars.binary_dir = argv[argix];
                    // cmake doesn't support separated -B <path>
                    pars.config_args.pop_back();
                    pars.config_args.emplace_back(string("-B") + pars.binary_dir);
                } else
                    pars.binary_dir = make_string(butleft(arg, 2));
            } else if (is_one_of(arg, {"-C", "-D", "-U", "-G", "-T", "-A"})) {
                if (++argix >= argc)
                    badpars_exit(stringf("Missing path after '%s'", arg.c_str()));
                pars.config_args.emplace_back(arg);
                pars.config_args_besides_binary_dir = true;
            } else if (!starts_with(arg, '-')) {
                if (fs::is_regular_file(arg + "/CMakeLists.txt")) {
                    pars.source_dir = arg;
                    pars.config_args_besides_binary_dir = true;
                } else if (fs::is_regular_file(arg + "/CMakeCache.txt")) {
                    pars.binary_dir = arg;
                } else {
                    badpars_exit(
                        stringf("%s is neither a valid source dir (no CMakeLists.txt), nor "
                                "an existing binary dir (no CMakeCache.txt)",
                                arg.c_str()));
                }
            } else {
                // some other argument we just pass to cmake config step
                pars.config_args_besides_binary_dir = true;
            }
        }  // else: not one of specific args
    }      // foreach arg
    if (pars.binary_dir.empty())
        pars.binary_dir = fs::current_path();

    return pars;
}

int main(int argc, char* argv[])
{
    nowide::args nwa(argc, argv);

    try {
        auto pars = process_command_line(argc, argv);
        auto eng = CMakeXEngine::create(pars);
    } catch (const exception& e) {
        LOG_FATAL("Exception: %s", e.what());
    } catch (...) {
        LOG_FATAL("Unhandled exception");
    }
    return EXIT_SUCCESS;
}
}

int main(int argc, char* argv[])
{
    return cmakex::main(argc, argv);
}
