#ifndef USING_DECLS_092374029347
#define USING_DECLS_092374029347

#include <chrono>
#include <exception>
#include <initializer_list>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include <adasworks/sx/array_view.h>
#include <adasworks/sx/maybe.h>
#include <adasworks/sx/string_par.h>
#include <adasworks/sx/stringf.h>

namespace cmakex {
using std::exception;
using std::unique_ptr;
using std::string;
using std::vector;
using std::initializer_list;
using std::move;
using std::chrono::system_clock;
using std::chrono::high_resolution_clock;
using dur_sec = std::chrono::duration<double>;
using std::tuple;
using std::runtime_error;

using adasworks::sx::stringf;
using adasworks::sx::string_par;
using adasworks::sx::array_view;
using adasworks::sx::maybe;
using adasworks::sx::nothing;
using adasworks::sx::just;
}

#endif
