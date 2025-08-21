#include "Channel.hpp"

Channel::Channel(const std::string &name) : _name(name),
                                            _topic(""),
                                            _key(""),
                                            _userLimit(0),
                                            _inviteOnly(false),
                                            _topicProtected(true)
{
}

Channel::~Channel() {}

const std::string &Channel::getName() const
{
    return _name;
}

const std::string &Channel::getTopic() const
{
    return _topic;
}

void Channel::setTopic(const std::string &topic)
{
    _topic = topic;
}

bool Channel::hasTopic() const
{
    return !_topic.empty();
}

void Channel::addClient(Client *client)
{
    _clients.insert(client);
}

void Channel::removeClient(Client *client)
{
    _clients.erase(client);
    _operators.erase(client);
}

bool Channel::hasClient(Client *client) const
{
    return _clients.find(client) != _clients.end();
}

void Channel::addOperator(Client *client)
{
    _operators.insert(client);
}

bool Channel::isOperator(Client *client) const
{
    return _operators.find(client) != _operators.end();
}

const std::set<Client *> &Channel::getClients() const
{
    return _clients;
}

void Channel::addInvited(Client *client)
{
    _invited.insert(client);
}

bool Channel::isInvited(Client *client) const
{
    return (_invited.find(client) != _invited.end());
}

std::string Channel::getUserList() const
{
    std::string list;
    for (std::set<Client *>::const_iterator it = _clients.begin(); it != _clients.end(); ++it)
    {
        if (isOperator(*it))
            list += "@" + (*it)->getNickname() + " ";
        else
            list += (*it)->getNickname() + " ";
    }
    if (!list.empty())
        list.erase(list.size() - 1); // remove trailing space
    return list;
}

void Channel::setInviteOnly(bool status)
{
    _inviteOnly = status;
}

bool Channel::isInviteOnly() const
{
    return _inviteOnly;
}

void Channel::setTopicProtected(bool status)
{
    _topicProtected = status;
}

bool Channel::isTopicProtected() const
{
    return _topicProtected;
}

void Channel::removeOperator(Client *client)
{
    _operators.erase(client);
}

void Channel::setKey(const std::string &key)
{
    _key = key;
}

bool Channel::hasKey() const
{
    return !_key.empty();
}

const std::string &Channel::getKey() const
{
    return _key;
}

void Channel::setUserLimit(long limit)
{
    _userLimit = limit;
}

long Channel::getUserLimit() const
{
    return _userLimit;
}

std::string Channel::getModes() const
{
    std::string modes = "+";
    std::string params = "";

    if (_inviteOnly)
        modes += 'i';
    if (_topicProtected)
        modes += 't';

    if (_userLimit > 0)
    {
        modes += 'l';
        std::ostringstream oss;
        oss << _userLimit;
        params += " " + oss.str();
    }
    if (modes.length() == 1)
        return "";
    return modes + params;
}