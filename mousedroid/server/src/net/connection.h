#ifndef CONNECTION_HPP
#define CONNECTION_HPP

#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <string>

#include <asio.hpp>

#include "input/input.h"
#include "net/device.h"

/// Owns one input TCP stream. Commands are read according to their fixed wire
/// length so TCP fragmentation/coalescing cannot corrupt input events.
class Connection : public std::enable_shared_from_this<Connection> {
    friend class Server;

public:
    static const int MAX_BUFFER_SIZE = 255;

    using tcp = asio::ip::tcp;
    using OnDisconnectedListener = std::function<void(std::shared_ptr<Connection>)>;

    Connection(const INPUT_MANAGER& inputmanager, tcp::socket socket,
               std::string deviceDetails, OnDisconnectedListener onDisconnectedListener);
    ~Connection();

    DeviceInfo GetDeviceInfo() const;

private:
    const INPUT_MANAGER& inputmanager_;
    tcp::socket socket_;
    OnDisconnectedListener onDisconnectedListener_;
    std::array<char, MAX_BUFFER_SIZE> byteBuffer_{};
    DeviceInfo deviceInfo_{};
    std::atomic<bool> started_{false};
    std::atomic<bool> closed_{false};
    std::atomic<bool> stoppedByServer_{false};
    std::atomic<bool> disconnectNotified_{false};
    unsigned char heartbeatReply_ = 0;

    void Start();
    void ReadNextCommand();
    void ReplyToHeartbeat();
    void Close(bool stoppedByServer = false);
    static int PayloadLength(unsigned char type);
};

#endif
