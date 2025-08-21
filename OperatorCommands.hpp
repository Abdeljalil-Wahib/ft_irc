#pragma once
#include <string>
#include <cstdlib>
class Client;
class Server;

namespace OperatorCommands
{
    void handleModeCommand(Server *server, Client *client, const std::string &args);
    void handleTopicCommand(Server *server, Client *client, const std::string &args);
    void handleKickCommand(Server *server, Client *client, const std::string &args);
    void handleInviteCommand(Server *server, Client *client, const std::string &args);
}
