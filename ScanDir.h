#pragma once

#include <vector>
#include <string>
#include <boost/filesystem.hpp>
#include <unordered_map>

class ScannerDirectory {
private:
    int max_level_scan;
    size_t min_file_size_;
    std::vector<std::string> masks_;
    std::vector<std::string> exclude_dirs_;

    bool is_excluded(const boost::filesystem::path& path);
    bool matches_mask(const std::string& filename);

public:
    ScannerDirectory(int max_level_scan,
                    size_t min_file_size,
                    const std::vector<std::string>& masks,
                    const std::vector<std::string>& exclude_dirs);

    // Сканирование одной директории
    std::vector<boost::filesystem::path> scan_single_directory(const boost::filesystem::path& dir_path);
    
    // Сканирование нескольких директорий
    std::vector<boost::filesystem::path> scan_directories(const std::vector<std::string>& dirs_to_scan);
    
    // Группировка файлов по размеру
    std::unordered_map<uintmax_t, std::vector<boost::filesystem::path>> 
    group_files_by_size(const std::vector<boost::filesystem::path>& files);
    
    // Получение групп потенциальных дубликатов (одинаковый размер)
    std::vector<std::vector<boost::filesystem::path>> 
    get_duplicate_groups_by_size(const std::vector<boost::filesystem::path>& files);
};