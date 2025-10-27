#ifndef QUERY_PDB_SERVER_DOWNLOADER_H
#define QUERY_PDB_SERVER_DOWNLOADER_H

#include <string>
#include <mutex>
#include <filesystem>

class downloader {
public:
    downloader(std::string path, std::string server);

    bool valid() const;

    bool download(const std::string &name, const std::string &guid, uint32_t age);

    std::filesystem::path
    get_path(const std::string &name, const std::string &guid, uint32_t age);

private:
    bool valid_;
    std::string path_;
    std::string server_;
    std::pair<std::string, std::string> server_split_;
    std::mutex mutex_;

    static std::string
    get_relative_path_str(const std::string &name, const std::string &guid, uint32_t age);

    std::pair<std::string, std::string> split_server_name();

    bool is_valid_pdb(const std::string &name, const std::filesystem::path &path);
};

#endif //QUERY_PDB_SERVER_DOWNLOADER_H
