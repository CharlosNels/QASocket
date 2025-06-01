#include "myudpsocket.h"
#include "mytcpsocket.h"

using namespace asio;

MyUdpSocket::MyUdpSocket(quint64 read_buffer_size, QObject *parent)
    : QIODevice{parent}, m_read_buffer_size(read_buffer_size)
{
    m_asio_socket = std::make_shared<ip::udp::socket>(*MyTcpSocket::MyIOContext::getIOContext());
    QIODevice::open(QIODevice::ReadWrite);
    setReadBufferSize(read_buffer_size);
}

void MyUdpSocket::setReadBufferSize(quint64 buf_size) {
    m_read_buffer_size = buf_size;
    m_asio_read_buf = std::make_unique<char[]>(buf_size);
}

bool MyUdpSocket::connectTo(QHostAddress host, quint16 port)
{
    std::error_code ec;
    m_remote_ep = ip::udp::endpoint(ip::address::from_string(host.toString().toStdString()), port);
    m_asio_socket->open(m_remote_ep.protocol(), ec);
    if(ec)
    {
        m_error_info = QString::fromStdString(ec.message());
        emit socketErrorOccurred();
        return false;
    }
    m_asio_socket->connect(m_remote_ep, ec);
    if(ec)
    {
        m_error_info = QString::fromStdString(ec.message());
        emit socketErrorOccurred();
        return false;
    }
    m_asio_socket->async_receive_from(
        buffer(m_asio_read_buf.get(), m_read_buffer_size), m_remote_ep,
        std::bind(&MyUdpSocket::asyncReceiveCallback, this, std::placeholders::_1, std::placeholders::_2));
    return true;
}

bool MyUdpSocket::bind(const QHostAddress &address, quint16 port)
{
    std::error_code ec;
    m_local_ep = ip::udp::endpoint(ip::address::from_string(address.toString().toStdString()), port);
    m_asio_socket->open(m_local_ep.protocol(), ec);
    if(ec)
    {
        m_error_info = QString::fromStdString(ec.message());
        emit socketErrorOccurred();
        return false;
    }
    m_asio_socket->bind(m_local_ep, ec);
    if(ec)
    {
        m_error_info = QString::fromStdString(ec.message());
        emit socketErrorOccurred();
        return false;
    }
    m_asio_socket->async_receive_from(
        buffer(m_asio_read_buf.get(), m_read_buffer_size), m_remote_ep,
        std::bind(&MyUdpSocket::asyncReceiveCallback, this, std::placeholders::_1, std::placeholders::_2));
    return true;
}

QString MyUdpSocket::getErrorString() const
{
    return m_error_info;
}

bool MyUdpSocket::isSequential() const { return true; }

void MyUdpSocket::close() {
    std::unique_lock<std::mutex> lock(m_socket_mutex);
    if (m_asio_socket->is_open()) {
        std::error_code ec;
        m_asio_socket->shutdown(ip::tcp::socket::shutdown_type::shutdown_both, ec);
        if(ec)
        {
            m_error_info = QString::fromStdString(ec.message());
            emit socketErrorOccurred();
        }
        m_asio_socket->close(ec);
        if(ec)
        {
            m_error_info = QString::fromStdString(ec.message());
            emit socketErrorOccurred();
        }
    }
}

qint64 MyUdpSocket::bytesAvailable() const { return m_recv_buffer.size(); }

qint64 MyUdpSocket::bytesToWrite() const { return 0; }

qint64 MyUdpSocket::readData(char *data, qint64 maxlen) {
    std::unique_lock<std::mutex> lock(m_socket_mutex);
    maxlen = maxlen == 0 ? UINT64_MAX : maxlen;
    int read_size = m_recv_buffer.size() <= maxlen ? m_recv_buffer.size() : maxlen;
    memcpy(data, m_recv_buffer.data(), read_size);
    m_recv_buffer.remove(0, read_size);

    return read_size;
}

qint64 MyUdpSocket::readLineData(char *data, qint64 maxlen) {
    std::unique_lock<std::mutex> lock(m_socket_mutex);
    maxlen = maxlen == 0 ? UINT64_MAX : maxlen;
    int read_size = 0;
    if (m_recv_buffer.contains('\n')) {
        int line_size = m_recv_buffer.indexOf('\n') + 1;
        read_size = line_size <= maxlen ? line_size : maxlen;
    } else {
        read_size = m_recv_buffer.size() <= maxlen ? m_recv_buffer.size() : maxlen;
    }
    memcpy(data, m_recv_buffer.data(), read_size);
    m_recv_buffer.remove(0, read_size);

    return read_size;
}

qint64 MyUdpSocket::writeData(const char *data, qint64 len) {
    std::unique_lock<std::mutex> lock(m_socket_mutex);
    m_asio_socket->async_send_to(
        const_buffer(data, len), m_remote_ep,
        std::bind(&MyUdpSocket::asyncSendCallback, this, std::placeholders::_1, std::placeholders::_2));

    return len;
}

void MyUdpSocket::asyncSendCallback(const asio::error_code &ec, size_t size) {
    if (ec) {
        m_error_info = QString::fromStdString(ec.message());
        emit socketErrorOccurred();
    }
}

void MyUdpSocket::asyncReceiveCallback(const asio::error_code &ec, size_t size) {
    if (ec) {
        m_error_info = QString::fromStdString(ec.message());
        emit socketErrorOccurred();
    } else {
        std::unique_lock<std::mutex> lock(m_socket_mutex);
        m_recv_buffer.append(m_asio_read_buf.get(), size);
        emit readyRead();
        m_asio_socket->async_receive_from(
            buffer(m_asio_read_buf.get(), m_read_buffer_size), m_remote_ep,
            std::bind(&MyUdpSocket::asyncReceiveCallback, this, std::placeholders::_1, std::placeholders::_2));
    }
}
