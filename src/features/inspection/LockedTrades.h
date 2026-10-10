#pragma once
namespace lamium::inspection::lockedTrades {
// Shows every trader level in the vanilla trade screen (L-129). The client
// already receives all levels' trades; vanilla's list hides levels beyond the
// next locked one. Display only: locked rows keep vanilla's locked look and
// cannot be selected.
bool start();
void stop();
}
