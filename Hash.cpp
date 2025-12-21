#include "Hash.h"
#include <boost/crc.hpp>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <algorithm>
#include <unordered_set>

Hash::Hash(size_t block_size) : block_size(block_size) {}

Hash::FileHandle::FileHandle(const boost::filesystem::path& file_path)
    : path(file_path), size(0), stream(), block_cache() {
    try {
        size = boost::filesystem::file_size(file_path);
        stream = std::make_unique<std::ifstream>(file_path.string(), std::ios::binary);
        if (!stream->is_open()) {
            stream.reset();
        }
    } catch (const std::exception&) {
        stream.reset();
    }
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

    if (!handle.is_valid()) {
        return "";
    }

    // Позиционируемся на нужный блок
    std::ifstream& file = *handle.stream;
    file.clear();
    uintmax_t file_pos = block_index * block_size;
    
    if (file_pos >= handle.size) {
        return "";
    }
    
    file.seekg(file_pos, std::ios::beg);

    // Определяем сколько байт читать
    uintmax_t bytes_to_read = std::min(block_size, handle.size - file_pos);
    
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
    if (handle1.size != handle2.size) {
        return false;
    }

    // Вычисляем количество блоков
    size_t total_blocks = (handle1.size + block_size - 1) / block_size;

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
Hash::find_real_duplicates(const std::vector<std::vector<boost::filesystem::path>>& size_groups) {
    std::vector<std::vector<boost::filesystem::path>> result;

    for (const auto& group : size_groups) {
        if (group.size() < 2) continue;

        // Создаем хэндлы для всех файлов
        std::vector<FileHandle> handles;
        handles.reserve(group.size());

        for (const auto& path : group) {
            handles.emplace_back(path);
        }

        // Вектор для отслеживания обработанных файлов
        std::vector<bool> processed(handles.size(), false);

        // Проходим по всем файлам
        for (size_t i = 0; i < handles.size(); ++i) {
            if (processed[i] || !handles[i].is_valid()) {
                continue;
            }

            // Группа дубликатов для текущего файла
            std::vector<boost::filesystem::path> duplicate_group;
            duplicate_group.push_back(group[i]);

            // Ищем дубликаты среди оставшихся файлов
            for (size_t j = i + 1; j < handles.size(); ++j) {
                if (processed[j] || !handles[j].is_valid()) {
                    continue;
                }

                if (compare_handles_from_block(handles[i], handles[j], 0)) {
                    duplicate_group.push_back(group[j]);
                    processed[j] = true;
                }
            }

            // Если нашли дубликаты
            if (duplicate_group.size() > 1) {
                result.push_back(std::move(duplicate_group));
            }

            processed[i] = true;
        }
    }

    return result;
}

std::vector<std::vector<boost::filesystem::path>>
Hash::find_real_duplicates_optimized(const std::vector<std::vector<boost::filesystem::path>>& size_groups) {
    std::vector<std::vector<boost::filesystem::path>> result;

    for (const auto& group : size_groups) {
        if (group.size() < 2) continue;

        // Создаем хэндлы
        std::vector<std::unique_ptr<FileHandle>> handles;
        for (const auto& path : group) {
            handles.push_back(std::make_unique<FileHandle>(path));
        }

        // Группы дубликатов
        std::vector<std::vector<size_t>> duplicate_indices;

        // Для каждого файла ищем дубликаты
        for (size_t i = 0; i < handles.size(); ++i) {
            if (!handles[i]->is_valid()) continue;

            bool found_group = false;
            
            // Проверяем существующие группы
            for (auto& dup_group : duplicate_indices) {
                size_t representative = dup_group[0];
                
                if (compare_handles_from_block(*handles[representative], *handles[i], 0)) {
                    dup_group.push_back(i);
                    found_group = true;
                    break;
                }
            }
            
            // Если не нашли группу, создаем новую
            if (!found_group) {
                duplicate_indices.push_back({i});
            }
        }

        // Преобразуем индексы в пути
        for (const auto& indices : duplicate_indices) {
            if (indices.size() > 1) {
                std::vector<boost::filesystem::path> duplicate_group;
                for (size_t idx : indices) {
                    duplicate_group.push_back(group[idx]);
                }
                result.push_back(std::move(duplicate_group));
            }
        }
    }

    return result;
}