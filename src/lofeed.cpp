#include <arpa/inet.h>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "book/book_replica.h"
#include "proto/itch.h"

int main(int argc, char** argv) {
    if (argc == 2) {
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
            return EXIT_FAILURE;
        }
        void* data = mmap(nullptr, statbuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

        if (data == MAP_FAILED) {
            std::cerr << "Failed to mmap file: " << filename << "\n";
            return EXIT_FAILURE;
        }

        if (close(fd) == -1) {
            std::cerr << "Failed to close file: " << filename << "\n";
            return EXIT_FAILURE;
        }

        BookReplica book_replica(16'000'000);

        const char* ptr = static_cast<const char*>(data);
        const char* end = ptr + statbuf.st_size;

        uint64_t message_count = 0;

        uint64_t add_order_count = 0;
        uint64_t order_delete_count = 0;
        uint64_t order_cancel_count = 0;
        uint64_t order_replace_count = 0;
        uint64_t execute_count = 0;
        uint64_t unknown_message_count = 0;

        uint64_t checksum = 0;
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

            MarketEvent event;
            bool parsed = parse_market_event(message_ptr, event);
            if (!parsed) {
                unknown_message_count++;
                message_count++;
                ptr += length;
                continue;
            } else {
                checksum += event.order_ref;
                checksum += event.quantity;
                checksum += event.price;
                checksum += event.timestamp_ns;
                checksum += event.new_order_ref;
                book_replica.apply(event);
            }

            switch (event.type) {
            case MarketEvent::Type::Add:
                add_order_count++;
                break;
            case MarketEvent::Type::Delete:
                order_delete_count++;
                break;
            case MarketEvent::Type::Cancel:
                order_cancel_count++;
                break;
            case MarketEvent::Type::Replace:
                order_replace_count++;
                break;
            case MarketEvent::Type::Execute:
                execute_count++;
                break;
            default:
                unknown_message_count++;
                break;
            }

            message_count++;

            ptr += length;
        }
        if (munmap(data, statbuf.st_size) == -1) {
            std::cerr << "Failed to munmap file: " << filename << "\n";
            return EXIT_FAILURE;
        }
        std::cout << "Message count: " << message_count << "\n";
        std::cout << "Add order count: " << add_order_count << "\n";
        std::cout << "Order delete count: " << order_delete_count << "\n";
        std::cout << "Order cancel count: " << order_cancel_count << "\n";
        std::cout << "Order replace count: " << order_replace_count << "\n";
        std::cout << "Execute count: " << execute_count << "\n";
        std::cout << "Unknown message count: " << unknown_message_count << "\n";
        std::cout << "Checksum: " << checksum << "\n";

        const BookStats& stats = book_replica.stats();
        std::cout << "Duplicate add: " << stats.duplicate_add << "\n";
        std::cout << "Missing delete: " << stats.missing_delete << "\n";
        std::cout << "Missing cancel: " << stats.missing_cancel << "\n";
        std::cout << "Missing replace: " << stats.missing_replace << "\n";
        std::cout << "Missing execute: " << stats.missing_execute << "\n";
        std::cout << "Over cancel: " << stats.over_cancel << "\n";
        std::cout << "Over execute: " << stats.over_execute << "\n";
        std::cout << "Live orders: " << book_replica.live_orders() << "\n";
    } else {
        std::cerr << "Usage: " << argv[0] << " <filename>\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}