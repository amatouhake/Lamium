#pragma once
class IClientInstance;
namespace lamium::interaction::sneak {
void start();
void stop();
void cancel();
bool active(IClientInstance& client);
void toggle(IClientInstance& client);
}
// Permanent Sprint shares the sneak raw-input hook; sneak::start/stop install it.
namespace lamium::interaction::sprint {
void cancel();
bool active(IClientInstance& client);
void toggle(IClientInstance& client);
}
