#include "mytcpserver.h"
#include "mytcpsocket.h"

using namespace asio;

MyTcpServer::MyTcpServer(QObject *parent)
    : QObject{parent}
{
    m_acceptor = std::make_unique<asio::ip::tcp::acceptor>(*MyTcpSocket::MyIOContext::getIOContext());
    m_used = false;
}

MyTcpServer::~MyTcpServer()
{
    std::unique_lock<std::mutex> lock(m_sock_mutex);
    std::error_code ec;
    m_acceptor->cancel(ec);
    if(ec)
    {
        qDebug() << ec.message();
    }
    m_acceptor->close(ec);
    if(ec)
    {
        qDebug() << ec.message();
    }
}

bool MyTcpServer::bind(const QHostAddress &address, quint16 port)
{
    std::unique_lock<std::mutex> lock(m_sock_mutex);
    std::error_code ec;
    ip::tcp::endpoint ep(ip::address::from_string(address.toString().toStdString()), port);
    m_acceptor->open(ep.protocol(), ec);
    if(ec)
    {
        m_error_info = QString::fromStdString(ec.message());
        emit errorOccurred();
        return false;
    }
    m_acceptor->set_option(ip::tcp::acceptor::reuse_address(true));
    m_acceptor->bind(ep, ec);
    if(ec)
    {
        m_error_info = QString::fromStdString(ec.message());
        emit errorOccurred();
        return false;
    }
    m_acceptor->listen(socket_base::max_listen_connections, ec);
    if(ec)
    {
        m_error_info = QString::fromStdString(ec.message());
        emit errorOccurred();
        return false;
    }
    socket_ptr sock_(new ip::tcp::socket(*MyTcpSocket::MyIOContext::getIOContext()));
    m_acceptor->async_accept(*sock_, std::bind(&MyTcpServer::asyncAcceptCallback, this, sock_, std::placeholders::_1));
    return true;
}

void MyTcpServer::close()
{
    std::unique_lock<std::mutex> lock(m_sock_mutex);
    std::error_code ec;
    m_acceptor->cancel(ec);
    if(ec)
    {
        m_error_info = QString::fromStdString(ec.message());
        emit errorOccurred();
    }
    m_acceptor->close(ec);
    if(ec)
    {
        m_error_info = QString::fromStdString(ec.message());
        emit errorOccurred();
    }
}

QString MyTcpServer::getErrorString() const
{
    return m_error_info;
}

void MyTcpServer::asyncAcceptCallback(socket_ptr sock, const std::error_code &ec)
{
    if(ec)
    {
        m_error_info = QString::fromStdString(ec.message());
        emit errorOccurred();
        return;
    }
    else
    {
        std::unique_lock<std::mutex> lock(m_sock_mutex);
        MyTcpSocket *new_connection = new MyTcpSocket(sock);
        emit newConnectionIncoming(new_connection);
        socket_ptr sock_(new ip::tcp::socket(*MyTcpSocket::MyIOContext::getIOContext()));
        m_acceptor->async_accept(*sock_, std::bind(&MyTcpServer::asyncAcceptCallback, this, sock_, std::placeholders::_1));
    }
}
