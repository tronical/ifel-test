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

std::atomic<int> g_thread_counter{0};

std::string make_label(std::string_view who, int id, std::string_view stage) {
    std::ostringstream os;
    os << "[" << who << " #" << id << "] " << stage;
    return os.str();
}

void spawn_worker(ComponentWeakHandle<ComponentInstance> weak,
                  std::string_view buttonID, const char *buttonState) {
    const int id = ++g_thread_counter;
    std::cout << "spawning worker " << buttonState << " #" << id << " on thread "
              << std::this_thread::get_id() << std::endl;

    std::thread([weak, buttonState, id, buttonID]() {
        std::cout << "worker " << buttonState << " #" << id
                  << " sleeping 1s on thread " << std::this_thread::get_id()
                  << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // First invoke_from_event_loop — hops from worker thread to the UI thread.
        slint::invoke_from_event_loop([weak, buttonState, id, buttonID]() {
            std::cout << "outer lambda for buttonID = " << buttonID
                      << " , buttonState = " << buttonState << " #" << id
                      << " running on thread " << std::this_thread::get_id()
                      << std::endl;

            slint::SharedString buttonStateSharedString(buttonState);
            bool pressed = buttonStateSharedString == "pressed";
            slint::SharedString buttonFieldToUpdate =
                    (buttonID == "yes") ? "Yes" : "No";

            if (auto app = weak.lock()) {

                // Update DeviceInterface.Button
                auto buttonStruct =
                        (*app)
                        ->get_global_property("DeviceInterface", "Button")
                        ->to_struct();
                buttonStruct->set_field(buttonFieldToUpdate, Value(pressed));
                std::cout << "Setting DeviceInterface.Button " << std::endl;

                (*app)->set_global_property("DeviceInterface", "Button",
                                            buttonStruct.value());

                // If release state happened, we need to call the handler callback
                if(!pressed) {
                    Value args[] = { Value(buttonID) };
                    (*app)->invoke_global("InputManagerInterface","handlerButtonReleased",args);
                }
            }
        });
    }).detach();
}

void register_callbacks(const ComponentHandle<ComponentInstance> &app) {
    ComponentWeakHandle<ComponentInstance> weak(app);

    app->set_callback(
                "simulate_press", [weak](std::span<const Value> args) -> Value {
        auto ButtonID = args[0].to_string().value_or(SharedString(""));
        spawn_worker(weak, ButtonID, "pressed");
        return Value();
    });
    app->set_callback(
                "simulate_release", [weak](std::span<const Value> args) -> Value {
        auto ButtonID = args[0].to_string().value_or(SharedString(""));
        spawn_worker(weak, ButtonID, "released");
        return Value();
    });

    // C++ handler
    bool ret = app->set_global_callback(
                "CallInterface", "signalSendMessage",
                [weak](std::span<const Value> args) -> Value {
            auto CallID = args[0].to_string().value_or(SharedString(""));
            auto Message = args[1].to_string().value_or(SharedString(""));

            std::cout << "\n C++ impl - CallInterface.signalSendMessage.  CallID = "
            << CallID << " Message = " << Message << std::endl;

            return Value();
            });

    std::cout << "\n app->set_callback(signalSendMessage) returned - " << ret
              << std::endl;
}

} // namespace

int main(int argc, char **argv) {
    const std::string slint_path = (argc > 1) ? argv[1] : SLINT_FILE_PATH;
    std::cout << "loading slint file: " << slint_path << std::endl;

    slint::interpreter::ComponentCompiler compiler;
    auto definition = compiler.build_from_path(slint_path);
    if (!definition) {
        std::cerr << "failed to compile " << slint_path << std::endl;
        for (const auto &d : compiler.diagnostics()) {
            std::cerr << "  " << d.source_file << ":" << d.line << ":" << d.column
                      << ": " << d.message << std::endl;
        }
        return 1;
    }

    auto app = definition->create();
    register_callbacks(app);
    app->run();
    return 0;
}
