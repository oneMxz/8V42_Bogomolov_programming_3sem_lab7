#include "ScanDir.h"
#include <vector>
#include <string>
#include <boost/filesystem.hpp>
#include <algorithm>
#include <cctype>
#include <functional>
#include <iostream>

ScannerDirectory::ScannerDirectory(int max_level_scan,
                size_t min_file_size,
                const std::vector<std::string>& masks,
                const std::vector<std::string>& exclude_dirs):
                    max_level_scan(max_level_scan),
                    min_file_size_(min_file_size),
                    masks_(masks),
                    exclude_dirs_(exclude_dirs) {}

bool ScannerDirectory::is_excluded(const boost::filesystem::path& path) {
    std::string string_path = path.string();
    for(const auto& excluded_dir: exclude_dirs_) {
        std::string excluded_lower = excluded_dir;
        std::transform(excluded_lower.begin(), excluded_lower.end(),
                      excluded_lower.begin(), ::tolower);
        
        std::string path_lower = string_path;
        std::transform(path_lower.begin(), path_lower.end(),
                      path_lower.begin(), ::tolower);
        
        if (path_lower.find(excluded_lower) == 0) {
            return true;
        }
    }
    return false;
}

bool ScannerDirectory::matches_mask(const std::string& filename) {
    if (masks_.empty()) {
        return true;
    }

    std::string filename_lower = filename;
    std::transform(filename_lower.begin(), filename_lower.end(),
                   filename_lower.begin(), ::tolower);

    for (const auto& mask : masks_) {
        std::string mask_lower = mask;
        std::transform(mask_lower.begin(), mask_lower.end(),
                       mask_lower.begin(), ::tolower);

        if (mask_lower == "*") {
            return true;
        }

        if (mask_lower.size() > 0 && mask_lower[0] == '*') {
            std::string extension = mask_lower.substr(1);
            
            if (filename_lower.size() >= extension.size()) {
                std::string file_ending = filename_lower.substr(
                    filename_lower.size() - extension.size()
                );
                
                if (file_ending == extension) {
                    return true;
                }
            }
        }
        else if (mask_lower.size() > 0 && mask_lower[mask_lower.size() - 1] == '*') {
            std::string prefix = mask_lower.substr(0, mask_lower.size() - 1);
            
            if (filename_lower.size() >= prefix.size()) {
                std::string file_prefix = filename_lower.substr(0, prefix.size());
                
                if (file_prefix == prefix) {
                    return true;
                }
            }
        }
        else if (filename_lower == mask_lower) {
            return true;
        }
    }

    return false;
}

std::vector<boost::filesystem::path> ScannerDirectory::scan_single_directory(
    const boost::filesystem::path& dir_path) {
    
    std::vector<boost::filesystem::path> found_files;
    
    if (!boost::filesystem::exists(dir_path) || 
        !boost::filesystem::is_directory(dir_path) ||
        is_excluded(dir_path)) {
        return found_files;
    }

    std::function<void(const boost::filesystem::path&, int)> scan_recursive;
    
    scan_recursive = [&](const boost::filesystem::path& current_dir, int current_depth) {
        if (max_level_scan >= 0 && current_depth > max_level_scan) {
            return;
        }

        try {
            for (const auto& entry : boost::filesystem::directory_iterator(current_dir)) {
                try {
                    if (boost::filesystem::is_directory(entry.status())) {
                        if (!is_excluded(entry.path())) {
                            scan_recursive(entry.path(), current_depth + 1);
                        }
                    }
                    else if (boost::filesystem::is_regular_file(entry.status())) {
                        try {
                            auto file_size = boost::filesystem::file_size(entry.path());
                            if (file_size >= min_file_size_) {
                                std::string filename = entry.path().filename().string();
                                if (matches_mask(filename)) {
                                    found_files.push_back(entry.path());
                                }
                            }
                        } catch (const boost::filesystem::filesystem_error&) {
                            // Пропускаем файлы с ошибками доступа к размеру
                            continue;
                        }
                    }
                } catch (const boost::filesystem::filesystem_error&) {
                    continue;
                }
            }
        } catch (const boost::filesystem::filesystem_error& e) {
            std::cerr << "Warning: Cannot access " << current_dir << ": " << e.what() << std::endl;
        }
    };

    scan_recursive(dir_path, 0);
    return found_files;
}

std::vector<boost::filesystem::path> ScannerDirectory::scan_directories(
    const std::vector<std::string>& dirs_to_scan) {
    
    std::vector<boost::filesystem::path> all_files;
    
    for (const auto& dir_str : dirs_to_scan) {
        boost::filesystem::path dir_path(dir_str);
        auto files = scan_single_directory(dir_path);
        all_files.insert(all_files.end(), files.begin(), files.end());
    }
    
    return all_files;
}

std::unordered_map<uintmax_t, std::vector<boost::filesystem::path>> 
ScannerDirectory::group_files_by_size(const std::vector<boost::filesystem::path>& files) {
    
    std::unordered_map<uintmax_t, std::vector<boost::filesystem::path>> size_map;
    
    for (const auto &file : files) {
        try {
            if (boost::filesystem::exists(file) && boost::filesystem::is_regular_file(file)) {
                uintmax_t size = boost::filesystem::file_size(file);
                size_map[size].push_back(file);
            }
        } catch (const boost::filesystem::filesystem_error&) {
            continue;
        }
    }
    return size_map;
}

std::vector<std::vector<boost::filesystem::path>> 
ScannerDirectory::get_duplicate_groups_by_size(const std::vector<boost::filesystem::path>& files) {
    
    auto size_map = group_files_by_size(files);
    std::vector<std::vector<boost::filesystem::path>> result;
    
    for (const auto &pair : size_map) {
        if (pair.second.size() > 1) {
            result.push_back(pair.second);
        }
    }
    
    return result;
}