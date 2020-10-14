// Server code
//
//  Our protocoldescription:
//      Client -> Server
//          SET_NAME=Mike - установить имя
//          MESSAGE_TO=id,message - отправить сообщение пользователю ID=id
//
//      Server -> Client
//          NEW_USER,Mike,1221
//
#include <iostream>             // std::cout
#include <sstream>              // std::stringstream
#include <string>               // std::string
#include <string_view>
#include <thread>
#include <regex>
#include <algorithm>
#include <uwebsockets/App.h>
#include "ServerCode.h"
using namespace std;

struct UserConnection {
    unsigned long user_id;
    string user_name;
};

string to_lower2(string str) {
    transform(str.begin(), str.end(), str.begin(), ::tolower);
    return str;
}

auto bot = [](auto* ws, string user_message) {
    unordered_map<string, string> database = {
        {"hello", "Oh, hello to you, hooman"},
        {"how are you", "I'm good"},
        {"what is your name", "AUTOMATED SUPERBOT 3000"},
        {"goodbye", "Oh, byyyeeeee !!!"},
    };

    for (auto entry : database) {
        auto expression = regex(".*" + entry.first + ".*");
        if (regex_match(user_message, expression)) {
            ws->send(entry.second, uWS::OpCode::TEXT);
        }
    }
};


template <class K, class V, class Compare = std::less<K>, class Allocator = std::allocator<std::pair<const K, V> > >
class guarded_map {
private:
    std::map<K, V, Compare, Allocator> _map;
    std::mutex _m;

public:
    void set(K key, V value) {
        std::lock_guard<std::mutex> lk(this->_m);
        this->_map[key] = value;
    }

    V& get(K key) {
        std::lock_guard<std::mutex> lk(this->_m);
        return this->_map[key];
    }

    bool empty() {
        std::lock_guard<std::mutex> lk(this->_m);
        return this->_map.empty();
    }

    vector<string> getNames() {
        vector<string> result;
        for (auto entry : this->_map) {
            result.push_back("NEW_USER," + entry.second + "," + to_string(entry.first));
        }
        return result;
    }

    // other public methods you need to implement
};
guarded_map<long, string> names;


void runServer()     // int main()
{
    atomic_ulong latest_user_id = 10;
    atomic_ulong total_users = 1;       //one bot is always in chat


    vector<thread*> threads(thread::hardware_concurrency());

    transform(threads.begin(), threads.end(), threads.begin(), [&latest_user_id, &total_users](auto* thr) {
        return new thread([&latest_user_id, &total_users]() {
            uWS::App().ws<UserConnection>("/*", {

                .open = [&latest_user_id, &total_users](auto* ws) {
                    // Что делать при подключении пользователя
                    UserConnection* data = (UserConnection*)ws->getUserData();
                    data->user_id = latest_user_id++;
                    cout << "New user connected, id " << data->user_id << endl;
                    cout << "Total users connected " << ++total_users << endl;
                    ws->subscribe("broadcast");
                    ws->subscribe("user#" + to_string(data->user_id));
                },

                .message = [&latest_user_id, &total_users](auto* ws, string_view message, uWS::OpCode opCode) {
                     // Что делать при получении сообщения
                    UserConnection* data = (UserConnection*)ws->getUserData();
                    cout << "New message " << message << " User ID " << data->user_id << endl;
                    
                    auto beginning = message.substr(0, 9);
                    if (beginning.compare("SET_NAME=") == 0) {
                        // Пользователь прислал свое имя
                        auto name = message.substr(9);
                        if (name.find("'") == string_view::npos && name.length() <= 255) {
                            data->user_name = string(name);
                            cout << "User set their name ID = " << data->user_id << " Name = " << data->user_name << endl;
                            string broadcast_message = "NEW_USER," + data->user_name + "," + to_string(data->user_id);
                            ws->publish("broadcast", string_view(broadcast_message), opCode, false);
                            for (string mess : names.getNames()) {
                                ws->send(mess, uWS::OpCode::TEXT, false);
                            }
                            names.set(data->user_id, data->user_name);
                        }
                    }

                    auto is_message_to = message.substr(0, 11);
                    if (is_message_to.compare("MESSAGE_TO=") == 0) {
                        // кто-то послал сообщение
                        auto rest = message.substr(11); // id,message
                        size_t position = rest.find(",");
                        if (position != string::npos) {
                            auto id_string = rest.substr(0, position);
                            auto user_message = rest.substr(position + 1);
                            unsigned long id = 0;
                            stringstream(string(id_string)) >> id;
                            if (id >= 10 && id < latest_user_id) {
                                ws->publish("user#" + string(id_string), user_message, opCode, false);
                            }
                            else {
                                if (id == 1) {
                                    // перекинуть сообщение боту
                                    bot(ws, to_lower2(string(user_message)));
                                }
                                else {
                                    string sError = "ERROR: there is no user with ID = " + string(id_string);
                                    ws->send(sError, uWS::OpCode::TEXT);
                                }
                            }
                        }
                    }
                },

                .close = [&latest_user_id, &total_users](auto* ws, int code, std::string_view message) {
                    UserConnection* data = (UserConnection*)ws->getUserData();
                    cout << "UserID " << data->user_id << " leaves our chat" <<endl;
                    cout << "Total users connected " << --total_users << endl;
                }


                }).listen(9999, 
                    [](auto* token) {
                        if (token) {
                            cout << "Server started and listening on port 9999" << endl;
                        }
                        else {
                            cout << "Server failed to start :(" << endl;
                        }
                    }).run();
        });
    });

    for_each(threads.begin(), threads.end(), [](auto* thr) {thr->join();});
}
