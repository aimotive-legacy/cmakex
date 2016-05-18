#ifndef INVOKE_0293740234
#define INVOKE_0293740234

#include <chrono>
#include <cmath>
#include <deque>
#include <functional>

#include <adasworks/sx/mutex.h>

#include "exec_process.h"
#include "using-decls.h"

namespace cmakex {

using adasworks::sx::atomic_flag_mutex;

struct out_err_message_base_t
{
    enum source_t
    {
        source_stdout,
        source_stderr
    };

    out_err_message_base_t(source_t source) : source(source) {}
    source_t source;
};

namespace internal {
struct out_err_message_internal_t : public out_err_message_base_t
{
    using msg_clock = std::chrono::high_resolution_clock;
    using time_point = msg_clock::time_point;

    out_err_message_internal_t(source_t source,
                               time_point t,
                               ptrdiff_t msg_begin,
                               ptrdiff_t msg_end)
        : out_err_message_base_t(source), t(t), msg_begin(msg_begin), msg_end(msg_end)
    {
    }

    time_point t;
    ptrdiff_t msg_begin, msg_end;
};
}

struct out_err_message_t : public out_err_message_base_t
{
    out_err_message_t(source_t source, double t, const std::string& text)
        : out_err_message_base_t(source), t(t), text(text)
    {
    }
    out_err_message_t(source_t source, double t, std::string&& text)
        : out_err_message_base_t(source), t(t), text(move(text))
    {
    }

    double t = NAN;  // time since process launch time
    string text;     // message text, as received from the pipe
};

// messages received on stdout and stderr of the launched process
class OutErrMessages
{
private:
    using strings_t = std::deque<char>;

public:
    using source_t = internal::out_err_message_internal_t::source_t;
    using msg_clock = internal::out_err_message_internal_t::msg_clock;

    OutErrMessages() { mark_process_start_time(); }
    OutErrMessages(const OutErrMessages&) = delete;
    OutErrMessages(OutErrMessages&&) = default;

    void mark_process_start_time();
    // add_msg is thread-safe
    void add_msg(source_t source, array_view<const char> msg, atomic_flag_mutex& mutex);

    bool empty() const { return messages.empty(); }
    ptrdiff_t size() const { return messages.size(); }
    out_err_message_t at(ptrdiff_t idx) const;

private:
    std::deque<internal::out_err_message_internal_t> messages;
    strings_t strings;
    msg_clock::time_point process_start_time;
    system_clock::time_point process_start_system_time;
};

class OutErrMessagesBuilder
{
public:
    OutErrMessagesBuilder(bool passthrough_callbacks = true)
        : passthrough_callbacks(passthrough_callbacks)
    {
    }
    exec_process_output_callback_t stdout_callback();
    exec_process_output_callback_t stderr_callback();
    OutErrMessages move_result() { return move(out_err_messages); }
private:
    const bool passthrough_callbacks;
    OutErrMessages out_err_messages;
    atomic_flag_mutex mutex;
};
}
#endif
