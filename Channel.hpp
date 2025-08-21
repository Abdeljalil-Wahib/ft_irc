#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <string>
#include <set>
#include <map>
#include "Client.hpp"
#include <sstream>

class Channel
{
private:
	std::string _name;
	std::string _topic;
	std::string _key;
	std::set<Client *> _clients;
	std::set<Client *> _invited;
	std::set<Client *> _operators;
	long _userLimit;
	bool _inviteOnly;
	bool _topicProtected;

public:
	Channel(const std::string &name);
	~Channel();

	const std::string &getName() const;
	const std::string &getTopic() const;
	const std::set<Client *> &getClients() const;
	std::string getUserList() const;

	// Client management
	void setTopic(const std::string &topic);
	void addClient(Client *client);
	void removeClient(Client *client);
	bool hasClient(Client *client) const;
	void addInvited(Client *client);
	bool isInvited(Client *client) const;

	// Operator management
	void addOperator(Client *client);
	void removeOperator(Client *client);
	bool isOperator(Client *client) const;

	// Mode setters & Getters
	void setInviteOnly(bool status);
	bool isInviteOnly() const;

	void setTopicProtected(bool status);
	bool isTopicProtected() const;
	bool hasTopic() const;

	void setKey(const std::string &key);
	bool hasKey() const;
	const std::string& getKey() const;

	void setUserLimit(long limit);
	long getUserLimit() const;

	std::string getModes() const;
};
#endif
