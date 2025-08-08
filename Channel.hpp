#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <string>
#include <set>
#include <map>
#include "Client.hpp"

class Channel
{
	private:
		std::string _name;
		std::string _topic;
		std::set<Client*> _clients;
		std::set<Client*> _invited;
		std::set<Client*> _operators;

		bool _inviteOnly;
	public:
		Channel(const std::string& name);
		~Channel();

		const std::string& getName() const;
		const std::string& getTopic() const;
		const std::set<Client*>& getClients() const;
		std::string getUserList() const;

		void setTopic(const std::string& topic);
		void addClient(Client* client);
		void removeClient(Client* client);
		bool hasClient(Client* client) const;
		void addOperator(Client* client);
		bool isOperator(Client* client) const;
		void addInvited(Client *client);
		bool isInvited(Client *client);

		void setInviteOnly(bool status);
		bool isInviteOnly() const;
};
#endif
