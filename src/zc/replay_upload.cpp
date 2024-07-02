#include "zc/replay_upload.h"
#include "base/expected.h"
#include "base/version.h"
#include "zc/replay.h"
#include "zconfig.h"
#include "zsyssimple.h"
#include <fstream>
#include <map>
#include <chrono>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <curl/curl.h>
#include <optional>

static const int TIME_ONE_DAY = 86400;
static const int TIME_ONE_WEEK = TIME_ONE_DAY * 7;
static const std::string UA = fmt::format("ZQuestClassic/{}", getVersionString());

namespace fs = std::filesystem;
using json = nlohmann::json;

struct http_response
{
	int status_code;
	std::string body;

	bool success() const
	{
		return status_code >= 200 && status_code < 300;
	}
};

template <typename T>
struct api_response
{
	int status_code;
	T data;
};

struct api_error
{
	int status_code;
	std::string message;

	bool server_error() const
	{
		return status_code >= 500 && status_code < 600;
	}
};

template <typename T>
static std::optional<std::string> try_deserialize(T& data, const json& json)
{
	try
	{
		data = json;
		return std::nullopt;
	}
	catch (json::exception& ex)
	{
		return ex.what();
	}
}

enum class state {
	untracked,
	ignored,
	try_later,
	tracked,	
};
NLOHMANN_JSON_SERIALIZE_ENUM(state, {
    {state::untracked, nullptr},
    {state::untracked, "untracked"},
    {state::ignored, "ignored"},
	{state::try_later, "try_later"},
    {state::tracked, "tracked"},
})

struct status_entry_t
{
	std::string key;
	state state;
	int64_t time;
	std::string error;

	void try_later(int when)
	{
		state = state::try_later;
		time = when;
	}

	void ignore()
	{
		state = state::ignored;
	}
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(status_entry_t, key, state, time, error)

static std::map<std::string, status_entry_t> status;

// TODO: refactor to http module, reuse in zupdater.
namespace http
{
	static size_t _write_callback(char *contents, size_t size, size_t nmemb, void *userp)
	{
		size_t total_size = size * nmemb;
		std::string* str = (std::string*)userp;
		str->insert(str->end(), contents, contents + total_size);
		return total_size;
	}

	static expected<http_response, std::string> get(std::string url)
	{
		http_response response{};

		CURL *curl_handle = curl_easy_init();
		curl_easy_setopt(curl_handle, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, _write_callback);
		curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&response.body);
		curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, UA.c_str());
		CURLcode res = curl_easy_perform(curl_handle);

		if (res != CURLE_OK)
		{
			std::string error = fmt::format("curl: {}", curl_easy_strerror(res));
			fmt::println(stderr, "{}", error);
			return make_unexpected(error);
		}

		curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &response.status_code);
		curl_easy_cleanup(curl_handle);

		return response;
	}

	static expected<http_response, std::string> upload(std::string url, fs::path path)
	{
		http_response response{};

		FILE* fd = fopen(path.string().c_str(), "rb");
		if (!fd)
			return make_unexpected("can't read file");

		struct stat file_info;
		if (fstat(fileno(fd), &file_info) != 0)
		{
			fclose(fd);
			return make_unexpected("can't stat file");
		}

		CURL *curl_handle = curl_easy_init();
		curl_easy_setopt(curl_handle, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl_handle, CURLOPT_UPLOAD, 1);
		curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, _write_callback);
		curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&response.body);
		// TODO ! windows cant do this
		curl_easy_setopt(curl_handle, CURLOPT_READDATA, fd);
		// curl_easy_setopt(curl_handle, CURLOPT_READFUNCTION, );
		curl_easy_setopt(curl_handle, CURLOPT_INFILESIZE_LARGE, (curl_off_t)file_info.st_size);
		curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, UA.c_str());
		CURLcode res = curl_easy_perform(curl_handle);
		fclose(fd);

		if (res != CURLE_OK)
		{
			std::string error = fmt::format("curl: {}", curl_easy_strerror(res));
			fmt::println(stderr, "{}", error);
			return make_unexpected(error);
		}

		curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &response.status_code);
		curl_easy_cleanup(curl_handle);

		return response;
	}
};

static int64_t get_last_write_time(fs::path path)
{
	auto file_time = std::filesystem::last_write_time(path);
	return std::chrono::duration_cast<std::chrono::seconds>(
		file_time.time_since_epoch()).count();
}

namespace api_client
{
	static std::string api_endpoint;

	template <typename T>
	static auto _parse_response(const expected<http_response, std::string>& response) -> expected<api_response<T>, api_error>
	{
		if (!response)
			return make_unexpected(api_error{0, response.error()});

		json json = json::parse(response->body, nullptr, false);
		if (json.is_discarded())
			return make_unexpected(api_error{response->status_code, "invalid json"});

		if (!response->success())
		{
			std::string error;
			if (json.contains("error") && json["error"].is_string())
				error = json["error"].template get<std::string>();
			else
				error = "server error";
			return make_unexpected(api_error{response->status_code, error});
		}

		T data;
		if (auto error = try_deserialize(data, json))
			return make_unexpected(api_error{response->status_code, *error});

		return api_response<T>{response->status_code, data};
	}

	struct quest {
		std::string hash;
	};
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(quest, hash)
	using quests_result = std::vector<quest>;

	static auto quests()
	{
		std::string url = fmt::format("{}/api/v1/quests", api_endpoint);
		return _parse_response<quests_result>(http::get(url));
	}

	struct replay_length_result {
		// -1 if replay guid has not been uploaded yet.
		int length;
	};
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(replay_length_result, length)

	static auto replay_length(std::string guid)
	{
		std::string url = fmt::format("{}/api/v1/replays/{}/length", api_endpoint, guid);
		return _parse_response<replay_length_result>(http::get(url));
	}

	struct upload_result {
		std::string key;
	};
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(upload_result, key)

	static auto replays_upload(fs::path path)
	{
		std::string url = fmt::format("{}/api/v1/replays", api_endpoint);
		return _parse_response<upload_result>(http::upload(url, path));
	}
}

static std::optional<api_client::quests_result> quests;

static fs::path get_status_path()
{
	fs::path replay_file_dir = zc_get_config("zeldadx", "replay_file_dir", "replays/");
	return replay_file_dir / "status.json";
}

bool replay_upload_auto_enabled()
{
	return zc_get_config("zeldadx", "replay_upload", false);
}

static bool should_process_replay(status_entry_t& status_entry, fs::path path, int64_t now_time)
{
	if (status_entry.state == state::ignored)
		return false;

	switch (status_entry.state)
	{
		case state::untracked:
		{
			return true;
		}
		break;

		case state::try_later:
		{
			return now_time > status_entry.time;
		}
		break;

		case state::tracked:
		{
			int last_write_time = get_last_write_time(path);
			return last_write_time > status_entry.time && now_time - status_entry.time > TIME_ONE_WEEK;
		}
		break;
	}

	return false;
}

static bool process_replay(status_entry_t& status_entry, fs::path path, std::string rel_fname, int64_t now_time)
{
	status_entry = {};
	status_entry.time = now_time;

	replay_load_meta(path);
	std::string replay_guid = replay_get_meta_str("guid");
	std::string qst_hash = replay_get_meta_str("qst_hash");
	int replay_length = replay_get_meta_int("length", -1);
	if (replay_guid.empty() || qst_hash.empty() || replay_length == -1)
	{
		status_entry.ignore();
		status_entry.error = "replay is too old";
		return false;
	}

	status_entry.key = fmt::format("{}/{}.zplay", qst_hash, replay_guid);

	// Check if the server has this qst.
	bool known_qst = false;
	for (auto& quest : *quests)
	{
		if (quest.hash == qst_hash)
		{
			known_qst = true;
			break;
		}
	}

	if (!known_qst)
	{
		status_entry.try_later(now_time + TIME_ONE_WEEK * 2);
		status_entry.error = "qst is not in database";
		return false;
	}

	if (auto r = api_client::replay_length(replay_guid); !r)
	{
		auto& error = r.error();
		status_entry.error = error.message;
		if (error.server_error() || error.status_code == 0)
			status_entry.try_later(now_time + TIME_ONE_DAY);
		else
			status_entry.ignore();
		return false;
	}
	else if (r->data.length == replay_length)
	{
		// Server already has the latest.
		status_entry.state = state::tracked;
		return true;
	}

	if (auto r = api_client::replays_upload(path); !r)
	{
		auto& error = r.error();
		status_entry.error = error.message;
		if (error.server_error() || error.status_code == 0)
			status_entry.try_later(now_time + TIME_ONE_DAY);
		else
			status_entry.ignore();
		return false;
	}

	status_entry.state = state::tracked;
	return true;
}

// TODO ! expected<>
int replay_upload()
{
	Z_message("Checking for replays to upload ...\n");

	api_client::api_endpoint = zc_get_config("zeldadx", "api_endpoint", "");
	if (api_client::api_endpoint.empty())
	{
		Z_message("api_endpoint not set\n");
		return 0;
	}

	fs::path replay_file_dir = zc_get_config("zeldadx", "replay_file_dir", "replays/");
	if (!fs::exists(replay_file_dir))
	{
		Z_message("No replays found\n");
		return 0;
	}

	if (!quests)
	{
		if (auto r = api_client::quests(); !r)
		{
			Z_message("Error fetching quests: %s\n", r.error().message.c_str());
			return 0;
		}
		else
		{
			quests = std::move(r->data);
		}
	}

	fs::path status_path = get_status_path();
	if (fs::exists(status_path))
	{
		std::ifstream f(status_path);
		json j = json::parse(f, nullptr, false);
		if (j.is_discarded())
		{
			Z_message("invalid json: %s\n", status_path.string().c_str());
			return 0;
		}
		if (auto error = try_deserialize(status, j))
		{
			Z_message("invalid json: %s\n", error.value().c_str());
			return 0;
		}
	}
	else
	{
		status.clear();
	}

	int replays_uploaded = 0;
	int64_t now_time = std::chrono::duration_cast<std::chrono::seconds>(
		std::chrono::system_clock::now().time_since_epoch()).count();

	for (const auto& entry : fs::recursive_directory_iterator(replay_file_dir))
	{
		if (!entry.is_regular_file() || entry.path().extension() != ".zplay")
			continue;

		std::string rel_fname = fs::relative(entry.path(), replay_file_dir).make_preferred().string();
		auto& status_entry = status[rel_fname];
		if (!should_process_replay(status_entry, entry.path(), now_time))
			continue;

		if (process_replay(status_entry, entry.path(), rel_fname, now_time))
		{
			replays_uploaded++;
			Z_message("[%s] Success\n", rel_fname.c_str());
		}
		else
		{
			Z_message("[%s] Failed - %s\n", rel_fname.c_str(), status_entry.error.c_str());
		}
	}

	std::ofstream out(status_path, std::ios::binary);
	json j = status;
	out << j.dump(2);

	Z_message("Uploaded %d replays.\n", replays_uploaded);
	return replays_uploaded;
}

void replay_upload_clear_cache()
{
	fs::path status_path = get_status_path();
	std::error_code ec;
	fs::remove(status_path, ec);
	if (ec)
	{
		std::string error = fmt::format("Failed to clear cache: {}", std::strerror(ec.value()));
		Z_message("%s\n", error.c_str());
	}
}
