#include "run_add_pkgs.h"

#include <map>

#include <adasworks/sx/check.h>

#include "misc_util.h"
#include "print.h"

namespace cmakex {

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
            if (++ix >= args.size()) {
                print_err("Missing argument after \"%s\".", arg.c_str());
                exit(EXIT_FAILURE);
            }
            if (r.count(arg) > 0) {
                print_err("Single-value argument '%s' specified multiple times.", arg.c_str());
                exit(EXIT_FAILURE);
            }
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
        } else {
            print_err("Invalid option: '%s'.", arg.c_str());
            exit(EXIT_FAILURE);
        }
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

struct pkg_props_t
{
    string name;
    string git_url;
    string git_tag;
    string source_dir;
    vector<string> depends;
    vector<string> cmake_args;
    vector<string> configs;
};
void run_add_pkgs(const cmakex_pars_t& pars)
{
    CHECK(!pars.add_pkgs.empty());
    CHECK(pars.source_desc.empty());
    for (auto& pkg_arg_str : pars.add_pkgs) {
        auto pkg_args = separate_arguments(pkg_arg_str);
        if (pkg_args.empty()) {
            print_err("Empty argument string for '--add_pkg'.");
            exit(EXIT_FAILURE);
        }
        pkg_props_t props;
        props.name = pkg_args[0];
        pkg_args.erase(pkg_args.begin());
        auto args = parse_arguments({}, {"GIT_REPOSITORY", "GIT_URL", "GIT_TAG", "SOURCE_DIR"},
                                    {"DEPENDS", "CMAKE_ARGS", "CONFIGS"}, pkg_args);
        for (auto c : {"GIT_REPOSITORY", "GIT_URL", "GIT_TAG", "SOURCE_DIR"}) {
            auto count = args.count(c);
            CHECK(count == 0 || args[c].size() == 1);
            if (count > 0 && args[c].empty()) {
                print_err("Empty string after '%s'.", c);
                exit(EXIT_FAILURE);
            }
        }
        string a, b;
        if (args.count("GIT_REPOSITORY") > 0)
            a = args["GIT_REPOSITORY"][0];
        if (args.count("GIT_URL") > 0)
            b = args["GIT_URL"][0];
        if (!a.empty()) {
            props.git_url = a;
            if (!b.empty()) {
                print_err("Both GIT_URL and GIT_REPOSITORY are specified.");
                exit(EXIT_FAILURE);
            }
        } else
            props.git_url = b;

        if (args.count("GIT_TAG") > 0)
            props.git_tag = args["GIT_TAG"][0];
        if (args.count("SOURCE_DIR") > 0)
            props.git_tag = args["SOURCE_DIR"][0];
        if (args.count("DEPENDS") > 0)
            props.depends = args["DEPENDS"];
        if (args.count("CMAKE_ARGS") > 0)
            props.depends = args["CMAKE_ARGS"];
        if (args.count("CONFIGS") > 0)
            props.depends = args["CONFIGS"];

        // props is filled at this point
        cmakex_pars_t ppars;
        ppars.c = true;
        ppars.b = true;
        ppars.t = pars.t;
        ppars.configs = props.configs.empty() ? pars.configs : props.configs;
        ppars.binary_dir = $cmakex_deps_binary_prefix + "/" + props.name;
        ppars.source_desc = $cmakex_deps_clone_prefix + "/" + props.name;
        if (!props.source_dir.empty())
            ppars.source_desc += "/" + props.source_dir;
        ppars.source_desc_kind = evaluate_source_descriptor(ppars.source_desc);
        ppars.config_args = pars.config_args;
        ppars.config_args.insert(ppars.config_args.end(), BEGINEND(props.cmake_args));
        ppars.build_args = pars.build_args;
        ppars.native_tool_args = pars.native_tool_args;
        ppars.build_targets = {"install"};
        ppars.config_args_besides_binary_dir = true;

        // check if no install prefix in configs
        // add -DCMAKE_INSTALL_PREFIX= arg
    }
}
}
