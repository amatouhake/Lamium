#pragma once
class IClientInstance;
namespace lamium::ui {
void start();
void stop();
void open(IClientInstance& client);
bool ownsInput();
}
