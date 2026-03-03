#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <cstdint>
#include <stdexcept>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

struct ListNode {
    ListNode* prev = nullptr;
    ListNode* next = nullptr;
    ListNode* rand = nullptr;
    std::string data;
};

/*
 * Бинарный формат файла на выходе (big-endian, без паддинга):
 *
 * 1. Заголовок (4 байта):
 *    uint32_t node_count — количество узлов в списке
 *
 * 2. Для каждого из node_count узлов:
 *    • uint32_t data_len — длина строки data (big-endian)
 *    • data_len байт — сами данные (UTF-8, без '\0')
 *    • int32_t prev_idx — индекс предыдущего узла или -1
 *    • int32_t next_idx — индекс следующего узла или -1
 *    • int32_t rand_idx — индекс произвольного узла (rand) или -1
 *
 */

// Чтение из файла
ListNode* build_from_file(const std::string& filename) {
    // Открываем текстовый файл inlet.in и читаем его построчно
    std::ifstream in(filename);
    if (!in.is_open()) {
        std::cerr << "Error opening input file: " << filename << std::endl;
        return nullptr;
    }

    // Временные массивы для хранения данных и индексов rand
    std::vector<std::string> datas;
    std::vector<int> rand_indices;
    std::string line;

    // Первый проход: парсим каждую строку формата "data;rand_index"
    while (std::getline(in, line)) {
        
        if (line.empty()) continue;
        size_t pos = line.rfind(';');

        if (pos == std::string::npos) {
            std::cerr << "Invalid line format: " << line << std::endl;
            continue;
        }

        std::string data = line.substr(0, pos);
        std::string ri_str = line.substr(pos + 1);
        int ri;

        try {
            ri = std::stoi(ri_str);
        } catch (const std::exception& e) {
            std::cerr << "Invalid rand_index in line: " << line << " - " << e.what() << std::endl;
            continue;
        }

        datas.push_back(std::move(data));
        rand_indices.push_back(ri);
    }

    size_t node_count = datas.size();
    if (node_count == 0) return nullptr;
    if (node_count > 1000000) {
        std::cerr << "Number of nodes exceeds limit: " << node_count << std::endl;
        return nullptr;
    }

    // Создаём узлы и заполняем данные
    std::vector<ListNode*> nodes(node_count);
    for (size_t i = 0; i < node_count; ++i) {
        nodes[i] = new ListNode();
        nodes[i]->data = std::move(datas[i]);
        if (nodes[i]->data.size() > 1000) {
            std::cerr << "Data length exceeds limit for node " << i << std::endl;
            for (size_t j = 0; j <= i; ++j) delete nodes[j];
            return nullptr;
        }
    }

    // Второй проход: устанавливаем связи prev/next и rand
    for (size_t i = 0; i < node_count; ++i) {
        if (i > 0) {
            nodes[i]->prev = nodes[i - 1];
            nodes[i - 1]->next = nodes[i];
        }
        int ri = rand_indices[i];
        if (ri != -1) {
            if (ri < 0 || static_cast<size_t>(ri) >= node_count) {
                std::cerr << "Invalid rand_index for node " << i << ": " << ri << std::endl;
                for (size_t j = 0; j < node_count; ++j) delete nodes[j];
                return nullptr;
            }
            nodes[i]->rand = nodes[ri];
        }
    }

    return nodes[0];
}

// Сериализация
void serialize_list(const ListNode* head, const std::string& filename) {
    if (!head) return;

    // Первый проход: строим карту указатель => индекс и считаем количество узлов
    std::unordered_map<const ListNode*, size_t> node_to_index;
    const ListNode* cur = head;
    size_t count = 0;
    while (cur) {
        node_to_index[cur] = count++;
        cur = cur->next;
    }
    std::uint32_t node_count = static_cast<std::uint32_t>(count);

    std::ofstream out(filename, std::ios::binary);
    if (!out.is_open()) {
        std::cerr << "Error opening output file: " << filename << std::endl;
        return;
    }

    // Заголовок: количество узлов (big-endian)
    std::uint32_t node_count_be = htonl(node_count);
    out.write(reinterpret_cast<const char*>(&node_count_be), sizeof(node_count_be));

    // Второй проход: записываем данные каждого узла
    cur = head;
    size_t i = 0;
    while (cur) {
        // 1. Длина строки data
        std::uint32_t data_len = static_cast<std::uint32_t>(cur->data.size());
        std::uint32_t data_len_be = htonl(data_len);
        out.write(reinterpret_cast<const char*>(&data_len_be), sizeof(data_len_be));

        // 2. Сама строка (UTF-8)
        out.write(cur->data.c_str(), data_len);

        // 3. Индексы связей
        std::int32_t prev_idx = (i == 0) ? -1 : static_cast<std::int32_t>(i - 1);
        std::int32_t next_idx = (i == node_count - 1) ? -1 : static_cast<std::int32_t>(i + 1);

        std::int32_t rand_idx = -1;
        if (cur->rand) {
            auto it = node_to_index.find(cur->rand);
            if (it != node_to_index.end()) {
                rand_idx = static_cast<std::int32_t>(it->second);
            }
        }

        // Преобразуем в big-endian и записываем в файл
        std::int32_t prev_be = static_cast<std::int32_t>(htonl(static_cast<std::uint32_t>(prev_idx)));
        std::int32_t next_be = static_cast<std::int32_t>(htonl(static_cast<std::uint32_t>(next_idx)));
        std::int32_t rand_be = static_cast<std::int32_t>(htonl(static_cast<std::uint32_t>(rand_idx)));

        out.write(reinterpret_cast<const char*>(&prev_be), sizeof(prev_be));
        out.write(reinterpret_cast<const char*>(&next_be), sizeof(next_be));
        out.write(reinterpret_cast<const char*>(&rand_be), sizeof(rand_be));

        ++i;
        cur = cur->next;
    }
}

// Десериализация
ListNode* deserialize_list(const std::string& filename) {
    
    std::ifstream in(filename, std::ios::binary);
    if (!in.is_open()) {
        std::cerr << "Error opening input file: " << filename << std::endl;
        return nullptr;
    }

    // Читаем заголовок — количество узлов
    std::uint32_t node_count_be;
    in.read(reinterpret_cast<char*>(&node_count_be), sizeof(node_count_be));
    std::uint32_t node_count = ntohl(node_count_be);

    if (node_count > 1000000) {
        std::cerr << "Number of nodes exceeds limit: " << node_count << std::endl;
        return nullptr;
    }

    // Подготавливаем списки для узлов и индексов
    std::vector<ListNode*> nodes(node_count);
    std::vector<std::int32_t> prev_indices(node_count);
    std::vector<std::int32_t> next_indices(node_count);
    std::vector<std::int32_t> rand_indices(node_count);

    // Первый проход: читаем данные каждого узла в соответствии с бинарным форматом
    for (size_t i = 0; i < node_count; ++i) {
        nodes[i] = new ListNode();

        // 1. Длина строки data
        std::uint32_t data_len_be;
        in.read(reinterpret_cast<char*>(&data_len_be), sizeof(data_len_be));
        std::uint32_t data_len = ntohl(data_len_be);

        if (data_len > 1000) {
            std::cerr << "Data length exceeds limit for node " << i << std::endl;
            for (size_t j = 0; j <= i; ++j) delete nodes[j];
            return nullptr;
        }

        // 2. Строка data (UTF-8)
        std::string data(data_len, '\0');
        in.read(&data[0], data_len);
        nodes[i]->data = std::move(data);

        // 3. Три индекса (prev, next, rand) в big-endian
        std::int32_t prev_be, next_be, rand_be;
        in.read(reinterpret_cast<char*>(&prev_be), sizeof(prev_be));
        in.read(reinterpret_cast<char*>(&next_be), sizeof(next_be));
        in.read(reinterpret_cast<char*>(&rand_be), sizeof(rand_be));

        prev_indices[i] = static_cast<std::int32_t>(ntohl(static_cast<std::uint32_t>(prev_be)));
        next_indices[i] = static_cast<std::int32_t>(ntohl(static_cast<std::uint32_t>(next_be)));
        rand_indices[i] = static_cast<std::int32_t>(ntohl(static_cast<std::uint32_t>(rand_be)));
    }

    // Второй проход: восстанавливаем указатели prev/next/rand по индексам
    for (size_t i = 0; i < node_count; ++i) {
        if (prev_indices[i] != -1) {
            nodes[i]->prev = nodes[prev_indices[i]];
        }
        if (next_indices[i] != -1) {
            nodes[i]->next = nodes[next_indices[i]];
        }
        if (rand_indices[i] != -1) {
            nodes[i]->rand = nodes[rand_indices[i]];
        }
    }

    return node_count > 0 ? nodes[0] : nullptr;
}

void print_list(const ListNode* head) {
    if (!head) return;
    auto current = head;
    int index = 0;
    while (current) {
        std::cout << "Node " << index << ": data=\"" << current->data << "\", ";
        std::cout << "prev=" << current->prev << ", next=" << current->next << ", rand=" << current->rand << std::endl;
        index++;
        current = current->next;
    }
}

void delete_list(ListNode* head) {
    ListNode* current = head;
    while (current) {
        ListNode* next = current->next;
        delete current;
        current = next;
    }
}

int main() {
    ListNode* head = build_from_file("inlet.in");
    if (head) {
        serialize_list(head, "outlet.out");
        delete_list(head);

        ListNode* deserialized_head = deserialize_list("outlet.out");
        if (deserialized_head) {
            std::cout << "Deserialized list:" << std::endl;
            print_list(deserialized_head);
            delete_list(deserialized_head);
        }
    }
    return 0;
}