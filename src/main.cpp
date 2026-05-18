#include <slint-interpreter.h>
#include <slint.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>

namespace {

using slint::ComponentHandle;
using slint::ComponentWeakHandle;
using slint::SharedString;
using slint::interpreter::ComponentInstance;
using slint::interpreter::Value;

std::atomic<int> g_thread_counter{ 0 };

std::string make_label(std::string_view who, int id, std::string_view stage)
{
    std::ostringstream os;
    os << "[" << who << " #" << id << "] " << stage;
    return os.str();
}

void spawn_worker(ComponentWeakHandle<ComponentInstance> weak, const char *who)
{
    const int id = ++g_thread_counter;
    std::cout << "spawning worker " << who << " #" << id << " on thread "
              << std::this_thread::get_id() << "\n";

    std::thread([weak, who, id]() {
        std::cout << "worker " << who << " #" << id << " sleeping 2s on thread "
                  << std::this_thread::get_id() << "\n";
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // First invoke_from_event_loop — hops from worker thread to the UI thread.
        slint::invoke_from_event_loop([weak, who, id]() {
            std::cout << "outer lambda for " << who << " #" << id
                      << " running on thread " << std::this_thread::get_id() << "\n";

            if (auto app = weak.lock()) {
                (*app)->set_property(
                        "status_text",
                        Value(SharedString(make_label(who, id,
                                                      "outer lambda — invoking nested_step")
                                                   .c_str())));
                // Hand off to the Slint-declared callback. Its C++ handler
                // (registered via set_callback below) schedules the nested
                // invoke_from_event_loop.
                Value args[] = { Value(SharedString(who)), Value(id) };
                (*app)->invoke("nested_step", args);
            }
        });
    }).detach();
}

void register_callbacks(const ComponentHandle<ComponentInstance> &app)
{
    ComponentWeakHandle<ComponentInstance> weak(app);

    app->set_callback("start_one", [weak](auto) -> Value {
        spawn_worker(weak, "A");
        return Value();
    });
    app->set_callback("start_two", [weak](auto) -> Value {
        spawn_worker(weak, "B");
        return Value();
    });
    app->set_callback("start_three", [weak](auto) -> Value {
        spawn_worker(weak, "C");
        return Value();
    });

    // C++ handler for the Slint-declared nested_step(string, int) callback.
    // Runs on the UI thread (invoked from the outer invoke_from_event_loop
    // lambda) and itself schedules a second invoke_from_event_loop whose
    // lambda updates the label — the nested case under investigation.
    app->set_callback("nested_step", [weak](std::span<const Value> args) -> Value {
        auto who_s = args[0].to_string().value_or(SharedString(""));
        auto id_d = args[1].to_number().value_or(0.0);
        int id = static_cast<int>(id_d);
        std::string who_str{ std::string_view(who_s) };

        std::cout << "nested_step callback for " << who_str << " #" << id
                  << " running on thread " << std::this_thread::get_id() << "\n";

        slint::invoke_from_event_loop([weak, who_str, id]() {
            std::cout << "inner (nested) lambda for " << who_str << " #" << id
                      << " running on thread " << std::this_thread::get_id() << "\n";

            if (auto app = weak.lock()) {
                (*app)->set_property(
                        "status_text",
                        Value(SharedString(make_label(who_str, id,
                                                      "inner nested lambda — label updated")
                                                   .c_str())));
            }
        });
        return Value();
    });
}

} // namespace

int main(int argc, char **argv)
{
    const std::string slint_path = (argc > 1) ? argv[1] : SLINT_FILE_PATH;
    std::cout << "loading slint file: " << slint_path << "\n";

    slint::interpreter::ComponentCompiler compiler;
    auto definition = compiler.build_from_path(slint_path);
    if (!definition) {
        std::cerr << "failed to compile " << slint_path << ":\n";
        for (const auto &d : compiler.diagnostics()) {
            std::cerr << "  " << d.source_file << ":" << d.line << ":" << d.column
                      << ": " << d.message << "\n";
        }
        return 1;
    }

    auto app = definition->create();
    register_callbacks(app);
    app->run();
    return 0;
}
