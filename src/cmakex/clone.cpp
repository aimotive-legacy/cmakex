#include "clone.h"

#include <Poco/DirectoryIterator.h>

#include <adasworks/sx/check.h>

#include "cmakex_utils.h"
#include "filesystem.h"
#include "git.h"
#include "misc_utils.h"
#include "print.h"

namespace cmakex {

namespace fs = filesystem;
// returns the package's clone dir's status, SHA, if git
tuple<pkg_clone_dir_status_t, string> pkg_clone_dir_status(string_par binary_dir,
                                                           string_par pkg_name)
{
    cmakex_config_t cfg(binary_dir);
    string clone_dir = cfg.cmakex_deps_clone_prefix + "/" + pkg_name.c_str();
    if (!fs::exists(clone_dir))
        return make_tuple(pkg_clone_dir_doesnt_exist, string{});
    // so it exists
    if (!fs::is_directory(clone_dir))
        return make_tuple(pkg_clone_dir_nonempty_nongit, string{});
    // so it is a directory
    if (Poco::DirectoryIterator(clone_dir) == Poco::DirectoryIterator{})
        return make_tuple(pkg_clone_dir_empty, string{});
    // so it is a non-empty directory
    string sha = git_rev_parse("HEAD", clone_dir);
    if (sha.empty())
        return make_tuple(pkg_clone_dir_nonempty_nongit, string{});
    // so it has valid sha
    auto git_status_result = git_status(clone_dir);
    return make_tuple(git_status_result.clean_or_untracked_only() ? pkg_clone_dir_git
                                                                  : pkg_clone_dir_git_local_changes,
                      sha);
}
void clone(string_par pkg_name, const pkg_clone_pars_t& cp, string_par binary_dir)
{
    log_info("Cloning package '%s'@%s", pkg_name.c_str(),
             cp.git_tag.empty() ? "HEAD" : cp.git_tag.c_str());

    cmakex_config_t cfg(binary_dir);
    string clone_dir = cfg.cmakex_deps_clone_prefix + "/" + pkg_name.c_str();
    vector<string> clone_args = {"--recurse"};
    string checkout;
    if (cp.git_tag.empty()) {
        if (cp.git_shallow)
            clone_args = {"--single-branch", "--depth", "1"};
    } else {
        auto git_tag_kind = initial_git_tag_kind(cp.git_tag);
        if (git_tag_kind == git_tag_could_be_sha) {  // find out if it's an sha
            auto r = git_ls_remote(cp.git_url, cp.git_tag);
            if (std::get<0>(r) == 2)
                git_tag_kind = git_tag_must_be_sha;
            else
                git_tag_kind = git_tag_is_not_sha;
        }
        if (git_tag_kind >= git_tag_must_be_sha) {
            checkout = cp.git_tag;
            if (cp.git_shallow) {
                // try to resolve corresponding reference
                auto git_tag = try_find_unique_ref_by_sha_with_ls_remote(cp.git_url, cp.git_tag);
                do {
                    if (git_tag.empty())
                        break;  // couldn't resolve, do full clone

                    // first attempt: clone resolved ref with --depth 1, then checkout sha
                    vector<string> args = {"--recurse",      "--branch", git_tag.c_str(),
                                           "--depth",        "1",        cp.git_url.c_str(),
                                           clone_dir.c_str()};
                    git_clone(args);
                    if (git_checkout({cp.git_tag}, clone_dir) == 0)
                        return;

                    // second attempt: clone resolved ref with unlimited depth, then checkout sha
                    fs::remove_all(clone_dir);
                    args = {"--recurse", "--branch", git_tag.c_str(), cp.git_url.c_str(),
                            clone_dir.c_str()};
                    git_clone(args);
                    if (git_checkout({cp.git_tag}, clone_dir) == 0)
                        return;

                    // do full clone
                    fs::remove_all(clone_dir);
                } while (false);
            }
        } else {
            if (!cp.git_shallow)
                clone_args = {"--branch", cp.git_tag.c_str(), "--no-single-branch"};
            else
                clone_args = {"--branch", cp.git_tag.c_str(), "--depth", "1"};
        }
    }
    clone_args.insert(clone_args.end(), {cp.git_url.c_str(), clone_dir.c_str()});
    log_exec("git", clone_args);
    git_clone(clone_args);
    if (checkout.empty()) {
        if (git_checkout({checkout}, clone_dir) != 0) {
            fs::remove_all(clone_dir.c_str());
            throwf("Failed to checkout the requested commit '%s' after a successful full clone.",
                   cp.git_tag.c_str());
        }
    }
}
void make_sure_exactly_this_sha_is_cloned_or_fail(string_par pkg_name,
                                                  const pkg_clone_pars_t& cp,
                                                  string_par binary_dir)
{
    CHECK(sha_like(cp.git_tag));

    cmakex_config_t cfg(binary_dir);
    string clone_dir = cfg.cmakex_deps_clone_prefix + "/" + pkg_name.c_str();

    log_info("Making sure the working copy in \"%s\" is checked out at remote's '%s'",
             clone_dir.c_str(), cp.git_tag.c_str());

    auto cds = pkg_clone_dir_status(binary_dir, pkg_name);

    string errormsg = stringf(
        "Remove the directory \"%s\" or the checkout the '%s' manually, then restart the build.",
        clone_dir.c_str(), cp.git_tag.c_str());

    switch (std::get<0>(cds)) {
        case pkg_clone_dir_doesnt_exist:
        case pkg_clone_dir_empty:
            clone(pkg_name, cp, binary_dir);
            break;
        case pkg_clone_dir_nonempty_nongit:
            throwf("The directory contains non-git files which are in the way. %s",
                   errormsg.c_str());
        case pkg_clone_dir_git:
            if (std::get<1>(cds) != cp.git_tag)
                throwf("The current working copy should be checked out at '%s' but it's at %s. %s",
                       cp.git_tag.c_str(), std::get<1>(cds).c_str(), errormsg.c_str());
            break;
        case pkg_clone_dir_git_local_changes:
            throwf(
                "The current working copy should be checked out at '%s' but it has local changes. "
                "%s",
                cp.git_tag.c_str(), errormsg.c_str());
            break;
        default:
            CHECK(false);
    }
}
void make_sure_exactly_this_git_tag_is_cloned(string_par pkg_name,
                                              const pkg_clone_pars_t& cp,
                                              string_par binary_dir,
                                              bool strict)
{
    cmakex_config_t cfg(binary_dir);
    string clone_dir = cfg.cmakex_deps_clone_prefix + "/" + pkg_name.c_str();
    string git_tag_or_head = cp.git_tag.empty() ? "HEAD" : cp.git_tag.c_str();

    if (strict)
        log_info("Making sure the working copy in \"%s\" is checked out at remote's '%s'",
                 clone_dir.c_str(), git_tag_or_head.c_str());
    auto cds = pkg_clone_dir_status(binary_dir, pkg_name);

    string errormsg, warnmsg;
    if (strict)
        errormsg = stringf(
            "Remove the directory \"%s\" or the checkout the '%s' manually, then restart the "
            "build.",
            clone_dir.c_str(), git_tag_or_head.c_str());
    else
        warnmsg = "Using the existing files. Use the '--strict-clone' option to prevent this.";

    switch (std::get<0>(cds)) {
        case pkg_clone_dir_doesnt_exist:
        case pkg_clone_dir_empty:
            clone(pkg_name, cp, binary_dir);
            break;
        case pkg_clone_dir_nonempty_nongit:
            if (strict)
                throwf("The directory \"%s\" contains non-git files which are in the way. %s",
                       clone_dir.c_str(), errormsg.c_str());
            else
                log_warn(
                    "The directory \"%s\" contains non-git files instead of the requested commit "
                    "'%s'. "
                    "%s",
                    clone_dir.c_str(), git_tag_or_head.c_str(), warnmsg.c_str());
            break;
        case pkg_clone_dir_git_local_changes:
            if (strict)
                throwf("The directory \"%s\" has local changes. %s", clone_dir.c_str(),
                       errormsg.c_str());
            else
                log_warn(
                    "The directory \"%s\" contains non-git files instead of the requested commit "
                    "'%s'. "
                    "%s",
                    clone_dir.c_str(), git_tag_or_head.c_str(), warnmsg.c_str());
            break;
        case pkg_clone_dir_git:
            // verify that get<1>(cds) ( = HEAD's SHA) equals to what cp.git_tag resolves to
            {
                string resolved_sha;
                auto r = git_resolve_ref_on_remote(cp.git_url, cp.git_tag);
                switch (std::get<0>(r)) {
                    case resolve_ref_error:
                        if (strict)
                            throwf(
                                "Failed to verify the current working copy is checked out at "
                                "remote's '%s', reason: git-ls-remote failed. %s",
                                git_tag_or_head.c_str(), errormsg.c_str());
                        else
                            log_warn(
                                "Failed to verify whether the current working copy is checked "
                                "out at remote's '%s', reason: git-ls-remote failed. %s",
                                git_tag_or_head.c_str(), warnmsg.c_str());
                        break;
                    case resolve_ref_success:
                        resolved_sha = std::get<1>(r);
                        break;
                    case resolve_ref_not_found:
                        if (strict)
                            throwf(
                                "Failed to verify the current working copy is checked out at "
                                "remote's '%s', reason: not found (tried with git-ls-remote). "
                                "%s",
                                git_tag_or_head.c_str(), errormsg.c_str());
                        else
                            log_warn(
                                "Failed to verify the current working copy is checked out at "
                                "remote's '%s', reason: not found (tried with git-ls-remote). "
                                "%s",
                                git_tag_or_head.c_str(), warnmsg.c_str());
                        break;
                    case resolve_ref_sha_like:
                        resolved_sha = git_rev_parse(cp.git_tag, clone_dir);
                        if (resolved_sha.empty()) {
                            if (strict)
                                throwf(
                                    "Failed to verify the current working copy is checked out "
                                    "at remote's '%s', reason: not found (tried with "
                                    "git-ls-remote and local git-rev-parse). %s",
                                    git_tag_or_head.c_str(), errormsg.c_str());
                            else
                                log_warn(
                                    "Failed to verify the current working copy is checked out "
                                    "at remote's '%s', reason: not found (tried with "
                                    "git-ls-remote and local git-rev-parse). %s",
                                    git_tag_or_head.c_str(), warnmsg.c_str());
                        }
                        break;
                    default:
                        CHECK(false);
                }
                if (std::get<1>(cds) != resolved_sha) {
                    if (strict)
                        throwf(
                            "The current working copy should be checked out at remote's '%s' "
                            "(= %s) but it's at %s. %s",
                            git_tag_or_head.c_str(), resolved_sha.c_str(), std::get<1>(cds).c_str(),
                            errormsg.c_str());
                    else
                        throwf(
                            "The current working copy should be checked out at remote's '%s' "
                            "(= %s) but it's at %s. %s",
                            git_tag_or_head.c_str(), resolved_sha.c_str(), std::get<1>(cds).c_str(),
                            warnmsg.c_str());
                }
            }
            break;
        default:
            CHECK(false);
    }
}
}
