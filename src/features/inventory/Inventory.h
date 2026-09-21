#pragma once
class IClientInstance;
namespace lamium::inventory {
bool start();
void stop();
void requestSort(IClientInstance& client);
}
