#include <arpa/inet.h>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

static bool read_full(int fd, char* data, size_t size) {
    size_t total = 0;
    while (total < size) {
        ssize_t n = read(fd, data + total, size - total);
        if (n <= 0) {
            return false;
        }
        total += static_cast<size_t>(n);
    }
    return true;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <filename>\n";
        return EXIT_FAILURE;
    }

    std::string filename = argv[1];
    std::filesystem::path path = std::filesystem::current_path();
    int fd = open((path / filename).c_str(), O_RDONLY);
    if (fd == -1) {
        std::cerr << "Failed to open file: " << filename << "\n";
        return EXIT_FAILURE;
    }

    struct stat statbuf;
    if (fstat(fd, &statbuf) == -1) {
        std::cerr << "Failed to fstat file: " << filename << "\n";
        close(fd);
        return EXIT_FAILURE;
    }

    std::vector<char> file_data(static_cast<size_t>(statbuf.st_size));
    if (!read_full(fd, file_data.data(), file_data.size())) {
        std::cerr << "Failed to read file: " << filename << "\n";
        close(fd);
        return EXIT_FAILURE;
    }

    if (close(fd) == -1) {
        std::cerr << "Failed to close file: " << filename << "\n";
        return EXIT_FAILURE;
    }

    const char* ptr = file_data.data();
    const char* end = ptr + file_data.size();

    uint64_t message_count = 0;
    uint64_t add_order_count = 0;
    uint64_t order_delete_count = 0;
    uint64_t order_cancel_count = 0;
    uint64_t order_replace_count = 0;
    uint64_t unknown_message_count = 0;

    while (ptr + 2 <= end) {
        uint16_t length_be;
        std::memcpy(&length_be, ptr, sizeof(length_be));
        uint16_t length = ntohs(length_be);
        ptr += 2;

        if (length == 0) {
            std::cerr << "Invalid zero-length message\n";
            return EXIT_FAILURE;
        }

        const char* message_ptr = ptr;
        if (message_ptr + length > end) {
            std::cerr << "Truncated message body\n";
            return EXIT_FAILURE;
        }

        char type = message_ptr[0];

        switch (type) {
        case 'A':
            add_order_count++;
            break;
        case 'F':
            add_order_count++;
            break;
        case 'D':
            order_delete_count++;
            break;
        case 'X':
            order_cancel_count++;
            break;
        case 'U':
            order_replace_count++;
            break;
        default:
            unknown_message_count++;
            break;
        }

        message_count++;

        ptr += length;
    }

    std::cout << "Message count: " << message_count << "\n";
    std::cout << "Add order count: " << add_order_count << "\n";
    std::cout << "Order delete count: " << order_delete_count << "\n";
    std::cout << "Order cancel count: " << order_cancel_count << "\n";
    std::cout << "Order replace count: " << order_replace_count << "\n";
    std::cout << "Unknown message count: " << unknown_message_count << "\n";

    return EXIT_SUCCESS;
}
