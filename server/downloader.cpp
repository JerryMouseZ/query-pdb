#include <algorithm>
#include <cctype>
#include <sstream>
#include <filesystem>
#include <regex>
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <httplib.h>
#include <spdlog/spdlog.h>
#include "pdb_parser.h"
#include "downloader.h"

downloader::downloader(std::string path, std::string server)
        : valid_(false),
          path_(std::move(path)),
          server_(std::move(server)) {

    spdlog::info("create downloader, path: {}, server: {}", path_, server_);
    if (server_.empty() || path_.empty()) {
        spdlog::error("invalid downloader, path: {}, server: {}", path_, server_);
        return;
    }
    if (server_.back() != '/') {
        server_.push_back('/');
    }

    server_split_ = split_server_name();
    if (server_split_.first.empty()) {
        spdlog::error("split server name failed, server: {}", server_);
        return;
    }

    valid_ = true;
}

bool downloader::valid() const {
    return valid_;
}

bool downloader::download(const std::string &name, const std::string &guid, uint32_t age) {
    std::lock_guard lock(mutex_);

    std::string relative_path = get_relative_path_str(name, guid, age);
    auto path = std::filesystem::path(path_).append(relative_path);

    spdlog::info("lookup pdb, path: {}", relative_path);

    if (std::filesystem::exists(path)) {
        spdlog::info("pdb already exists, path: {}", relative_path);
        return true;
    }

    spdlog::info("download pdb, path: {}", relative_path);

    std::string buf;
    httplib::Client client(server_split_.first);
    client.set_follow_location(true);
    auto res = client.Get(server_split_.second + relative_path);
    if (!res || res->status != 200) {
        spdlog::error("failed to download pdb, path: {}", relative_path);
        return false;
    }

    // download file size check
    size_t content_length = 0;
    if (res->has_header("Content-Length")) {
        content_length = std::stoul(res->get_header_value("Content-Length"));
    }
    if (content_length == 0 || content_length != res->body.size()) {
        spdlog::error("downloaded pdb size mismatch, path: {}", relative_path);
        return false;
    }

    std::filesystem::create_directories(path.parent_path());
    auto tmp_path = path;
    tmp_path.replace_extension(".tmp");
    std::ofstream f(tmp_path, std::ios::binary);
    if (!f.is_open()) {
        spdlog::error("failed to open file, path: {}", tmp_path.string());
        return false;
    }
    f.write(res->body.c_str(), static_cast<std::streamsize>(res->body.size()));
    f.close();

    if (!is_valid_pdb(name, tmp_path)) {
        spdlog::error("downloaded pdb file is invalid, path: {}", relative_path);
        return false;
    }

    std::filesystem::rename(tmp_path, path);
    spdlog::info("download pdb success, path: {}", relative_path);
    return true;
}

static std::string to_upper(const std::string &s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(), toupper);
    return s;
}

static std::string to_lower(const std::string &s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(), tolower);
    return s;
}

std::string
downloader::get_relative_path_str(const std::string &name, const std::string &guid, uint32_t age) {
    std::stringstream ss;
    ss << std::hex;
    ss << to_lower(name) << '/' << to_upper(guid) << age << '/' << to_lower(name);
    return ss.str();
}

std::filesystem::path
downloader::get_path(const std::string &name, const std::string &guid, uint32_t age) {
    std::string relative_path = get_relative_path_str(name, guid, age);
    auto path = std::filesystem::path(path_).append(relative_path);
    return path;
}

std::pair<std::string, std::string> downloader::split_server_name() {
    std::regex regex(R"(^((?:(?:http|https):\/\/)?[^\/]+)(\/.*)$)");
    std::smatch match;

    if (!std::regex_match(server_, match, regex)) {
        return {};
    }

    return {match[1].str(), match[2].str()};
}

bool downloader::is_valid_pdb(const std::string &name, const std::filesystem::path &path) {
    pdb_parser parser(path.string());
    pdb_stats stats = parser.get_stats();

    std::string lower_name = to_lower(name);
    if (lower_name == "ntoskrnl.pdb" || lower_name == "ntkrnlmp.pdb") {
        // ignore kernel pdb file with no type info
        if (stats.type_count == 0) {
            return false;
        }
    }

    return true;
}
