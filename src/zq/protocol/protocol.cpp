#include "zq/protocol/protocol.h"
#include <system_error>

#ifdef __EMSCRIPTEN__

void protocol_server_start() {}
void protocol_server_poll() {}

#else

#include <iostream>
#include "dialog/info.h"

#define ASIO_STANDALONE
#include "zq/protocol/protocol.h"
#include "zq/protocol/impl.h"
#include "websocketpp/config/asio_no_tls.hpp"
#include "websocketpp/server.hpp"
#include "json/json.h"

typedef websocketpp::server<websocketpp::config::asio> server;
typedef std::set<websocketpp::connection_hdl,std::owner_less<websocketpp::connection_hdl> > con_list;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;
using giri::json::JSON;

server m_server;
con_list connections;

static server zq_server;
static bool enabled;

static void on_open(websocketpp::connection_hdl hdl)
{
	connections.insert(hdl);
}

static void on_close(websocketpp::connection_hdl hdl)
{
	auto it = connections.find(hdl);
	if (it != connections.end())
		connections.erase(it);
}

static void on_message(server* s, websocketpp::connection_hdl hdl, server::message_ptr msg)
{
	auto payload = msg->get_payload();
	std::error_code ec;
	auto json = JSON::Load(payload, ec);
	if (ec) return;

	auto& id_json = json["id"];
	if (!id_json.IsIntegral()) return;

	auto& type_json = json["type"];
	if (!type_json.IsString()) return;

	auto& params_json = json["params"];
	if (!params_json.IsObject()) return;

	protocol::commands::type type = protocol_parse_command(type_json.ToString());
	int id = json["id"].ToInt();
	// TODO: error if seen id before for this connection.

	auto result_json = protocol_handle_command(type, params_json);
	if (result_json.hasKey("error"))
	{
		JSON response_json({"id", id, "error", result_json["error"]});
		s->send(hdl, response_json.ToString(), websocketpp::frame::opcode::value::TEXT);
	}
	else
	{
		JSON response_json({"id", id, "result", result_json});
		s->send(hdl, response_json.ToString(), websocketpp::frame::opcode::value::TEXT);
	}
}

static void close_server()
{
	if (zq_server.is_listening())
		zq_server.stop_listening();
	for (auto hdl : connections)
		zq_server.close(hdl, websocketpp::close::status::going_away, "shutting down");
	connections.clear();
	enabled = false;
}

void protocol_server_start()
{
	zq_server.set_open_handler(bind(&on_open,::_1));
	zq_server.set_close_handler(bind(&on_close,::_1));
	zq_server.set_message_handler(bind(&on_message,&zq_server,::_1,::_2));
	zq_server.set_access_channels(websocketpp::log::alevel::all);
	zq_server.set_error_channels(websocketpp::log::elevel::all);
	zq_server.set_reuse_addr(true);
	zq_server.init_asio();
	zq_server.listen(9002);
	zq_server.start_accept();

	atexit(close_server);
	printf("Protocol server started\n");
	fflush(stdout);
	enabled = true;
}

void protocol_server_poll()
{
	if (enabled)
		zq_server.poll();
}

void protocol_broadcast_event(protocol::events::type type, JSON& params_json)
{
	if (connections.empty())
		return;

	std::string name = protocol_event_to_string(type);
	JSON event_json({"method", name, "params", params_json});
	std::string event_json_string = event_json.ToString();
	for (auto& hdl : connections)
		zq_server.send(hdl, event_json_string, websocketpp::frame::opcode::value::TEXT);
}

#endif
