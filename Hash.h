#pragma once

#include <boost/filesystem.hpp>
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <fstream>

class Hash {
private:
    size_t block_size;

    // Структура для хранения информации о файле
    struct FileHandle {
        boost::filesystem::path path;           // первый - инициализируется первым
        uintmax_t size;                         // второй
        std::unique_ptr<std::ifstream> stream;  // третий
        std::unordered_map<size_t, std::string> block_cache;  // четвертый
    
        FileHandle(const boost::filesystem::path& file_path);
    
        bool is_valid() const { return stream && stream->is_open(); }
        void close() { stream.reset(); }
    };

    // Хеш-функция CRC32
    std::string hash_crc32(const std::string &data);
    
    // Получить хеш блока (с кэшированием)
    std::string get_block_hash(FileHandle& handle, size_t block_index);
    
    // Сравнить два файла, начиная с определенного блока
    bool compare_handles_from_block(FileHandle& handle1, FileHandle& handle2,
                                   size_t start_block = 0);

public:
    Hash(size_t block_size);
    
    // Основной метод - находит настоящие дубликаты
    std::vector<std::vector<boost::filesystem::path>>
    find_real_duplicates(const std::vector<std::vector<boost::filesystem::path>>& size_groups);
    
    // Оптимизированная версия для больших групп
    std::vector<std::vector<boost::filesystem::path>>
    find_real_duplicates_optimized(const std::vector<std::vector<boost::filesystem::path>>& size_groups);
};