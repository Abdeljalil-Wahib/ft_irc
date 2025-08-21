#include "Client.hpp"

Client::Client(int fd) : _fd(fd), _hasSentPass(false), _hasSentNick(false),
	  _hasSentUser(false), _isRegistered(false) {}

Client::~Client() {}

int Client::getFd() const
{
	return _fd;
}

const std::string &Client::getNickname() const
{
	return _nickname;
}

const std::string &Client::getUsername() const
{
	return _username;
}

bool Client::isRegistered() const
{
	return _isRegistered;
}

bool Client::hasSentPass() const
{
	return _hasSentPass;
}

bool Client::hasSentNick() const
{
	return _hasSentNick;
}

bool Client::hasSentUser() const
{
	return _hasSentUser;
}

void Client::setNickname(const std::string &nick)
{
	_nickname = nick;
	_hasSentNick = true;
}

void Client::setUsername(const std::string &user)
{
	_username = user;
	_hasSentUser = true;
}

void Client::setPassAccepted(bool ok)
{
	_hasSentPass = ok;
}

void Client::markRegistered()
{
	_isRegistered = true;
}

std::string Client::getFullMask() const {
	return _nickname + "!" + _username + "@" + "ircserver";
}

const std::string& Client::getBuffer() const
{
    return _buffer;
}

void Client::setBuffer(const std::string& buffer)
{
    _buffer = buffer;
}