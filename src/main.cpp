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

void spawn_worker(ComponentWeakHandle<ComponentInstance> weak, std::string_view buttonID,  const char *who)
{
    const int id = ++g_thread_counter;
    std::cout << "spawning worker " << who << " #" << id << " on thread "
              << std::this_thread::get_id() <<  std::endl;

    std::thread([weak, who, id, buttonID]() {
        std::cout << "worker " << who << " #" << id << " sleeping 1s on thread "
                  << std::this_thread::get_id()  << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // First invoke_from_event_loop — hops from worker thread to the UI thread.
        slint::invoke_from_event_loop([weak, who, id, buttonID]() {
            std::cout << "outer lambda for " << who << " #" << id
                      << " running on thread " << std::this_thread::get_id()  << std::endl;

            slint::SharedString buttonState(who);
            bool pressed = buttonState == "pressed";
            slint::SharedString propertyToUpdate = (buttonID == "yes") ? "yesButtonPressed" : "noButtonPressed";

            if (auto app = weak.lock()) {
                (*app)->set_property(
                            propertyToUpdate,
                            Value(pressed));

                // Hand off to the Slint-declared callback. Its C++ handler
                // (registered via set_callback below) schedules the nested
                // invoke_from_event_loop.
                if(!pressed) {
                    Value args[] = { Value(buttonID) };
                    (*app)->invoke("handlerButtonReleased", args);
                }
            }
        });
    }).detach();
}

void register_callbacks(const ComponentHandle<ComponentInstance> &app)
{
    ComponentWeakHandle<ComponentInstance> weak(app);

    app->set_callback("start_one", [weak](std::span<const Value> args) -> Value {
        auto ButtonID = args[0].to_string().value_or(SharedString(""));
        spawn_worker(weak, ButtonID,  "pressed");
        return Value();
    });
    app->set_callback("start_two", [weak](std::span<const Value> args) -> Value {
        auto ButtonID = args[0].to_string().value_or(SharedString(""));
        spawn_worker(weak, ButtonID, "released");
        return Value();
    });


    // C++ handler for the Slint-declared nested_step(string, int) callback.
    // Runs on the UI thread (invoked from the outer invoke_from_event_loop
    // lambda) and itself schedules a second invoke_from_event_loop whose
    // lambda updates the label — the nested case under investigation.
    bool ret = app->set_callback("signalSendMessage", [weak](std::span<const Value> args) -> Value {
        auto CallID = args[0].to_string().value_or(SharedString(""));
        auto Message = args[1].to_string().value_or(SharedString(""));

        std::cout << "\n C++ impl - signalSendMessage.  CallID = " << CallID << " Message = " << Message << std::endl ;

        return Value();
    });

    std::cout << "\n app->set_callback(signalSendMessage) returned - " << ret << std::endl;
}

} // namespace

int main(int argc, char **argv)
{
    const std::string slint_path = (argc > 1) ? argv[1] : SLINT_FILE_PATH;
    std::cout << "loading slint file: " << slint_path  << std::endl;

    slint::interpreter::ComponentCompiler compiler;
    auto definition = compiler.build_from_path(slint_path);
    if (!definition) {
        std::cerr << "failed to compile " << slint_path  << std::endl;
        for (const auto &d : compiler.diagnostics()) {
            std::cerr << "  " << d.source_file << ":" << d.line << ":" << d.column
                      << ": " << d.message  << std::endl;
        }
        return 1;
    }

    auto app = definition->create();
    register_callbacks(app);
    app->run();
    return 0;
}
