#include "ScanDir.h"
#include "Hash.h"
#include <iostream>
#include <vector>
#include <boost/program_options.hpp>

namespace po = boost::program_options;

int main(int argc, char* argv[]) {
    try {
        // Парсинг аргументов командной строки
        po::options_description desc("Bayan - Duplicate File Finder");
        desc.add_options()
            ("help,h", "Show help message")
            ("include,i", po::value<std::vector<std::string>>()->multitoken()->required(),
                "Directories to scan (required, multiple allowed)")
            ("exclude,e", po::value<std::vector<std::string>>()->multitoken(),
                "Directories to exclude from scanning")
            ("level,l", po::value<int>()->default_value(0),
                "Scan depth (0 = only specified directory)")
            ("min-size,m", po::value<size_t>()->default_value(1),
                "Minimum file size in bytes")
            ("mask,M", po::value<std::vector<std::string>>()->multitoken(),
                "File masks (case-insensitive, multiple allowed)")
            ("block-size,b", po::value<size_t>()->default_value(4096),
                "Block size for reading files in bytes")
            ("hash,H", po::value<std::string>()->default_value("crc32"),
                "Hash algorithm (crc32)");

        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);

        if (vm.count("help")) {
            std::cout << desc << std::endl;
            return 0;
        }

        po::notify(vm);

        // Получаем параметры
        std::vector<std::string> include_dirs = vm["include"].as<std::vector<std::string>>();
        std::vector<std::string> exclude_dirs;
        if (vm.count("exclude")) {
            exclude_dirs = vm["exclude"].as<std::vector<std::string>>();
        }

        int level = vm["level"].as<int>();
        size_t min_file_size = vm["min-size"].as<size_t>();
        size_t block_size = vm["block-size"].as<size_t>();
        
        std::vector<std::string> masks;
        if (vm.count("mask")) {
            masks = vm["mask"].as<std::vector<std::string>>();
        }

        // Шаг 1: Сканирование директорий
        std::cout << "Scanning directories..." << std::endl;
        ScannerDirectory scanner(level, min_file_size, masks, exclude_dirs);
        auto all_files = scanner.scan_directories(include_dirs);
        
        if (all_files.empty()) {
            std::cout << "No files found matching criteria." << std::endl;
            return 0;
        }
        
        std::cout << "Found " << all_files.size() << " file(s)." << std::endl;

        // Шаг 2: Группировка по размеру
        auto size_groups = scanner.get_duplicate_groups_by_size(all_files);
        std::cout << "Found " << size_groups.size() << " group(s) of files with same size." << std::endl;

        // Шаг 3: Поиск реальных дубликатов
        std::cout << "Comparing file contents..." << std::endl;
        Hash hash(block_size);
        auto duplicates = hash.find_real_duplicates(size_groups);

        // Шаг 4: Вывод результатов
        if (duplicates.empty()) {
            std::cout << "No duplicates found." << std::endl;
        } else {
            std::cout << "\nFound " << duplicates.size() << " group(s) of duplicates:\n" << std::endl;
            for (const auto& group : duplicates) {
                for (const auto& file : group) {
                    std::cout << file.string() << std::endl;
                }
                std::cout << std::endl;
            }
        }

    } catch (const po::required_option& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        std::cerr << "Use --help for usage information." << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error:" << e.what() << std::endl;
        return 1;
    }

    return 0;
}