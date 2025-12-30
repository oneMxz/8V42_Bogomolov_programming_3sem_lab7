#include "Hash.h"
#include <boost/crc.hpp>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <algorithm>
#include <unordered_set>

Hash::Hash(size_t block_size) : block_size(block_size) {}

// Конструктор - НЕ читает с диска!
Hash::FileHandle::FileHandle(const boost::filesystem::path& file_path)
    : path(file_path), size(0), stream(nullptr), block_cache() {
    // Пусто - ничего не читаем и не открываем!
}

// Ленивое получение размера файла
uintmax_t Hash::FileHandle::get_size() const {
    if (size == 0) {  // Еще не читали размер
        try {
            size = boost::filesystem::file_size(path);
        } catch (const std::exception&) {
            size = 0;
        }
    }
    return size;
}

// Ленивое открытие файла
void Hash::FileHandle::ensure_opened() const {
    if (!stream) {  // Еще не открывали файл
        stream = std::make_unique<std::ifstream>(path.string(), std::ios::binary);
        if (!stream->is_open()) {
            stream.reset();
        }
    }
}

// Ленивое получение потока
std::ifstream& Hash::FileHandle::get_stream() const {
    ensure_opened();  // Открываем файл при первом обращении
    return *stream;
}

// Закрытие потока
void Hash::FileHandle::close_stream() {
    if (stream) {
        stream->close();
    }
}

bool Hash::FileHandle::is_valid() const {
    return get_size() > 0;  // Вызовет ленивое чтение размера
}

void Hash::FileHandle::close() { 
    stream.reset(); 
    size = 0;
    block_cache.clear();
}

std::string Hash::hash_crc32(const std::string &data) {
    boost::crc_32_type crc_calculator;
    crc_calculator.process_bytes(data.data(), data.size());
    uint32_t checksum = crc_calculator.checksum();

    std::stringstream ss;
    ss << std::hex << std::setw(8) << std::setfill('0') << checksum;
    return ss.str();
}

std::string Hash::get_block_hash(FileHandle& handle, size_t block_index) {
    // Проверяем кэш
    auto it = handle.block_cache.find(block_index);
    if (it != handle.block_cache.end()) {
        return it->second;
    }

    // Ленивая проверка валидности
    if (!handle.is_valid()) {
        return "";
    }

    uintmax_t file_size = handle.get_size();  // Уже кэширован после is_valid()
    
    // Позиционируемся на нужный блок
    std::ifstream& file = handle.get_stream();  // Откроет файл только сейчас!
    if (!file.is_open()) {
        return "";
    }
    
    file.clear();
    uintmax_t file_pos = block_index * block_size;
    
    if (file_pos >= file_size) {
        return "";
    }
    
    file.seekg(file_pos, std::ios::beg);

    // Определяем сколько байт читать
    uintmax_t bytes_to_read = std::min(block_size, file_size - file_pos);
    
    if (bytes_to_read == 0) {
        return "";
    }

    // Читаем данные
    std::vector<char> buffer(bytes_to_read);
    file.read(buffer.data(), bytes_to_read);
    std::streamsize bytes_read = file.gcount();

    if (bytes_read == 0) {
        return "";
    }

    // Создаём полный блок размера S с нулями
    std::vector<char> full_block(block_size, '\0');
    std::copy(buffer.begin(), buffer.begin() + bytes_read, full_block.begin());

    // Вычисляем хеш
    std::string hash = hash_crc32(std::string(full_block.data(), block_size));

    // Кэшируем результат
    handle.block_cache[block_index] = hash;
    return hash;
}

bool Hash::compare_handles_from_block(FileHandle& handle1, FileHandle& handle2,
                                     size_t start_block) {
    // Ленивое сравнение размеров
    if (handle1.get_size() != handle2.get_size()) {
        return false;
    }

    // Вычисляем количество блоков
    size_t total_blocks = (handle1.get_size() + block_size - 1) / block_size;

    // Сравниваем блок за блоком
    for (size_t block_idx = start_block; block_idx < total_blocks; block_idx++) {
        std::string hash1 = get_block_hash(handle1, block_idx);
        std::string hash2 = get_block_hash(handle2, block_idx);

        if (hash1.empty() || hash2.empty()) {
            return false;
        }

        if (hash1 != hash2) {
            return false;
        }
    }

    return true;
}

std::vector<std::vector<boost::filesystem::path>>
Hash::find_real_duplicates_lazy(const std::vector<std::vector<boost::filesystem::path>>& size_groups) {
    std::vector<std::vector<boost::filesystem::path>> result;

    for (const auto& group : size_groups) {
        if (group.size() < 2) continue;

        // Вектор для отслеживания обработанных файлов
        std::vector<bool> processed(group.size(), false);

        for (size_t i = 0; i < group.size(); ++i) {
            if (processed[i]) continue;

            // Создаем FileHandle для текущего файла ТОЛЬКО СЕЙЧАС
            std::unique_ptr<FileHandle> handle_i = std::make_unique<FileHandle>(group[i]);
            if (!handle_i->is_valid()) {
                processed[i] = true;
                continue;
            }

            // Группа дубликатов для текущего файла
            std::vector<boost::filesystem::path> duplicate_group;
            duplicate_group.push_back(group[i]);

            // Сравниваем с остальными файлами
            for (size_t j = i + 1; j < group.size(); ++j) {
                if (processed[j]) continue;

                // Создаем FileHandle для второго файла ТОЛЬКО СЕЙЧАС
                std::unique_ptr<FileHandle> handle_j = std::make_unique<FileHandle>(group[j]);
                if (!handle_j->is_valid()) {
                    processed[j] = true;
                    continue;
                }

                // Сравниваем файлы
                if (compare_handles_from_block(*handle_i, *handle_j, 0)) {
                    duplicate_group.push_back(group[j]);
                    processed[j] = true;
                    // Закрываем поток второго файла, так как он больше не понадобится
                    handle_j->close_stream();
                } else {
                    // Закрываем поток второго файла, чтобы не держать открытым
                    handle_j->close_stream();
                }
            }

            // Если нашли дубликаты
            if (duplicate_group.size() > 1) {
                result.push_back(std::move(duplicate_group));
            }

            processed[i] = true;
            // Закрываем поток текущего файла
            handle_i->close_stream();
        }
    }

    return result;
}