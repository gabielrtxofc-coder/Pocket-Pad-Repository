#ifndef SERVER_HPP
#define SERVER_HPP

#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include <asio.hpp>

#include "adb/AdbCoordinator.h"
#include "adb/AdbMonitor.h"
#include "connection.h"
#include "input/input.h"
#include "net/device.h"

/// Loopback-only server for the custom input protocol. USB access is provided
/// by a serial-scoped ADB reverse rule; network clients are intentionally not
/// exposed until an authenticated pairing protocol is implemented.
class Server : public std::enable_shared_from_this<Server> {
public:
    using tcp = asio::ip::tcp;
    using udp = asio::ip::udp;
    using HostInfo = std::tuple<std::string, std::string, std::string>;

    struct ConnectionListener {
        virtual ~ConnectionListener() = default;
        virtual void OnDeviceConnected(std::string device) = 0;
        virtual void OnDeviceDisconnected(std::string device) = 0;
        virtual void OnAdbStateChanged(const PocketPad::Adb::Snapshot& state) = 0;
    };

    Server(int port, ConnectionListener& listener, const INPUT_MANAGER& inputmanager);
    ~Server();

    HostInfo GetHostInfo() const;
    void Start();
    void Close();
    void SetPreferredAdbSerial(std::string serial);
    std::vector<DeviceInfo> GetConnectedDevicesInfo() const;

private:
    int port_;
    const INPUT_MANAGER& inputmanager_;
    ConnectionListener& connectionListener_;
    PocketPad::Adb::Monitor adbMonitor_;

    asio::io_context context_;
    tcp::acceptor acceptor_;
    udp::socket udpSocket_;
    udp::endpoint remoteEndpoint_;
    std::array<char, Connection::MAX_BUFFER_SIZE> udpBuffer_{};

    std::thread thread_;
    std::atomic<bool> started_{false};
    std::atomic<bool> closed_{false};
    mutable std::mutex connectionsMutex_;
    std::vector<std::shared_ptr<Connection>> connections_;

    struct PendingClient;
    void TCPDoAccept();
    void ReadDeviceHandshake(const std::shared_ptr<PendingClient>& pending);
    void StartReceive();
    void OnConnectionDisconnected(std::shared_ptr<Connection> connection);
};

#endif
