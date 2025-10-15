#include "UdpDiscoveryServer.h"

using namespace std;

void UdpDiscoveryServer::handle_receive_from(
        const boost::system::error_code &error, size_t bytes_recvd) {
    if (!error && bytes_recvd > 0) {
        // Null-terminate received data to prevent buffer over-read
        size_t safe_size = (bytes_recvd < max_length) ? bytes_recvd : (max_length - 1);
        data_[safe_size] = '\0';

        cout << "\tAutodiscovery request from " << sender_endpoint_ << ": "
             << std::string(data_, safe_size) << endl;

        // Fix: Use .length() instead of sizeof() on std::string
        // sizeof(std::string) returns size of string object (24-32 bytes), not content length
        socket_.async_send_to(
                boost::asio::buffer(return_data.c_str(), return_data.length()), sender_endpoint_,
                boost::bind(&UdpDiscoveryServer::handle_send_to, this,
                            boost::asio::placeholders::error,
                            boost::asio::placeholders::bytes_transferred));
    } else {
        socket_.async_receive_from(
                boost::asio::buffer(data_, max_length), sender_endpoint_,
                boost::bind(&UdpDiscoveryServer::handle_receive_from, this,
                            boost::asio::placeholders::error,
                            boost::asio::placeholders::bytes_transferred));
    }
}

void UdpDiscoveryServer::handle_send_to(const boost::system::error_code &error,
                                        size_t bytes_sent) {
    cout << "\tSending discovery info to " << sender_endpoint_ << endl;
    // @TODO: Send back some sort of validation
    socket_.async_receive_from(
            boost::asio::buffer(data_, max_length), sender_endpoint_,
            boost::bind(&UdpDiscoveryServer::handle_receive_from, this,
                        boost::asio::placeholders::error,
                        boost::asio::placeholders::bytes_transferred));
}
