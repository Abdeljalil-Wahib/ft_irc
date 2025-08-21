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
#include "OperatorCommands.hpp"

Server::Server(int port, const std::string &password) : _port(port), _password(password), _serverSocket(-1) {}

Server::~Server()
{
	// Delete all dynamically allocated clients
	for (std::map<int, Client *>::iterator it = _clients.begin(); it != _clients.end(); ++it)
	{
		delete it->second;
	}
	_clients.clear();
	for (std::map<std::string, Channel *>::iterator it = _channels.begin(); it != _channels.end(); ++it)
	{
		delete it->second;
	}
	_channels.clear();
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
	char tempBuffer[512];
	Client *client = _clients[clientFd];

	// Read new data from the socket
	memset(tempBuffer, 0, sizeof(tempBuffer));
	int bytesRead = recv(clientFd, tempBuffer, sizeof(tempBuffer) - 1, 0);

	if (bytesRead <= 0)
	{
		// Client disconnection logic (your existing code is correct)
		std::cout << "❌ Client disconnected: fd=" << clientFd << std::endl;
		for (std::vector<pollfd>::iterator it = _pollFds.begin(); it != _pollFds.end(); ++it)
		{
			if (it->fd == clientFd)
			{
				_pollFds.erase(it);
				break;
			}
		}
		close(clientFd);
		delete client;
		_clients.erase(clientFd);
		return;
	}

	// 1. Append new data to the client's persistent buffer
	std::string receivedData(tempBuffer, bytesRead);
	std::string clientBuffer = client->getBuffer() + receivedData;

	// 2. Process all complete commands (ending in \n) from the buffer
	size_t pos;
	while ((pos = clientBuffer.find('\n')) != std::string::npos)
	{
		// Extract a single command line from the buffer
		std::string line = clientBuffer.substr(0, pos);

		// Remove the processed command (and the \n) from the start of the buffer
		clientBuffer.erase(0, pos + 1);

		// Trim the trailing \r if it exists
		if (!line.empty() && line[line.size() - 1] == '\r')
			line.erase(line.size() - 1);

		// Handle the command if it's not empty
		if (!line.empty())
		{
			std::cout << "📨 [" << clientFd << "] " << line << std::endl;
			handleCommand(client, line);
		}
	}
	// 3. Save any remaining partial command back to the client's buffer
	client->setBuffer(clientBuffer);
}

void Server::handleCommand(Client *client, const std::string &line)
{
	if (line.empty())
		return;

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
		OperatorCommands::handleKickCommand(this, client, args);
	else if (command == "MODE")
		OperatorCommands::handleModeCommand(this, client, args);
	else if (command == "INVITE")
		OperatorCommands::handleInviteCommand(this, client, args);
	else if (command == "TOPIC")
		OperatorCommands::handleTopicCommand(this, client, args);
	else
		sendToClient(client, "421 * " + command + " :Unknown command");
}

void Server::handlePassCommand(Client *client, const std::string &args)
{
	if (client->isRegistered())
	{
		sendError(client, "462", ":You may not reregister");
		return;
	}
	if (_password.empty())
	{
		client->setPassAccepted(true);
		std::cout << "✅ Client [" << client->getFd() << "] password accepted (no password required)" << std::endl;
		checkRegistration(client);
		return;
	}
	if (args == _password)
	{
		client->setPassAccepted(true);
		std::cout << "✅ Client [" << client->getFd() << "] password accepted" << std::endl;
	}
	else
	{
		client->setPassAccepted(false);
		sendToClient(client, "464 * :Password incorrect");
		std::cout << "❌ Client [" << client->getFd() << "] wrong password" << std::endl;
		return;
	}

	checkRegistration(client);
}

void Server::handleNickCommand(Client *client, const std::string &args)
{
	if (!_password.empty() && !client->hasSentPass())
	{
		sendError(client, "464", ":Password required");
		return;
	}
	if (args.empty())
	{
		sendToClient(client, "431 * :No nickname given");
		return;
	}

	std::string nick = args;
	size_t spacePos = nick.find(' ');
	if (spacePos != std::string::npos)
	{
		nick = nick.substr(0, spacePos);
	}
	if (isNicknameInUse(nick))
	{
		sendToClient(client, "433 * " + nick + " :Nickname is already in use");
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
	if (!_password.empty() && !client->hasSentPass())
	{
		sendError(client, "464", ":Password required");
		return;
	}
	if (!client->hasSentNick())
	{
		sendError(client, "451", ":You have not registered");
		return;
	}
	if (args.empty())
	{
		sendToClient(client, "461 * USER :Not enough parameters");
		return;
	}
	std::string username = args;
	size_t spacePos = username.find(' ');
	if (spacePos != std::string::npos)
		username = username.substr(0, spacePos);
	client->setUsername(username);
	std::cout << "✅ Client [" << client->getFd() << "] set username: " << username << std::endl;
	checkRegistration(client);
}

void Server::handleJoinCommand(Client *client, const std::string &args)
{
	if (args.empty())
	{
		sendError(client, "461", "JOIN :Not enough parameters");
		return;
	}

	std::istringstream iss(args);
	std::string channelName;
	std::string providedKey;

	while (std::getline(iss, channelName, ','))
	{
		size_t spacePos = channelName.find(' ');
		if (spacePos != std::string::npos)
		{
			providedKey = channelName.substr(spacePos + 1);
			channelName = channelName.substr(0, spacePos);
		}
		else
		{
			providedKey.clear();
		}

		if (!isValidChannelName(channelName))
		{
			sendError(client, "403", channelName + " :No such channel");
			continue;
		}

		Channel *channel;
		bool isNewChannel = false;

		if (!channelExists(channelName))
		{
			channel = createChannel(channelName);
			channel->setTopic("");
			isNewChannel = true;
		}
		else
		{
			channel = getChannel(channelName);
		}

		if (channel->hasClient(client))
			continue;

		if (channel->isInviteOnly() && !channel->isInvited(client))
		{
			sendError(client, "473", channelName + " :Cannot join channel (+i)");
			continue;
		}

		if (channel->hasKey() && (providedKey.empty() || providedKey != channel->getKey()))
		{
			sendError(client, "475", channelName + " :Cannot join channel (+k)");
			continue;
		}

		channel->addClient(client);

		if (isNewChannel)
			channel->addOperator(client);

		sendToClient(client, ":" + client->getFullMask() + " JOIN :" + channelName);

		sendToClient(client, ":ircserver 332 " + client->getNickname() + " " + channelName + " :" + channel->getTopic());

		std::string namesList;
		const std::set<Client *> &clientsInChannel = channel->getClients();
		for (std::set<Client *>::const_iterator it = clientsInChannel.begin(); it != clientsInChannel.end(); ++it)
		{
			Client *c = *it;
			if (channel->isOperator(c))
				namesList += "@";
			namesList += c->getNickname() + " ";
		}

		if (!namesList.empty() && namesList[namesList.length() - 1] == ' ')
			namesList = namesList.substr(0, namesList.length() - 1);

		sendToClient(client, ":ircserver 353 " + client->getNickname() + " = " + channelName + " :" + namesList);
		sendToClient(client, ":ircserver 366 " + client->getNickname() + " " + channelName + " :End of NAMES list");

		broadcastToChannels(channel, ":" + client->getFullMask() + " JOIN :" + channelName, client, true);
	}
}

void Server::sendToClient(Client *client, const std::string &message)
{
	int fd = client->getFd();
	std::string fullMsg = message;
	if (fullMsg.length() < 2 || fullMsg.substr(fullMsg.length() - 2) != "\r\n")
		fullMsg += "\r\n";

	size_t totalSent = 0;
	size_t toSend = fullMsg.length();
	const char *data = fullMsg.c_str();

	while (totalSent < toSend)
	{
		int sent = send(fd, data + totalSent, toSend - totalSent, 0);
		if (sent <= 0)
		{
			break;
		}
		totalSent += sent;
	}
}

bool Server::isNicknameInUse(const std::string &nick)
{
	for (std::map<int, Client *>::iterator it = _clients.begin(); it != _clients.end(); ++it)
	{
		if (it->second->getNickname() == nick)
			return true;
	}
	return false;
}

void Server::checkRegistration(Client *client)
{
	bool passOk = _password.empty() || client->hasSentPass();
	bool nickOk = client->hasSentNick();
	bool userOk = client->hasSentUser();

	if (passOk && nickOk && userOk && !client->isRegistered())
	{
		client->markRegistered();
		std::cout << "🎉 Client [" << client->getFd() << "] (" << client->getNickname() << ") is now registered!" << std::endl;

		sendToClient(client, "001 " + client->getNickname() + " :Welcome to the IRC Network " + client->getNickname() + "!" + client->getUsername() + "@localhost");
		sendToClient(client, "002 " + client->getNickname() + " :Your host is ircserver, running version 1.0");
		sendToClient(client, "003 " + client->getNickname() + " :This server was created today");
		sendToClient(client, "004 " + client->getNickname() + " ircserver 1.0 o o");
	}
}

void Server::handlePrivMsgCommand(Client *client, const std::string &args)
{
	if (args.empty())
	{
		sendToClient(client, "461 * PRIVMSG :Not enough parameters");
		return;
	}
	size_t spacePos = args.find(' ');
	if (spacePos == std::string::npos)
	{
		sendToClient(client, "412 * PRIVMSG :No text to send");
		return;
	}

	std::string receiver = args.substr(0, spacePos);
	std::string message = args.substr(spacePos + 1);
	if (message.empty() || message[0] != ':')
	{
		sendToClient(client, "412 * PRIVMSG :No text to send");
		return;
	}
	message = message.substr(1);

	if (receiver[0] == '#')
	{
		if (!channelExists(receiver))
		{
			sendToClient(client, "403 " + client->getNickname() + " " + receiver + " :No such channel");
			return;
		}
		Channel *channel = getChannel(receiver);
		if (!channel->hasClient(client))
		{
			sendToClient(client, "404 " + client->getNickname() + " " + receiver + " :Cannot send to channel");
			return;
		}
		broadcastToChannels(channel, ":" + client->getFullMask() + " PRIVMSG " + receiver + " :" + message, client, true);
	}
	else
	{
		Client *target = getClientByNick(receiver);
		if (!target)
		{
			sendToClient(client, "401 " + client->getNickname() + " " + receiver + " :No such nickname");
			return;
		}
		sendToClient(target, ":" + client->getFullMask() + " PRIVMSG " + receiver + " :" + message);
	}
}

bool Server::channelExists(const std::string &name) const
{
	return _channels.find(name) != _channels.end();
}

Channel *Server::getChannel(const std::string &name)
{
	std::map<std::string, Channel *>::iterator it = _channels.find(name);
	if (it != _channels.end())
		return it->second;
	return NULL;
}

Channel *Server::createChannel(const std::string &name)
{
	if (channelExists(name))
		return getChannel(name);
	Channel *newChannel = new Channel(name);
	_channels[name] = newChannel;
	return newChannel;
}

void Server::removeChannel(const std::string &name)
{
	std::map<std::string, Channel *>::iterator it = _channels.find(name);
	if (it != _channels.end())
	{
		delete it->second;
		_channels.erase(it);
	}
}

void Server::broadcastToChannels(Channel *channel, const std::string &message, Client *sender, bool skipSender)
{
	const std::set<Client *> &clients = channel->getClients();
	for (std::set<Client *>::const_iterator it = clients.begin(); it != clients.end(); ++it)
	{
		Client *client = *it;
		if (client == sender && skipSender)
			continue;
		sendToClient(client, message);
	}
}

void Server::sendReply(Client *client, const std::string &message)
{
	sendToClient(client, ":" + client->getNickname() + " " + message);
}

void Server::sendError(Client *client, const std::string &errorCode, const std::string &errorMsg)
{
	std::string nick = client->getNickname().empty() ? "*" : client->getNickname();
	std::string fullMsg = ":" + std::string("ircserver") + " " + errorCode + " " + nick + " " + errorMsg;

	sendToClient(client, fullMsg);
}

bool Server::isValidChannelName(const std::string &name)
{
	if (name.empty() || name[0] != '#')
		return false;
	if (name.length() > 50)
		return false;
	if (name.find_first_of(" ,:") != std::string::npos)
		return false;
	return true;
}

Client *Server::getClientByNick(const std::string &nickname)
{
	for (std::map<int, Client *>::iterator it = _clients.begin(); it != _clients.end(); ++it)
	{
		if (it->second->getNickname() == nickname)
		{
			return it->second;
		}
	}
	return NULL;
}
