#include "out_err_messages.h"

#include <deque>
#include <mutex>
#include <thread>

#include <Poco/Pipe.h>
#include <Poco/Process.h>

#include <adasworks/sx/check.h>
#include <adasworks/sx/log.h>

namespace cmakex {

// add_msg is thread-safe
void OutErrMessagesBuilder::add_msg(source_t source, array_view<const char> msg)
{
    internal::out_err_message_internal_t** last_unfinished_message;
    strings_t* strings;
    // char source_char = 0;
    if (source == out_err_message_base_t::source_stdout) {
        last_unfinished_message = &last_unfinished_stdout_message;
        strings = &out_err_messages.stdout_strings;
        // source_char = 'O';
    } else {
        last_unfinished_message = &last_unfinished_stderr_message;
        strings = &out_err_messages.stderr_strings;
        // source_char = 'E';
    }
    std::lock_guard<atomic_flag_mutex> lock(mutex);
    if (*last_unfinished_message) {
        // auto s0 = (*last_unfinished_message)->msg_end;
        (*last_unfinished_message)->msg_end += msg.size();
        // LOG_TRACE("S:%c, msg_end (%d) += %d", source_char, (int)s0, (int)msg.size());
    } else {
        ptrdiff_t begin_idx = strings->size();
        out_err_messages.messages.emplace_back(source, msg_clock::now(), begin_idx,
                                               begin_idx + msg.size());
        *last_unfinished_message = &out_err_messages.messages.back();
        // LOG_TRACE("S:%c, new msg (%d..%d)", source_char, (int)begin_idx, (int)(begin_idx +
        // msg.size()));
    }
    // LOG_TRACE("S:%c, strings (%d) += %d", source_char, (int)strings->size(), (int)msg.size());
    strings->insert(strings->end(), msg.begin(), msg.end());
    const char c_line_feed = 10;
    if (!msg.empty() && msg.end()[-1] == c_line_feed)
        *last_unfinished_message = nullptr;
}

out_err_message_t OutErrMessages::at(ptrdiff_t idx) const
{
    auto& x = messages[idx];
    auto& strings = x.source == source_t::source_stdout ? stdout_strings : stderr_strings;
    string s(strings.begin() + x.msg_begin, strings.begin() + x.msg_end);
    return out_err_message_t(x.source, std::chrono::duration<double>(x.t - start_time).count(), s);
}
void OutErrMessages::mark_start_time()
{
    start_time = msg_clock::now();
    start_system_time_ = std::chrono::system_clock::now();
}
void OutErrMessages::mark_end_time()
{
    end_system_time_ = std::chrono::system_clock::now();
}
exec_process_output_callback_t OutErrMessagesBuilder::stdout_callback()
{
    if (stdout_mode == pipe_echo)
        return {};
    return [this](array_view<const char> x) {
        add_msg(out_err_message_base_t::source_stdout, x);
        if (stdout_mode == pipe_echo_and_capture)
            exec_process_callbacks::print_to_stdout(x);
    };
}
exec_process_output_callback_t OutErrMessagesBuilder::stderr_callback()
{
    if (stderr_mode == pipe_echo)
        return {};
    return [this](array_view<const char> x) {
        add_msg(out_err_message_base_t::source_stderr, x);
        if (stderr_mode == pipe_echo_and_capture)
            exec_process_callbacks::print_to_stderr(x);
    };
}
}
