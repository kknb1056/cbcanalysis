// All of this code was heavily copied from the boost asio examples so I'll include
// the licence from that.
//
//
// Copyright (c) 2003-2013 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//


#include "SLHCUpgradeTracker/CBCAnalysis/interface/HttpServer.h"

#include <fstream>
#include <thread>
#include <set>
#include <boost/asio.hpp>

namespace // Use the unnamed namespace for things only used in this file
{
	namespace status_strings
	{
		const std::string ok="HTTP/1.0 200 OK\r\n";
		const std::string created="HTTP/1.0 201 Created\r\n";
		const std::string accepted="HTTP/1.0 202 Accepted\r\n";
		const std::string no_content="HTTP/1.0 204 No Content\r\n";
		const std::string multiple_choices="HTTP/1.0 300 Multiple Choices\r\n";
		const std::string moved_permanently="HTTP/1.0 301 Moved Permanently\r\n";
		const std::string moved_temporarily="HTTP/1.0 302 Moved Temporarily\r\n";
		const std::string not_modified="HTTP/1.0 304 Not Modified\r\n";
		const std::string bad_request="HTTP/1.0 400 Bad Request\r\n";
		const std::string unauthorized="HTTP/1.0 401 Unauthorized\r\n";
		const std::string forbidden="HTTP/1.0 403 Forbidden\r\n";
		const std::string not_found="HTTP/1.0 404 Not Found\r\n";
		const std::string internal_server_error="HTTP/1.0 500 Internal Server Error\r\n";
		const std::string not_implemented="HTTP/1.0 501 Not Implemented\r\n";
		const std::string bad_gateway="HTTP/1.0 502 Bad Gateway\r\n";
		const std::string service_unavailable="HTTP/1.0 503 Service Unavailable\r\n";

		boost::asio::const_buffer to_buffer( httpserver::HttpServer::Reply::StatusType status );

	} // namespace status_strings

	namespace misc_strings
	{
		const char name_value_separator[]={ ':', ' ' };
		const char crlf[]={ '\r', '\n' };
	} // namespace misc_strings

	std::vector<boost::asio::const_buffer> reply_to_buffers( httpserver::HttpServer::Reply& reply );


	namespace stock_replies
	{
		const char ok[]="";
		const char created[]="<html>"
				"<head><title>Created</title></head>"
				"<body><h1>201 Created</h1></body>"
				"</html>";
		const char accepted[]="<html>"
				"<head><title>Accepted</title></head>"
				"<body><h1>202 Accepted</h1></body>"
				"</html>";
		const char no_content[]="<html>"
				"<head><title>No Content</title></head>"
				"<body><h1>204 Content</h1></body>"
				"</html>";
		const char multiple_choices[]="<html>"
				"<head><title>Multiple Choices</title></head>"
				"<body><h1>300 Multiple Choices</h1></body>"
				"</html>";
		const char moved_permanently[]="<html>"
				"<head><title>Moved Permanently</title></head>"
				"<body><h1>301 Moved Permanently</h1></body>"
				"</html>";
		const char moved_temporarily[]="<html>"
				"<head><title>Moved Temporarily</title></head>"
				"<body><h1>302 Moved Temporarily</h1></body>"
				"</html>";
		const char not_modified[]="<html>"
				"<head><title>Not Modified</title></head>"
				"<body><h1>304 Not Modified</h1></body>"
				"</html>";
		const char bad_request[]="<html>"
				"<head><title>Bad Request</title></head>"
				"<body><h1>400 Bad Request</h1></body>"
				"</html>";
		const char unauthorized[]="<html>"
				"<head><title>Unauthorized</title></head>"
				"<body><h1>401 Unauthorized</h1></body>"
				"</html>";
		const char forbidden[]="<html>"
				"<head><title>Forbidden</title></head>"
				"<body><h1>403 Forbidden</h1></body>"
				"</html>";
		const char not_found[]="<html>"
				"<head><title>Not Found</title></head>"
				"<body><h1>404 Not Found</h1></body>"
				"</html>";
		const char internal_server_error[]="<html>"
				"<head><title>Internal Server Error</title></head>"
				"<body><h1>500 Internal Server Error</h1></body>"
				"</html>";
		const char not_implemented[]="<html>"
				"<head><title>Not Implemented</title></head>"
				"<body><h1>501 Not Implemented</h1></body>"
				"</html>";
		const char bad_gateway[]="<html>"
				"<head><title>Bad Gateway</title></head>"
				"<body><h1>502 Bad Gateway</h1></body>"
				"</html>";
		const char service_unavailable[]="<html>"
				"<head><title>Service Unavailable</title></head>"
				"<body><h1>503 Service Unavailable</h1></body>"
				"</html>";

		std::string to_string( httpserver::HttpServer::Reply::StatusType status );

	} // namespace stock_replies


	class Connection;

	/// Manages open connections so that they may be cleanly stopped when the server
	/// needs to shut down.
	class ConnectionManager
	{
	public:
		ConnectionManager( const ConnectionManager& )=delete;
		ConnectionManager& operator=( const ConnectionManager& )=delete;
		ConnectionManager( ConnectionManager&& )=delete;
		ConnectionManager& operator=( ConnectionManager&& )=delete;

		/// Construct a connection manager.
		ConnectionManager();

		/// Add the specified connection to the manager and start it.
		void start( std::shared_ptr<Connection> c );

		/// Stop the specified connection.
		void stop( std::shared_ptr<Connection> c );

		/// Stop all connections.
		void stop_all();

	private:
		/// The managed connections.
		std::set<std::shared_ptr<Connection>> connections_;
	};

	/// Parser for incoming requests.
	class RequestParser
	{
	public:
		/// Construct ready to parse the request method.
		RequestParser();

		/// Reset to initial parser state.
		void reset();

		/// Result of parse.
		enum result_type { good, bad, indeterminate };

		/// Parse some data. The enum return value is good when a complete request has
		/// been parsed, bad if the data is invalid, indeterminate when more data is
		/// required. The InputIterator return value indicates how much of the input
		/// has been consumed.
		template <typename InputIterator>
		std::tuple<result_type, InputIterator> parse( httpserver::HttpServer::Request& req, InputIterator begin, InputIterator end )
		{
			while (begin != end)
			{
				result_type result = consume(req, *begin++);
				if (result == good || result == bad) return std::make_tuple(result, begin);
			}
			return std::make_tuple(indeterminate, begin);
		}

	private:
		/// Handle the next character of input.
		result_type consume( httpserver::HttpServer::Request& req, char input );

		/// Check if a byte is an HTTP character.
		static bool is_char(int c);

		/// Check if a byte is an HTTP control character.
		static bool is_ctl(int c);

		/// Check if a byte is defined as an HTTP tspecial character.
		static bool is_tspecial(int c);

		/// Check if a byte is a digit.
		static bool is_digit(int c);

		/// The current state of the parser.
		enum state
		{
			method_start,
			method,
			uri,
			http_version_h,
			http_version_t_1,
			http_version_t_2,
			http_version_p,
			http_version_slash,
			http_version_major_start,
			http_version_major,
			http_version_minor_start,
			http_version_minor,
			expecting_newline_1,
			header_line_start,
			header_lws,
			header_name,
			space_before_header_value,
			header_value,
			expecting_newline_2,
			expecting_newline_3
		} state_;
	};

	/// Represents a single connection from a client.
	class Connection : public std::enable_shared_from_this<Connection>
	{
	public:
		Connection( const Connection& )=delete;
		Connection& operator=( const Connection& )=delete;
		Connection( Connection&& )=delete;
		Connection& operator=( Connection&& )=delete;

		/// Construct a connection with the given socket.
		explicit Connection( boost::asio::ip::tcp::socket socket, ::ConnectionManager& manager, httpserver::HttpServer::IRequestHandler& handler );

		/// Start the first asynchronous operation for the connection.
		void start();

		/// Stop all asynchronous operations associated with the connection.
		void stop();

	private:
		/// Perform an asynchronous read operation.
		void do_read();

		/// Perform an asynchronous write operation.
		void do_write();

		/// Socket for the connection.
		boost::asio::ip::tcp::socket socket_;

		/// The manager for this connection.
		::ConnectionManager& connectionManager_;

		/// The handler used to process the incoming request.
		httpserver::HttpServer::IRequestHandler& requestHandler_;

		/// Buffer for incoming data.
		std::array<char,8192> buffer_;

		/// The incoming request.
		httpserver::HttpServer::Request request_;

		/// The parser for the incoming request.
		::RequestParser request_parser_;

		/// The reply to be sent back to the client.
		httpserver::HttpServer::Reply reply_;
	};

	/** Find and replace in strings. Copied from
	 * "http://stackoverflow.com/questions/1494399/how-do-i-search-find-and-replace-in-a-standard-string"
	 * I removed the templatedness (that's a word right?) because it made calling it with
	 * differing types awkward.
	 */
	void inline findAndReplace(std::string& source, const std::string& find, const std::string& replace)
	{
		size_t fLen = find.size();
		size_t rLen = replace.size();
		for( size_t pos=0; (pos=source.find(find, pos))!=std::string::npos; pos+=rLen)
		{
			source.replace(pos, fLen, replace);
		}
	}

} // unnamed namespace

namespace httpserver
{
	class HttpServerPrivateMembers
	{
	public:
		HttpServerPrivateMembers( httpserver::HttpServer::IRequestHandler& requestHandler );

		/// Perform an asynchronous accept operation.
		void do_accept();

		/// Wait for a request to stop the server.
		void do_await_stop();

		/// The io_service used to perform asynchronous operations.
		boost::asio::io_service io_service_;

		/// The signal_set is used to register for process termination notifications.
		boost::asio::signal_set signals_;

		/// Acceptor used to listen for incoming connections.
		boost::asio::ip::tcp::acceptor acceptor_;

		/// The connection manager which owns all live connections.
		::ConnectionManager connectionManager_;

		/// The next socket to be accepted.
		boost::asio::ip::tcp::socket socket_;

		/// The handler for all incoming requests.
		httpserver::HttpServer::IRequestHandler& requestHandler_;

		/// Thread for the run loop
		std::thread runThread_;
	};
}

httpserver::HttpServer::Reply httpserver::HttpServer::Reply::stockReply( httpserver::HttpServer::Reply::StatusType status )
{
	Reply reply;
	reply.status = status;
	reply.content = stock_replies::to_string(status);
	reply.headers.resize(2);
	reply.headers[0].name = "Content-Length";
	reply.headers[0].value = std::to_string(reply.content.size());
	reply.headers[1].name = "Content-Type";
	reply.headers[1].value = "text/html";
	return reply;
}

void httpserver::HttpServer::urlDecode( std::string& url )
{
	findAndReplace( url, "+", " " );
	findAndReplace( url, "%20", " " );
	findAndReplace( url, "%2B", "+" );
	findAndReplace( url, "%2b", "+" );
	findAndReplace( url, "%2F", "/" );
	findAndReplace( url, "%2f", "/" );
	findAndReplace( url, "%26", "&" );
	findAndReplace( url, "%3F", "?" );
	findAndReplace( url, "%3f", "?" );

	// This one obviously has to go last
	findAndReplace( url, "%25", "%" );
}

void httpserver::HttpServer::splitURI( const std::string& URI, std::string& resource, std::vector< std::pair<std::string,std::string> >& parameters, bool decodeSymbols )
{
	size_t characterPosition=URI.find_first_of("?");
	resource=URI.substr(0,characterPosition);
	if( decodeSymbols ) urlDecode(resource);

	if( characterPosition!=std::string::npos )
	{
		std::string parameterString=URI.substr(characterPosition+1);
		do
		{
			// Parameters are separated by an ampersand, so see if there are any of those
			// in the string and analyse each parameter individually.
			characterPosition=parameterString.find_first_of("&");
			std::string currentParameterAndValue=parameterString.substr(0,characterPosition);
			// If there are other parameters, strip off this one ready for the next loop
			if( characterPosition!=std::string::npos ) parameterString=parameterString.substr(characterPosition+1);
			else parameterString=""; // Set this so that the loop breaks

			// See if the parameter has been given a value (split with a "="), or is just
			// a parameter name.
			characterPosition=currentParameterAndValue.find_first_of("=");
			std::string parameter=currentParameterAndValue.substr(0,characterPosition);
			std::string value;
			if( characterPosition!=std::string::npos ) value=currentParameterAndValue.substr(characterPosition+1);
			// Only add it if the parameter name is valid
			if( !parameter.empty() )
			{
				if( decodeSymbols )
				{
					urlDecode(parameter);
					urlDecode(value);
				}
				parameters.push_back( std::make_pair( parameter, value ) );
			}
		}
		while( !parameterString.empty() );
	}
}

httpserver::HttpServer::HttpServer( IRequestHandler& requestHandler )
	: pImple( new httpserver::HttpServerPrivateMembers(requestHandler) )
{

}

httpserver::HttpServer::~HttpServer()
{
	stop();
}

void httpserver::HttpServer::start( const std::string& address, const std::string& port )
{
	// Make sure the server isn't already running
	if( !pImple->runThread_.joinable() )
	{
		pImple->do_await_stop();

		// Open the acceptor with the option to reuse the address (i.e. SO_REUSEADDR).
		boost::asio::ip::tcp::resolver resolver(pImple->io_service_);
		boost::asio::ip::tcp::endpoint endpoint = *resolver.resolve({address, port});
		pImple->acceptor_.open(endpoint.protocol());
		pImple->acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
		pImple->acceptor_.bind(endpoint);
		pImple->acceptor_.listen();

		pImple->do_accept();

		// Start the io_service running in a new thread because it blocks
		pImple->runThread_=std::thread( [&]{ pImple->io_service_.run(); } );
	}
}

void httpserver::HttpServer::stop()
{
	// Check to see the server is running
	if( pImple->runThread_.joinable() )
	{
		pImple->io_service_.stop();
		pImple->acceptor_.close();
		pImple->runThread_.join();
	}
}

void httpserver::HttpServer::blockUntilFinished()
{
	pImple->runThread_.join();
}

httpserver::HttpServerPrivateMembers::HttpServerPrivateMembers( httpserver::HttpServer::IRequestHandler& requestHandler )
	: io_service_(),
	  signals_(io_service_),
	  acceptor_(io_service_),
	  connectionManager_(),
	  socket_(io_service_),
	  requestHandler_(requestHandler)
{
	// Register to handle the signals that indicate when the server should exit.
	// It is safe to register for the same signal multiple times in a program,
	// provided all registration for the specified signal is made through Asio.
	signals_.add(SIGINT);
	signals_.add(SIGTERM);
#	if defined(SIGQUIT)
		signals_.add(SIGQUIT);
#	endif // defined(SIGQUIT)
}


void httpserver::HttpServerPrivateMembers::do_accept()
{
	acceptor_.async_accept(socket_,
		[this](boost::system::error_code ec)
		{
			// Check whether the server was stopped by a signal before this
			// completion handler had a chance to run.
			if (!acceptor_.is_open())
			{
				return;
			}

			if (!ec)
			{
				connectionManager_.start(std::make_shared< ::Connection>(std::move(socket_), connectionManager_, requestHandler_));
			}

			do_accept();
		});
}

void httpserver::HttpServerPrivateMembers::do_await_stop()
{
	signals_.async_wait(
		[this](boost::system::error_code /*ec*/, int /*signo*/)
		{
			// The server is stopped by cancelling all outstanding asynchronous
			// operations. Once all operations have finished the io_service::run()
			// call will exit.
			acceptor_.close();
			connectionManager_.stop_all();
		});
}

//------------------------------------------------------------------------

namespace // unnamed namespace
{
	namespace status_strings
	{
		boost::asio::const_buffer to_buffer( httpserver::HttpServer::Reply::StatusType status )
		{
			switch( status )
			{
				case httpserver::HttpServer::Reply::ok:
					return boost::asio::buffer( ok );
				case httpserver::HttpServer::Reply::created:
					return boost::asio::buffer( created );
				case httpserver::HttpServer::Reply::accepted:
					return boost::asio::buffer( accepted );
				case httpserver::HttpServer::Reply::no_content:
					return boost::asio::buffer( no_content );
				case httpserver::HttpServer::Reply::multiple_choices:
					return boost::asio::buffer( multiple_choices );
				case httpserver::HttpServer::Reply::moved_permanently:
					return boost::asio::buffer( moved_permanently );
				case httpserver::HttpServer::Reply::moved_temporarily:
					return boost::asio::buffer( moved_temporarily );
				case httpserver::HttpServer::Reply::not_modified:
					return boost::asio::buffer( not_modified );
				case httpserver::HttpServer::Reply::bad_request:
					return boost::asio::buffer( bad_request );
				case httpserver::HttpServer::Reply::unauthorized:
					return boost::asio::buffer( unauthorized );
				case httpserver::HttpServer::Reply::forbidden:
					return boost::asio::buffer( forbidden );
				case httpserver::HttpServer::Reply::not_found:
					return boost::asio::buffer( not_found );
				case httpserver::HttpServer::Reply::internal_server_error:
					return boost::asio::buffer( internal_server_error );
				case httpserver::HttpServer::Reply::not_implemented:
					return boost::asio::buffer( not_implemented );
				case httpserver::HttpServer::Reply::bad_gateway:
					return boost::asio::buffer( bad_gateway );
				case httpserver::HttpServer::Reply::service_unavailable:
					return boost::asio::buffer( service_unavailable );
				default:
					return boost::asio::buffer( internal_server_error );
			} // switch (status)
		} // function to_buffer
	} // namespace status_strings

	std::vector<boost::asio::const_buffer> reply_to_buffers( httpserver::HttpServer::Reply& reply )
	{
		std::vector<boost::asio::const_buffer> buffers;
		buffers.push_back( ::status_strings::to_buffer( reply.status ) );
		for( std::size_t i=0; i<reply.headers.size(); ++i )
		{
			httpserver::HttpServer::Header& h=reply.headers[i];
			buffers.push_back( boost::asio::buffer( h.name ) );
			buffers.push_back( boost::asio::buffer( misc_strings::name_value_separator ) );
			buffers.push_back( boost::asio::buffer( h.value ) );
			buffers.push_back( boost::asio::buffer( misc_strings::crlf ) );
		}
		buffers.push_back( boost::asio::buffer( misc_strings::crlf ) );
		buffers.push_back( boost::asio::buffer( reply.content ) );
		return buffers;
	}

	std::string stock_replies::to_string( httpserver::HttpServer::Reply::StatusType status )
	{
		switch( status )
		{
			case httpserver::HttpServer::Reply::ok:
				return ok;
			case httpserver::HttpServer::Reply::created:
				return created;
			case httpserver::HttpServer::Reply::accepted:
				return accepted;
			case httpserver::HttpServer::Reply::no_content:
				return no_content;
			case httpserver::HttpServer::Reply::multiple_choices:
				return multiple_choices;
			case httpserver::HttpServer::Reply::moved_permanently:
				return moved_permanently;
			case httpserver::HttpServer::Reply::moved_temporarily:
				return moved_temporarily;
			case httpserver::HttpServer::Reply::not_modified:
				return not_modified;
			case httpserver::HttpServer::Reply::bad_request:
				return bad_request;
			case httpserver::HttpServer::Reply::unauthorized:
				return unauthorized;
			case httpserver::HttpServer::Reply::forbidden:
				return forbidden;
			case httpserver::HttpServer::Reply::not_found:
				return not_found;
			case httpserver::HttpServer::Reply::internal_server_error:
				return internal_server_error;
			case httpserver::HttpServer::Reply::not_implemented:
				return not_implemented;
			case httpserver::HttpServer::Reply::bad_gateway:
				return bad_gateway;
			case httpserver::HttpServer::Reply::service_unavailable:
				return service_unavailable;
			default:
				return internal_server_error;
		}
	}

	//------------------------------------------------------------------------
	//------------------------------------------------------------------------
	//-------       Definitions for the ConnectionManager class      ---------
	//------------------------------------------------------------------------
	//------------------------------------------------------------------------
	ConnectionManager::ConnectionManager()
	{
	}

	void ConnectionManager::start( std::shared_ptr<Connection> pConnection )
	{
		connections_.insert( pConnection );
		pConnection->start();
	}

	void ConnectionManager::stop( std::shared_ptr<Connection> pConnection )
	{
		connections_.erase( pConnection );
		pConnection->stop();
	}

	void ConnectionManager::stop_all()
	{
		for( auto pConnection : connections_ ) pConnection->stop();
		connections_.clear();
	}

	//------------------------------------------------------------------------
	//------------------------------------------------------------------------
	//-------          Definitions for the Connection class          ---------
	//------------------------------------------------------------------------
	//------------------------------------------------------------------------
	Connection::Connection( boost::asio::ip::tcp::socket socket, ::ConnectionManager& manager, httpserver::HttpServer::IRequestHandler& handler ) :
			socket_( std::move( socket ) ), connectionManager_( manager ), requestHandler_( handler )
	{
	}

	void Connection::start()
	{
		do_read();
	}

	void Connection::stop()
	{
		socket_.close();
	}

	void Connection::do_read()
	{
		auto self( shared_from_this() );
		socket_.async_read_some( boost::asio::buffer( buffer_ ), [this, self](boost::system::error_code ec, std::size_t bytes_transferred)
		{
			if (!ec)
			{
				::RequestParser::result_type result;
				std::tie(result, std::ignore) = request_parser_.parse(
						request_, buffer_.data(), buffer_.data() + bytes_transferred);

				if (result == ::RequestParser::good)
				{
					requestHandler_.handleRequest(request_, reply_);
					do_write();
				}
				else if (result == ::RequestParser::bad)
				{
					reply_ = httpserver::HttpServer::Reply::stockReply(httpserver::HttpServer::Reply::bad_request);
					do_write();
				}
				else
				{
					do_read();
				}
			}
			else if (ec != boost::asio::error::operation_aborted)
			{
				connectionManager_.stop(shared_from_this());
			}
		} );
	}

	void Connection::do_write()
	{
		auto self( shared_from_this() );
		boost::asio::async_write( socket_, reply_to_buffers(reply_), [this, self](boost::system::error_code ec, std::size_t)
		{
			if (!ec)
			{
				// Initiate graceful connection closure.
				boost::system::error_code ignored_ec;
				socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both,
						ignored_ec);
			}

			if (ec != boost::asio::error::operation_aborted)
			{
				connectionManager_.stop(shared_from_this());
			}
		} );
	}


	//------------------------------------------------------------------------
	//------------------------------------------------------------------------
	//-------         Definitions for the RequestParser class        ---------
	//------------------------------------------------------------------------
	//------------------------------------------------------------------------
	RequestParser::RequestParser()
	  : state_(method_start)
	{
	}

	void RequestParser::reset()
	{
		state_ = method_start;
	}

	RequestParser::result_type RequestParser::consume( httpserver::HttpServer::Request& req, char input )
	{
		switch (state_)
		{
			case method_start:
				if (!is_char(input) || is_ctl(input) || is_tspecial(input))
				{
					return bad;
				}
				else
				{
					state_ = method;
					req.method.push_back(input);
					return indeterminate;
				}
			case method:
				if (input == ' ')
				{
					state_ = uri;
					return indeterminate;
				}
				else if (!is_char(input) || is_ctl(input) || is_tspecial(input))
				{
					return bad;
				}
				else
				{
					req.method.push_back(input);
					return indeterminate;
				}
			case uri:
				if (input == ' ')
				{
					state_ = http_version_h;
					return indeterminate;
				}
				else if (is_ctl(input))
				{
					return bad;
				}
				else
				{
					req.uri.push_back(input);
					return indeterminate;
				}
			case http_version_h:
				if (input == 'H')
				{
					state_ = http_version_t_1;
					return indeterminate;
				}
				else
				{
					return bad;
				}
			case http_version_t_1:
				if (input == 'T')
				{
					state_ = http_version_t_2;
					return indeterminate;
				}
				else
				{
					return bad;
				}
			case http_version_t_2:
				if (input == 'T')
				{
					state_ = http_version_p;
					return indeterminate;
				}
				else
				{
					return bad;
				}
			case http_version_p:
				if (input == 'P')
				{
					state_ = http_version_slash;
					return indeterminate;
				}
				else
				{
					return bad;
				}
			case http_version_slash:
				if (input == '/')
				{
					req.http_version_major = 0;
					req.http_version_minor = 0;
					state_ = http_version_major_start;
					return indeterminate;
				}
				else
				{
					return bad;
				}
			case http_version_major_start:
				if (is_digit(input))
				{
					req.http_version_major = req.http_version_major * 10 + input - '0';
					state_ = http_version_major;
					return indeterminate;
				}
				else
				{
					return bad;
				}
			case http_version_major:
				if (input == '.')
				{
					state_ = http_version_minor_start;
					return indeterminate;
				}
				else if (is_digit(input))
				{
					req.http_version_major = req.http_version_major * 10 + input - '0';
					return indeterminate;
				}
				else
				{
					return bad;
				}
			case http_version_minor_start:
				if (is_digit(input))
				{
					req.http_version_minor = req.http_version_minor * 10 + input - '0';
					state_ = http_version_minor;
					return indeterminate;
				}
				else
				{
					return bad;
				}
			case http_version_minor:
				if (input == '\r')
				{
					state_ = expecting_newline_1;
					return indeterminate;
				}
				else if (is_digit(input))
				{
					req.http_version_minor = req.http_version_minor * 10 + input - '0';
					return indeterminate;
				}
				else
				{
					return bad;
				}
			case expecting_newline_1:
				if (input == '\n')
				{
					state_ = header_line_start;
					return indeterminate;
				}
				else
				{
					return bad;
				}
			case header_line_start:
				if (input == '\r')
				{
					state_ = expecting_newline_3;
					return indeterminate;
				}
				else if (!req.headers.empty() && (input == ' ' || input == '\t'))
				{
					state_ = header_lws;
					return indeterminate;
				}
				else if (!is_char(input) || is_ctl(input) || is_tspecial(input))
				{
					return bad;
				}
				else
				{
					req.headers.push_back( httpserver::HttpServer::Header() );
					req.headers.back().name.push_back(input);
					state_ = header_name;
					return indeterminate;
				}
			case header_lws:
				if (input == '\r')
				{
					state_ = expecting_newline_2;
					return indeterminate;
				}
				else if (input == ' ' || input == '\t')
				{
					return indeterminate;
				}
				else if (is_ctl(input))
				{
					return bad;
				}
				else
				{
					state_ = header_value;
					req.headers.back().value.push_back(input);
					return indeterminate;
				}
			case header_name:
				if (input == ':')
				{
					state_ = space_before_header_value;
					return indeterminate;
				}
				else if (!is_char(input) || is_ctl(input) || is_tspecial(input))
				{
					return bad;
				}
				else
				{
					req.headers.back().name.push_back(input);
					return indeterminate;
				}
			case space_before_header_value:
				if (input == ' ')
				{
					state_ = header_value;
					return indeterminate;
				}
				else
				{
					return bad;
				}
			case header_value:
				if (input == '\r')
				{
					state_ = expecting_newline_2;
					return indeterminate;
				}
				else if (is_ctl(input))
				{
					return bad;
				}
				else
				{
					req.headers.back().value.push_back(input);
					return indeterminate;
				}
			case expecting_newline_2:
				if (input == '\n')
				{
					state_ = header_line_start;
					return indeterminate;
				}
				else
				{
					return bad;
				}
			case expecting_newline_3:
				return (input == '\n') ? good : bad;
			default:
				return bad;
		}
	}

	bool RequestParser::is_char(int c)
	{
		return c >= 0 && c <= 127;
	}

	bool RequestParser::is_ctl(int c)
	{
		return (c >= 0 && c <= 31) || (c == 127);
	}

	bool RequestParser::is_tspecial(int c)
	{
		switch (c)
		{
			case '(': case ')': case '<': case '>': case '@':
			case ',': case ';': case ':': case '\\': case '"':
			case '/': case '[': case ']': case '?': case '=':
			case '{': case '}': case ' ': case '\t':
				return true;
			default:
				return false;
		}
	}

	bool RequestParser::is_digit(int c)
	{
		return c >= '0' && c <= '9';
	}

} // unnamed namespace
