#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>

class Client
{
	private:
		int _fd;
		std::string _nickname;
		std::string _username;
		std::string _hostname;
		bool _hasSentPass;
		bool _hasSentNick;
		bool _hasSentUser;
		bool _isRegistered;

	public:
		Client(int fd);
		~Client();

		int getFd() const;
		const std::string &getNickname() const;
		const std::string &getUsername() const;
		bool isRegistered() const;
		void setNickname(const std::string &nick);
		void setUsername(const std::string &user);
		void setPassAccepted(bool ok);
		void markRegistered();
		bool hasSentPass() const;
		bool hasSentNick() const;
		bool hasSentUser() const;
		std::string getFullMask() const;
};

#endif
