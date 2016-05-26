
#ifndef INVOKE_0293740234
#define INVOKE_0293740234

#include <chrono>
#include <cmath>
#include <deque>
#include <functional>

#include <adasworks/sx/mutex.h>

#include "exec_process.h"

namespace cmakex {

using adasworks::sx::atomic_flag_mutex;
using std::move;

struct out_err_message_base_t
{
    enum source_t
    {
        source_invalid,
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

class OutErrMessagesBuilder;

// messages received on stdout and stderr of the launched process
class OutErrMessages
{
private:
    using strings_t = std::deque<char>;
    using system_clock = std::chrono::system_clock;

public:
    using source_t = internal::out_err_message_internal_t::source_t;
    using msg_clock = internal::out_err_message_internal_t::msg_clock;

    OutErrMessages() : end_system_time_(system_clock::time_point::min()) { mark_start_time(); }
    OutErrMessages(const OutErrMessages&) = delete;
    OutErrMessages(OutErrMessages&& x)
        : messages(move(x.messages)),
          stdout_strings(move(x.stdout_strings)),
          stderr_strings(move(x.stderr_strings)),
          start_time(move(x.start_time)),
          start_system_time_(move(x.start_system_time_)),
          end_system_time_(move(x.end_system_time_))
    {
    }

    void mark_start_time();  // also called in ctor
    void mark_end_time();
    system_clock::time_point start_system_time() const { return start_system_time_; }
    system_clock::time_point end_system_time() const { return end_system_time_; }
    bool empty() const { return messages.empty(); }
    ptrdiff_t size() const { return messages.size(); }
    out_err_message_t at(ptrdiff_t idx) const;

private:
    friend class OutErrMessagesBuilder;
    using messages_t = std::deque<internal::out_err_message_internal_t>;
    messages_t messages;
    strings_t stdout_strings;
    strings_t stderr_strings;
    msg_clock::time_point start_time;
    system_clock::time_point start_system_time_;
    system_clock::time_point end_system_time_;
};

class OutErrMessagesBuilder
{
public:
    OutErrMessagesBuilder(bool passthrough_callbacks = true)
        : passthrough_callbacks(passthrough_callbacks)
    {
        clear();
    }
    exec_process_output_callback_t stdout_callback();
    exec_process_output_callback_t stderr_callback();
    OutErrMessages move_result()
    {
        out_err_messages.mark_end_time();
        auto tmp = move(out_err_messages);
        clear();
        return tmp;
    }

private:
    using source_t = OutErrMessages::source_t;
    using msg_clock = OutErrMessages::msg_clock;
    using strings_t = OutErrMessages::strings_t;

    void clear() { last_unfinished_stdout_message = last_unfinished_stderr_message = nullptr; }
    void add_msg(source_t source, array_view<const char> msg);

    const bool passthrough_callbacks;
    OutErrMessages out_err_messages;
    atomic_flag_mutex mutex;
    internal::out_err_message_internal_t* last_unfinished_stdout_message = nullptr;
    internal::out_err_message_internal_t* last_unfinished_stderr_message = nullptr;
};
}
#endif
