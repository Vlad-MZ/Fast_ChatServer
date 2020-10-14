// Client code
//
/*
 * Copyright (c) 2014, Peter Thorson. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the WebSocket++ Project nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL PETER THORSON BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

 /** ====== WARNING ========
  * This example is presently used as a scratch space. It may or may not be broken
  * at any given time.
  */

#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>

#include <iostream>
#include <chrono>
#include <thread>
#include <string>
#include <string_view>
#include "ClientCode.h"
using namespace std;


string userName;


typedef websocketpp::client<websocketpp::config::asio_client> client;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

// pull out the type of messages sent by our config
typedef websocketpp::config::asio_tls_client::message_type::ptr message_ptr;
typedef websocketpp::lib::shared_ptr<boost::asio::ssl::context> context_ptr;
typedef client::connection_ptr connection_ptr;



class perftest {
public:
    typedef perftest type;
    typedef std::chrono::duration<int, std::micro> dur_type;

    perftest() {
        m_endpoint.set_access_channels(websocketpp::log::alevel::none); // none => no log to console
        m_endpoint.set_error_channels(websocketpp::log::elevel::none);  // none => no log to console

        // Initialize ASIO
        m_endpoint.init_asio();

        m_endpoint.set_message_handler(bind(&type::on_message, this, ::_1, ::_2));
        m_endpoint.set_open_handler(bind(&type::on_open, this, ::_1));
        
        m_endpoint.set_fail_handler(bind(&type::on_fail, this, ::_1));
    }

    void start(std::string uri) {
        websocketpp::lib::error_code ec;
        con = m_endpoint.get_connection(uri, ec);

        if (ec) {
            m_endpoint.get_alog().write(websocketpp::log::alevel::app, ec.message());
            return;
        }

        //con->set_proxy("http://humupdates.uchicago.edu:8443");

        m_endpoint.connect(con);

        // Start the ASIO io_service run loop
        m_endpoint.run();
    }

    
    context_ptr on_tls_init(websocketpp::connection_hdl) {
        context_ptr ctx = websocketpp::lib::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::tlsv1);

        try {
            ctx->set_options(boost::asio::ssl::context::default_workarounds |
                boost::asio::ssl::context::no_sslv2 |
                boost::asio::ssl::context::no_sslv3 |
                boost::asio::ssl::context::single_dh_use);
        }
        catch (std::exception& e) {
            std::cout << e.what() << std::endl;
        }
        return ctx;
    }
    

    void on_fail(websocketpp::connection_hdl hdl) {
        client::connection_ptr con = m_endpoint.get_con_from_hdl(hdl);

        std::cout << "Fail handler" << std::endl;
        std::cout << con->get_state() << std::endl;
        std::cout << con->get_local_close_code() << std::endl;
        std::cout << con->get_local_close_reason() << std::endl;
        std::cout << con->get_remote_close_code() << std::endl;
        std::cout << con->get_remote_close_reason() << std::endl;
        std::cout << con->get_ec() << " - " << con->get_ec().message() << std::endl;
    }
    

    void on_open(websocketpp::connection_hdl hdl) {
        cout << "Connected" << endl;
        m_endpoint.send(hdl, "SET_NAME=" + userName, websocketpp::frame::opcode::text);
    }
    void on_message(websocketpp::connection_hdl hdl, message_ptr msg) {
        cout << ">> " + msg->get_payload() << endl;
        // m_endpoint.close(hdl, websocketpp::close::status::going_away, "");
    }
    void stop() {
        m_endpoint.stop();
    }

    void sendMessage(string message) {
        m_endpoint.send(con->get_handle(), message, websocketpp::frame::opcode::text);
    }
private:
    client::connection_ptr con;
    client m_endpoint;
};


void runClient() {     //int main(int argc, char* argv[]) {
    perftest endpoint;
    cout << "Enter your name: ";
    getline(cin, userName);
    auto thr = new thread([&endpoint](){
        std::string uri = "ws://127.0.0.1:9999";
        try {
            endpoint.start(uri);
        }
        catch (websocketpp::exception const& e) {
            std::cout << e.what() << std::endl;
        }
        catch (std::exception const& e) {
            std::cout << e.what() << std::endl;
        }
        catch (...) {
            std::cout << "other exception" << std::endl;
        }
    });

    string input;
    while (input.compare("exit") != 0) {
        getline(cin, input);
        endpoint.sendMessage(input);
    }
    // close connection
    endpoint.stop();
    // join thread
    thr->join();
}