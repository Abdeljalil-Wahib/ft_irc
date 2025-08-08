#include "Server.hpp"
#include <iostream>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cctype>
#include <sstream>

Server::Server(int port, const std::string &password) : _port(port), _password(password), _serverSocket(-1) {}

Server::~Server()
{
	// Delete all dynamically allocated clients
	for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it)
	{
		delete it->second;
	}
	_clients.clear();
	// Delete all dynamically allocated channels
	// for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it)
	// {
	// 	delete it->second;
	// }
	// _channels.clear();
	if (_serverSocket != -1)
		close(_serverSocket);
}

void Server::start()
{
	_serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (_serverSocket < 0)
	{
		perror("socket");
		exit(1);
	}

	// Set socket options
	int opt = 1;
	if (setsockopt(_serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
	{
		perror("setsockopt");
		exit(1);
	}

	// Make socket non-blocking
	if (fcntl(_serverSocket, F_SETFL, O_NONBLOCK) < 0)
	{
		perror("fcntl");
		exit(1);
	}

	struct sockaddr_in addr;
	std::memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(_port);

	if (bind(_serverSocket, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		perror("bind");
		exit(1);
	}
	if (listen(_serverSocket, 10) < 0)
	{
		perror("listen");
		exit(1);
	}
	std::cout << "✅ Server listening on port " << _port << std::endl;
	// Simple event loop to keep server running
	std::cout << "Server is running. Press Ctrl+C to stop." << std::endl;
	_pollFds.push_back((struct pollfd){_serverSocket, POLLIN, 0});
	while (true)
	{
		int ret = poll(&_pollFds[0], _pollFds.size(), -1);
		if (ret < 0)
		{
			perror("poll");
			break;
		}
		for (size_t i = 0; i < _pollFds.size(); ++i)
		{
			if (_pollFds[i].revents & POLLIN)
			{
				if (_pollFds[i].fd == _serverSocket)
					handleNewConnection();
				else
					handleClientData(_pollFds[i].fd);
			}
		}
	}
}

// handle new connections

void Server::handleNewConnection()
{
	struct sockaddr_in clientAddr;
	socklen_t addrLen = sizeof(clientAddr);

	int clientFd = accept(_serverSocket, (struct sockaddr *)&clientAddr, &addrLen);
	if (clientFd < 0)
	{
		perror("accept");
		return;
	}
	// Set new client socket to non-blocking
	if (fcntl(clientFd, F_SETFL, O_NONBLOCK) < 0)
	{
		perror("fcntl");
		close(clientFd);
		return;
	}
	_pollFds.push_back((struct pollfd){clientFd, POLLIN, 0});
	_clients[clientFd] = new Client(clientFd);
	std::cout << "🔌 New client connected: fd=" << clientFd << std::endl;
}

// handle client data

void Server::handleClientData(int clientFd)
{
	char buffer[512];
	std::memset(buffer, 0, sizeof(buffer));

	int bytesRead = recv(clientFd, buffer, sizeof(buffer) - 1, 0);
	if (bytesRead <= 0) {
		std::cout << "❌ Client disconnected: fd=" << clientFd << std::endl;
		close(clientFd);
		// Clean up client object
		std::map<int, Client*>::iterator it = _clients.find(clientFd);
		if (it != _clients.end())
		{
			delete it->second;
			_clients.erase(it);
		}
		// Remove from poll list
		for (std::vector<pollfd>::iterator it = _pollFds.begin(); it != _pollFds.end(); ++it)
		{
			if (it->fd == clientFd)
			{
				_pollFds.erase(it);
				break;
			}
		}
		return;
	}
	buffer[bytesRead] = '\0';
	Client *client = _clients[clientFd];
	std::string input(buffer);
	// std::cout << "📥 Received from client [" << clientFd << "]: " << input << std::endl;
	// Used this while loop because nc sends lines with only \n, not \r\n so i use this to test but should go back to the original one below and use IRSSI
	size_t pos;
	while ((pos = input.find('\n')) != std::string::npos)
	{
		std::string line = input.substr(0, pos);
		if (!line.empty() && line[line.size() - 1] == '\r') // trim trailing \r if present
			line.erase(line.size() - 1);
		input.erase(0, pos + 1);
		std::cout << "📨 [" << clientFd << "] " << line << std::endl;
		handleCommand(client, line);
	}
	// ------------------------------------------------this while loop should be used in the final testing-----------------------------------------
	//++++ Basic line-by-line split (we’ll refine later)+++++
	// size_t pos;
	// while ((pos = input.find("\r\n")) != std::string::npos)
	// {
	// 	std::string line = input.substr(0, pos);
	// 	input.erase(0, pos + 2);
	// 	std::cout << " 📨 [" << clientFd << "] " << line << std::endl;
	// 	// TODO: handleCommand(client, line);
	// }
}
// Command handling methods
void Server::handleCommand(Client *client, const std::string &line)
{
	if (line.empty()) return;

	std::string command;
	std::string args;

	size_t spacePos = line.find(' ');
	if (spacePos == std::string::npos)
	{
		command = line;
	}
	else
	{
		command = line.substr(0, spacePos);
		args = line.substr(spacePos + 1);
	}
	// Convert command to uppercase for comparison
	for (size_t i = 0; i < command.length(); ++i)
	{
		command[i] = std::toupper(command[i]);
	}
	if (command == "PASS")
		handlePassCommand(client, args);
	else if (command == "NICK")
		handleNickCommand(client, args);
	else if (command == "USER")
		handleUserCommand(client, args);
	else if (command == "JOIN")
		handleJoinCommand(client, args);
	else if (command == "PRIVMSG")
		handlePrivMsgCommand(client, args);
	else if (command == "KICK")
		handleKickCommand(client, args);
	else if (command == "MODE")
		handleModeCommand(client, args);
	else if (command == "INVITE")
		handleInviteCommand(client, args);
	else
		sendToClient(client->getFd(), "421 * " + command + " :Unknown command\r\n");
		// Unknown command
}

void Server::handlePassCommand(Client *client, const std::string &args)
{
	// Check if password is required

	//-------------possible error hna need check -------------------
	if (_password.empty())
	{
		client->setPassAccepted(true);
		std::cout << "✅ Client [" << client->getFd() << "] password accepted (no password required)" << std::endl;
		checkRegistration(client);
		return;
	}

	// Check if password matches
	if (args == _password)
	{
		client->setPassAccepted(true);
		std::cout << "✅ Client [" << client->getFd() << "] password accepted" << std::endl;
	}
	else
	{
		client->setPassAccepted(false);
		sendToClient(client->getFd(), "464 * :Password incorrect\r\n");
		std::cout << "❌ Client [" << client->getFd() << "] wrong password" << std::endl;
		return;
	}

	checkRegistration(client);
}

void Server::handleNickCommand(Client *client, const std::string &args)
{
	if (args.empty())
	{
		sendToClient(client->getFd(), "431 * :No nickname given\r\n");
		return;
	}

	// Extract nickname (first word)
	std::string nick = args;
	size_t spacePos = nick.find(' ');
	if (spacePos != std::string::npos)
	{
		nick = nick.substr(0, spacePos);
	}
	// Check if nickname is already in use
	if (isNicknameInUse(nick))
	{
		sendToClient(client->getFd(), "433 * " + nick + " :Nickname is already in use\r\n");
		return;
	}
	std::string oldNick = client->getNickname();
	client->setNickname(nick);
	if (oldNick.empty())
		std::cout << "✅ Client [" << client->getFd() << "] set nickname: " << nick << std::endl;
	else
		std::cout << "✅ Client [" << client->getFd() << "] changed nickname from " << oldNick << " to " << nick << std::endl;

	checkRegistration(client);
}

void Server::handleUserCommand(Client *client, const std::string &args)
{
	if (args.empty())
	{
		sendToClient(client->getFd(), "461 * USER :Not enough parameters\r\n");
		return;
	}
	// Parse USER command: USER <username> <hostname> <servername> :<realname>
	// For now, we only store the username
	std::string username = args;
	size_t spacePos = username.find(' ');
	if (spacePos != std::string::npos)
		username = username.substr(0, spacePos);
	client->setUsername(username);
	std::cout << "✅ Client [" << client->getFd() << "] set username: " << username << std::endl;
	checkRegistration(client);
}

void Server::sendToClient(int fd, const std::string &message)
{
	send(fd, message.c_str(), message.length(), 0);
}

bool Server::isNicknameInUse(const std::string &nick)
{
	for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it)
	{
		if (it->second->getNickname() == nick)
			return true;
	}
	return false;
}

void Server::checkRegistration(Client *client)
{
	// Check if client has sent all required commands and password is accepted
	bool passOk = _password.empty() || client->hasSentPass();
	bool nickOk = client->hasSentNick();
	bool userOk = client->hasSentUser();

	if (passOk && nickOk && userOk && !client->isRegistered())
	{
		client->markRegistered();
		std::cout << "🎉 Client [" << client->getFd() << "] (" << client->getNickname() << ") is now registered!" << std::endl;
		// Send welcome messages (RPL_WELCOME)
		sendToClient(client->getFd(), "001 " + client->getNickname() + " :Welcome to the IRC Network " + client->getNickname() + "!" + client->getUsername() + "@localhost\r\n");
		sendToClient(client->getFd(), "002 " + client->getNickname() + " :Your host is ircserver, running version 1.0\r\n");
		sendToClient(client->getFd(), "003 " + client->getNickname() + " :This server was created today\r\n");
		sendToClient(client->getFd(), "004 " + client->getNickname() + " ircserver 1.0 o o\r\n");
	}
}


void Server::handleJoinCommand(Client* client, const std::string &args)
{
    if (args.empty()) {
        sendError(client, "461", "JOIN :Not enough parameters");
        return;
    }

    std::istringstream iss(args);
    std::string channelName;
    while (std::getline(iss, channelName, ',')) {
        if (!isValidChannelName(channelName)) {
            sendError(client, "403", channelName + " :No such channel");
            continue;
        }

        Channel* channel;
        bool isNewChannel = false;
        if (!channelExists(channelName)) {
            channel = createChannel(channelName);
            channel->setTopic("");
            isNewChannel = true;
        } else {
            channel = getChannel(channelName);
        }

        if (channel->hasClient(client)) {
            continue; // client already in the channel
        }
		if (channel->isInviteOnly() && !channel->isInvited(client)) {
			sendError(client, "473", channelName + " :Cannot join channel (+i)");
			return;
		}
        channel->addClient(client);

        // If channel just created, add client as operator
        if (isNewChannel) {
            channel->addOperator(client);
        }

        // Send JOIN confirmation to client (standard format)
        sendToClient(client->getFd(), ":" + client->getFullMask() + " JOIN " + channelName + "\r\n");

        // Send topic message
        sendReply(client, "332 " + client->getNickname() + " " + channelName + " :" + channel->getTopic());

        // Send NAMES list
        sendReply(client, "353 " + client->getNickname() + " = " + channelName + " :" + channel->getUserList());
        sendReply(client, "366 " + client->getNickname() + " " + channelName + " :End of NAMES list");

        // Broadcast JOIN to all other clients in the channel (skip sender)
        broadcastToChannels(channel, ":" + client->getFullMask() + " JOIN " + channelName, client, 1);
    }
}


void Server::handlePrivMsgCommand(Client *client, const std::string &args) {
	if (args.empty())
	{
		sendToClient(client->getFd(), "461 * PRIVMSG :Not enough parameters\r\n");
		return;
	}
	size_t spacePos = args.find(' ');
	if (spacePos == std::string::npos)
	{
		sendToClient(client->getFd(), "412 * PRIVMSG :No text to send\r\n");
		return;
	}
	std::string receiver = args.substr(0, spacePos);
	std::string message = args.substr(spacePos + 1);
	if (message.empty() || message[0] != ':')
	{
		sendToClient(client->getFd(), "412 * PRIVMSG :No text to send\r\n");
		return;
	}
	message = message.substr(1);
	if (receiver[0] == '#')
	{
		if (!channelExists(receiver))
		{
			sendToClient(client->getFd(), "403 " + client->getNickname() + " " + receiver + " :No such channel\r\n");
			return;
		}
		Channel *channel = getChannel(receiver);
		if (!channel->hasClient(client))
		{
			sendToClient(client->getFd(), "404 " + client->getNickname() + " " + receiver + " :Cannot send to channel\r\n");
			return;
		}
		std::string fullMsg = ":" + client->getFullMask() + " PRIVMSG " + receiver + " :" + message + "\r\n";
		broadcastToChannels(channel, fullMsg, client, 1);
	}
	else
	{
		Client *target = getClientByNick(receiver);
		if (!target)
		{
			sendToClient(client->getFd(), "401 " + client->getNickname() + " " + receiver + " :No such nickname\r\n");
			return;
		}
		std::string fullMsg = ":" + client->getFullMask() + " PRIVMSG " + receiver + " :" + message + "\r\n";
		sendToClient(target->getFd(), fullMsg);
	}
}

void Server::handleKickCommand(Client *client, const std::string& args) {
    if (!client->isRegistered())
        return;

    if (args.empty()) {
        sendError(client, "461", "KICK :Not enough parameters");
        return;
    }

    std::string channelName;
    std::string targetName;
    std::istringstream iss(args);
    iss >> channelName >> targetName;

    if (channelName.empty() || targetName.empty()) {
        sendError(client, "461", "KICK :Not enough parameters");
        return;
    }

    if (!channelExists(channelName)) {
        sendError(client, "403", channelName + " :No such channel");
        return;
    }

    Channel *channel = getChannel(channelName);

    if (!channel->hasClient(client)) {
        sendError(client, "442", channelName + " :You're not on that channel");
        return;
    }

    if (!channel->isOperator(client)) {
        sendError(client, "482", channelName + " :You're not channel operator");
        return;
    }

    Client *target = getClientByNick(targetName);

    if (!target) {
        sendError(client, "401", targetName + " :No such nickname/channel");
        return;
    }

    if (!channel->hasClient(target)) {
        sendError(client, "441", targetName + " " + channelName + " :They aren't on that channel");
        return;
    }

    std::string message = ":" + client->getFullMask() +
                          " KICK " + channelName + " " + targetName + 
                          " :" + client->getNickname() + "\r\n";

    broadcastToChannels(channel, message, client, 0);
    channel->removeClient(target);
}

void Server::handleInviteCommand(Client *inviter, const std::string &args) {
	if (!inviter->isRegistered())
		return;
	if (args.empty()) {
		sendError(inviter, "461", "INVITE :Not enough parameters");
		return;
	}
	std::string targetName;
	std::string channelName;
	std::istringstream iss(args);
	iss >> targetName >> channelName;
	if (targetName.empty() || channelName.empty()) {
		sendError(inviter, "461", "INVITE :Not enough parameters");
		return;
	}
	if (!channelExists(channelName)) {
        sendError(inviter, "403", channelName + " :No such channel");
		return;
	}
	Channel *channel = getChannel(channelName);
	if (!channel->hasClient(inviter)) {
		sendError(inviter, "442", channelName + " :You're not on that channel");
        return;
	}
	if (!channel->isOperator(inviter)) {
        sendError(inviter, "482", channelName + " :You're not channel operator");
        return;
    }
	Client *targetClient = getClientByNick(targetName);
	if (!targetClient) {
		sendError(inviter, "401", targetName + " :No such nickname");
        return;
	}
	if (channel->hasClient(targetClient)) {
		sendError(inviter, "443", targetName + " :is already on this channel");
        return;
	}

	channel->addInvited(targetClient);
	sendToClient(inviter->getFd(),
    "341 " + inviter->getNickname() + " " + targetName + " " + channelName + "\r\n");

    sendToClient(targetClient->getFd(),
    ":" + inviter->getFullMask() + " INVITE " + targetName + " :" + channelName + "\r\n");
}

void Server::handleModeCommand(Client *client, const std::string& args) {
	if (!client->isRegistered())
		return;
	std::string targetChannel;
	std::string mode;
	std::istringstream iss(args);
	iss >> targetChannel >> mode;

	if (!channelExists(targetChannel)) {
        sendError(client, "403", targetChannel + " :No such channel");
		return;
	}
	Channel *channel = getChannel(targetChannel);
	if (!channel->hasClient(client)) {
		sendError(client, "442", targetChannel + " :You're not on that channel");
        return;
	}
	if (!channel->isOperator(client)) {
        sendError(client, "482", targetChannel + " :You're not channel operator");
        return;
    }
	if (mode == "+i")
		channel->setInviteOnly(true);
	else
		channel->setInviteOnly(false);
	sendToClient(client->getFd(), ":ircserver MODE " + channel->getName() + " " + mode + "\r\n");
}






































bool Server::channelExists(const std::string& name) const {
    return _channels.find(name) != _channels.end();
}

Channel* Server::getChannel(const std::string& name) {
    std::map<std::string, Channel*>::iterator it = _channels.find(name);
    if (it != _channels.end())
        return it->second;
    return NULL;
}

Channel* Server::createChannel(const std::string& name) {
    if (channelExists(name))
        return getChannel(name);
    Channel* newChannel = new Channel(name);
    _channels[name] = newChannel;
    return newChannel;
}

void Server::removeChannel(const std::string& name) {
    std::map<std::string, Channel*>::iterator it = _channels.find(name);
    if (it != _channels.end()) {
        delete it->second;
        _channels.erase(it);
    }
}

void Server::broadcastToChannels(Channel* channel, const std::string& message, Client* sender, bool skipSender) {

    const std::set<Client*>& clients = channel->getClients();
    for (std::set<Client*>::const_iterator it = clients.begin(); it != clients.end(); ++it) {
        Client* client = *it;

        // in case of a KICK command the Sender is not skipped
        if (client == sender && skipSender)
            continue;
        sendToClient(client->getFd(), message);
    }
}

void Server::sendReply(Client* client, const std::string& message)
{
    // Replies start with numeric code (e.g. "001"), so prefix with client's nick
    sendToClient(client->getFd(), ":" + client->getNickname() + " " + message + "\r\n");
}

void Server::sendError(Client* client, const std::string& errorCode, const std::string& errorMsg)
{
    // Format: ":serverName errorCode nick :errorMsg"
    // For simplicity, use "*" as nick if client nick not set
    std::string nick = client->getNickname().empty() ? "*" : client->getNickname();
    std::string fullMsg = ":" + std::string("ircserver") + " " + errorCode + " " + nick + " :" + errorMsg + "\r\n";
    sendToClient(client->getFd(), fullMsg);
}


bool Server::isValidChannelName(const std::string &name) {
    if (name.empty() || name[0] != '#') return false;
    // Further validation rules as per IRC spec if needed
    return true;
}

Client* Server::getClientByNick(const std::string& nickname) {
    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        if (it->second->getNickname() == nickname) {
            return it->second;
        }
    }
    return NULL;
}
