#include "OperatorCommands.hpp"
#include "Server.hpp"
#include "Channel.hpp"
#include "Client.hpp"
#include <sstream>

namespace OperatorCommands
{
    void handleKickCommand(Server *server, Client *client, const std::string &args)
    {
        if (!client->isRegistered())
            return;

        std::istringstream iss(args);
        std::string channelName, targetName;
        iss >> channelName >> targetName;

        if (args.empty() || channelName.empty() || targetName.empty())
        {
            server->sendError(client, "461", "KICK :Not enough parameters");
            return;
        }

        if (!server->channelExists(channelName))
        {
            server->sendError(client, "403", channelName + " :No such channel");
            return;
        }

        Channel *channel = server->getChannel(channelName);

        if (!channel->hasClient(client))
        {
            server->sendError(client, "442", channelName + " :You're not on that channel");
            return;
        }

        if (!channel->isOperator(client))
        {
            server->sendError(client, "482", channelName + " :You're not channel operator");
            return;
        }

        Client *target = server->getClientByNick(targetName);

        if (!target)
        {
            server->sendError(client, "401", targetName + " :No such nickname/channel");
            return;
        }

        if (!channel->hasClient(target))
        {
            server->sendError(client, "441", targetName + " " + channelName + " :They aren't on that channel");
            return;
        }

        std::string message = ":" + client->getFullMask() + " KICK " + channelName + " " + targetName + " :" + client->getNickname() + "\r\n";
        server->broadcastToChannels(channel, message, client, 0);
        channel->removeClient(target);
    }

    void handleInviteCommand(Server *server, Client *inviter, const std::string &args)
    {
        if (!inviter->isRegistered())
            return;

        std::istringstream iss(args);
        std::string targetName, channelName;
        iss >> targetName >> channelName;

        if (args.empty() || targetName.empty() || channelName.empty())
        {
            server->sendError(inviter, "461", "INVITE :Not enough parameters");
            return;
        }

        if (!server->channelExists(channelName))
        {
            server->sendError(inviter, "403", channelName + " :No such channel");
            return;
        }

        Channel *channel = server->getChannel(channelName);

        if (!channel->hasClient(inviter))
        {
            server->sendError(inviter, "442", channelName + " :You're not on that channel");
            return;
        }

        if (!channel->isOperator(inviter))
        {
            server->sendError(inviter, "482", channelName + " :You're not channel operator");
            return;
        }

        Client *targetClient = server->getClientByNick(targetName);

        if (!targetClient)
        {
            server->sendError(inviter, "401", targetName + " :No such nickname");
            return;
        }

        if (channel->hasClient(targetClient))
        {
            server->sendError(inviter, "443", targetName + " :is already on this channel");
            return;
        }

        channel->addInvited(targetClient);

        server->sendToClient(inviter, "341 " + inviter->getNickname() + " " + targetName + " " + channelName);
        server->sendToClient(targetClient, ":" + inviter->getFullMask() + " INVITE " + targetName + " :" + channelName);
    }
    void handleTopicCommand(Server *server, Client *client, const std::string &args)
    {
        if (!client->isRegistered())
            return;

        std::istringstream iss(args);
        std::string channelName;
        iss >> channelName;

        if (channelName.empty())
        {
            server->sendError(client, "461", "TOPIC :Not enough parameters");
            return;
        }

        if (!server->channelExists(channelName))
        {
            server->sendError(client, "403", channelName + " :No such channel");
            return;
        }

        Channel *channel = server->getChannel(channelName);
        if (!channel->hasClient(client))
        {
            server->sendError(client, "442", channelName + " :You're not on that channel");
            return;
        }

        size_t topicPos = args.find(':');
        if (topicPos == std::string::npos)
        {
            if (channel->getTopic().empty())
                server->sendToClient(client, ":ircserver 331 " + client->getNickname() + " " + channelName + " :No topic is set");
            else
                server->sendToClient(client, ":ircserver 332 " + client->getNickname() + " " + channelName + " :" + channel->getTopic());
        }
        else // Set topic
        {
            if (channel->isTopicProtected() && !channel->isOperator(client))
            {
                server->sendError(client, "482", channelName + " :You're not channel operator");
                return;
            }

            std::string newTopic = args.substr(topicPos + 1);
            channel->setTopic(newTopic);

            std::string topicMsg = ":" + client->getFullMask() + " TOPIC " + channelName + " :" + newTopic;
            server->broadcastToChannels(channel, topicMsg, NULL, false); // Broadcast to all, including sender
        }
    }
    void handleModeCommand(Server *server, Client *client, const std::string &args)
    {
        if (!client->isRegistered())
            return;

        std::istringstream iss(args);
        std::string channelName, modeStr;
        iss >> channelName >> modeStr;

        if (channelName.empty())
        {
            server->sendError(client, "461", "MODE :Not enough parameters");
            return;
        }
        if (!server->channelExists(channelName))
        {
            server->sendError(client, "403", channelName + " :No such channel");
            return;
        }

        Channel *channel = server->getChannel(channelName);

        // If no modes are provided, just send the current channel modes
        if (modeStr.empty())
        {
            server->sendToClient(client, "324 " + client->getNickname() + " " + channelName + " " + channel->getModes());
            return;
        }

        if (!channel->isOperator(client))
        {
            server->sendError(client, "482", channelName + " :You're not channel operator");
            return;
        }

        bool addMode = true;
        std::string params;
        std::getline(iss, params); // Get the rest of the line for mode parameters
        std::istringstream paramStream(params);

        std::string fullModeChangeStr;
        std::string affectedParams;

        for (size_t i = 0; i < modeStr.length(); ++i)
        {
            char mode = modeStr[i];
            if (mode == '+')
            {
                addMode = true;
                continue;
            }
            if (mode == '-')
            {
                addMode = false;
                continue;
            }

            switch (mode)
            {
            case 'i':
                channel->setInviteOnly(addMode);
                fullModeChangeStr += (addMode ? "+i" : "-i");
                break;
            case 't':
                channel->setTopicProtected(addMode);
                fullModeChangeStr += (addMode ? "+t" : "-t");
                break;
            case 'k':
            {
                std::string key;
                paramStream >> key;
                if (addMode && key.empty())
                {
                    server->sendError(client, "461", "MODE :You must specify a password for +k");
                    continue;
                }
                channel->setKey(addMode ? key : "");
                fullModeChangeStr += (addMode ? "+k" : "-k");
                if (addMode)
                    affectedParams += " " + key;
                break;
            }
            case 'o':
            {
                std::string targetNick;
                paramStream >> targetNick;
                if (targetNick.empty())
                {
                    server->sendError(client, "461", "MODE :You must specify a user for +/-o");
                    continue;
                }
                Client *target = server->getClientByNick(targetNick);
                if (!target || !channel->hasClient(target))
                {
                    server->sendError(client, "401", targetNick + " :No such nick/channel");
                    continue;
                }
                if (addMode)
                    channel->addOperator(target);
                else
                    channel->removeOperator(target);
                fullModeChangeStr += (addMode ? "+o" : "-o");
                affectedParams += " " + targetNick;
                break;
            }
            case 'l':
            {
                if (addMode)
                {
                    std::string limitStr;
                    paramStream >> limitStr;
                    long limit = atol(limitStr.c_str());
                    if (limit <= 0)
                    {
                        server->sendError(client, "461", "MODE :You must specify a valid limit for +l");
                        continue;
                    }
                    channel->setUserLimit(limit);
                    fullModeChangeStr += "+l";
                    affectedParams += " " + limitStr;
                }
                else
                {
                    channel->setUserLimit(0);
                    fullModeChangeStr += "-l";
                }
                break;
            }
            default:
                server->sendError(client, "472", std::string(1, mode) + " :is unknown mode char to me");
                break;
            }
        }
        if (!fullModeChangeStr.empty())
        {
            std::string modeMsg = ":" + client->getFullMask() + " MODE " + channelName + " " + fullModeChangeStr + affectedParams;
            server->broadcastToChannels(channel, modeMsg, NULL, false);
        }
    }

}