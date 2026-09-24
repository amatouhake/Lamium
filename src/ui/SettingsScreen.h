#pragma once
class IClientInstance;
namespace lamium::ui {
void start();
void stop();
void open(IClientInstance& client);
void openShapes(IClientInstance& client);
void openHotkeys(IClientInstance& client);
bool ownsInput();
void cancelInputCapture();
}
