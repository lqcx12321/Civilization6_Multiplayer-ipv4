#pragma once

#include <sockaddrin.h>
#include <map>

// 将真实 IPv4 映射到 127.0.127.x，规避文明6对私有 IPv4 地址的过滤
struct FakeAddrPool {

    struct IPv4Addr {
        in_addr addr;

        IPv4Addr() { memset(&this->addr, 0, sizeof(in_addr)); }

        IPv4Addr(const in_addr &addr) : addr(addr) {}

        bool operator<(const IPv4Addr &other) const {
            return memcmp(&addr, &other.addr, sizeof(in_addr)) < 0;
        }
    };

    std::map<IPv4Addr, IPv4Addr> fake_to_real;
    std::map<IPv4Addr, IPv4Addr> real_to_fake;
    in_addr start;
    in_addr end;
    in_addr current;

    FakeAddrPool(in_addr start, u_long range) {
        this->start = start;
        this->end = next(start, range);
        this->current = start;
    }

    sockaddr_in get_fake_addr(const sockaddr_in *addr) {
        return get_fake_addr(IPv4Addr(addr->sin_addr));
    }

    sockaddr_in get_fake_addr(const IPv4Addr &addr) {
        in_addr fake_ip;
        if (real_to_fake.count(addr)) {
            fake_ip = real_to_fake[addr].addr;
        } else {
            fake_ip = search_available_addr(current);
            IPv4Addr fake_addr4(fake_ip);
            real_to_fake[addr] = fake_addr4;
            fake_to_real[fake_addr4] = addr;
        }
        sockaddr_in fake_addr;
        memset(&fake_addr, 0, sizeof(fake_addr));
        fake_addr.sin_family = AF_INET;
        fake_addr.sin_addr = fake_ip;
        return fake_addr;
    }

    in_addr search_available_addr(in_addr search_start) {
        if (!fake_to_real.count(IPv4Addr(search_start))) {
            return search_start;
        }
        in_addr addr = next(search_start);
        while (memcmp(&addr, &search_start, sizeof(in_addr)) != 0) {
            if (!fake_to_real.count(IPv4Addr(addr))) {
                return addr;
            }
            addr = next(addr);
            if (memcmp(&addr, &search_start, sizeof(in_addr)) == 0) {
                addr = start;
            }
        }
        return search_start;
    }

    in_addr next(const in_addr &addr, int step = 1) {
        ULONG hl = (ULONG)addr.S_un.S_un_b.s_b1 << 24 |
                   (ULONG)addr.S_un.S_un_b.s_b2 << 16 |
                   (ULONG)addr.S_un.S_un_b.s_b3 << 8 |
                   (ULONG)addr.S_un.S_un_b.s_b4;
        hl += step;
        in_addr next_addr;
        next_addr.S_un.S_un_b.s_b1 = (unsigned char)(hl >> 24);
        next_addr.S_un.S_un_b.s_b2 = (unsigned char)(hl >> 16);
        next_addr.S_un.S_un_b.s_b3 = (unsigned char)(hl >> 8);
        next_addr.S_un.S_un_b.s_b4 = (unsigned char)hl;
        return next_addr;
    }
};
