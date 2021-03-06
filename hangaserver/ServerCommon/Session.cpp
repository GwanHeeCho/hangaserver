#include "Session.h"

void Session::CSession::start()
{
	m_world.connect(shared_from_this());
	
	// shared_from_this는 this 포인터를 shared_ptr로 얻을 수 있는 기능으로 가리키는 곳을 변경하지 못하게 막는다.
	auto self(shared_from_this());
	
	m_socket.async_read_some(
		boost::asio::buffer(m_read.data(), HEADER),
		[this, self](boost::system::error_code errorCode, size_t /*length*/)
		{
			if (!errorCode)
			{
				readHeader(errorCode);
			}
		}
	);
}

void Session::CSession::send(const Message::CData &message)
{
	bool writeProgress = !m_write.empty();
	m_write.push_back(message);

	if (!writeProgress)
	{
		auto self(shared_from_this());
		boost::asio::async_write(
			m_socket,
			boost::asio::buffer(m_write.front().data(), m_write.front().length()),
			[this, self](boost::system::error_code errorCode, size_t /*length*/)
			{
				write(errorCode);
			}
		);
	}
}

void Session::CSession::readHeader(const boost::system::error_code &errorCode)
{
	if (!errorCode)
	{
		if (m_read.decode())
		{
			auto self(shared_from_this());
			
			m_socket.async_read_some(
				boost::asio::buffer(m_read.body(), m_read.body_length()),
				[this, self](boost::system::error_code errorCode, size_t /*length*/)
				{
					if (!errorCode)
					{
						readBody(errorCode);
					}
				}
			);
		}
		else
		{
			m_world.disconnect(shared_from_this());
		}
	}
}

void Session::CSession::readBody(const boost::system::error_code &errorCode)
{
	if (!errorCode)
	{
		m_world.send(m_read);
		
		boost::asio::async_read(
			m_socket,
			boost::asio::buffer(m_read.data(), HEADER),
			boost::bind(&CSession::readHeader, shared_from_this(), boost::asio::placeholders::error)
			);
	}
	else
	{
		m_world.disconnect(shared_from_this());
	}
}

void Session::CSession::write(const boost::system::error_code &errorCode)
{
	auto self(shared_from_this());
	
	if (!errorCode)
	{
		m_write.pop_front();

		if (!m_write.empty())
		{
			
			boost::asio::async_write(
				m_socket,
				boost::asio::buffer(m_write.front().data(), m_write.front().length()),
				[this, self](boost::system::error_code errorCode, std::size_t /*length*/)
				{
					if (errorCode)
					{
						m_world.disconnect(shared_from_this());
					}
				}
			);
		}
	}
	else
	{
		m_world.disconnect(self);
	}
}

// async_read : 정해진 버퍼의 크기가 모두 찰 때까지 수신을 계속해서 기다리는 함수
// buffer.size() = 4096 인데 7777 byte 크기의 패킷이 수신된 경우에 4096 전송하고 남은 3681Byte는 전송되지 않는다.
// 부족한 버퍼만큼 채워서 보내주는 패딩 방식이나 담아놓고 나중에 전송하는 방식을 채택해서 보완하자
/*
boost::asio::async_read(
	m_socket,
	boost::asio::buffer(m_read.data(), HEADER),
	boost::bind(&CSession::readHeader, shared_from_this(), boost::asio::placeholders::error)
);
boost::asio::async_write(
		m_socket,
		boost::asio::buffer(m_write.front().data(), m_write.front().length()),
		boost::bind(&CSession::write, shared_from_this(), boost::asio::placeholders::error)
	);
*/