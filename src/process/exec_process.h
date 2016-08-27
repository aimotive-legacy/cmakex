#ifndef EXEC_PROCESS_0394723
#define EXEC_PROCESS_0394723

#include <functional>

#include <adasworks/sx/array_view.h>
#include <adasworks/sx/mutex.h>
#include <adasworks/sx/string_par.h>

namespace cmakex {

using adasworks::sx::atomic_flag_mutex;
using adasworks::sx::array_view;
using adasworks::sx::string_par;
using std::vector;
using std::string;

using exec_process_output_callback_t = std::function<void(array_view<const char>)>;

namespace exec_process_callbacks {

inline void print_to_stdout(array_view<const char> x)
{
    printf("%.*s", (int)x.size(), x.data());
}
inline void print_to_stderr(array_view<const char> x)
{
    fprintf(stderr, "%.*s", (int)x.size(), x.data());
}

}  // namespace exec_process_callbacks

// launch new process with args (synchronous)
int exec_process(string_par path,
                 const vector<string>& args,
                 string_par working_directory,
                 exec_process_output_callback_t stdout_callback = nullptr,
                 exec_process_output_callback_t stderr_callback = nullptr);

inline int exec_process(string_par path,
                        const vector<string>& args,
                        exec_process_output_callback_t stdout_callback = nullptr,
                        exec_process_output_callback_t stderr_callback = nullptr)
{
    return exec_process(path, args, "", stdout_callback, stderr_callback);
}

inline int exec_process(string_par path,
                        exec_process_output_callback_t stdout_callback = nullptr,
                        exec_process_output_callback_t stderr_callback = nullptr)
{
    return exec_process(path, vector<string>{}, "", stdout_callback, stderr_callback);
}
}  // namespace cmakex

#endif
