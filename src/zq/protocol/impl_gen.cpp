// This file is generated by scripts/generate_protocol.py

#include "zq/protocol/impl.h"
#include "zq/protocol/protocol.h"
#include "json/json.h"
#include "fmt/format.h"
#include <optional>

using giri::json::JSON;

protocol::commands::type protocol_parse_command(std::string command)
{
	if (command == "show_message") return protocol::commands::type::show_message;
	if (command == "quit") return protocol::commands::type::quit;
	if (command == "load_quest") return protocol::commands::type::load_quest;
	if (command == "get_items") return protocol::commands::type::get_items;
	if (command == "get_item_names") return protocol::commands::type::get_item_names;
	if (command == "click_screen") return protocol::commands::type::click_screen;
	return protocol::commands::type::none;
}

std::string protocol_event_to_string(protocol::events::type type)
{
	if (type == protocol::events::type::quest_loaded) return "quest_loaded";
	if (type == protocol::events::type::quest_saved) return "quest_saved";
	if (type == protocol::events::type::dialog_opened) return "dialog_opened";
	return "unknown";
}

static int read_int(JSON& json_object, std::string param, std::optional<int> default_value, std::string& error)
{
	const auto& json_value = json_object[param];
	if (json_value.IsNull())
	{
		if (default_value) return *default_value;
		error = fmt::format("Value missing for required param {}", param);
		return 0;
	}
	if (!json_value.IsIntegral())
	{
		error = fmt::format("Value found for param {}, but not of expected type {}", param, "int");
		return 0;
	}
	return json_value.ToInt();
}

static std::string read_string(JSON& json_object, std::string param, std::optional<std::string> default_value, std::string& error)
{
	const auto& json_value = json_object[param];
	if (json_value.IsNull())
	{
		if (default_value) return *default_value;
		error = fmt::format("Value missing for required param {}", param);
		return "";
	}
	if (!json_value.IsString())
	{
		error = fmt::format("Value found for param {}, but not of expected type {}", param, "string");
		return "";
	}
	return json_value.ToString();
}

static bool read_bool(JSON& json_object, std::string param, std::optional<bool> default_value, std::string& error)
{
	const auto& json_value = json_object[param];
	if (json_value.IsNull())
	{
		if (default_value) return *default_value;
		error = fmt::format("Value missing for required param {}", param);
		return false;
	}
	if (!json_value.IsBoolean())
	{
		error = fmt::format("Value found for param {}, but not of expected type {}", param, "bool");
		return false;
	}
	return json_value.ToBool();
}

static void write_type(JSON& json, const std::string value)
{
	json = value;
}

static void write_type(JSON& json, const int value)
{
	json = value;
}

static void write_type(JSON& json, const bool value)
{
	json = value;
}

static void write_type(JSON& json, const protocol::types::Widget value)
{
	json = JSON::Make(JSON::Class::Object);
	json["name"] = value.name;
	json["type"] = value.type;
}

static void write_type(JSON& json, const protocol::types::Item value)
{
	json = JSON::Make(JSON::Class::Object);
	json["id"] = value.id;
	json["name"] = value.name;
	json["tile"] = value.tile;
}


template <typename T>
static void write_type(JSON& json, const std::vector<T>& values)
{
	json = JSON::Make(JSON::Class::Array);
	json[values.size() - 1] = 0;
	for (size_t i = 0; i < values.size(); i++)
		write_type(json[i], values[i]);
}

JSON protocol_handle_command(protocol::commands::type type, JSON& params_json)
{
	std::string error;
	switch (type)
	{
		case protocol::commands::type::show_message:
		{
			protocol::commands::show_message::params params;
			params.type = read_string(params_json, "type", std::nullopt, error);
			if (!error.empty())
				break;
			if (!(params.type == "info" || params.type == "alert"))
			{
				error = fmt::format("invalid value for enumeration: {}", params.type);
				break;
			}
			params.title = read_string(params_json, "title", std::nullopt, error);
			if (!error.empty())
				break;
			params.content = read_string(params_json, "content", std::nullopt, error);
			if (!error.empty())
				break;
			protocol::commands::show_message::handle(params);
			return {};
		}
		case protocol::commands::type::quit:
		{
			protocol::commands::quit::handle({});
			return {};
		}
		case protocol::commands::type::load_quest:
		{
			protocol::commands::load_quest::params params;
			params.path = read_string(params_json, "path", std::nullopt, error);
			if (!error.empty())
				break;
			params.show_progress = read_bool(params_json, "show_progress", std::make_optional(true), error);
			if (!error.empty())
				break;
			auto result = protocol::commands::load_quest::handle(params);
			JSON result_json;
			result_json["success"] = result.success;
			result_json["error_code"] = result.error_code;
			return result_json;
		}
		case protocol::commands::type::get_items:
		{
			auto result = protocol::commands::get_items::handle({});
			JSON result_json;
			write_type(result_json["items"], result.items);
			return result_json;
		}
		case protocol::commands::type::get_item_names:
		{
			auto result = protocol::commands::get_item_names::handle({});
			JSON result_json;
			write_type(result_json["names"], result.names);
			return result_json;
		}
		case protocol::commands::type::click_screen:
		{
			protocol::commands::click_screen::params params;
			params.x = read_int(params_json, "x", std::nullopt, error);
			if (!error.empty())
				break;
			params.y = read_int(params_json, "y", std::nullopt, error);
			if (!error.empty())
				break;
			protocol::commands::click_screen::handle(params);
			return {};
		}
	}

	return JSON({"error", error});
}

void protocol::events::quest_loaded::emit(protocol::events::quest_loaded::params params)
{
	JSON params_json;
	params_json["path"] = params.path;
	protocol_broadcast_event(protocol::events::type::quest_loaded, params_json);
}

void protocol::events::quest_saved::emit(protocol::events::quest_saved::params params)
{
	JSON params_json;
	params_json["path"] = params.path;
	protocol_broadcast_event(protocol::events::type::quest_saved, params_json);
}

void protocol::events::dialog_opened::emit(protocol::events::dialog_opened::params params)
{
	JSON params_json;
	write_type(params_json["widget"], params.widget);
	protocol_broadcast_event(protocol::events::type::dialog_opened, params_json);
}
