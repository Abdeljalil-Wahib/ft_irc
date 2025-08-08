#include "Channel.hpp"

Channel::Channel(const std::string& name) : _name(name), _topic(""), _inviteOnly(false) {}

Channel::~Channel() {}

const std::string& Channel::getName() const
{
	return _name;
}

const std::string& Channel::getTopic() const
{
	return _topic;
}

void Channel::setTopic(const std::string& topic)
{
	_topic = topic;
}

void Channel::addClient(Client* client)
{
	_clients.insert(client);
}

void Channel::removeClient(Client* client)
{
	_clients.erase(client);
	_operators.erase(client); // remove op status if they leave
}

bool Channel::hasClient(Client* client) const
{
	return _clients.find(client) != _clients.end();
}

void Channel::addOperator(Client* client)
{
	_operators.insert(client);
}

bool Channel::isOperator(Client* client) const
{
	return _operators.find(client) != _operators.end();
}

const std::set<Client*>& Channel::getClients() const
{
	return _clients;
}


void Channel::addInvited(Client *client) {
	_invited.insert(client);
}

bool Channel::isInvited(Client *client) {
	return (_invited.find(client) != _invited.end());
}

std::string Channel::getUserList() const {
    std::string list;
    for (std::set<Client*>::const_iterator it = _clients.begin(); it != _clients.end(); ++it) {
        if (isOperator(*it))
            list += "@" + (*it)->getNickname() + " ";
        else
            list += (*it)->getNickname() + " ";
    }
    if (!list.empty())
        list.erase(list.size() - 1); // remove trailing space
    return list;
}


void Channel::setInviteOnly(bool status) {
	_inviteOnly = status;
}

bool Channel::isInviteOnly() const {
	return _inviteOnly;
}