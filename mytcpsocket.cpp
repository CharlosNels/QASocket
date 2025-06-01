#include "mytcpsocket.h"
#include <QDebug>
#include <QHostAddress>

using namespace asio;

asio::io_context *MyTcpSocket::MyIOContext::io_context = nullptr;
std::thread *MyTcpSocket::MyIOContext::io_thread = nullptr;
std::mutex MyTcpSocket::MyIOContext::io_mutex;

MyTcpSocket::MyTcpSocket(socket_ptr sock_ptr, quint64 read_buffer_size) : QIODevice(nullptr)
{
    m_asio_socket = sock_ptr;
    setReadBufferSize(read_buffer_size);
    QIODevice::open(QIODevice::ReadWrite);
    m_asio_socket->async_read_some(
        buffer(m_asio_read_buf.get(), m_read_buffer_size),
        std::bind(&MyTcpSocket::asyncReadCallback, this, std::placeholders::_1, std::placeholders::_2));
}

MyTcpSocket::MyTcpSocket(quint64 read_buffer_size, QObject *parent) : QIODevice(parent)
{
    m_asio_socket = std::make_shared<ip::tcp::socket>(*MyIOContext::getIOContext());
    setReadBufferSize(read_buffer_size);
}

MyTcpSocket::~MyTcpSocket()
{
    std::unique_lock<std::mutex> lock(m_socket_mutex);
    if (m_asio_socket->is_open()) {
        std::error_code ec;
        m_asio_socket->shutdown(ip::tcp::socket::shutdown_type::shutdown_both, ec);
        m_asio_socket->close(ec);
        if(ec)
        {
            qDebug() << ec.message();
        }
    }
}

bool MyTcpSocket::isSequential() const { return true; }

void MyTcpSocket::close()
{
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

qint64 MyTcpSocket::bytesAvailable() const { return m_recv_buffer.size(); }

qint64 MyTcpSocket::bytesToWrite() const { return 0; }

bool MyTcpSocket::waitForReadyRead(int msecs) { return false; }

bool MyTcpSocket::waitForBytesWritten(int msecs) { return false; }

qint64 MyTcpSocket::readData(char *data, qint64 maxlen)
{
    std::unique_lock<std::mutex> lock(m_socket_mutex);
    maxlen = maxlen == 0 ? UINT64_MAX : maxlen;
    int read_size = m_recv_buffer.size() <= maxlen ? m_recv_buffer.size() : maxlen;
    memcpy(data, m_recv_buffer.data(), read_size);
    m_recv_buffer.remove(0, read_size);

    return read_size;
}

qint64 MyTcpSocket::readLineData(char *data, qint64 maxlen)
{
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

qint64 MyTcpSocket::writeData(const char *data, qint64 len)
{
    std::unique_lock<std::mutex> lock(m_socket_mutex);
    m_asio_socket->async_write_some(const_buffer(data, len), std::bind(&MyTcpSocket::asyncWriteCallback, this,
                                                                       std::placeholders::_1, std::placeholders::_2));

    return len;
}

void MyTcpSocket::asyncConnectCallback(const asio::error_code &ec)
{
    if (ec) {
        m_error_info = QString::fromStdString(ec.message());
        emit socketErrorOccurred();
        emit connectFinished(false);
    } else {
        std::unique_lock<std::mutex> lock(m_socket_mutex);
        QIODevice::open(QIODevice::ReadWrite);
        emit connectFinished(true);
        m_asio_socket->async_read_some(
            buffer(m_asio_read_buf.get(), m_read_buffer_size),
            std::bind(&MyTcpSocket::asyncReadCallback, this, std::placeholders::_1, std::placeholders::_2));
    }
}

void MyTcpSocket::setReadBufferSize(quint64 buf_size)
{
    std::unique_lock<std::mutex> lock(m_socket_mutex);
    m_read_buffer_size = buf_size;
    m_asio_read_buf  = std::make_unique<char[]>(buf_size);
}

QString MyTcpSocket::peerAddress() const
{
    return QString::fromStdString(m_asio_socket->remote_endpoint().address().to_string());
}

quint16 MyTcpSocket::peerPort() const
{
    return m_asio_socket->remote_endpoint().port();
}

QString MyTcpSocket::localAddress() const
{
    return QString::fromStdString(m_asio_socket->local_endpoint().address().to_string());
}

quint16 MyTcpSocket::localPort() const
{
    return m_asio_socket->local_endpoint().port();
}

QString MyTcpSocket::getErrorString() const
{
    return m_error_info;
}

void MyTcpSocket::connectToHost(const QString &hostName, quint16 port)
{
    std::unique_lock<std::mutex> lock(m_socket_mutex);
    ip::tcp::endpoint host(ip::address::from_string(hostName.toStdString()), port);
    m_asio_socket->async_connect(host, std::bind(&MyTcpSocket::asyncConnectCallback, this, std::placeholders::_1));
}

void MyTcpSocket::disconnectFromHost()
{
    std::unique_lock<std::mutex> lock(m_socket_mutex);
    m_asio_socket->shutdown(ip::tcp::socket::shutdown_type::shutdown_both);
}

void MyTcpSocket::asyncReadCallback(const std::error_code &ec, size_t size)
{
    if (!ec) {
        std::unique_lock<std::mutex> lock(m_socket_mutex);
        m_recv_buffer.append(m_asio_read_buf.get(), size);
        emit readyRead();
        m_asio_socket->async_read_some(
            buffer(m_asio_read_buf.get(), m_read_buffer_size),
            std::bind(&MyTcpSocket::asyncReadCallback, this, std::placeholders::_1, std::placeholders::_2));
    } else {
        m_error_info = QString::fromStdString(ec.message());
        emit socketErrorOccurred();
        emit disconnectedFromHost();
    }
}

void MyTcpSocket::asyncWriteCallback(const std::error_code &ec, size_t size)
{
    if (ec) {
        m_error_info = QString::fromStdString(ec.message());
        emit socketErrorOccurred();
    }
}

asio::io_context *MyTcpSocket::MyIOContext::getIOContext()
{
    if (io_context == nullptr) {
        io_mutex.lock();
        io_context = new asio::io_context();
        io_thread = new std::thread([]() {
            io_context::work worker(*io_context);
            io_context->run();
        });
        io_mutex.unlock();
    }
    return io_context;
}
