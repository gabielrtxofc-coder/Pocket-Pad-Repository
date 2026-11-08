#include "net/server.h"

#include "logger.h"

#include <algorithm>
#include <functional>
#include <utility>

struct Server::PendingClient {
    explicit PendingClient(tcp::socket acceptedSocket)
        : socket(std::move(acceptedSocket)) {}

    tcp::socket socket;
    std::array<unsigned char, 2> lengthBuffer{};
    std::string deviceDetails;
};

Server::Server(int port, ConnectionListener& listener, const INPUT_MANAGER& inputmanager)
    : port_(port),
      inputmanager_(inputmanager),
      connectionListener_(listener),
      adbMonitor_([this](const PocketPad::Adb::Snapshot& state) {
          connectionListener_.OnAdbStateChanged(state);
      }),
      acceptor_(context_),
      udpSocket_(context_) {
    const auto loopback = asio::ip::address_v4::loopback();

    const tcp::endpoint tcpEndpoint(loopback, static_cast<unsigned short>(port_));
    acceptor_.open(tcpEndpoint.protocol());
    acceptor_.set_option(tcp::acceptor::reuse_address(true));
    acceptor_.bind(tcpEndpoint);
    acceptor_.listen();

    udpSocket_.open(udp::v4());
    udpSocket_.bind(udp::endpoint(loopback, static_cast<unsigned short>(port_)));
}

Server::~Server() {
    Close();
}

Server::HostInfo Server::GetHostInfo() const {
    const std::string name = asio::ip::host_name();
    const std::string endpoint = "127.0.0.1:" + std::to_string(port_) + " (USB/ADB only)";
    return {name, endpoint, std::to_string(port_)};
}

void Server::Start() {
    if (closed_.load() || started_.exchange(true)) return;
    try {
        TCPDoAccept();
        StartReceive();
        thread_ = std::thread([this] {
            try {
                context_.run();
            } catch (const std::exception& error) {
                LOG("[SERVER] I/O loop stopped: ", error.what());
            }
        });
        adbMonitor_.Start();
        LOG("[SERVER] Started; TCP/UDP listeners are loopback-only.\n");
    } catch (const std::exception& error) {
        started_.store(false);
        LOG("[SERVER] Start failed: ", error.what(), "\n");
    }
}

void Server::Close() {
    if (closed_.exchange(true)) return;

    LOG("[SERVER] Closing...\n");
    adbMonitor_.Stop();

    asio::error_code error;
    acceptor_.close(error);
    if (error) LOG("[SERVER] Error closing acceptor: ", error.message(), "\n");
    udpSocket_.close(error);

    std::vector<std::shared_ptr<Connection>> connections;
    {
        std::lock_guard<std::mutex> lock(connectionsMutex_);
        connections.swap(connections_);
    }
    for (const auto& connection : connections) connection->Close(true);

    context_.stop();
    if (thread_.joinable()) thread_.join();
    LOG("[SERVER] Closed. The shared ADB server was left running.\n");
}

void Server::SetPreferredAdbSerial(std::string serial) {
    adbMonitor_.SetPreferredSerial(std::move(serial));
}

std::vector<DeviceInfo> Server::GetConnectedDevicesInfo() const {
    std::vector<DeviceInfo> information;
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    information.reserve(connections_.size());
    for (const auto& connection : connections_) information.push_back(connection->GetDeviceInfo());
    return information;
}

void Server::TCPDoAccept() {
    if (closed_.load()) return;
    acceptor_.async_accept([this](const asio::error_code& error, tcp::socket socket) {
        if (!error && !closed_.load()) {
            asio::error_code endpointError;
            const auto endpoint = socket.remote_endpoint(endpointError);
            if (!endpointError) LOG("[SERVER] USB client connected from ", endpoint, "\n");
            ReadDeviceHandshake(std::make_shared<PendingClient>(std::move(socket)));
        }
        if (!closed_.load()) TCPDoAccept();
    });
}

void Server::ReadDeviceHandshake(const std::shared_ptr<PendingClient>& pending) {
    asio::async_read(pending->socket, asio::buffer(pending->lengthBuffer),
        [this, pending](const asio::error_code& headerError, std::size_t) {
            if (headerError || closed_.load()) return;

            const std::size_t length =
                (static_cast<std::size_t>(pending->lengthBuffer[0]) << 8) |
                static_cast<std::size_t>(pending->lengthBuffer[1]);
            if (length == 0 || length > 127) {
                asio::error_code ignored;
                pending->socket.close(ignored);
                LOG("[SERVER] Rejected invalid device handshake length.\n");
                return;
            }

            pending->deviceDetails.resize(length);
            asio::async_read(pending->socket, asio::buffer(pending->deviceDetails),
                [this, pending](const asio::error_code& detailsError, std::size_t) {
                    if (detailsError || closed_.load()) return;
                    try {
                        auto connection = std::make_shared<Connection>(
                            inputmanager_, std::move(pending->socket), pending->deviceDetails,
                            [this](std::shared_ptr<Connection> disconnected) {
                                OnConnectionDisconnected(std::move(disconnected));
                            });
                        const DeviceInfo device = connection->GetDeviceInfo();
                        {
                            std::lock_guard<std::mutex> lock(connectionsMutex_);
                            if (closed_.load()) {
                                connection->Close(true);
                                return;
                            }
                            connections_.push_back(connection);
                        }
                        connection->Start();
                        connectionListener_.OnDeviceConnected(device.Name);
                        LOG("[SERVER] Device registered: ", device.Name, "\n");
                    } catch (const std::exception& error) {
                        LOG("[SERVER] Invalid device handshake: ", error.what(), "\n");
                        asio::error_code ignored;
                        pending->socket.close(ignored);
                    }
                });
        });
}

void Server::StartReceive() {
    if (closed_.load()) return;
    udpSocket_.async_receive_from(asio::buffer(udpBuffer_), remoteEndpoint_,
        [this](const asio::error_code& error, std::size_t bytesReceived) {
            if (!error && bytesReceived > 0 && !closed_.load()) {
                inputmanager_.execute(udpBuffer_.data(), bytesReceived);
            }
            if (!closed_.load()) StartReceive();
        });
}

void Server::OnConnectionDisconnected(std::shared_ptr<Connection> connection) {
    bool removed = false;
    {
        std::lock_guard<std::mutex> lock(connectionsMutex_);
        const auto found = std::find(connections_.begin(), connections_.end(), connection);
        if (found != connections_.end()) {
            connections_.erase(found);
            removed = true;
        }
    }
    if (removed && !closed_.load()) {
        const DeviceInfo device = connection->GetDeviceInfo();
        LOG("[SERVER] Device disconnected: ", device.Name, "\n");
        connectionListener_.OnDeviceDisconnected(device.Name);
    }
}
