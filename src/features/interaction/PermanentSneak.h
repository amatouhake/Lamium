#pragma once
class IClientInstance;
namespace lamium::interaction::sneak {
void start();
void stop();
void cancel();
void toggle(IClientInstance& client);
}
