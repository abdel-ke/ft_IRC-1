#ifndef __SOCKET_H__
#define __SOCKET_H__

#include "AddressInfo.h"

#include <memory>

#define DEFAULT_BACKLOG 20
#define BLOCKING true
#define NON_BLOCKING false

namespace IRC::TCP
{
    enum SocketState
    {
        kUnInitialized = -1,
        kConnected,
        kDisconnected,
		kReadyToRead
    };

	enum SocketType
	{
		kUnknown = -1,
		kClientSocket = 0,
		kListenerSocket = 1
	};

    class Socket
    {
    private:
        int socket_fd_;
		SocketType type_;
        SocketState state_;
        socklen_t address_size_;
        struct sockaddr_storage address_;

        auto InitSocket(struct addrinfo *addr_info, bool block) -> int;
        auto Clear() -> void;
        auto GetSockAddrIn(struct sockaddr *address) const -> void *;

    public:
        Socket();
        ~Socket();

        auto Listen(AddressInfo &address_info,
                    int backlog = DEFAULT_BACKLOG,
                    bool block = NON_BLOCKING) -> void;

        auto Connect(AddressInfo &address_info, bool block = BLOCKING) -> void;
		auto Close() -> void;

        auto Recv() -> std::string;
        auto Accept(int listener_fd) -> void;
        auto Send(const std::string &data) -> void;

        auto GetFD() const -> int { return socket_fd_; };

		auto GetType() const -> int { return type_; };

        auto GetState() const -> int { return state_; };
		auto SetState(SocketState state) -> void { this->state_ = state; }

        class Error : public std::runtime_error
        {
        public:
            Error(const char *msg) : std::runtime_error(msg) {}
        };

        class WouldBlock : public Error
        {
            public:
            WouldBlock() : Error("Socket would block") {}
        };

        class Closed : public Error
        {
            public:
            Closed() : Error("Socket closed") {}
        };
        friend auto operator<<(std::ostream& os, const Socket& socket) -> std::ostream &;

        Socket (Socket& other) = delete;
        Socket &operator =(Socket& other) = delete;
        Socket (Socket&& other) = delete;
        Socket &operator= (Socket&& other) = delete;
    };

    auto operator<<(std::ostream& os, const Socket& socket) -> std::ostream &;
} // namespace IRC::TCP

#endif