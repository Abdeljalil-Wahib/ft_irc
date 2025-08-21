#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <vector>
#include <poll.h>
#include "Client.hpp"
#include "Channel.hpp"
#include "OperatorCommands.hpp"
#include <map>

class Server
{
private:
	int _port;									// Port number to listen on
	std::string _password;						// Connection password
	int _serverSocket;							// File descriptor for the main listening socket
	std::vector<struct pollfd> _pollFds;		// List of file descriptors to poll
	std::map<int, Client *> _clients;			// fd -> Client * (Client pointer for each connected client)
	std::map<std::string, Channel *> _channels; // channel name -> Channel*

	void handleNewConnection();
	void handleClientData(int clientFd);
	void handleCommand(Client *client, const std::string &line);
	void handlePassCommand(Client *client, const std::string &args);
	void handleNickCommand(Client *client, const std::string &args);
	void handleUserCommand(Client *client, const std::string &args);

	// NEW
	void handleJoinCommand(Client *client, const std::string &args);
	void handlePrivMsgCommand(Client *client, const std::string &args);

	public:

	// Helpers
	void sendToClient(Client *client, const std::string &message);
	bool isNicknameInUse(const std::string &nick);
	void checkRegistration(Client *client);
	Client *getClientByNick(const std::string &nickname);
	Channel *getChannel(const std::string &name);
	Channel *createChannel(const std::string &name);
	bool isValidChannelName(const std::string &name);
	bool channelExists(const std::string &name) const;
	void sendReply(Client *, const std::string &reply);
	void sendError(Client *, const std::string &code, const std::string &err);
	void removeChannel(const std::string &name);
	void broadcastToChannels(Channel *channel, const std::string &message, Client *sender, bool skipSender);


	Server(int port, const std::string &password);
	~Server();
	void start(); // Starts the server (binds, listens, etc.)
};

#endif