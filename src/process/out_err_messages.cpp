#include "out_err_messages.h"

#include <deque>
#include <thread>

#include <Poco/Pipe.h>
#include <Poco/Process.h>

#include <adasworks/sx/check.h>
#include <adasworks/sx/log.h>

namespace cmakex {

// add_msg is thread-safe
void OutErrMessages::add_msg(source_t source, array_view<const char> msg, atomic_flag_mutex& mutex)
{
    auto now = msg_clock::now();
    std::lock_guard<atomic_flag_mutex> lock(mutex);
    ptrdiff_t begin_idx = strings.size();
    strings.insert(strings.end(), msg.begin(), msg.end());
    messages.emplace_back(source, now, begin_idx, begin_idx + msg.size());
}

out_err_message_t OutErrMessages::at(ptrdiff_t idx) const
{
    auto& x = messages[idx];
    string s(strings.begin() + x.msg_begin, strings.begin() + x.msg_end);
    return out_err_message_t(x.source,
                             std::chrono::duration<double>(x.t - process_start_time).count(), s);
}
void OutErrMessages::mark_process_start_time()
{
    process_start_time = msg_clock::now();
    process_start_system_time = std::chrono::system_clock::now();
}
exec_process_output_callback_t OutErrMessagesBuilder::stdout_callback()
{
    return [this](array_view<const char> x) {
        out_err_messages.add_msg(out_err_message_base_t::source_stdout, x, mutex);
        if (passthrough_callbacks)
            exec_process_callbacks::print_to_stdout(x);
    };
}
exec_process_output_callback_t OutErrMessagesBuilder::stderr_callback()
{
    return [this](array_view<const char> x) {
        out_err_messages.add_msg(out_err_message_base_t::source_stderr, x, mutex);
        if (passthrough_callbacks)
            exec_process_callbacks::print_to_stderr(x);
    };
}
}
