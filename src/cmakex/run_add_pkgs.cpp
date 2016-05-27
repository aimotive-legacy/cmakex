#include "run_add_pkgs.h"

#include <map>

#include <adasworks/sx/check.h>

#include "cmakex_utils.h"
#include "filesystem.h"
#include "git.h"
#include "misc_utils.h"
#include "out_err_messages.h"
#include "print.h"

namespace cmakex {

namespace fs = filesystem;

//
vector<string> separate_arguments(string_par x)
{
    bool in_dq = false;
    vector<string> r;
    bool valid = false;
    auto new_empty_if_invalid = [&r, &valid]() {
        if (!valid) {
            r.emplace_back();
            valid = true;
        }
    };
    auto append = [&r, &valid](char c) {
        if (valid)
            r.back() += c;
        else {
            r.emplace_back(1, c);
            valid = true;
        }
    };
    for (const char* c = x.c_str(); *c; ++c) {
        if (in_dq) {
            if (*c == '"')
                in_dq = false;
            else
                append(*c);
        } else {
            if (*c == '"') {
                in_dq = true;
                new_empty_if_invalid();
            } else if (*c == ' ')
                valid = false;
            else
                append(*c);
        }
    }
    return r;
}
template <class T, class Container>
bool is_one_of(const T& x, const Container& c)
{
    return std::find(c.begin(), c.end(), x) != c.end();
}
std::map<string, vector<string>> parse_arguments(const vector<string>& options,
                                                 const vector<string>& onevalue_args,
                                                 const vector<string>& multivalue_args,
                                                 const vector<string>& args)
{
    std::map<string, vector<string>> r;
    for (int ix = 0; ix < args.size(); ++ix) {
        const auto& arg = args[ix];
        if (is_one_of(arg, options))
            r[arg];
        else if (is_one_of(arg, onevalue_args)) {
            if (++ix >= args.size())
                throwf("Missing argument after \"%s\".", arg.c_str());
            if (r.count(arg) > 0)
                throwf("Single-value argument '%s' specified multiple times.", arg.c_str());
            r[arg] = vector<string>(1, args[ix]);
        } else if (is_one_of(arg, multivalue_args)) {
            for (; ix < args.size(); ++ix) {
                const auto& v = args[ix];
                if (is_one_of(v, options) || is_one_of(v, onevalue_args)) {
                    --ix;
                    break;
                }
                r[arg].emplace_back(v);
            }
        } else
            throwf("Invalid option: '%s'.", arg.c_str());
    }
    return r;
}

std::map<string, vector<string>> parse_arguments(const vector<string>& options,
                                                 const vector<string>& onevalue_args,
                                                 const vector<string>& multivalue_args,
                                                 string_par argstr)
{
    return parse_arguments(options, onevalue_args, multivalue_args, separate_arguments(argstr));
}

struct pkg_request_t
{
    string name;
    string git_url;
    string git_tag;
    string source_dir;
    vector<string> depends;
    vector<string> cmake_args;
    vector<string> configs;
    string install_prefix;
    string clone_dir;
};
void run_add_pkgs(const cmakex_pars_t& pars)
{
    CHECK(!pars.add_pkgs.empty());
    CHECK(pars.source_desc.empty());
    for (auto& pkg_arg_str : pars.add_pkgs) {
        auto pkg_args = separate_arguments(pkg_arg_str);
        if (pkg_args.empty())
            throwf("Empty argument string for '--add_pkg'.");
        pkg_request_t request;
        request.name = pkg_args[0];
        pkg_args.erase(pkg_args.begin());
        auto args = parse_arguments({}, {"GIT_REPOSITORY", "GIT_URL", "GIT_TAG", "SOURCE_DIR"},
                                    {"DEPENDS", "CMAKE_ARGS", "CONFIGS"}, pkg_args);
        for (auto c : {"GIT_REPOSITORY", "GIT_URL", "GIT_TAG", "SOURCE_DIR"}) {
            auto count = args.count(c);
            CHECK(count == 0 || args[c].size() == 1);
            if (count > 0 && args[c].empty())
                throwf("Empty string after '%s'.", c);
        }
        string a, b;
        if (args.count("GIT_REPOSITORY") > 0)
            a = args["GIT_REPOSITORY"][0];
        if (args.count("GIT_URL") > 0)
            b = args["GIT_URL"][0];
        if (!a.empty()) {
            request.git_url = a;
            if (!b.empty())
                throwf("Both GIT_URL and GIT_REPOSITORY are specified.");
        } else
            request.git_url = b;

        if (args.count("GIT_TAG") > 0)
            request.git_tag = args["GIT_TAG"][0];
        if (args.count("SOURCE_DIR") > 0)
            request.source_dir = args["SOURCE_DIR"][0];
        if (args.count("DEPENDS") > 0)
            request.depends = args["DEPENDS"];
        if (args.count("CMAKE_ARGS") > 0) {
            // join some cmake options for easier search
            for (auto& a : args["CMAKE_ARGS"]) {
                if (!request.cmake_args.empty() &&
                    is_one_of(request.cmake_args.back(), {"-C", "-D", "-U", "-G", "-T", "-A"})) {
                    request.cmake_args.back() += a;
                } else
                    request.cmake_args.emplace_back(a);
            }
            request.cmake_args = args["CMAKE_ARGS"];
        }
        if (args.count("CONFIGS") > 0)
            request.configs = args["CONFIGS"];

        const cmakex_config_t cfg(pars.binary_dir);

        request.install_prefix = cfg.cmakex_deps_install_prefix + "/" + request.name;
        request.clone_dir = cfg.cmakex_deps_clone_prefix + "/" + request.name;

        // request is filled at this point

        cmakex_pars_t ppars;
        ppars.c = true;
        ppars.b = true;
        ppars.t = pars.t;
        ppars.configs = request.configs.empty() ? pars.configs : request.configs;
        ppars.binary_dir = cfg.cmakex_deps_binary_prefix + "/" + request.name;
        ppars.source_desc = cfg.cmakex_deps_clone_prefix + "/" + request.name;
        if (!request.source_dir.empty())
            ppars.source_desc += "/" + request.source_dir;
        ppars.config_args = pars.config_args;
        ppars.config_args.insert(ppars.config_args.end(), BEGINEND(request.cmake_args));
        ppars.build_args = pars.build_args;
        ppars.native_tool_args = pars.native_tool_args;
        ppars.build_targets = {"install"};
        ppars.config_args_besides_binary_dir = true;

        // check if no install prefix in configs
        for (auto s : ppars.config_args) {
            if (starts_with(s, "-DCMAKE_INSTALL_PREFIX:") ||
                starts_with(s, "-DCMAKE_INSTALL_PREFIX="))
                throwf("In '--add-pkg' mode 'CMAKE_INSTALL_PREFIX' can't be set manually.");
        }

        ppars.config_args.emplace_back(string("-DCMAKE_INSTALL_PREFIX:PATH=") +
                                       request.install_prefix);

        // clone
        // remove clone dir if empty
        if (fs::is_directory(request.clone_dir)) {
            try {
                fs::remove(request.clone_dir);
            } catch (...) {
            }
        }
        // clone if not exists
        if (fs::is_directory(request.clone_dir)) {
            {
                // attempt to get SHA from remote
                int result;
                string output;
                tie(result, output) = git_ls_remote(request.git_url, request.git_tag.empty() ? "HEAD" : request.git_tag);
            }
            {
                vector<string> args = {"clone"};
                //   if (!cfg.full_clone)

                //     execute_git({"clone", "--depth 1", "--branch", "--recursive"});
            }
        }

        ppars.source_desc_kind = evaluate_source_descriptor(ppars.source_desc);
    }
}
}
