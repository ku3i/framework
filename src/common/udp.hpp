#ifndef UDP_HPP
#define UDP_HPP

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <time.h>

#include <common/log_messages.h>
#include <common/lock.h>

/*
    This UDP class is optionally able to do a multicast connection.
    Attention: this might spam the network if datagram size and frequency is large.
    Example usage for UDP Multicast Sender and Receiver
    group: 239.255.255.252
    port:  1900

    Otherwise, just specify address (group) and port.

*/

namespace network {

    const uint16_t default_port = 7331;

    template <typename T, typename B>
    std::size_t getfrom(T& var, const B* buf, std::size_t offset)
    {
        var = T{};
        memcpy(&var, buf+offset, sizeof(T));
        return offset+sizeof(T);
    }

    template <typename B>
    bool validate(const B* buf, std::size_t maxl)
    {
        B sum = B{0};
        for (std::size_t i = 0; i < maxl; ++i)
            sum += buf[i];
        return sum == 0;
    }


template <unsigned NBytes>
class UDPSender {

    int sockfd;
    struct sockaddr_in addr{};

    uint8_t msg[NBytes];
    uint8_t buf[NBytes];

    common::mutex_t l_buf{};
    common::mutex_t l_adr{};
    bool tx_ready = false;

public:
    UDPSender(std::string const& group, uint16_t port = default_port)
    : sockfd(socket(AF_INET, SOCK_DGRAM, 0)) /* create UDP socket */
    , msg(), buf()
    {
        /* check for problems*/
        if (sockfd < 0)
            err_msg(__FILE__,__LINE__,"Cannot create UDP socket with group %s and port %u.\n%s", group.c_str(), port, strerror(errno));

        /* set destination */
        common::lock_t lock(l_adr);
        memset(&addr, 0, sizeof(addr)); //TODO constructor
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr(group.c_str());
        addr.sin_port = htons(port);
    }

    void transmit(void)
    {
        common::lock_t lock(l_adr);
        int nbytes = sendto(sockfd, msg, NBytes, 0, (struct sockaddr*) &addr, sizeof(addr));

        { common::lock_t lock(l_buf);
            std::swap(msg, buf); /* swap buffers */
            tx_ready = false; // reset flag, waiting for data
        } /*end lock*/

        if (nbytes < 0)
            wrn_msg("Sending failed: %s", strerror(errno));

        promise(nbytes == NBytes,__FILE__,__LINE__,"Sending failed, message too short.");
    }

    void set_buffer(const uint8_t* src, std::size_t size) {
        assertion(size == NBytes, "Buffer length does not match. %u =!= %u", size, NBytes);
        common::lock_t lock(l_buf);
        memcpy(buf, src, NBytes);
        tx_ready = true; // set flag, ready for transmission
    }

    bool data_ready(void) const { return tx_ready; }

    void change_destination(std::string const& dest, uint16_t port = default_port)
    {
        common::lock_t lock(l_adr);
        addr.sin_addr.s_addr = inet_addr(dest.c_str());
        addr.sin_port = htons(port);
    }
};


template <unsigned NBytes>
class UDPReceiver {

    int sockfd;
    struct sockaddr_in addr{};

    uint8_t msg[NBytes];
    bool rx_ready = false;

public:
    UDPReceiver(const char* group, uint16_t port = default_port, bool multicast = false)
    : sockfd(socket(AF_INET, SOCK_DGRAM, 0)) /* create UDP socket */
    , msg()
    {
        /* check for problems*/
        if (sockfd < 0)
            err_msg(__FILE__,__LINE__,"Cannot create UDP socket with group %s and port %d.\n%s", group, port, strerror(errno));

        /* allow multiple sockets to use the same port */
        unsigned optval = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char*) &optval, sizeof(optval)) < 0)
            err_msg(__FILE__,__LINE__,"Reusing address failed. Cannot allow multiple sockets to use the same port.\n%s", strerror(errno));

        /* set destination */
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY); /* <- Note: allow any, differs from sender */
        addr.sin_port = htons(port);

        /* bind receive address */
        if (bind(sockfd, (struct sockaddr*) &addr, sizeof(addr)) < 0)
            err_msg(__FILE__,__LINE__,"Cannot bind receive address.\n%s", strerror(errno));

        if (multicast) {
            /* request the kernel to join a multicast group */
            struct ip_mreq mreq;
            mreq.imr_multiaddr.s_addr = inet_addr(group);
            mreq.imr_interface.s_addr = htonl(INADDR_ANY);
            if (setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*) &mreq, sizeof(mreq)) < 0)
                err_msg(__FILE__,__LINE__,"Cannot set socket options for multicast group.\n%s", strerror(errno));
        }

        struct timeval read_timeout;
        read_timeout.tv_sec = 1;
        read_timeout.tv_usec = 0;
        if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof(read_timeout)) < 0)
            err_msg(__FILE__,__LINE__,"Cannot set socket options for read timeout.\n%s", strerror(errno));
    }

    bool receive_message(void)
    {
        assert(rx_ready == false && "Received data not acknowledged yet.");

        socklen_t addrlen = sizeof(addr);
        int nbytes = recvfrom(sockfd, msg, NBytes, 0, (struct sockaddr *) &addr, &addrlen);
        if (nbytes <  0) {
            wrn_msg("Receiving failed: %s", strerror(errno));
            return false;
        }

        if (nbytes == 0) return true;

        if (nbytes != NBytes)
            wrn_msg("Invalid message length: %u != %u.", nbytes, NBytes);

        rx_ready = true;
        return true;
    }

    const uint8_t* get_message(void) const { return msg; }

    bool data_received(void) const { return rx_ready; }

    void acknowledge(void) { rx_ready = false; /* clear flag, ready to receive */ }

}; /* class UDP_Receiver */


template <unsigned NBytes>
class Sendbuffer {
	static const unsigned NumSyncBytes = 2;
	static const uint8_t chk_init = 0xFE; /* (0xff + 0xff) % 256 */
	uint16_t  ptr = NumSyncBytes;
	uint8_t   buffer[NBytes];
	uint8_t   checksum = chk_init;
public:
	Sendbuffer()
	{
		static_assert(NBytes > NumSyncBytes, "Invalid buffer size.");
		for (uint8_t i = 0; i < NumSyncBytes; ++i)
			buffer[i] = 0xFF; // init sync bytes once
	}
	void add_byte(uint8_t byte) {
		assertion(ptr < (NBytes-1), "ptr=%u N=%u", ptr, NBytes);
		buffer[ptr++] = byte;
		checksum += byte;
	}

    template <typename T>
	Sendbuffer& add(T var) {
	    const uint8_t * const bytes = (const uint8_t *) &var;
        for (unsigned i = 0; i < sizeof(T); ++i)
            add_byte(bytes[i]);
        return *this;
	}

	void reset(void) { ptr = NumSyncBytes; }

	void add_checksum() {
		assertion(ptr < NBytes, "ptr=%u N=%u", ptr, NBytes);
		buffer[ptr++] = ~checksum + 1; /* two's complement checksum */
		checksum = chk_init;
	}

	const uint8_t* get(void) const { return buffer; }

	uint16_t size(void) const { return ptr; }

}; /* Sendbuffer */

} /* namespace network */

#endif /* UDP_HPP */
