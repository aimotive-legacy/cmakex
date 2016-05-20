#include <cstdio>
#include <cstdlib>
#include <string>

#include <adasworks/sx/check.h>
#include <adasworks/sx/log.h>
#include <adasworks/sx/stringf.h>

#include "exec_process.h"
#include "filesystem.h"
#include "out_err_messages.h"

using std::string;
namespace fs = filesystem;
using std::vector;
using adasworks::sx::stringf;

void dump_oem(const cmakex::OutErrMessages& oem)
{
    LOG_INFO("oem.size=%d", (int)oem.size());
    for (int i = 0; i < oem.size(); ++i) {
        auto msg = oem.at(i);
        string src;
        switch (msg.source) {
            case cmakex::out_err_message_t::source_stdout:
                src = "stdout";
                break;
            case cmakex::out_err_message_t::source_stderr:
                src = "stderr";
                break;
            default:
                src = stringf("invalid (%d)", (int)msg.source);
        }
        string text;
        for (auto c : msg.text) {
            if (isprint(c))
                text += c;
            else
                text += stringf("<%02X>", (int)c);
        }
        LOG_INFO("%d: [%s]@%f \"%s\"", i, src.c_str(), msg.t, text.c_str());
    }
}
// $1 = path to cmakex
// $2 = path to test project source dir
// $3 = path to test project binary dir (to be created)
int main(int argc, char* argv[])
{
    try {
        adasworks::log::Logger global_logger(adasworks::log::global_tag, argc, argv, AW_TRACE);

        CHECK(argc == 4);

        string cmakex_path = argv[1];
        string source_dir = argv[2];
        string build_dir = argv[3];
        string install_dir = build_dir + "/o";

        LOG_INFO("cmakex_path: %s", cmakex_path.c_str());
        LOG_INFO("source_dir: %s", source_dir.c_str());
        LOG_INFO("build_dir: %s", build_dir.c_str());

        try {
            fs::remove_all(build_dir);
        } catch (...) {
        }
        fs::create_directories(build_dir);

        {
            vector<string> args = {"cdri",
                                   "-H",
                                   "nosuchsourcedir",
                                   "-B",
                                   build_dir.c_str(),
                                   (string("-DCMAKE_INSTALL_PREFIX=") + install_dir).c_str(),
                                   "-DDEFINE_THIS=scoobydoo"};
            int r = cmakex::exec_process(cmakex_path, args);
            CHECK(r != 0);
        }
        {
            vector<string> args = {"cdri",
                                   "-H",
                                   source_dir.c_str(),
                                   "-B",
                                   build_dir.c_str(),
                                   (string("-DCMAKE_INSTALL_PREFIX=") + install_dir).c_str(),
                                   "-DDEFINE_THIS=scoobydoo",
                                   "--clean-first"};
            int r = cmakex::exec_process(cmakex_path, args);
            CHECK(r == 0);
        }

        const string expected_out = "XscoobydooX";
        for (auto a : {'d', 'r'}) {
            auto exe = install_dir + "/bin/exe";
            if (a == 'd')
                exe += 'd';
            cmakex::OutErrMessagesBuilder oeb(true);
            int r = cmakex::exec_process(exe, oeb.stdout_callback(), oeb.stderr_callback());
            CHECK(r == 0);
            string expected_err = a == 'd' ? "Debug" : "Release";
            auto oem = oeb.move_result();
            dump_oem(oem);
            CHECK(oem.size() == 2);
            auto m1 = oem.at(0);
            auto m2 = oem.at(1);
            if (m1.source == cmakex::out_err_message_t::source_stderr) {
                using std::swap;
                swap(m1, m2);
            }
            CHECK(m1.source == cmakex::out_err_message_t::source_stdout);
            CHECK(m1.text.substr(0, expected_out.size()) == expected_out);
            CHECK(m2.source == cmakex::out_err_message_t::source_stderr);
            CHECK(m2.text.substr(0, expected_err.size()) == expected_err);
        }

        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        fprintf(stderr, "Exception: %s\n", e.what());
    } catch (...) {
        fprintf(stderr, "Unknown exception\n");
    }
    return EXIT_FAILURE;
}
