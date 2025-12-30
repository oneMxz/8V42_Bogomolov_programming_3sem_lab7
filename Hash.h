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
        boost::filesystem::path path;                      // Путь к файлу
        mutable uintmax_t size;                            // Размер файла (ленивое чтение)
        mutable std::unique_ptr<std::ifstream> stream;     // Поток для чтения (ленивое открытие)
        mutable std::unordered_map<size_t, std::string> block_cache;  // Кэш вычисленных хешей блоков
    
        FileHandle(const boost::filesystem::path& file_path);
    
        // Ленивые методы доступа
        uintmax_t get_size() const;
        std::ifstream& get_stream() const;
        void ensure_opened() const;
        void close_stream();
        
        bool is_valid() const;
        void close();
    };

    // Хеш-функция CRC32
    std::string hash_crc32(const std::string &data);
    
    // Получить хеш блока (с ленивым чтением и кэшированием)
    std::string get_block_hash(FileHandle& handle, size_t block_index);
    
    // Сравнить два файла, начиная с определенного блока
    bool compare_handles_from_block(FileHandle& handle1, FileHandle& handle2,
                                   size_t start_block = 0);

public:
    Hash(size_t block_size);
    
    // Основной метод - находит настоящие дубликаты с ленивым чтением
    std::vector<std::vector<boost::filesystem::path>>
    find_real_duplicates_lazy(const std::vector<std::vector<boost::filesystem::path>>& size_groups);
};