#pragma once
class IClientInstance;
namespace lamium::interaction::periodic {
enum class Action { Attack, Use };
void start();
void stop();
void cancel();
bool active(IClientInstance& client, Action action);
void toggle(IClientInstance&, Action);
}
