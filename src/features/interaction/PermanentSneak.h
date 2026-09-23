#pragma once
class IClientInstance;
namespace lamium::interaction::sneak {
void start();
void stop();
void cancel();
bool active(IClientInstance& client);
void toggle(IClientInstance& client);
}
