#include "IOController.h"
#include <sys/select.h>
#include <sys/time.h>
#include <string.h>
#include <memory>
/**
 * @brief Construct a new TCP::IOController server object
 * 
 * @param address_info address info for the listener socket
 * @param recv_message_queue incomming msg queue
 * @param send_message_queue outgoing msg queue
 * @param backlog amount of pending connections
 * @param max_retries maximum amount of send retries
 * 
 * @exception Socket::Error
 */
TCP::IOController::IOController(AddressInfo &address_info,
                                std::queue<TCP::Message> &recv_message_queue,
                                std::queue<TCP::Message> &send_message_queue,
                                int backlog,
                                int max_retries) :
                                recv_message_queue_(recv_message_queue),
                                send_message_queue_(send_message_queue),
                                max_retries_(max_retries),
                                type_(kServer)
{
    main_socket_ = std::make_shared<Socket>();
    main_socket_->Listen(address_info, backlog);
    Init();
}

/**
 * @brief Construct a new TCP::IOController client object
 * 
 * @param client_socket client socket object
 * @param recv_message_queue incomming msg queue
 * @param send_message_queue outgoing msg queue
 * @param max_retries maximum amount of send retries
 */
TCP::IOController::IOController(std::shared_ptr<Socket> client_socket,
                                std::queue<TCP::Message> &recv_message_queue,
                                std::queue<TCP::Message> &send_message_queue,
                                int max_retries) :
                                recv_message_queue_(recv_message_queue),
                                send_message_queue_(send_message_queue),
                                max_retries_(max_retries),
                                type_(kClient)
{
    main_socket_ = client_socket;
    Init();
}

TCP::IOController::~IOController() {}

auto TCP::IOController::Init() -> void
{
    std::cout << *main_socket_ << std::endl;

    FD_ZERO(&master_fd_list_);
    FD_SET(main_socket_->GetFD(), &master_fd_list_);
    max_fd_ = main_socket_->GetFD();
    sockets_.insert(std::make_pair(main_socket_->GetFD(), main_socket_));
}


/**
 * @brief Accepts new connections and sends/receives messages
 * 
 * @exception TCP::IOController::Error when select fails
 * @exception TCP::IOController::Error when main socket closes
 * @param timeout in seconds
 */
auto TCP::IOController::RunOnce() -> void
{
    /* Set time_val to 0 so select doesn't block */
    struct timeval time_val = {0, 0};
    
    fd_set read_fds;
    fd_set write_fds;
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    read_fds = master_fd_list_;
    write_fds = master_fd_list_;

    int total_ready_fds = select(max_fd_ + 1, &read_fds, &write_fds, NULL, &time_val);
    if (total_ready_fds == -1)
        throw TCP::IOController::Error(strerror(errno));

    if (total_ready_fds == 0)
        return ;

    HandleSendQueue(&write_fds);
    HandleRecvQueue(&read_fds);
}

auto TCP::IOController::HandleSendQueue(fd_set *write_fds) -> void
{
    for (size_t message_count = send_message_queue_.size(); message_count > 0; message_count--)
    {
        auto message = send_message_queue_.front();
        try {
            SendMessage(message, write_fds);
        }
        catch (TCP::IOController::FailedToSend &ex)
        {
            std::cerr << "Failed to send message: " << ex.what();
            std::cerr << " Retries: " << message.GetRetries() << std::endl;
            if (message.GetRetries() < max_retries_)
            {
                message.Retry();
                send_message_queue_.push(std::move(message));
            }
        }
        send_message_queue_.pop();
    };
}

/**
 * @brief Validate that a socket is still ok to use and return a mutable reference
 * 
 * @param socket 
 * @return Socket& 
 */
auto TCP::IOController::ValidateSocket(std::shared_ptr<const Socket> socket) -> std::shared_ptr<Socket>
{
    int socket_fd = socket->GetFD();

    if (FD_ISSET(socket_fd, &master_fd_list_) == false)
        throw TCP::IOController::InvalidSocket("fd unknown");
    

    auto mutable_socket = sockets_.find(socket_fd);
    if (mutable_socket->second == socket)
        return mutable_socket->second;
    else
        throw TCP::IOController::InvalidSocket("fd unknown");
}

auto TCP::IOController::SendMessage(TCP::Message &message, fd_set *write_fds) -> void
{
    try {
        std::shared_ptr<Socket> socket = ValidateSocket(message.GetSocket());
        if (FD_ISSET(message.GetFD(), write_fds))
            socket->Send(*message.GetData());
        else
            throw TCP::IOController::FailedToSend("Socket not ready for writing");
    }
    catch (TCP::IOController::InvalidSocket &ex)
    {
        throw TCP::IOController::FailedToSend(ex.what());
    }
    catch (TCP::Socket::Closed &ex)
    {
        DeleteSocket(message.GetFD());
        throw TCP::IOController::FailedToSend(ex.what());
    }
    catch (TCP::Socket::WouldBlock &ex)
    {
        throw TCP::IOController::FailedToSend(ex.what());
    }
    catch (TCP::Socket::Error &ex)
    {
        DeleteSocket(message.GetFD());
        throw TCP::IOController::FailedToSend(ex.what());
    }
}

auto TCP::IOController::HandleRecvQueue(fd_set *read_fds) -> void
{
    int i = 0;
    while (i <= max_fd_)
    {
        if (FD_ISSET(i, read_fds))
        {
            if (type_ == kServer && i == main_socket_->GetFD())
                AcceptNewConnection();
            else
                ReadFromSocket(i);
        }
        i++;
    }
}

auto TCP::IOController::AcceptNewConnection() -> void
{
    auto new_socket = std::make_shared<Socket>();

    try {
        new_socket->Accept(main_socket_->GetFD());
    }
    catch (TCP::Socket::Error &ex)
    {
        std::cerr << "Could not accept new connection: " << ex.what() << std::endl;
        return ; 
    }

    FD_SET(new_socket->GetFD(), &master_fd_list_);
    if (new_socket->GetFD() > max_fd_)
        max_fd_ = new_socket->GetFD();

    sockets_.insert(std::make_pair(new_socket->GetFD(), new_socket));

    recv_message_queue_.push(TCP::Message(new_socket, ""));
}

auto TCP::IOController::ReadFromSocket(int socket_fd) -> void
{
    auto socket = sockets_.find(socket_fd);
    try {
        std::string data = socket->second->Recv();
        recv_message_queue_.push(TCP::Message(socket->second, std::move(data)));
    }
    catch (TCP::Socket::WouldBlock &ex)
    {
        std::cerr << "Read failed: " << ex.what() << std::endl;
    }
    catch (TCP::Socket::Error &ex)
    {
        std::cerr << "Read failed: " << ex.what() << std::endl;
        DeleteSocket(socket_fd);
    }
}

auto TCP::IOController::DeleteSocket(int socket_fd) -> void
{
    /* Notify user with an empty message */
    auto socket = sockets_.find(socket_fd);

    if (socket->second == main_socket_)
        throw TCP::IOController::Error("Main socket closed unexpectedly");

    recv_message_queue_.push(TCP::Message(socket->second, ""));

    FD_CLR(socket_fd, &master_fd_list_);

    /* Lower max_fd when we delete the previous highest fd */
    if (socket_fd == max_fd_)
    {
        while (FD_ISSET(max_fd_, &master_fd_list_) == false && max_fd_ > 0)
            max_fd_--;
    }
    sockets_.erase(socket_fd);    
}