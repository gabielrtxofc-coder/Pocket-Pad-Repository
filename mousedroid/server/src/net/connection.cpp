#include "net/connection.h"

#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

Connection::Connection(const INPUT_MANAGER& inputmanager, tcp::socket socket,
                       std::string deviceDetails,
                       OnDisconnectedListener onDisconnectedListener)
    : inputmanager_(inputmanager),
      socket_(std::move(socket)),
      onDisconnectedListener_(std::move(onDisconnectedListener)) {
    std::stringstream stream(deviceDetails);
    std::vector<std::string> tokens;
    std::string token;
    while (std::getline(stream, token, '/')) tokens.push_back(token);
    if (tokens.size() != 4) throw std::invalid_argument("device details must contain four fields");

    deviceInfo_.Manufacturer = tokens[0];
    deviceInfo_.Name = tokens[1].empty() ? "Android device" : tokens[1];
    deviceInfo_.Model = tokens[2];
    try {
        deviceInfo_.ctype = std::stoi(tokens[3]);
    } catch (const std::exception&) {
        throw std::invalid_argument("device connection type is invalid");
    }

    asio::error_code endpointError;
    const auto endpoint = socket_.remote_endpoint(endpointError);
    if (endpointError) throw std::runtime_error("could not read client endpoint");
    deviceInfo_.Address = endpoint.address().to_string();
}

Connection::~Connection() {
    Close(true);
}

DeviceInfo Connection::GetDeviceInfo() const {
    return deviceInfo_;
}

void Connection::Start() {
    if (closed_.load() || started_.exchange(true)) return;
    ReadNextCommand();
}

int Connection::PayloadLength(unsigned char type) {
    switch (type) {
        case 0x01:  // left click
        case 0x02:  // right click
        case 0x03:  // drag down
        case 0x04:  // drag up
            return 0;
        case 0x05:  // relative movement: dx, dy
        case 0x07:  // key press: keycode, modifier
            return 2;
        case 0x06:  // vertical scroll
        case 0x08:  // horizontal scroll
        case 0x09:  // zoom
        case 0x0A:  // media action
            return 1;
        default:
            return -1;
    }
}

void Connection::ReadNextCommand() {
    if (closed_.load()) return;
    const auto self = shared_from_this();
    asio::async_read(socket_, asio::buffer(byteBuffer_.data(), 1),
        [self](const asio::error_code& error, std::size_t) {
            if (error) {
                self->Close();
                return;
            }

            const auto type = static_cast<unsigned char>(self->byteBuffer_[0]);
            if (type == 0x00) {
                self->ReplyToHeartbeat();
                return;
            }

            const int payloadLength = PayloadLength(type);
            if (payloadLength < 0) {
                self->ReadNextCommand();
                return;
            }
            if (payloadLength == 0) {
                try {
                    self->inputmanager_.execute(self->byteBuffer_.data(), 1);
                } catch (const std::exception&) {
                    self->Close();
                    return;
                }
                self->ReadNextCommand();
                return;
            }

            asio::async_read(self->socket_,
                asio::buffer(self->byteBuffer_.data() + 1, static_cast<std::size_t>(payloadLength)),
                [self, payloadLength](const asio::error_code& payloadError, std::size_t) {
                    if (payloadError) {
                        self->Close();
                        return;
                    }
                    try {
                        self->inputmanager_.execute(self->byteBuffer_.data(),
                                                    static_cast<std::size_t>(payloadLength + 1));
                    } catch (const std::exception&) {
                        self->Close();
                        return;
                    }
                    self->ReadNextCommand();
                });
        });
}

void Connection::ReplyToHeartbeat() {
    if (closed_.load()) return;
    const auto self = shared_from_this();
    asio::async_write(socket_, asio::buffer(&heartbeatReply_, sizeof(heartbeatReply_)),
        [self](const asio::error_code& error, std::size_t) {
            if (error) {
                self->Close();
                return;
            }
            self->ReadNextCommand();
        });
}

void Connection::Close(bool stoppedByServer) {
    if (stoppedByServer) stoppedByServer_.store(true);
    if (closed_.exchange(true)) return;

    asio::error_code ignored;
    socket_.cancel(ignored);
    socket_.shutdown(tcp::socket::shutdown_both, ignored);
    socket_.close(ignored);

    if (!stoppedByServer_.load() && !disconnectNotified_.exchange(true) && onDisconnectedListener_) {
        onDisconnectedListener_(shared_from_this());
    }
}
